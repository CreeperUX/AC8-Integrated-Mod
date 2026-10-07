#include "src/mouse_aim.cpp"
#include <cassert>
#include <cstdio>
static void step(unsigned manual=0,bool stale=false,float roll_rate=0,bool gap=false){
 auto now=GetTickCount64();assist_speed_tick=stale?now-1000:now;adaptive_owner=65536;adaptive_history.clear();
 for(auto t=now-900;t<=now;t+=5)if(!gap||t<now-300||t==now)adaptive_history.add({t,.1f,.2f,.05f});
 observe_assist_input(65536,.1f,.2f,manual,.05f);
 float p=0,r=0,y=0;auto b=flight::basis(0,0,0);auto goal=flight::basis(35,15,0).f;
 apply_model_control(35,80,15,0,roll_rate,0,0,roll_rate,0,.02f,p,r,y,140,{},goal,{0,0,1},b);
 assert(std::isfinite(p)&&std::isfinite(r)&&std::isfinite(y));
 if(manual&4)assert(std::abs(r-.2f)<1e-5f);
 if(manual&1)assert(std::abs(p-.1f)<1e-5f);
 if(manual&2)assert(std::abs(y-.05f)<1e-5f);
}
// Aligned 10 degree upper target, wings level: the v11 rate path should own pitch/yaw.
static void aligned_step(){
 auto now=GetTickCount64();assist_speed_tick=now;adaptive_owner=65536;adaptive_history.clear();
 for(auto t=now-900;t<=now;t+=5)adaptive_history.add({t,0,0,0});
 observe_assist_input(65536,0,0,0,0);
 float p=0,r=0,y=0;auto b=flight::basis(0,0,0);auto goal=flight::basis(10,0,0).f;
 apply_model_control(10,0,0,0,0,0,0,0,0,.02f,p,r,y,140,{},goal,{0,0,1},b);
 assert(std::isfinite(p)&&std::isfinite(r)&&std::isfinite(y));
}
int main(){
 aircraft=65536;assist_plane_type=5010;assist_speed_pawn=65536;assist_speed=400;assist_brake=0;
 assist_environment_unsafe=true;game_paused=false;gaze_active=false;model_assist_enabled=true;
 control_mode=3;full_model_new_flight=true;config.model_assist_strength=.2f;
 for(int i=0;i<50;++i)step();
 assert(heritage_diagnostics[0]==1&&heritage_diagnostics[1]==1&&heritage_diagnostics[2]==11.2f);
 assert(heritage_diagnostics[16]==1&&heritage_diagnostics[11]==1);
 assert(heritage_diagnostics[17]>=0&&heritage_diagnostics[19]<=heritage_diagnostics[18]+.05001f);
 assert(heritage_diagnostics[22]==1&&heritage_diagnostics[24]==0&&heritage_diagnostics[25]==0);
 assert(primary_controllers[1].last.pursuit==0&&primary_controllers[1].last.input_limit==1);
 assert(vector_mode_weight==0&&vector_diagnostics[109]==0&&vector_performance_diagnostics[5]==11.2f);
 step(0,false,150);assert(heritage_diagnostics[3]==0&&heritage_diagnostics[9]==0&&heritage_diagnostics[10]==0);
 for(unsigned mask:{1u,2u,4u,7u}){step(mask);assert(heritage_diagnostics[1]==0&&heritage_diagnostics[9]==0&&heritage_diagnostics[10]==0);}
 step(0,true);assert(heritage_diagnostics[1]==0&&primary_controllers[0].weight<1);
 assert(heritage_diagnostics[21]==0&&((int)heritage_diagnostics[24].load()&8));
 step(0,false,0,true);assert(heritage_diagnostics[1]==0&&((int)heritage_diagnostics[24].load()&64));
 assert(heritage_diagnostics[21]==0);
 full_model_new_flight=true;assist_plane_type=8010;step();assert(learning_current->id==8010&&primary_controllers[0].weight<.05f);
 control_mode=0;full_model_new_flight=true;for(int i=0;i<50;++i)step();
 assert(heritage_diagnostics[0]==0&&heritage_diagnostics[9]==0&&heritage_diagnostics[10]==0);
 assert(primary_controllers[0].last.input_limit==.85f&&primary_controllers[2].last.input_limit==.7f);
 assert(control_modes::next(0)==3&&control_modes::next(3)==0&&control_modes::next(4)==0&&control_modes::normalize(1)==0&&control_modes::normalize(2)==3);
 control_mode=3;full_model_new_flight=true;for(int i=0;i<50;++i)aligned_step();
 assert(heritage_diagnostics[2]==11.2f&&heritage_diagnostics[15]==3&&heritage_diagnostics[21]==2&&heritage_diagnostics[22]==1&&heritage_diagnostics[23]==2);
 assert(vector_performance_diagnostics[0]==1&&vector_performance_diagnostics[1]>0&&std::abs(vector_performance_diagnostics[2])<1e-3f&&vector_performance_diagnostics[5]==11.2f);
 assert(primary_controllers[0].last.valid&&primary_controllers[0].last.command>0&&primary_controllers[0].last.damping==1);
 // Target-plane correction telemetry is finite and bounded (weight 0..1).
 assert(std::isfinite(vector_decision_diagnostics[0].load())&&vector_decision_diagnostics[0]>=0&&vector_decision_diagnostics[0]<=1);
 assert(std::isfinite(vector_decision_diagnostics[1].load())&&std::isfinite(vector_decision_diagnostics[2].load())&&vector_decision_diagnostics[2]>=0);
 puts("PASS v11.2 rate path + target-plane planning selected and recorded for an aligned upper target");
 puts("PASS new WAR runtime: PEACE roll, guarded AGILE pitch/yaw, VECTOR disconnected, telemetry, manual ownership, stale data, aircraft reset and two-mode mapping");
}
