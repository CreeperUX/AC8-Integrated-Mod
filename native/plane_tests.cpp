// WAR v11.1 target-plane planning: invariants over a grid of roll+pitch states.
#include "src/war11_control.h"
#include <cassert>
#include <cstdio>
int main(){
 std::array<shared_braking::Model,3> prior{shared_braking::pitch,shared_braking::roll,shared_braking::yaw};
 int checked=0,corrected=0;float worst=0;
 for(float hz:{30.f,60.f,120.f})for(float gp:{-20.f,-5.f,0.f,5.f,25.f,50.f})for(float gy:{-60.f,-20.f,-4.f,4.f,20.f,60.f})
 for(float roll_rate:{-120.f,-40.f,0.f,40.f,120.f})for(float pitch_rate:{-5.f,0.f,15.f,30.f}){
  adaptive_braking::History h;uint64_t now=5000;for(uint64_t t=3000;t<=now;t+=5)h.add({t,.3f,roll_rate>0?.6f:roll_rate<0?-.6f:0.f,0});
  auto goal=flight::basis(gp,gy,0).f;flight::V g{goal.x,goal.y,goal.z};
  float ep=std::atan2(g.z,std::max(.02f,g.x))/flight::rad,ey=std::atan2(g.y,std::max(.02f,g.x))/flight::rad;
  float er=std::clamp(std::atan2(g.y,std::max(.12f,g.z))/flight::rad,-90.f,90.f);
  std::array<float,3> errors{ep,er,ey},rates{pitch_rate,roll_rate,0},previous{.3f,0,0},refs{},caps{45,140,7};
  float dt=1/hz;std::array<model_control::Plan,3> peace{},sel{};
  auto ctx=peace_agile::coordinate(g,rates,refs,.5f);bool ok=true;
  for(int i=0;i<3;++i){peace[i]=model_control::predict(prior[i],errors[i],rates[i],previous[i],450,i==0?.85f:i==1?1.f:.7f,caps[i],i==1?1.f:.2f,h,now,i,0,2,false);
   auto ag=i==1?peace[i]:model_control::predict(prior[i],errors[i],rates[i],previous[i],450,i==0?.85f:.7f,caps[i],.2f,h,now,i,0,2,true);
   sel[i]=peace_agile::select(peace[i],ag,ctx,i,errors[i],1).plan;ok=ok&&peace[i].valid;}
  if(!ok)continue;
  peace_agile::JointInput in;in.models=prior;in.rates=rates;in.previous=previous;in.errors=errors;in.references=refs;in.caps=caps;
  in.damping={2,2,2};in.goal=g;in.speed=450;in.dt=dt;in.now=now;
  war11::Params off;off.plane_mpc=0;war11::Params on;
  auto a=war11::choose(in,h,peace,sel,1,nullptr,off);war10::Diagnostics d;auto b=war11::choose(in,h,peace,sel,1,&d,on);
  for(auto& p:b.plans){assert(p.valid&&std::isfinite(p.command));}
  assert(std::abs(b.plans[0].command)<=1.f+1e-6f&&std::abs(b.plans[2].command)<=.85f+1e-6f);
  assert(b.plans[1].command==a.plans[1].command);                 // roll is never touched
  float theta=peace_agile::angle({1,0,0},flight::unit(g));
  if(theta<=on.plane_theta){assert(b.plans[0].command==a.plans[0].command&&b.plans[2].command==a.plans[2].command);}
  // never more push than the base plan
  assert(b.plans[0].command>=std::min(a.plans[0].command,0.f)-1e-5f);
  auto ta=war11::plane_track(in,{a.plans[0].command,a.plans[1].command,a.plans[2].command},h,1);
  auto tb=war11::plane_track(in,{b.plans[0].command,b.plans[1].command,b.plans[2].command},h,1);
  if(ta.valid&&tb.valid){
   float ca=war11::plane_cost(ta,ta,on),cb=war11::plane_cost(tb,ta,on);
   assert(cb<=ca+1e-4f);                                            // predicted plane cost never increases
   if(d.plane>0){++corrected;worst=std::max(worst,std::abs(tb.cross[23])-std::abs(ta.cross[23]));}
  }
  ++checked;
 }
 printf("PASS %d roll+pitch states: target-plane cost never increases, roll untouched, no extra push, terminal (<=3 deg) unchanged; corrected %d\n",checked,corrected);
 assert(checked>1000&&corrected>100);
 // Path anchor: kept while the target is tracked smoothly, renewed on a target step.
 {war11::Params k;war11::PathAnchor a;auto b=flight::basis(0,0,0);auto tg=flight::basis(0,30,0).f;
  auto o1=a.update(b,tg,0,30,k);assert(std::abs(o1.x-1)<1e-5f);
  auto b2=flight::basis(2,5,10);auto o2=a.update(b2,tg,.5f,25,k);
  assert(std::abs(flight::dot(a.origin,flight::basis(0,0,0).f)-1)<1e-5f&&o2.x<.999f);   // origin stays at maneuver start
  a.update(b2,flight::basis(0,60,0).f,5,40,k);assert(std::abs(flight::dot(a.origin,b2.f)-1)<1e-5f); // step: renewed at current nose
  puts("PASS path anchor kept during smooth tracking, renewed on target step");}
 // Measured aircraft: known table entries use their push ratio; unknown ids fall back to conservative constants.
 {const auto& f15c=ac8_models::find(4010);const auto& unk=ac8_models::find(999999);
  assert(f15c.known&&f15c.push>.6f&&!unk.known&&unk.push<f15c.push&&unk.roll.delay>.1f);
  puts("PASS aircraft table lookup: F-15C measured, unknown aircraft conservative");}
 // Roll path keeps PEACE's bank goal: command sign follows the PEACE roll error when far from the goal.
 {adaptive_braking::History h;uint64_t now=5000;for(uint64_t t=3000;t<=now;t+=5)h.add({t,0,0,0});
  peace_agile::JointInput in;in.models={shared_braking::pitch,shared_braking::roll,shared_braking::yaw};in.now=now;in.speed=450;in.caps={45,140,7};
  for(float er:{-80.f,-30.f,30.f,80.f}){in.errors={0,er,0};in.rates={0,0,0};model_control::Plan pr;pr.valid=true;pr.command=0;
   auto r=war11::roll_path_plan(in,h,pr,1,war11::Params{});assert(r.valid&&r.command*er>0&&std::abs(r.command)<=1);}
  puts("PASS roll path drives toward PEACE's bank goal, bounded");}
}
