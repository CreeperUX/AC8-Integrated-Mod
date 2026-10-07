// Build-specific offsets independently resolved from the read-only probe and
// native getter disassembly. Only location/rotation and the adjacent float FOV are eligible for changes.
using CameraUpdate = void(__fastcall*)(void*,float);
CameraUpdate original_camera_update=nullptr;
struct CameraCommand { uintptr_t manager=0,pawn=0; double p=0,y=0,r=0; ULONGLONG tick=0; };
std::mutex camera_command_mutex;
CameraCommand camera_command;
// A rejected frame (seen on the transition frame into a Mission005 cutscene) only pauses the override: the
// next attempt waits native_camera_retry_ms and must again pass every gate and validity check. It used to
// disable the camera (and the final camera/HUD snapshot) for the rest of the session.
bool native_camera_fault=false;
ULONGLONG native_camera_retry_tick=0;unsigned native_camera_rejects=0;
constexpr ULONGLONG native_camera_retry_ms=500;
// Which validity check rejected the last attempt (logged; the original log could not tell).
enum NativeCameraReject {CAM_OK=0,CAM_VTABLE,CAM_ROOT,CAM_FOV,CAM_ACTOR,CAM_POV,CAM_COMMAND,CAM_MODE,CAM_RIG_DISTANCE,CAM_EXCEPTION};
int native_camera_reject=CAM_OK,native_camera_fail_reason=CAM_OK;
inline const char* native_camera_reject_name(int r){static const char* n[]={"ok","vtable","root component","fov","actor position","pov","camera command","view mode","rig distance/height","exception"};return r>=0&&r<=CAM_EXCEPTION?n[r]:"?";}
// Single game-thread publisher; try-lock also makes an unexpected concurrent
// callback nonblocking. Old commands still expire at the original 250ms limit.
bool receive_camera(uintptr_t manager,uintptr_t pawn,double p,double y,double r) {
    std::unique_lock<std::mutex> lock(camera_command_mutex,std::try_to_lock);
    if(!lock.owns_lock()) return false;
    camera_command={manager,pawn,p,y,r,GetTickCount64()};
    return true;
}
struct NativeCameraRig {uintptr_t pawn=0;flight::V local{-3000,0,600};bool calibrated=false;};
NativeCameraRig native_camera_rig;
struct CameraSample {double actor[3]{},pov[6]{};float fov=0;};
// Amplify only the game's own zoom excursion; preserve its hold threshold and timing.
struct NativeZoomEnvelope {
    float baseline=0;bool engaged=false;
    void reset(){baseline=0;engaged=false;}
    float step(float original,float factor,bool held){
        if(!std::isfinite(original)||original<5||original>170||!std::isfinite(factor)||factor<1||factor>3){reset();return original;}
        if(!engaged){
            if(!held){baseline=original;return original;}
            if(baseline<=0)baseline=original;
            engaged=true;
        }
        const float base=std::tan(baseline*.5f*flight::rad);
        const float magnification=base/std::tan(original*.5f*flight::rad);
        if(!held&&magnification<=1.001f){engaged=false;baseline=original;return original;}
        if(magnification<=1||factor==1)return original;
        const float enhanced=1+(magnification-1)*factor;
        const float result=2*std::atan(base/enhanced)/flight::rad;
        return std::clamp(result,std::min(original,15.f),original);
    }
};
NativeZoomEnvelope native_zoom;
// Custom zoom keys cannot rely on the game's original RMB binding. Use a
// bounded, smooth optical envelope; original RMB still uses NativeZoomEnvelope.
struct CustomZoomEnvelope {
 float hold_seconds=0,weight=0;
 void reset(){hold_seconds=weight=0;}
 float step(float original,float factor,bool held,float dt){
  if(!std::isfinite(original)||original<5||original>170||!std::isfinite(factor)||factor<1||factor>3||!std::isfinite(dt)||dt<=0||dt>.25f){reset();return original;}
  hold_seconds=held?hold_seconds+dt:0;
  float goal=held&&hold_seconds>=.18f?1.f:0.f;
  weight+=(goal-weight)*(1-std::exp(-12*dt));weight=std::clamp(weight,0.f,1.f);
  if(!held&&weight<.001f){weight=0;return original;}
  if(weight==0)return original; // Preserve exact native FOV before hold threshold.
  // Twofold baseline optical zoom; factor expands its magnification excursion.
  float result=2*std::atan(std::tan(original*.5f*flight::rad)/(1+factor*weight))/flight::rad;
  return std::clamp(result,std::min(15.f,original),original);
 }
};
CustomZoomEnvelope custom_zoom;
bool apply_native_camera(void* manager,const CameraCommand& cmd,int mode,CameraSample& sample,float dt=1.f/60) {
    __try {
        auto base=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
        native_camera_reject=CAM_VTABLE;
        if(*reinterpret_cast<uintptr_t*>(manager)!=base+0xC9D3160) return false;
        native_camera_reject=CAM_ROOT;
        auto root=*reinterpret_cast<uintptr_t*>(cmd.pawn+0x1A0);
        if(root<65536||(root&7)) return false;
        memcpy(sample.actor,reinterpret_cast<const void*>(root+0x220),sizeof(sample.actor));
        memcpy(sample.pov,static_cast<unsigned char*>(manager)+0x14A0,sizeof(sample.pov));
        memcpy(&sample.fov,static_cast<unsigned char*>(manager)+0x14D0,sizeof(float));
        native_camera_reject=CAM_FOV;if(!std::isfinite(sample.fov)||sample.fov<5||sample.fov>170)return false;
        native_camera_reject=CAM_ACTOR;for(double n:sample.actor)if(!std::isfinite(n)||std::abs(n)>1e12)return false;
        native_camera_reject=CAM_POV;for(double n:sample.pov)if(!std::isfinite(n)||std::abs(n)>1e12)return false;
        native_camera_reject=CAM_COMMAND;for(double n:{cmd.p,cmd.y,cmd.r})if(!std::isfinite(n)||std::abs(n)>1e9)return false;
        native_camera_reject=CAM_MODE;if(mode!=0&&mode!=1)return false;
        if(native_rig_reset_requested.exchange(false)||native_camera_rig.pawn!=cmd.pawn){
            native_camera_rig={};native_camera_rig.pawn=cmd.pawn;native_zoom.reset();custom_zoom.reset();
        }
        // Sample the game's framing before our override. Keep this local rig
        // fixed throughout C and its release grace period, so native focus/orbit
        // animation cannot move the pivot or zoom while we are looking around.
        if(!manual_camera_active.load()){
            auto original_axes=flight::basis(float(sample.pov[3]),float(sample.pov[4]),float(sample.pov[5]));
            flight::V delta{float(sample.pov[0]-sample.actor[0]),float(sample.pov[1]-sample.actor[1]),float(sample.pov[2]-sample.actor[2])};
            flight::V local{flight::dot(delta,original_axes.f),flight::dot(delta,original_axes.r),flight::dot(delta,original_axes.u)};
            if(local.x<-100&&flight::dot(local,local)<20000.f*20000.f){
                if(!native_camera_rig.calibrated)log_line("NATIVE_CAMERA_RIG local_cm=(%.1f,%.1f,%.1f)",local.x,local.y,local.z);
                native_camera_rig.local=local;native_camera_rig.calibrated=true;
            }
        }
        auto axes=flight::basis(float(cmd.p),float(cmd.y),float(cmd.r));
        const float distance=camera_distance_cm.load(),height=camera_height_cm.load();
        native_camera_reject=CAM_RIG_DISTANCE;if(!std::isfinite(distance)||!std::isfinite(height)||distance<1000||distance>10000||height<0||height>2000)return false;
        native_camera_reject=CAM_OK;
        auto local=mode==0?native_camera_rig.local:flight::V{-distance,0,height};
        auto offset=axes.f*local.x+axes.r*local.y+axes.u*local.z;
        double result[6]={sample.actor[0]+offset.x,sample.actor[1]+offset.y,sample.actor[2]+offset.z,cmd.p,cmd.y,cmd.r};
        // Preserve post effects; only C+RMB adds a bounded optical zoom.
        memcpy(static_cast<unsigned char*>(manager)+0x14A0,result,sizeof(result));
        memcpy(sample.pov,result,sizeof(result));
        const auto bindings=keybindings.load();
        auto held=[](int key){return (GetAsyncKeyState(key)&0x8000)!=0;};
        const bool zooming=manual_camera_active.load()&&foreground_is_game()&&custom_keys::down(bindings,custom_keys::Action::FreeLook,held)&&custom_keys::down(bindings,custom_keys::Action::Zoom,held);
        const bool original_zoom_key=custom_keys::get(bindings,custom_keys::Action::Zoom)==VK_RBUTTON;
        const float zoomed=original_zoom_key?native_zoom.step(sample.fov,free_look_zoom.load(),zooming):custom_zoom.step(sample.fov,free_look_zoom.load(),zooming,dt);
        if(zoomed!=sample.fov){memcpy(static_cast<unsigned char*>(manager)+0x14D0,&zoomed,sizeof(float));sample.fov=zoomed;}
        return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) { native_camera_reject=CAM_EXCEPTION; return false; }
}
void __fastcall update_native_camera(void* manager,float dt) {
    original_camera_update(manager,dt);
    PerfSpan timing(perf_camera);
    if(!running.load() || !active.load() || !enabled.load() || game_paused.load() || gaze_active.load() || context_suspended.load()) return;
    CameraCommand cmd;
    {
        std::unique_lock<std::mutex> lock(camera_command_mutex,std::try_to_lock);
        if(!lock.owns_lock()) return;
        cmd=camera_command;
    }
    // Report actual ownership interruptions, without relaxing release safeguards.
    // Ignore other camera managers rather than counting them as interruptions.
    if(cmd.manager && reinterpret_cast<uintptr_t>(manager)!=cmd.manager) return;
    if(cmd.manager)rate_camera.fetch_add(1,std::memory_order_relaxed);
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
    const ULONGLONG attempt_tick=GetTickCount64();
    if(native_camera_fault && attempt_tick-native_camera_retry_tick<native_camera_retry_ms) return;
    CameraSample sample;
    if(!apply_native_camera(manager,cmd,camera_view_mode.load(),sample,dt)) {
        if(!native_camera_fault)log_line("native camera disabled: invalid live camera/aircraft data (%s); retrying every %llums",native_camera_reject_name(native_camera_reject),native_camera_retry_ms);
        native_camera_fault=true;native_camera_retry_tick=attempt_tick;++native_camera_rejects;native_camera_fail_reason=native_camera_reject;
        return;
    }
    if(native_camera_fault){
        log_line("native camera recovered after %u rejected attempt(s); last reject: %s",native_camera_rejects,native_camera_reject_name(native_camera_fail_reason));
        native_camera_fault=false;native_camera_rejects=0;
    }
    rate_camera_ok.fetch_add(1,std::memory_order_relaxed);
    effective_camera_fov=sample.fov;effective_fov_manager=reinterpret_cast<uintptr_t>(manager);effective_fov_tick=GetTickCount64();
    view_fov=sample.fov;
    HudFrame frame;
    if(read_pending_hud_frame(frame) && frame.pawn==cmd.pawn && GetTickCount64()-frame.tick<250){
        frame.camera_qpc=hud_qpc();camera_gaps.event(frame.camera_qpc,hud_frequency());frame.camera_sequence=++camera_sequence;frame.camera_manager=cmd.manager;frame.epoch=hud_epoch.load();
        frame.fov=sample.fov;
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
