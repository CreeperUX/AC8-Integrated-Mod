#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdint>
static HWND WINAPI fixture_foreground(){return reinterpret_cast<HWND>(uintptr_t(0x1238));}
static DWORD WINAPI fixture_process(HWND,LPDWORD p){*p=GetCurrentProcessId();return GetCurrentThreadId();}
#define GetForegroundWindow fixture_foreground
#define GetWindowThreadProcessId fixture_process
#include "src/mouse_aim.cpp"
#include <cassert>
static uintptr_t forwarded=0;
static bool forced_arg=false,broadcast_arg=false;
static helmet::Select relocated=nullptr;
static uintptr_t __fastcall relocation_detour(uintptr_t component,bool force,uintptr_t target,bool broadcast){
    return relocated(component,force,target,broadcast)+7;
}
static uintptr_t __fastcall original_fixture(uintptr_t,bool force,uintptr_t target,bool broadcast){
    forwarded=target;forced_arg=force;broadcast_arg=broadcast;return target;
}
int main(){
    alignas(8) std::array<unsigned char,0x1b00> pawn{};
    alignas(8) std::array<unsigned char,0x900> component{};
    uintptr_t items[]={0x20000,0x30000};
    const auto p=reinterpret_cast<uintptr_t>(pawn.data()),c=reinterpret_cast<uintptr_t>(component.data());
    // Execute the real 21-byte prologue, detour, relocated prologue and jump
    // back. A short epilogue returns the third argument without any game calls.
    auto code=static_cast<unsigned char*>(VirtualAlloc(nullptr,4096,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    assert(code);memcpy(code,helmet::entry_bytes,sizeof(helmet::entry_bytes));
    const unsigned char tail[]={0x4c,0x89,0xc0,0x48,0x83,0xc4,0x20,0x41,0x5f,0x5f,0x5d,0x48,0x8b,0x5c,0x24,0x18,0xc3};
    memcpy(code+sizeof(helmet::entry_bytes),tail,sizeof(tail));
    DWORD old=0;assert(VirtualProtect(code,4096,PAGE_EXECUTE_READ,&old));
    FlushInstructionCache(GetCurrentProcess(),code,4096);
    auto entry=reinterpret_cast<helmet::Select>(code);
    assert(entry(c,false,123,false)==123);
    assert(helmet::install_absolute(code,reinterpret_cast<void*>(&relocation_detour),relocated));
    assert(entry(c,false,123,false)==130&&relocated(c,false,456,false)==456);
    helmet::Select rejected=nullptr;
    assert(!helmet::install_absolute(code,reinterpret_cast<void*>(&relocation_detour),rejected)&&!rejected);
    VirtualFree(reinterpret_cast<void*>(relocated),0,MEM_RELEASE);VirtualFree(code,0,MEM_RELEASE);relocated=nullptr;
    puts("PASS executable absolute detour/trampoline roundtrip and changed-signature rejection");
    *reinterpret_cast<uintptr_t*>(p+0x1ae8)=c;
    *reinterpret_cast<uintptr_t*>(c+0x750)=items[0];
    *reinterpret_cast<uintptr_t*>(c+0x870)=reinterpret_cast<uintptr_t>(items);
    *reinterpret_cast<int*>(c+0x878)=2;
    aircraft=p;bridge_thread=GetCurrentThreadId();helmet_enabled=true;enabled=true;active=true;
    game_paused=false;gaze_active=false;context_suspended=false;helmet::original=original_fixture;
    const auto caller=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr))+helmet::release_return_rva;
    auto sample=[&]{helmet::snapshot={p,c,items[0],items[1],GetTickCount64()};};
    sample();assert(helmet::current_member(helmet::snapshot,c));
    assert(helmet::dispatch(caller,c,false,0,false)==items[1]);assert(!forced_arg&&!broadcast_arg);
    assert(!helmet::snapshot.candidate); // never reuse stale selection after an event
    assert(helmet::dispatch(caller,c,false,0,false)==0);
    sample();assert(helmet::dispatch(caller+1,c,false,0,false)==0); // other call site
    sample();assert(helmet::dispatch(caller,c,true,0,false)==0&&forced_arg);
    sample();assert(helmet::dispatch(caller,c,false,0x40000,false)==0x40000);
    sample();assert(helmet::dispatch(caller,c,false,0,true)==0&&broadcast_arg);
    sample();helmet::snapshot.tick=GetTickCount64()-101;assert(helmet::dispatch(caller,c,false,0,false)==0);
    sample();game_paused=true;assert(helmet::dispatch(caller,c,false,0,false)==0);game_paused=false;
    sample();gaze_active=true;assert(helmet::dispatch(caller,c,false,0,false)==0);gaze_active=false;
    sample();context_suspended=true;assert(helmet::dispatch(caller,c,false,0,false)==0);context_suspended=false;
    sample();helmet_enabled=false;assert(helmet::dispatch(caller,c,false,0,false)==0);helmet_enabled=true;
    sample();enabled=false;assert(helmet::dispatch(caller,c,false,0,false)==0);enabled=true;
    sample();bridge_thread=0;assert(helmet::dispatch(caller,c,false,0,false)==0);bridge_thread=GetCurrentThreadId();
    sample();items[1]=0x50000;assert(helmet::dispatch(caller,c,false,0,false)==0); // target left candidate array
    sample();*reinterpret_cast<uintptr_t*>(c+0x750)=0;assert(!helmet::current_member(helmet::snapshot,c));
    *reinterpret_cast<uintptr_t*>(c+0x750)=items[0];
    sample();*reinterpret_cast<uintptr_t*>(p+0x1ae8)=0;assert(!helmet::current_member(helmet::snapshot,c));
    *reinterpret_cast<uintptr_t*>(p+0x1ae8)=c;
    sample();*reinterpret_cast<int*>(c+0x878)=5000;assert(!helmet::current_member(helmet::snapshot,c));
    assert(!helmet::current_member(helmet::snapshot,1));
    puts("PASS HMD player-only dispatch, argument preservation, pause/context/off gates, snapshot expiry, owner/current/member guards");
}
