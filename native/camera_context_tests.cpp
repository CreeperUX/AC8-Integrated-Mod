#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdint>
static HWND WINAPI fixture_foreground(){return reinterpret_cast<HWND>(uintptr_t(0x1238));}
static DWORD WINAPI fixture_process(HWND,LPDWORD p){*p=GetCurrentProcessId();return GetCurrentThreadId();}
static SHORT WINAPI fixture_key(int){return 0;}
#define GetForegroundWindow fixture_foreground
#define GetWindowThreadProcessId fixture_process
#define GetAsyncKeyState fixture_key
#include "src/mouse_aim.cpp"
#include <cassert>
#include <cstdio>
int main(){
 QueryPerformanceFrequency(&perf_frequency);
 alignas(8) std::array<unsigned char,0x300> pawn{},root{};
 alignas(8) std::array<unsigned char,0x1600> manager{};
 manager.fill(0x5a);
 auto base=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));
 *reinterpret_cast<uintptr_t*>(manager.data())=base+0xC9D3160;
 *reinterpret_cast<uintptr_t*>(pawn.data()+0x1A0)=reinterpret_cast<uintptr_t>(root.data());
 double position[3]={100000000,200000000,300000000};memcpy(root.data()+0x220,position,sizeof(position));
 double originalPOV[6]={99997000,200000000,300000600,2,3,4};memcpy(manager.data()+0x14A0,originalPOV,sizeof(originalPOV));
 float fov=77.5f;memcpy(manager.data()+0x14D0,&fov,sizeof(fov));
 auto originalBytes=manager;
 CameraCommand command;command.pawn=reinterpret_cast<uintptr_t>(pawn.data());command.p=0;command.y=0;command.r=0;
 CameraSample sample;
 assert(apply_native_camera(manager.data(),command,0,sample));
 assert(sample.pov[0]==originalPOV[0]&&sample.pov[1]==originalPOV[1]&&sample.pov[2]==originalPOV[2]);
 assert(sample.pov[3]==0&&sample.pov[4]==0);
 for(size_t i=0;i<manager.size();++i)if(i<0x14B8||i>=0x14D0)assert(manager[i]==originalBytes[i]);
 command.p=12;command.y=77;command.r=-3;
 assert(apply_native_camera(manager.data(),command,0,sample));auto nativeView=sample;
 assert(apply_native_camera(manager.data(),command,1,sample));
 for(int i=3;i<6;++i)assert(sample.pov[i]==nativeView.pov[i]);
 assert(sample.pov[0]!=nativeView.pov[0]);
 command.p=0;command.y=0;command.r=0;
 camera_distance_cm=3600;camera_height_cm=600;
 assert(apply_native_camera(manager.data(),command,1,sample));
 assert(sample.pov[0]==position[0]-3600&&sample.pov[1]==position[1]&&sample.pov[2]==position[2]+600);
 for(size_t i=0;i<manager.size();++i)if(i<0x14A0||i>=0x14D0)assert(manager[i]==originalBytes[i]);
 assert(*reinterpret_cast<float*>(manager.data()+0x14D0)==fov);
 camera_distance_cm=3000;command.y=90;
 assert(apply_native_camera(manager.data(),command,1,sample));assert(std::abs(sample.pov[1]-(position[1]-3000))<.01);
 camera_distance_cm=NAN;auto beforeInvalid=manager;assert(!apply_native_camera(manager.data(),command,1,sample));assert(manager==beforeInvalid);
 camera_distance_cm=3600;
 // Native-mode HUD uses the actual final game POV, including its position.
 manager=originalBytes;running=true;active=true;enabled=true;context_suspended=false;game_paused=false;gaze_active=false;
 aircraft=command.pawn;pose_tick=GetTickCount64();camera_view_mode=0;
 original_camera_update=+[](void*,float){};
 receive_camera(reinterpret_cast<uintptr_t>(manager.data()),command.pawn,0,0,0);
 HudFrame frame;frame.pawn=command.pawn;frame.tick=GetTickCount64();frame.fov=fov;stage_hud_frame(frame);
 update_native_camera(manager.data(),.016f);
 HudFrame displayed;assert(read_hud_frame(displayed));
 assert(memcmp(manager.data()+0x14A0,originalBytes.data()+0x14A0,24)==0);
 assert(displayed.cp==0&&displayed.cy==0&&displayed.ox==-3000&&displayed.oz==600&&displayed.fov==fov);
 originalBytes=manager;
 camera_view_mode=1;context_suspended=true;update_native_camera(manager.data(),.016f);assert(manager==originalBytes);
 context_suspended=false;game_paused=true;update_native_camera(manager.data(),.016f);assert(manager==originalBytes);game_paused=false;
 // A suspended context cannot steer and cannot consume the pending one-shot centre.
 active=true;enabled=true;game_paused=false;gaze_active=false;context_suspended=true;transition_center_requested=true;
 target_pitch=60;target_yaw=-120;command_pitch=.8f;command_roll=.5f;command_yaw=.2f;
 update_commands();assert(command_pitch==0&&command_roll==0&&command_yaw==0&&transition_center_requested);
 double pose[13]={65536,10,30,0,10,30,0,80,-3000,0,600,0,0};
 context_suspended=false;resume_center_requested=true;mouse_delta.add(500,500);
 receive_pose(pose);
 assert(!transition_center_requested&&!resume_center_requested);
 assert(std::abs(target_pitch.load()-10)<.001&&std::abs(target_yaw.load()-30)<.001);
 // C/native gaze is a temporary yield: returning preserves the existing world target.
 pose[12]=1;receive_pose(pose);pose[1]=20;pose[2]=40;pose[12]=0;receive_pose(pose);
 assert(std::abs(target_pitch.load()-10)<.001&&std::abs(target_yaw.load()-30)<.001);
 // Switching camera profiles alone must not recenter the flight target.
 camera_view_mode=0;receive_pose(pose);camera_view_mode=1;receive_pose(pose);
 assert(std::abs(target_pitch.load()-10)<.001&&std::abs(target_yaw.load()-30)<.001);
 bridge_thread=GetCurrentThreadId();running=true;camera_view_mode=1;
 camera_toggle_requested=true;ac8_mouseaim_begin(nullptr);assert(camera_view_mode==0&&!camera_toggle_requested);
 ac8_mouseaim_begin(nullptr);assert(camera_view_mode==0);
 camera_toggle_requested=true;ac8_mouseaim_begin(nullptr);assert(camera_view_mode==1&&camera_notice_until>GetTickCount64());
 assert(std::abs(target_pitch.load()-10)<.001&&std::abs(target_yaw.load()-30)<.001);
 puts("PASS native position plus shared mouse-follow rotation, 36m geometry, exact48-byte write/FOV preservation, pending recenter, gaze return and view-switch target preservation.");
}
