#include "src/mouse_aim.cpp"
#include <cassert>
#include <cstdio>
int main(){
 assert(control_modes::normalize(0)==0&&control_modes::normalize(1)==0&&control_modes::normalize(2)==3&&control_modes::normalize(3)==3);
 assert(control_modes::normalize(4)==3&&control_modes::normalize(5)==3&&control_modes::normalize(9)==3&&control_modes::normalize(-2)==0);
 assert(control_modes::next(0)==3&&control_modes::next(1)==3&&control_modes::next(3)==0&&control_modes::next(2)==0&&control_modes::next(4)==0&&control_modes::next(5)==0);
 int active=0;for(int i=0;i<30;++i){active=control_modes::next(active);assert(active==0||active==3);}
 assert(std::string(control_modes::name(2))=="WAR"&&std::string(control_modes::name(1))=="PEACE"&&std::string(control_modes::name(4))=="WAR"&&std::string(control_modes::name(3))=="WAR");
 assert(!control_modes::exact(0)&&!control_modes::exact(1)&&control_modes::exact(3)&&control_modes::exact(4)&&control_modes::exact(5));
 control_modes::KeyLatch key;
 assert(!key.press(true,false));assert(!key.press(true,true));assert(!key.press(false,true));assert(key.press(true,true));assert(!key.press(true,true));
 assert(!key.press(false,true));assert(key.press(true,true));
 control_modes::Blend blend;float prior=1;
 for(int i=0;i<40;++i){float x=blend.step(false,.01f);assert(x<=prior&&prior-x<=.01f/.35f+1e-6);prior=x;}assert(blend.value==0);
 for(int i=0;i<40;++i)blend.step(true,.01f);assert(blend.value==1);assert(blend.step(false,NAN)==1);
 adaptive_braking::History h;for(uint64_t t=1000;t<=2000;t+=10)h.add({t,.1f,.2f,.3f});
 unsigned plans=0;
 for(auto model:{shared_braking::pitch,shared_braking::roll,shared_braking::yaw})for(float e:{-90.f,-15.f,-1.f,0.f,1.f,15.f,90.f})for(float w:{-100.f,-10.f,0.f,10.f,100.f}){
  auto classic=model_control::predict(model,e,w,.2f,600,.85f,45,.2f,h,2000,0,0,2,false);
  auto agile=model_control::predict(model,e,w,.2f,600,.85f,45,.2f,h,2000,0,0,2,true);
  assert(classic.valid&&agile.valid);
  assert(control_modes::combine(classic,agile,0).command==classic.command&&control_modes::combine(classic,agile,1).command==agile.command);
  auto mid=control_modes::combine(classic,agile,.5f);assert(std::isfinite(mid.command)&&std::abs(mid.command)<=mid.input_limit+1e-5f);++plans;
 }
 aircraft=65536;assist_plane_type=14010;assist_speed_pawn=65536;assist_speed=600;assist_brake=0;assist_environment_unsafe=true;
 game_paused=false;gaze_active=false;model_assist_enabled=true;config.model_assist_strength=.2f;control_mode=3;
 auto step=[](){auto now=GetTickCount64();assist_speed_tick=now;adaptive_owner=65536;adaptive_history.clear();for(auto t=now-900;t<=now;t+=5)adaptive_history.add({t,0,0,0});observe_assist_input(65536,0,0,0,0);float p=0,r=0,y=0;apply_model_control(60,90,20,0,0,0,0,0,0,.01f,p,r,y,140,{},flight::unit({1,std::tan(20*flight::rad),std::tan(60*flight::rad)}));assert(assist_profile==6);return std::array<float,3>{p,r,y};};
 for(int i=0;i<100;++i)step();assert(control_mode_blend==1&&heritage_diagnostics[16]==1&&heritage_diagnostics[1]==1);
 auto* bank=learning_current;auto* served=serving_current;bank->pitch.banks[1].accepted=true;bank->pitch.banks[1].promotions=7;served->axes[0].cells[1].deployments=9;
 auto last=step();control_mode=0;
 for(int i=0;i<40;++i){auto u=step();assert(std::abs(u[0]-last[0])<=.12001f&&std::abs(u[2]-last[2])<=.06001f);last=u;}
 assert(control_mode_blend==0&&assist_pursuit[0]==0&&std::abs(last[0])<=.85001f&&std::abs(last[2])<=.70001f);
 assert(learning_current==bank&&serving_current==served&&bank->pitch.banks[1].accepted&&bank->pitch.promotions()==7&&served->axes[0].deployments()==9);
 assert(primary_controllers[0].weight==1&&primary_controllers[2].weight==1);
 control_mode=3;for(int i=0;i<40;++i)step();assert(control_mode_blend==1&&heritage_diagnostics[16]==1&&heritage_diagnostics[1]==1);
 control_mode=0;reset_model_assist();assert(control_mode_blend==0&&bank->pitch.promotions()==7);
 printf("PASS F4 edge/focus gating,105policy endpoint/blend cases,0.35s transition, actual command slew, exact classic caps, bank identity/counters retained, pause-mode reset; plans=%u\n",plans);
}
