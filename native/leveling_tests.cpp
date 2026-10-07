#include "src/mouse_aim.cpp"
#include <cassert>
#include <cstdio>
#include <chrono>
struct ResultMetrics {float leveled=10,final_bank=0,peak_error=0,peak_roll=0;};
ResultMetrics coupled(float bank_start,float residual,float gain_scale=1,float tau_scale=1,float speed=360){
 using namespace flight;
 std::array<shared_braking::Model,3> models{shared_braking::pitch,shared_braking::roll,shared_braking::yaw},truth=models;
 for(auto& m:truth){m.g0*=gain_scale;m.g1*=gain_scale;m.tau*=tau_scale;}
 Basis b=basis(0,0,bank_start);V target=basis(residual,0,0).f;
 std::array<float,3> w{},previous{},caps{1,1,.85f},slews{12,16,6};std::array<model_control::Controller,3> controller{};
 for(auto& c:controller){c.weight=1;c.initialized=true;}
 adaptive_braking::History history;for(uint64_t t=1000;t<=2000;t+=10)history.add({t,0,0,0});
 capture_roll::Guidance guide;ResultMetrics result;unsigned good=0;bool ever_captured=false;
 for(unsigned frame=0;frame<500;++frame){float dt=.02f;uint64_t now=2020+20*frame;
  V goal{dot(target,b.f),dot(target,b.r),dot(target,b.u)},gravity{b.f.z,b.r.z,b.u.z};
  float angle=std::acos(std::clamp(goal.x,-1.f,1.f))/rad;
  capture_roll::Input in;in.dt=dt;in.angle=angle;in.up=goal.z;in.right=goal.y;in.pitch_rate=w[0];in.yaw_rate=w[2];in.roll_rate=w[1];
  in.turn_error=std::clamp(std::atan2(goal.y,std::max(.12f,goal.z))/rad,-90.f,90.f);
  in.level_error=std::atan2(b.r.z,b.u.z)/rad;in.pole_clearance=std::hypot(b.r.z,b.u.z);
  auto plan_roll=guide.update(in);assert(plan_roll.valid);ever_captured=ever_captured||plan_roll.leveling;
  std::array<float,3> errors{std::atan2(goal.z,std::max(.02f,goal.x))/rad,plan_roll.roll_error,std::atan2(goal.y,std::max(.02f,goal.x))/rad};
  std::array<float,3> rates{45,plan_roll.rate_limit,7},zones{.2f,1,.2f},command{},targets{};
  for(int i=0;i<3;++i){auto plan=capture_control::axis_plan(models[i],errors[i],w[i],previous[i],speed,caps[i],rates[i],zones[i],history,now,i,0,2,dt,0,plan_roll.leveling);
   assert(plan.valid);targets[i]=plan.command;command[i]=controller[i].apply_full(plan,0,caps[i],slews[i],dt);}
  auto refined=capture_control::refine(models,w,previous,command,caps,goal,errors[1],{},speed,history,now,dt,1,targets,{},gravity,plan_roll.leveling);
  if(refined.valid){assert(refined.after<=refined.before+1e-5f);command=refined.command;for(int i=0;i<3;++i)controller[i].actual=command[i];}
  for(int i=0;i<3;++i){assert(std::isfinite(command[i])&&std::abs(command[i])<=caps[i]+1e-5f&&std::abs(command[i]-previous[i])<=slews[i]*dt+1e-5f);}
  history.add({now,command[0],command[1],command[2]});auto before=w;
  for(int i=0;i<3;++i){float input=history.at_axis(now-uint64_t(truth[i].delay*1000),i),decay=std::exp(-dt/truth[i].tau);
   w[i]=decay*w[i]+(1-decay)*shared_braking::gain(truth[i],speed)*input;}
  V omega=b.f*(-.5f*(before[1]+w[1]))+b.r*(-.5f*(before[0]+w[0]))+b.u*(.5f*(before[2]+w[2]));
  b.f=capture_control::rotate(b.f,omega,dt);b.r=capture_control::rotate(b.r,omega,dt);b.u=capture_control::rotate(b.u,omega,dt);previous=command;
  float actual_bank=std::atan2(b.r.z,b.u.z)/rad;
  result.peak_error=std::max(result.peak_error,angle);result.peak_roll=std::max(result.peak_roll,std::abs(w[1]));result.final_bank=actual_bank;
  if(ever_captured&&std::abs(actual_bank)<3&&std::abs(w[1])<5){++good;if(good==10)result.leveled=frame*dt-.18f;}else good=0;
 }
 return result;
}
int main(){
 setvbuf(stdout,nullptr,_IONBF,0);auto start=std::chrono::steady_clock::now();unsigned cases=0;
 for(float bank:{-100.f,-60.f,-30.f,30.f,60.f,100.f})for(float residual:{.2f,.6f,1.f,2.f,5.f}){
  auto m=coupled(bank,residual);printf("LEVEL bank=%+.0f residual=%.1f leveled=%.2fs final_bank=%+.3f peak_pointing=%.3f peak_roll=%.2f\n",bank,residual,m.leveled,m.final_bank,m.peak_error,m.peak_roll);
  assert(std::abs(m.final_bank)<3&&m.leveled<6&&m.peak_error<std::max(3.f,residual+2));++cases;
 }
 for(float scale:{.85f,1.15f})for(float speed:{360.f,750.f}){
  auto m=coupled(100,.65f,scale,scale<1?1.2f:.85f,speed);printf("BLIND bank=100 residual=.65 response=%.2f speed=%.0f leveled=%.2f final=%+.3f peak=%.3f\n",scale,speed,m.leveled,m.final_bank,m.peak_error);
  assert(std::abs(m.final_bank)<3&&m.leveled<7&&m.peak_error<4);++cases;
 }
 // Actual trial failure inputs: a stable .65deg residual no longer erases a 104deg level objective.
 capture_roll::Guidance replay;capture_roll::Input in;in.angle=.65f;in.level_error=104;in.pitch_rate=.132f;in.yaw_rate=0;
 for(int i=0;i<6;++i)replay.update(in);auto out=replay.update(in);assert(out.leveling&&out.roll_error==104&&out.rate_limit>70);
 in.angle=1.108f;in.pitch_rate=.0347f;out=replay.update(in);assert(out.leveling&&out.roll_error==104);
 // Runtime manual / stale observations still preserve ownership and fallback.
 aircraft=65536;assist_plane_type=5010;assist_speed_pawn=65536;assist_speed=360;assist_brake=0;assist_environment_unsafe=true;game_paused=false;gaze_active=false;model_assist_enabled=true;control_mode=2;
 auto step=[](unsigned mask,bool stale){auto now=GetTickCount64();assist_speed_tick=stale?now-1000:now;adaptive_owner=65536;adaptive_history.clear();for(auto t=now-900;t<=now;t+=5)adaptive_history.add({t,0,0,0});observe_assist_input(65536,.1f,.2f,mask,.3f);float p=0,r=0,y=0;
  apply_model_control(.65f,104,0,0,0,0,0,0,0,.02f,p,r,y,90,{},flight::unit({1,0,.011f}),{0,-.9703f,-.2419f});return std::array<float,3>{p,r,y};};
 step(0,false);for(int i=0;i<6;++i)capture_roll_guidance.update(in);assert(capture_roll_guidance.leveling);
 auto u=step(4,false);assert(!capture_roll_guidance.leveling&&std::abs(u[1]-.2f)<1e-6);
 assert(capture_level_phase==0&&capture_level_weight==0&&capture_joint_roll_delta==0);
 step(0,true);assert(assist_model_source[1]==-1);
 // A learned response multiplier must not inflate the recovery pointing budget.
 step(0,false);for(int i=0;i<6;++i)capture_roll_guidance.update(in);assert(capture_roll_guidance.leveling);
 auto& roll_served=serving_current->axes[1];roll_served.gain_scale=1.4f;roll_served.tau_scale=.6f;
 capture_mode_weight=1;capture_level_phase=3;capture_level_weight=1;step(0,false);
 assert(primary_controllers[1].last.rate_limit<=90.0001f);
 reset_capture_roll_state();assert(!capture_roll_guidance.leveling&&capture_level_phase==0&&capture_level_weight==0&&capture_cross_rate==0);
 printf("PASS persistent level goal, %u coupled attitude/model cases, recorded residual regressions, pointing protection, command bounds/slew and runtime manual/stale guards; elapsed=%.3fs\n",cases,std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count());
}
