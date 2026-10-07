// Cutscene recovery: one rejected camera frame (Mission005: the frame before VIEW_CONTEXT suspended=1) must not
// stop the native camera and the final camera/HUD snapshot for the rest of the session.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdint>
static HWND WINAPI fixture_foreground(){return reinterpret_cast<HWND>(uintptr_t(0x1238));}
static bool fixture_foreground_game=true;
static DWORD WINAPI fixture_process(HWND,LPDWORD p){*p=fixture_foreground_game?GetCurrentProcessId():GetCurrentProcessId()+1;return GetCurrentThreadId();}
static SHORT WINAPI fixture_key(int){return 0;}
static ULONGLONG fixture_now=10'000'000;
static ULONGLONG WINAPI fixture_tick(){return fixture_now;}
#define GetForegroundWindow fixture_foreground
#define GetWindowThreadProcessId fixture_process
#define GetAsyncKeyState fixture_key
#define GetTickCount64 fixture_tick
#include "src/mouse_aim.cpp"
#include <cassert>
#include <cstdio>

alignas(8) static std::array<unsigned char,0x300> pawn{},root{};
alignas(8) static std::array<unsigned char,0x1600> manager{};
static uintptr_t P=0;
static void set_fov(float f){memcpy(manager.data()+0x14D0,&f,sizeof(f));}
// One game frame: Lua bridge refreshes the camera command and pose, the game thread stages its snapshot, then the
// camera manager's post-update runs.
static void frame(ULONGLONG ms=16){
 fixture_now+=ms;
 receive_camera(reinterpret_cast<uintptr_t>(manager.data()),P,0,0,0);pose_tick=fixture_now;
 HudFrame f;f.pawn=P;f.tick=fixture_now;f.cp=0;f.cy=0;f.fov=77.5f;f.ox=-3000;f.oz=600;stage_hud_frame(f);
 update_native_camera(manager.data(),.016f);
}
static uint64_t published(){return rate_publish_ok.load();}
static bool snapshot_fresh(){HudFrame f;return read_hud_frame(f)&&f.pawn==P&&fixture_now-f.tick<250;}

