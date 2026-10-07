// Reference gun cross and HMD-in-free-look support: the nose projection must not depend on the ring being visible,
// the HMD look marker exists only in C free look, and HMD selection follows the view direction while it is held.
// 2.4.1: the cross can be switched off (hud_boresight, Alt+F7 for the session) without touching F7.
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
#include <cstdio>
int main(){
 const uintptr_t P=0x7CCF2CE0;
 running=true;active=true;enabled=true;hud_enabled=true;game_paused=false;gaze_active=false;context_suspended=false;aircraft=P;
 auto frame=[&](float tp,float ty,ULONGLONG age=0){HudFrame f;f.pawn=P;f.tick=GetTickCount64()-age;f.epoch=hud_epoch.load();
  f.p=0;f.y=0;f.r=0;f.cp=0;f.cy=0;f.cr=0;f.tp=tp;f.ty=ty;f.fov=90;f.ox=-3600;f.oy=0;f.oz=600;publish_hud_frame(f);};
 float x=0,y=0,r=0,nx=0,ny=0,lx=0,ly=0;
 // 1. flight target behind the camera (ring status -5): the cross is still projected, just below the screen centre
 frame(0,180);assert(canvas_position_status(P,1920,1080,x,y,r)==-5);
 assert(canvas_nose_position(P,1920,1080,nx,ny)&&std::abs(nx-960)<.5f&&ny>540&&ny<560);
 // 2. flight target off to the side (ring status -7): cross unchanged
 frame(0,80);assert(canvas_position_status(P,1920,1080,x,y,r)==-7);
 assert(canvas_nose_position(P,1920,1080,nx,ny)&&std::abs(nx-960)<.5f);
 // 3. ring visible and on the nose: ring and cross coincide (same 500 m sphere)
 frame(0,0);assert(canvas_position_status(P,1920,1080,x,y,r)==1);
 assert(canvas_nose_position(P,1920,1080,nx,ny)&&std::abs(nx-x)<.01f&&std::abs(ny-y)<.01f);
 // 4. stale frame: neither
 frame(0,0,300);assert(canvas_position_status(P,1920,1080,x,y,r)==-4&&!canvas_nose_position(P,1920,1080,nx,ny));
 // 5. another aircraft's frame
 frame(0,0);aircraft=P+8;assert(canvas_position_status(P,1920,1080,x,y,r)==-3&&!canvas_nose_position(P,1920,1080,nx,ny));aircraft=P;
 puts("PASS reference cross projected from the fresh frame independent of the ring (behind/off-screen), coincident with the ring on target, stale/other-aircraft rejected");
 // 6. HMD look marker only while C free look is held
 frame(0,180);canvas_position_status(P,1920,1080,x,y,r);
 free_look.held=false;look_pitch=0;look_yaw=0;assert(!canvas_look_position(P,1920,1080,lx,ly));
 free_look.held=true;assert(canvas_look_position(P,1920,1080,lx,ly)&&std::abs(lx-960)<.5f);
 look_yaw=180;assert(!canvas_look_position(P,1920,1080,lx,ly));   // view direction behind the camera
 // 7. HMD selection direction: flight target normally, view direction during free look
 target_pitch=5;target_yaw=10;look_pitch=-3;look_yaw=95;float hp=0,hy=0;
 free_look.held=false;assert(!helmet_aim_direction(hp,hy)&&hp==5&&hy==10);
 free_look.held=true;assert(helmet_aim_direction(hp,hy)&&hp==-3&&hy==95);
 free_look.held=false;
 puts("PASS HMD look marker only in C free look; HMD selection follows the view direction while free look is held");
 // 8. 2.4.1 switch: cross shown by default; plain F7 keeps toggling the HUD (also with Ctrl/Shift held for throttle),
 //    Alt+F7 toggles only the cross, Alt with Ctrl/Shift does nothing, no edge does nothing.
 using hud_toggles::F7Action;using hud_toggles::f7_action;
 assert(hud_boresight.load());
 assert(f7_action(true,false,false,false)==F7Action::Hud&&f7_action(true,false,true,false)==F7Action::Hud&&f7_action(true,false,false,true)==F7Action::Hud);
 assert(f7_action(true,true,false,false)==F7Action::GunCross);
 assert(f7_action(true,true,true,false)==F7Action::None&&f7_action(true,true,false,true)==F7Action::None);
 for(int m=0;m<8;++m)assert(f7_action(false,m&1,m&2,m&4)==F7Action::None);
 puts("PASS gun cross shown by default; F7 = HUD (Ctrl/Shift allowed), Alt+F7 = gun cross only, Alt+Ctrl/Shift ignored");
}
