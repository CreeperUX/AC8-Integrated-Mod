// AC8 build 25201480. SelectTarget(bool bForcedSelect, LiveGameObject*
// OverrideTarget, bool bForceBroadcast). Only the player release call site is
// eligible; automatic, scripted and weapon-change calls retain stock behavior.
#include <intrin.h>
namespace helmet {
constexpr uintptr_t select_rva=0x7296f40, release_return_rva=0x7d630e0;
struct Snapshot {uintptr_t pawn=0,component=0,selected=0,candidate=0;uint64_t tick=0;};
Snapshot snapshot; // Written and consumed only on the verified game thread.
using Select=uintptr_t(__fastcall*)(uintptr_t,bool,uintptr_t,bool);
Select original=nullptr;
bool supported=false;
// Whole instructions, no relative branch or RIP-relative memory operand.
// Include movss xmm0,[rcx+0x6fc] so a 14-byte absolute jump never splits it.
constexpr unsigned char entry_bytes[]={0x48,0x89,0x5c,0x24,0x18,0x55,0x57,0x41,0x57,0x48,0x83,0xec,0x20,
    0xf3,0x0f,0x10,0x81,0xfc,0x06,0x00,0x00};
bool install_absolute(unsigned char* target,void* detour,Select& trampoline_out) {
    if(memcmp(target,entry_bytes,sizeof(entry_bytes))!=0)return false;
    constexpr size_t jump_size=14,copy_size=sizeof(entry_bytes),size=copy_size+jump_size;
    auto trampoline=static_cast<unsigned char*>(VirtualAlloc(nullptr,size,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    if(!trampoline)return false;
    auto jump=[](unsigned char* dst,const void* to){
        const unsigned char opcode[]={0xff,0x25,0,0,0,0};
        memcpy(dst,opcode,sizeof(opcode));memcpy(dst+6,&to,sizeof(to));
    };
    memcpy(trampoline,target,copy_size);jump(trampoline+copy_size,target+copy_size);
    DWORD protection=0;
    if(!VirtualProtect(trampoline,size,PAGE_EXECUTE_READ,&protection)){VirtualFree(trampoline,0,MEM_RELEASE);return false;}
    FlushInstructionCache(GetCurrentProcess(),trampoline,size);
    if(!VirtualProtect(target,copy_size,PAGE_EXECUTE_READWRITE,&protection)){VirtualFree(trampoline,0,MEM_RELEASE);return false;}
    // Publish the callable original before the entry point becomes reachable.
    trampoline_out=reinterpret_cast<Select>(trampoline);
    jump(target,detour);memset(target+jump_size,0x90,copy_size-jump_size);
    FlushInstructionCache(GetCurrentProcess(),target,copy_size);
    DWORD ignored=0;VirtualProtect(target,copy_size,protection,&ignored);
    return true;
}

// Never dereference a cached target unless it is still in the engine-owned
// candidate array. Check owner and selected state again at the actual key event.
bool current_member(const Snapshot& s,uintptr_t component) {
    __try {
        if(!s.candidate||s.component!=component||s.pawn!=aircraft.load())return false;
        if(*reinterpret_cast<uintptr_t*>(s.pawn+0x1ae8)!=component)return false;
        if(*reinterpret_cast<uintptr_t*>(component+0x750)!=s.selected)return false;
        auto data=*reinterpret_cast<const uintptr_t**>(component+0x870);
        auto count=*reinterpret_cast<const int*>(component+0x878);
        if(!data||count<0||count>4096)return false;
        for(int i=0;i<count;++i)if(data[i]==s.candidate)return true;
    } __except(EXCEPTION_EXECUTE_HANDLER) {return false;}
    return false;
}
uintptr_t dispatch(uintptr_t caller,uintptr_t component,bool forced,uintptr_t target,bool broadcast) {
    const auto base=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
    if(caller==base+release_return_rva && !forced && !target && !broadcast &&
       on_bridge_thread() && helmet_enabled.load() && enabled.load() && active.load() &&
       !game_paused.load() && !gaze_active.load() && !context_suspended.load() &&
       foreground_is_game() && GetTickCount64()-snapshot.tick<=100 &&
       current_member(snapshot,component)) {
        target=snapshot.candidate;
        snapshot={}; // One use; a second event needs a new game-thread sample.
        const auto result=original(component,forced,target,broadcast);
        log_line("HMD_SELECT requested=%llX selected=%llX accepted=%d",
            static_cast<unsigned long long>(target),static_cast<unsigned long long>(result),result==target);
        return result;
    }
    return original(component,forced,target,broadcast);
}
uintptr_t __fastcall select(uintptr_t component,bool forced,uintptr_t target,bool broadcast) {
    return dispatch(reinterpret_cast<uintptr_t>(_ReturnAddress()),component,forced,target,broadcast);
}
bool install() {
    auto base=reinterpret_cast<unsigned char*>(GetModuleHandleW(nullptr));
    const unsigned char caller[]={0x48,0x8b,0x8b,0xe8,0x1a,0,0,0x45,0x33,0xc9,0x45,0x33,0xc0,0x33,0xd2,0xe8};
    bool match=false;
    __try {match=memcmp(base+select_rva,entry_bytes,sizeof(entry_bytes))==0 &&
        memcmp(base+0x7d630cc,caller,sizeof(caller))==0 &&
        *reinterpret_cast<const int32_t*>(base+0x7d630dc)==int32_t(select_rva-release_return_rva);
    } __except(EXCEPTION_EXECUTE_HANDLER) {match=false;}
    if(!match){log_line("HMD unavailable: selection signature mismatch; stock selection retained");return false;}
    auto status=MH_CreateHook(base+select_rva,reinterpret_cast<void*>(&select),reinterpret_cast<void**>(&original));
    if(status==MH_ERROR_MEMORY_ALLOC){
        supported=install_absolute(base+select_rva,reinterpret_cast<void*>(&select),original);
        log_line(supported?"HMD ready: absolute trampoline fallback (near allocation unavailable); F2 toggle":
            "HMD unavailable: absolute trampoline fallback failed; stock selection retained");
        return supported;
    }
    if(status!=MH_OK){log_line("HMD unavailable: create hook %s",MH_StatusToString(status));return false;}
    status=MH_EnableHook(base+select_rva);
    if(status!=MH_OK){MH_RemoveHook(base+select_rva);original=nullptr;log_line("HMD unavailable: enable hook %s",MH_StatusToString(status));return false;}
    supported=true;log_line("HMD ready: F2 toggles mouse-priority selection; original target-switch binding retained");return true;
}
}
