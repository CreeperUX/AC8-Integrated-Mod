#include "src/peace_agile_control.h"
#include <cassert>
#include <chrono>
#include <cstdio>
int main(){
 adaptive_braking::History history;for(uint64_t t=1000;t<=2000;t+=5)history.add({t,0,0,0});
 int cases=0,boosts=0,transports=0;auto start=std::chrono::steady_clock::now();
 for(int quadrant=1;quadrant<=4;++quadrant)for(float theta:{2.f,3.f,4.f,4.9f,5.f,5.1f,6.f,8.f,10.f,12.f,20.f,40.f})for(float roll_rate:{-80.f,0.f,80.f})for(float scale:{.7f,1.f,1.5f}){
  peace_agile::JointInput in;in.models={shared_braking::pitch,shared_braking::roll,shared_braking::yaw};in.now=2000;
  for(auto& m:in.models){m.g0*=scale;m.g1*=scale;}
  float sign_y=quadrant==1||quadrant==4?1.f:-1.f,sign_z=quadrant<=2?1.f:-1.f;
  in.goal={std::cos(theta*flight::rad),sign_y*std::sin(theta*flight::rad)/std::sqrt(2.f),sign_z*std::sin(theta*flight::rad)/std::sqrt(2.f)};
  flight::LevelBlend level;float blend=0;for(int j=0;j<90;++j)blend=level.step(theta,.02f);
  in.errors={std::atan2(in.goal.z,in.goal.x)/flight::rad,std::clamp(std::atan2(in.goal.y,std::max(.12f,in.goal.z))/flight::rad,-90.f,90.f)*blend,std::atan2(in.goal.y,in.goal.x)/flight::rad};
  in.rates={2,roll_rate,.5f};in.caps={45,70+70*blend,7};in.damping={2,2,2};
  auto context=peace_agile::coordinate(in.goal,in.rates,in.references,.5f);
  std::array<model_control::Plan,3> peace{},boosted{};
  for(int i=0;i<3;++i){peace[i]=model_control::predict(in.models[i],in.errors[i],in.rates[i],0,400,in.limits[i],in.caps[i],i==1?1.f:.2f,history,2000,i,0,2,false);
   auto agile=model_control::predict(in.models[i],in.errors[i],in.rates[i],0,400,in.limits[i],in.caps[i],i==1?1.f:.2f,history,2000,i,0,2,true);
   boosted[i]=peace_agile::select(peace[i],agile,context,i,in.errors[i],1).plan;}
  auto selected=peace_agile::choose(in,history,peace,boosted,1);assert(selected.checked);
  assert(selected.plans[1].command==peace[1].command);assert(std::abs(in.errors[1])<=90);
  assert(selected.after.cost<=selected.before.cost&&selected.after.path<=selected.before.path+.050001f&&selected.after.crossing<=selected.before.crossing+.020001f);
  if(theta<=5){assert(selected.choice==0);for(int i=0;i<3;++i)assert(selected.plans[i].command==peace[i].command);}
  if(theta==8&&quadrant>=3&&roll_rate==0)assert(std::abs(in.errors[1])<40); // v8 requested 135 degrees here.
  auto off=peace_agile::choose(in,history,peace,boosted,0);for(int i=0;i<3;++i)assert(off.plans[i].command==peace[i].command);
  boosts+=selected.choice==1;transports+=selected.choice==2;++cases;
 }
 assert(boosts>0&&transports>0);
 peace_agile::JointInput in;in.models={shared_braking::pitch,shared_braking::roll,shared_braking::yaw};in.now=2000;in.goal=flight::basis(8,8,0).f;
 auto motion=peace_agile::preview(in,{0,1,0},history);assert(motion.valid&&motion.roll_travel>0&&motion.path<.0001f); // Pure roll cannot rotate the forward vector.
 in.references={4,0,3};auto moving=peace_agile::preview(in,{0,0,0},history);assert(moving.valid&&moving.error>peace_agile::angle({1,0,0},in.goal));
 in.rates[1]=NAN;assert(!peace_agile::preview(in,{0,0,0},history).valid);in.rates[1]=0;
 adaptive_braking::History empty;assert(!peace_agile::preview(in,{0,0,0},empty).valid);
 printf("PASS %d common-frame quadrant/rate/capability cases; accepted boost=%d transport=%d; time_ms=%.3f; PEACE roll/terminal preserved, pure-roll geometry and moving target checked\n",cases,boosts,transports,std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count());
}
