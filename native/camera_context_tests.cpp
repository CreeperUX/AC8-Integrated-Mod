#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <cstdint>
static HWND WINAPI fixture_foreground(){return reinterpret_cast<HWND>(uintptr_t(0x1238));}
static DWORD WINAPI fixture_process(HWND,LPDWORD p){*p=GetCurrentProcessId();return GetCurrentThreadId();}
static bool fixture_c=false,fixture_rmb=false;static bool fixture_keys[256]{};
static SHORT WINAPI fixture_key(int key){return ((key=='C'&&fixture_c)||(key==VK_RBUTTON&&fixture_rmb)||(key>0&&key<256&&fixture_keys[key]))?SHORT(0x8000):0;}
#define GetForegroundWindow fixture_foreground
#define GetWindowThreadProcessId fixture_process
#define GetAsyncKeyState fixture_key
#include "src/mouse_aim.cpp"
#include <cassert>
#include <cstdio>
int main(){
 // Runtime sensitivity: edge-triggered, gated, bounded, no held-key repeat.
 configured_sensitivity=.1f;live_sensitivity=.1f;
 sensitivity_keys(false,false,false,true);sensitivity_keys(true,false,false,false);assert(live_sensitivity.load()==.1f);
 sensitivity_keys(true,false,false,true);assert(live_sensitivity.load()==.1f);
 sensitivity_keys(false,false,false,true);sensitivity_keys(true,false,false,true);assert(std::abs(live_sensitivity.load()-.11f)<.00001f);
 sensitivity_keys(true,false,false,true);assert(std::abs(live_sensitivity.load()-.11f)<.00001f);
 sensitivity_keys(false,false,false,true);sensitivity_keys(false,true,false,true);assert(std::abs(live_sensitivity.load()-.1f)<.00001f);
 live_sensitivity=1; sensitivity_keys(true,false,false,true);assert(live_sensitivity.load()==1);
 sensitivity_keys(false,false,false,true);live_sensitivity=.01f;sensitivity_keys(false,true,false,true);assert(live_sensitivity.load()==.01f);
 sensitivity_keys(false,false,true,true);assert(live_sensitivity.load()==configured_sensitivity.load());
 sensitivity_keys(false,false,false,true);
 configured_zoom=1.75f;free_look_zoom=1.75f;
 zoom_keys(false,false,false,true);zoom_keys(true,false,false,false);assert(free_look_zoom==1.75f);
 zoom_keys(true,false,false,true);assert(free_look_zoom==1.75f);
 zoom_keys(false,false,false,true);zoom_keys(true,false,false,true);assert(free_look_zoom==2.f);
 zoom_keys(true,false,false,true);assert(free_look_zoom==2.f);
 zoom_keys(false,false,false,true);free_look_zoom=3;zoom_keys(true,false,false,true);assert(free_look_zoom==3);
 zoom_keys(false,false,false,true);free_look_zoom=1;zoom_keys(false,true,false,true);assert(free_look_zoom==1);
 zoom_keys(false,false,true,true);assert(free_look_zoom==1.75f);zoom_keys(false,false,false,true);
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
 // Follow stock zoom timing: a press with unchanged native FOV must never jump.
 NativeZoomEnvelope envelope;assert(envelope.step(80,1.75,false)==80);
 assert(envelope.step(80,1.75,true)==80);
 float prev=80;
 for(float native:{78.f,74.f,68.f,60.f,50.f}){float value=envelope.step(native,1.75,true);assert(value<prev&&value<=native);prev=value;}
 float held=envelope.step(50,1.75,true);assert(held==prev);
 for(float native:{54.f,62.f,72.f,80.f}){float value=envelope.step(native,1.75,false);assert(value>=prev);prev=value;}
 assert(prev==80&&!envelope.engaged);
 NativeZoomEnvelope identity;identity.step(80,1,false);assert(identity.step(50,1,true)==50);
 // Final POV and HUD share amplified native FOV; no change outside the52-byte region.
 manager=originalBytes;manual_camera_active=false;fixture_c=fixture_rmb=false;
 update_native_camera(manager.data(),.016f);
 manual_camera_active=true;fixture_c=fixture_rmb=true;manager=originalBytes;
 update_native_camera(manager.data(),.016f);assert(*reinterpret_cast<float*>(manager.data()+0x14D0)==fov);
 float nativeFov=50;manager=originalBytes;memcpy(manager.data()+0x14D0,&nativeFov,4);
 update_native_camera(manager.data(),.016f);assert(read_hud_frame(displayed));
 float zoomed=*reinterpret_cast<float*>(manager.data()+0x14D0);assert(zoomed<nativeFov&&displayed.fov==zoomed&&view_fov==zoomed);
 for(size_t i=0;i<manager.size();++i)if(i<0x14A0||i>=0x14D4)assert(manager[i]==originalBytes[i]);
 fixture_c=fixture_rmb=false;manager=originalBytes;update_native_camera(manager.data(),.016f);
 assert(*reinterpret_cast<float*>(manager.data()+0x14D0)==fov);manual_camera_active=false;
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
 // Raw producer must remain enabled while independent target consumption is active.
 original_get_raw_input_data=+[](HRAWINPUT,UINT,LPVOID,PUINT,UINT)->UINT{return sizeof(RAWINPUT);};
 independent_mouse=true;active=true;enabled=true;game_paused=false;gaze_active=false;context_suspended=false;
 mouse_delta.clear();RAWINPUT raw{};raw.header.dwType=RIM_TYPEMOUSE;raw.data.mouse.lLastX=17;raw.data.mouse.lLastY=-9;
 UINT raw_size=sizeof(raw);capture_get_raw_input_data(nullptr,RID_INPUT,&raw,&raw_size,sizeof(RAWINPUTHEADER));
 auto captured=mouse_delta.take();assert(captured.x==17&&captured.y==-9);
 captured=mouse_delta.take();assert(captured.x==0&&captured.y==0);
 // Independent target advances with no new pose, no new HUD source frame, and no control write.
 independent_mouse=true;fixture_c=false;active=true;enabled=true;game_paused=false;gaze_active=false;context_suspended=false;
 recenter_requested=false;resume_center_requested=false;transition_center_requested=false;
 HudFrame fresh;fresh.pawn=aircraft.load();fresh.tick=GetTickCount64();fresh.cp=10;fresh.cy=30;fresh.fov=80;fresh.ox=-3000;fresh.oz=600;
 publish_hud_frame(fresh);free_look.reset();
 const auto source_before=hud_source_sequence.load();const float before_yaw=target_yaw.load(),before_command=command_yaw.load();
 for(int i=0;i<12;++i){mouse_delta.add(2,0);target_input_tick();}
 const float new_yaw=target_yaw.load();assert(std::abs(new_yaw-before_yaw)>.1f);
 assert(hud_source_sequence==source_before&&command_yaw==before_command);
 target_input_tick();assert(std::abs(target_yaw.load()-new_yaw)<.0001f); // no duplicate delta consumption
 receive_pose(pose);assert(std::abs(target_yaw.load()-new_yaw)<.0001f); // controller consumes same goal
 fixture_c=true;target_input_tick();mouse_delta.add(50,0);target_input_tick();
 assert(std::abs(target_yaw.load()-new_yaw)<.0001f);fixture_c=false;target_input_tick();
 context_suspended=true;mouse_delta.add(100,100);target_input_tick();assert(std::abs(target_yaw.load()-new_yaw)<.0001f);
 context_suspended=false;independent_mouse=false;
 puts("PASS independent target updates without new pose/control steps, shared target consumption, no double deltas, C retention and suspension guards");
 // Display prediction cannot mutate the real command, target, camera or policy.
 const std::array<float,8> real_before{command_pitch.load(),command_roll.load(),command_yaw.load(),target_pitch.load(),target_yaw.load(),look_pitch.load(),look_yaw.load(),float(control_mode.load())};
 hud_prediction::Predictor display_only;
 for(unsigned i=0;i<40;++i){display_only.observe({1,2,3,i+1,i*16.667,0,i*.1f,0,62});
  for(bool enabled:{false,true}){auto result=display_only.evaluate(i*16.667+8,flight::basis(0,i*.1f,0).f,{1,0,0},{},1920,1080,enabled);(void)result;}
 }
 const std::array<float,8> real_after{command_pitch.load(),command_roll.load(),command_yaw.load(),target_pitch.load(),target_yaw.load(),look_pitch.load(),look_yaw.load(),float(control_mode.load())};
 assert(real_before==real_after);puts("PASS display prediction on/off leaves real controls, targets, camera commands and policy unchanged");
 // Canvas ring projection uses the same real snapshot and target, never a display prediction.
 independent_mouse=false;aircraft=65536;HudFrame canvas_frame;canvas_frame.pawn=65536;canvas_frame.tick=GetTickCount64();canvas_frame.fov=62;canvas_frame.epoch=hud_epoch.load();
 target_pitch=target_yaw=0;publish_hud_frame(canvas_frame);float cx=0,cy=0,cr=0;
 assert(canvas_position(65536,1920,1080,cx,cy,cr)&&cx==960&&cy==540&&cr==30);
 assert(!canvas_position(77777,1920,1080,cx,cy,cr));
 canvas_frame.tick-=251;publish_hud_frame(canvas_frame);assert(!canvas_position(65536,1920,1080,cx,cy,cr));
 puts("PASS Canvas projection, player identity and stale-data guards");
 // The high-rate worker no longer mutates the goal; one game frame consumes
 // the complete accumulated packet pair and stages the exact control goal.
 independent_mouse=false;context_suspended=false;game_paused=false;gaze_active=false;
 active=true;enabled=true;recenter_requested=false;resume_center_requested=false;transition_center_requested=false;
 free_look.reset();mouse_delta.clear();target_pitch=0;target_yaw=0;fixture_c=false;
 double synced_pose[13]={65536,0,0,0,0,0,0,62,0,0,0,0,0};
 mouse_delta.add(10,0);mouse_delta.add(15,0);target_input_tick();assert(target_yaw.load()==0);
 receive_pose(synced_pose);float synced_yaw=target_yaw.load();assert(synced_yaw>0);
 HudFrame staged;assert(read_pending_hud_frame(staged)&&staged.ty==synced_yaw&&staged.tp==target_pitch.load());
 assert(mouse_delta.take().x==0);receive_pose(synced_pose);assert(target_yaw.load()==synced_yaw);
 fixture_c=true;receive_pose(synced_pose);mouse_delta.add(50,0);receive_pose(synced_pose);assert(target_yaw.load()==synced_yaw);
 fixture_c=false;mouse_delta.add(30,0);receive_pose(synced_pose);assert(target_yaw.load()==synced_yaw);
 puts("PASS frame-owned packet consumption, exact staged goal, no duplicate input and C target retention");
 auto original_bindings=keybindings.load();auto new_bindings=custom_keys::defaults;new_bindings[0]='V';new_bindings[1]=VK_XBUTTON1;keybindings=custom_keys::pack(new_bindings);
 fixture_c=true;fixture_rmb=true;assert(!bound_key_down(custom_keys::Action::FreeLook)&&!bound_key_down(custom_keys::Action::Zoom));
 fixture_keys['V']=true;fixture_keys[VK_XBUTTON1]=true;assert(bound_key_down(custom_keys::Action::FreeLook)&&bound_key_down(custom_keys::Action::Zoom));
 free_look.reset();receive_pose(synced_pose);mouse_delta.add(40,0);receive_pose(synced_pose);assert(target_yaw.load()==synced_yaw); // remapped free look preserves actual flight goal
 fixture_keys['V']=false;fixture_keys[VK_XBUTTON1]=false;fixture_c=false;fixture_rmb=false;keybindings=original_bindings;
 puts("PASS runtime remapped free-look and zoom predicates ignore old C/RMB; frame integration retains the flight goal");
 CustomZoomEnvelope optical;float zoom=62;for(int i=0;i<8;++i){zoom=optical.step(62,1.75f,true,1.f/60);assert(zoom==62);}
 float last=62;for(int i=0;i<60;++i){zoom=optical.step(62,1.75f,true,1.f/60);assert(zoom<=last&&zoom>=15);last=zoom;}
 assert(zoom<30);last=zoom;for(int i=0;i<90;++i){zoom=optical.step(62,1.75f,false,1.f/60);assert(zoom>=last&&zoom<=62);last=zoom;}assert(zoom==62);
 optical.reset();for(int i=0;i<100;++i)zoom=optical.step(25,3,true,1.f/60);assert(zoom>=15);assert(optical.step(62,1.75f,true,NAN)==62);
 puts("PASS custom zoom independent of native RMB FOV, hold threshold, monotonic entry/return, optical floor and invalid-delta reset");



 puts("PASS shared aircraft orbit, frozen native C rig and mouse-follow rotation, 36m geometry, 48-byte pose plus bounded C+RMB optical FOV zoom, HUD sync/release restoration, pending recenter, gaze return and view-switch target preservation.");
}