int main(){
 QueryPerformanceFrequency(&perf_frequency);
 manager.fill(0x5a);
 const auto base=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
 *reinterpret_cast<uintptr_t*>(manager.data())=base+0xC9D3160;
 *reinterpret_cast<uintptr_t*>(pawn.data()+0x1A0)=reinterpret_cast<uintptr_t>(root.data());
 double position[3]={1e8,2e8,3e8};memcpy(root.data()+0x220,position,sizeof(position));
 double pov[6]={1e8-3000,2e8,3e8+600,0,0,0};memcpy(manager.data()+0x14A0,pov,sizeof(pov));
 set_fov(77.5f);
 P=reinterpret_cast<uintptr_t>(pawn.data());
 running=true;active=true;enabled=true;context_suspended=false;game_paused=false;gaze_active=false;
 aircraft=P;camera_view_mode=0;camera_distance_cm=3600;camera_height_cm=600;
 original_camera_update=+[](void*,float){};

 // 1. Normal operation publishes every frame.
 for(int i=0;i<5;++i)frame();
 const uint64_t normal=published();assert(normal>=5&&snapshot_fresh()&&!native_camera_fault);

 // 2. One invalid frame (FOV=0 stands in for whichever live check fails; the actual failing field in the
 //    Mission005 logs is unknown) faults the camera; nothing is published and the manager is left untouched.
 set_fov(0);auto before_bad=manager;frame();
 assert(native_camera_fault&&published()==normal&&native_camera_fail_reason==CAM_FOV);
 assert(memcmp(manager.data()+0x14A0,before_bad.data()+0x14A0,0x34)==0);
 set_fov(77.5f);

 // 3. Valid data again: no retry inside the 500 ms back-off ...
 for(int i=0;i<20;++i)frame(16);                        // 320 ms
 assert(native_camera_fault&&published()==normal);
 // ... then the override and the final camera/HUD snapshot come back (the old code stayed dead > 5 s).
 for(int i=0;i<20;++i)frame(16);                        // 640 ms after the fault
 assert(!native_camera_fault&&published()>normal&&snapshot_fresh());
 puts("PASS one rejected frame pauses the native camera; valid data recovers it after the 500 ms back-off");

 // 4. Persistently invalid data keeps it paused: retries at most every 500 ms, logged once, manager untouched.
 set_fov(0);const uint64_t p4=published();const unsigned r0=native_camera_rejects;
 for(int i=0;i<125;++i)frame(16);                       // 2 s
 assert(native_camera_fault&&published()==p4);
 const unsigned retries=native_camera_rejects-r0;assert(retries>=4&&retries<=5);
 // 5. A retry still requires every gate: suspended (cutscene), paused, gaze, other aircraft, foreground lost,
 //    stale command/pose -- no attempt, even with valid data and the back-off elapsed.
 set_fov(77.5f);fixture_now+=600;
 auto blocked=[&](auto&& setup,auto&& restore){const unsigned r=native_camera_rejects;const uint64_t p=published();
  setup();frame(16);const bool none=native_camera_fault&&native_camera_rejects==r&&published()==p;restore();return none;};
 assert(blocked([]{context_suspended=true;},[]{context_suspended=false;}));
 assert(blocked([]{game_paused=true;},[]{game_paused=false;}));
 assert(blocked([]{gaze_active=true;},[]{gaze_active=false;}));
 assert(blocked([]{fixture_foreground_game=false;},[]{fixture_foreground_game=true;}));
 assert(blocked([]{aircraft=P+8;},[]{aircraft=P;}));
 {const unsigned r=native_camera_rejects;const uint64_t p=published();fixture_now+=300;pose_tick=fixture_now-300;   // stale pose
  update_native_camera(manager.data(),.016f);assert(native_camera_fault&&native_camera_rejects==r&&published()==p);}
 frame(16);assert(!native_camera_fault&&snapshot_fresh());
 puts("PASS retries are rate-limited and still require matching camera, aircraft, fresh pose/command, foreground, no pause/gaze/cutscene");

 // 6. Mouse input (independent consumption path): prefers the final camera snapshot; when that is stale it uses
 //    the game thread's fresh pose/view snapshot; every original guard still applies.
 independent_mouse=true;recenter_requested=false;resume_center_requested=false;transition_center_requested=false;
 free_look.reset();mouse_delta.clear();target_pitch=0;target_yaw=0;
 auto stage=[](uintptr_t pawn_,ULONGLONG age){HudFrame f;f.pawn=pawn_;f.tick=fixture_now-age;f.fov=77.5f;f.ox=-3000;f.oz=600;stage_hud_frame(f);};
 auto stale_final=[]{HudFrame f;f.pawn=P;f.tick=fixture_now-400;f.fov=77.5f;publish_hud_frame(f);};
 auto moved=[](int dx){const float y0=target_yaw.load();mouse_delta.add(dx,0);target_input_tick();return std::abs(target_yaw.load()-y0)>1e-4f;};
 fixture_now+=1000;stale_final();stage(P,0);
 assert(moved(20));                                     // fallback keeps the mouse alive
 stage(P,300);assert(!moved(20));                       // both snapshots stale: rejected, delta dropped
 assert(mouse_delta.take().x==0);
 stage(P+8,0);assert(!moved(20));                       // another aircraft's snapshot
 stage(P,0);game_paused=true;assert(!moved(20));game_paused=false;
 context_suspended=true;assert(!moved(20));context_suspended=false;
 gaze_active=true;assert(!moved(20));gaze_active=false;
 assert(moved(20));
 // A fresh final snapshot is still preferred: with it fresh, an unusable staged snapshot does not matter.
 {HudFrame f;f.pawn=P;f.tick=fixture_now;f.fov=77.5f;publish_hud_frame(f);stage(P+8,0);assert(moved(20));stage(P,300);assert(moved(20));}
 independent_mouse=false;
 puts("PASS mouse input falls back to the fresh game-thread snapshot; stale, other-aircraft, pause, cutscene and gaze still reject");
 puts("PASS cutscene recovery");
}
