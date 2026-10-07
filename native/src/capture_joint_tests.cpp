#include "capture_joint.h"
#include <cassert>
#include <cstdio>
#include <chrono>
int main(){
 setvbuf(stdout,nullptr,_IONBF,0);
 std::array<shared_braking::Model,3> models{shared_braking::pitch,shared_braking::roll,shared_braking::yaw};
 std::array<float,3> limits{1,1,.85f},slews{12,16,6};
 std::array<robust_intercept::Envelope,3> bounds;for(auto& e:bounds)e={.1f,.15f,.025f,.1f,.8f};
 unsigned cases=0,improved=0,near_improved=0;
 for(float bank:{0.f,45.f,105.f,-120.f})for(float degrees:{.05f,.2f,1.f,5.f})for(float hz:{30.f,60.f,120.f}){
  float dt=1/hz;std::array<float,3> previous{.02f,-.05f,.01f},rates{.2f,3.f,.1f},base{},ref{};
  adaptive_braking::History history;for(uint64_t now=1000;now<=2000;now+=10)history.add({now,previous[0],previous[1],previous[2]});
  std::array<robust_intercept::Result,3> plans{};
  plans[0]=robust_intercept::predict(models[0],degrees,rates[0],previous[0],360,1,45,.025f,history,2000,0,0,12,dt,0,bounds[0]);
  plans[2]=robust_intercept::predict(models[2],degrees*.6f,rates[2],previous[2],360,.85f,7,.025f,history,2000,2,0,6,dt,0,bounds[2]);
  assert(plans[0].plan.valid&&plans[2].plan.valid);
  plans[1].plan.valid=true;plans[1].desired_command=std::clamp(bank/70.f,-1.f,1.f);
  plans[1].issued_command=plans[1].plan.command=previous[1]+std::clamp(plans[1].desired_command-previous[1],-16*dt,16*dt);
  for(int i=0;i<3;++i)base[i]=plans[i].issued_command;
  auto goal=flight::unit({1,std::tan(.6f*degrees*flight::rad),std::tan(degrees*flight::rad)});
  auto up=flight::V{0,std::sin(bank*flight::rad),std::cos(bank*flight::rad)};
  auto result=capture_joint::refine(models,rates,previous,base,limits,plans,bounds,goal,bank,ref,360,history,2000,dt,1,{},up,std::abs(bank)>1);
  printf("JOINT bank=%.0f e=%.2f hz=%.0f valid=%d cost=%.7f->%.7f crossing=%.5f->%.5f\n",bank,degrees,hz,result.valid,result.before,result.after,result.before_crossing,result.crossing);
  assert(result.valid&&std::isfinite(result.after)&&result.after<=result.before+1e-5&&result.crossing<=result.before_crossing+.0051f);
  for(int i=0;i<3;++i)assert(std::abs(result.command[i])<=limits[i]+1e-5&&std::abs(result.command[i]-previous[i])<=slews[i]*dt+1e-5);
  if(result.after<result.before-1e-6){++improved;if(degrees<.36f)++near_improved;}++cases;
 }
 // Shared source event ladder: perturbation changes physical delay, not the
 // source command itself, and the initial first frame is held immediately.
 robust_intercept::SourceTrace source;source.frame_dt=.02f;source.count=2;source.inputs[0]=.24f;source.inputs[1]=.48f;source.final_input=0;source.duration=.04f;source.valid=true;
 adaptive_braking::History history;for(uint64_t now=1000;now<=2000;now+=10)history.add({now,0,0,0});
 robust_intercept::Plant p;p.delay=.035;
 auto queued=capture_joint::timeline(p,source,history,2000,0,.5);
 bool first_seen=false,second_seen=false;for(size_t i=0;i<queued.count;++i){if(std::abs(queued.events[i].time-.035)<1e-8&&std::abs(queued.events[i].input-.24)<1e-6)first_seen=true;if(std::abs(queued.events[i].time-.055)<1e-8&&std::abs(queued.events[i].input-.48)<1e-6)second_seen=true;}
 assert(queued.valid&&first_seen&&second_seen&&improved>0&&near_improved>0);
 std::array<float,3> previous{},rates{},base{.02f,.02f,.01f},ref{};
 std::array<robust_intercept::Result,3> plans;for(int i=0;i<3;++i){plans[i].plan.valid=true;plans[i].desired_command=base[i];}
 auto pole=capture_joint::refine(models,rates,previous,base,limits,plans,bounds,flight::unit({1,.002f,.001f}),90,ref,360,history,2000,.02f,1,{},flight::unit({1,0,.01f}),true);
 assert(pole.valid&&std::isfinite(pole.after)&&pole.after<=pole.before+1e-5);
 auto invalid=capture_joint::refine(models,rates,previous,{.8f,0,0},limits,plans,bounds,{1,0,0},0,ref,360,history,2000,.01f,1);
 assert(!invalid.valid);
 auto begin=std::chrono::steady_clock::now();constexpr int count=500;volatile float sum=0;
 for(int i=0;i<count;++i){auto result=capture_joint::refine(models,rates,previous,base,limits,plans,bounds,flight::unit({1,.002f,.001f}),45,ref,360,history,2000,.02f,1,{},flight::unit({0,1,1}),true);sum=sum+result.after;}
 double us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-begin).count()/count;
 printf("PASS joint %u bank/near-target/frame cases, %u improvements including %u below .36deg; %.1fus/call sum=%.2f\n",cases,improved,near_improved,us,float(sum));
 assert(us<2000);
}
