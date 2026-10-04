#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdint>
static HWND WINAPI fixture_foreground(){return reinterpret_cast<HWND>(uintptr_t(0x1238));}
static DWORD WINAPI fixture_process(HWND,LPDWORD p){*p=GetCurrentProcessId();return GetCurrentThreadId();}
static bool fixture_c=false;
static SHORT WINAPI fixture_key(int key){return key=='C'&&fixture_c?SHORT(0x8000):0;}
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
 double originalPOV[6]={99997000,200000000,300000600,0,0,0};memcpy(manager.data()+0x14A0,originalPOV,sizeof(originalPOV));
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
 // Both positions orbit the aircraft with a constant radius during C.
 manager=originalBytes;manual_camera_active=false;native_rig_reset_requested=true;
 command.p=command.y=command.r=0;camera_distance_cm=3600;
 assert(apply_native_camera(manager.data(),command,0,sample));
 manual_camera_active=true;
 // Simulate the game switching to a different native focus rig during a hold.
 double drift[6]={position[0]+15000,position[1]+4000,position[2]+9000,30,110,20};
 for(int mode:{0,1})for(float yaw:{0.f,45.f,90.f,180.f,-90.f})for(float pitch:{-30.f,0.f,30.f}){
  memcpy(manager.data()+0x14A0,drift,sizeof(drift));command.y=yaw;command.p=pitch;
  assert(apply_native_camera(manager.data(),command,mode,sample));
  double radius=0;for(int i=0;i<3;++i)radius+=std::pow(sample.pov[i]-position[i],2);
  double expected=std::sqrt((mode?3600.*3600:3000.*3000)+600.*600);
  assert(std::abs(std::sqrt(radius)-expected)<.01);
 }
 command.p=command.y=command.r=0;manual_camera_active=false;
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
 // Actual C key uses the same controller orbit for short and long holds.
 fixture_c=true;manual_camera_active=true;mouse_delta.add(90,90);receive_pose(pose);
 mouse_delta.add(50,0);receive_pose(pose);
 assert(std::abs(target_yaw.load()-30)<.001&&std::abs(look_yaw.load()-target_yaw.load())>1);
 fixture_c=false;mouse_delta.add(90,90);receive_pose(pose);
 assert(std::abs(target_pitch.load()-10)<.001&&std::abs(target_yaw.load()-30)<.001);manual_camera_active=false;
 // Switching camera profiles alone must not recenter the flight target.
 camera_view_mode=0;receive_pose(pose);camera_view_mode=1;receive_pose(pose);
 assert(std::abs(target_pitch.load()-10)<.001&&std::abs(target_yaw.load()-30)<.001);
 bridge_thread=GetCurrentThreadId();running=true;camera_view_mode=1;
 camera_toggle_requested=true;ac8_mouseaim_begin(nullptr);assert(camera_view_mode==0&&!camera_toggle_requested);
 ac8_mouseaim_begin(nullptr);assert(camera_view_mode==0);
 camera_toggle_requested=true;ac8_mouseaim_begin(nullptr);assert(camera_view_mode==1&&camera_notice_until>GetTickCount64());
 assert(std::abs(target_pitch.load()-10)<.001&&std::abs(target_yaw.load()-30)<.001);
 puts("PASS shared aircraft orbit, frozen native C rig and mouse-follow rotation, 36m geometry, exact48-byte write/FOV preservation, pending recenter, gaze return and view-switch target preservation.");
}
