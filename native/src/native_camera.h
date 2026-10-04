// Build-specific offsets independently resolved from the read-only probe and
// native getter disassembly. Only the first 48 bytes of final camera POV change.
using CameraUpdate = void(__fastcall*)(void*,float);
CameraUpdate original_camera_update=nullptr;
struct CameraCommand { uintptr_t manager=0,pawn=0; double p=0,y=0,r=0; ULONGLONG tick=0; };
std::mutex camera_command_mutex;
CameraCommand camera_command;
bool native_camera_fault=false;
// Single game-thread publisher; try-lock also makes an unexpected concurrent
// callback nonblocking. Old commands still expire at the original 250ms limit.
bool receive_camera(uintptr_t manager,uintptr_t pawn,double p,double y,double r) {
    std::unique_lock<std::mutex> lock(camera_command_mutex,std::try_to_lock);
    if(!lock.owns_lock()) return false;
    camera_command={manager,pawn,p,y,r,GetTickCount64()};
    return true;
}
struct CameraSample {double actor[3]{},pov[6]{};};
bool apply_native_camera(void* manager,const CameraCommand& cmd,int mode,CameraSample& sample) {
    __try {
        auto base=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
        if(*reinterpret_cast<uintptr_t*>(manager)!=base+0xC9D3160) return false;
        auto root=*reinterpret_cast<uintptr_t*>(cmd.pawn+0x1A0);
        if(root<65536||(root&7)) return false;
        memcpy(sample.actor,reinterpret_cast<const void*>(root+0x220),sizeof(sample.actor));
        memcpy(sample.pov,static_cast<unsigned char*>(manager)+0x14A0,sizeof(sample.pov));
        for(double n:sample.actor)if(!std::isfinite(n)||std::abs(n)>1e12)return false;
        for(double n:sample.pov)if(!std::isfinite(n)||std::abs(n)>1e12)return false;
        // Native mode only samples the finished game camera. No POV/FOV writes.
        if(mode==0)return true;
        auto axes=flight::basis(float(cmd.p),float(cmd.y),float(cmd.r));
        const float distance=camera_distance_cm.load(),height=camera_height_cm.load();
        if(!std::isfinite(distance)||!std::isfinite(height)||distance<1000||distance>10000||height<0||height>2000)return false;
        auto offset=axes.f*(-distance)+axes.u*height;
        double result[6]={sample.actor[0]+offset.x,sample.actor[1]+offset.y,sample.actor[2]+offset.z,cmd.p,cmd.y,cmd.r};
        // Exactly location+rotation; preserve the game's FOV and post effects.
        memcpy(static_cast<unsigned char*>(manager)+0x14A0,result,sizeof(result));
        memcpy(sample.pov,result,sizeof(result));
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) { return false; }
}
void __fastcall update_native_camera(void* manager,float dt) {
    original_camera_update(manager,dt);
    PerfSpan timing(perf_camera);
    if(native_camera_fault || !running.load() || !active.load() || !enabled.load() || game_paused.load() || gaze_active.load() || context_suspended.load()) return;
    CameraCommand cmd;
    {
        std::unique_lock<std::mutex> lock(camera_command_mutex,std::try_to_lock);
        if(!lock.owns_lock()) return;
        cmd=camera_command;
    }
    // Report actual ownership interruptions, without relaxing release safeguards.
    // Ignore other camera managers rather than counting them as interruptions.
    if(cmd.manager && reinterpret_cast<uintptr_t>(manager)!=cmd.manager) return;
    const char* reason=nullptr;
    if(!cmd.manager) reason="Lua release";
    else if(cmd.pawn!=aircraft.load()) reason="aircraft changed";
    else if(GetTickCount64()-cmd.tick>250) reason="camera command timeout";
    else if(GetTickCount64()-pose_tick.load()>250) reason="pose timeout";
    else if(!foreground_is_game()) reason="foreground lost";
    static const char* previous_reason=nullptr;
    if(reason!=previous_reason) {
        log_line("native camera ownership: %s",reason?reason:"resumed");
        previous_reason=reason;
    }
    if(reason) return;
    CameraSample sample;
    if(!apply_native_camera(manager,cmd,camera_view_mode.load(),sample)) {
        native_camera_fault=true;
        log_line("native camera disabled: invalid live camera/aircraft data");
        return;
    }
    HudFrame frame;
    if(read_pending_hud_frame(frame) && frame.pawn==cmd.pawn && GetTickCount64()-frame.tick<250){
        frame.cp=float(sample.pov[3]);frame.cy=float(sample.pov[4]);frame.cr=float(sample.pov[5]);
        frame.ox=float(sample.pov[0]-sample.actor[0]);
        frame.oy=float(sample.pov[1]-sample.actor[1]);
        frame.oz=float(sample.pov[2]-sample.actor[2]);
        // Keep the original pose timestamp: camera-only ticks cannot freshen stale aim.
        publish_hud_frame(frame);
    }
    static bool reported=false;
    if(!reported) { reported=true; log_line("native camera POST-UPDATE ACTIVE: live aircraft position, final POV cache"); }
}
bool install_native_camera() {
    auto* base=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
    auto* dos=reinterpret_cast<IMAGE_DOS_HEADER*>(base);
    auto* nt=reinterpret_cast<IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);
    const unsigned char update_bytes[]={0x48,0x8B,0xC4,0x48,0x89,0x58,0x18,0x57,0x48,0x81,0xEC,0x40,0x01,0,0};
    const unsigned char cache_bytes[]={0x48,0x8D,0x81,0xA0,0x14,0,0,0xC3};
    const unsigned char actor_bytes[]={0x48,0x8B,0x81,0xA0,0x01,0,0};
    if(nt->FileHeader.TimeDateStamp!=0x6AA0E27D || nt->OptionalHeader.SizeOfImage!=0x2195A000 ||
       memcmp(base+0x70C8ED0,update_bytes,sizeof(update_bytes)) ||
       memcmp(base+0x48FC6B0,cache_bytes,sizeof(cache_bytes)) ||
       memcmp(base+0x3FD7430+0x19,actor_bytes,sizeof(actor_bytes))) {
        log_line("native camera refused: executable identity/signature mismatch"); return false;
    }
    auto slot=reinterpret_cast<void**>(base+0xC9D3160+241*sizeof(void*));
    if(*slot!=base+0x70C8ED0) { log_line("native camera refused: vtable slot already changed"); return false; }
    DWORD protection=0;
    if(!VirtualProtect(slot,sizeof(void*),PAGE_READWRITE,&protection)) return false;
    original_camera_update=reinterpret_cast<CameraUpdate>(*slot);
    InterlockedExchangePointer(slot,reinterpret_cast<void*>(&update_native_camera));
    DWORD ignored=0; VirtualProtect(slot,sizeof(void*),protection,&ignored);
    log_line("native camera post-update registered; activation requires matching live manager and fresh pose");
    return true;
}
