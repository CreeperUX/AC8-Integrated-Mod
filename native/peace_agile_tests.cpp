#include "src/peace_agile_control.h"
#include <cassert>
#include <cstdio>
int main(){
 adaptive_braking::History h;for(uint64_t t=1000;t<=2000;t+=5)h.add({t,0,0,0});
 std::array<shared_braking::Model,3> models{shared_braking::pitch,shared_braking::roll,shared_braking::yaw};
 std::array<float,3> caps{.85f,1,.7f},rates{45,140,7};
 for(int i=0;i<3;++i)for(float error:{-60.f,-2.f,0.f,2.f,60.f})for(float w:{-20.f,0.f,20.f}){
  auto a=model_control::predict(models[i],error,w,0,400,caps[i],rates[i],.2f,h,2000,i,0,2,false);
  auto b=model_control::predict(models[i],error,w,0,400,caps[i],rates[i],.2f,h,2000,i,0,2,true);
  auto context=peace_agile::coordinate(flight::basis(std::abs(error),0,0).f,{w,0,0},{},.4f);
  auto r=peace_agile::select(a,b,context,i,error,1);assert(r.plan.valid);
  assert(r.plan.command>=std::min(a.command,b.command)-1e-6f&&r.plan.command<=std::max(a.command,b.command)+1e-6f);
  if(i==1||std::abs(error)<=5||(i==0&&error<0))assert(r.boost==0&&r.plan.command==a.command);
  auto off=peace_agile::select(a,b,context,i,error,0);assert(off.plan.command==a.command);
 }
 auto goal=flight::basis(30,10,0).f;
 auto clear=peace_agile::coordinate(goal,{0,0,0},{},.5f);
 auto rolling=peace_agile::coordinate(goal,{0,120,0},{},.5f);
 auto transverse=peace_agile::coordinate(goal,{0,0,20},{},.5f);
 assert(clear.weight>.99f&&rolling.weight==0&&transverse.weight==0);
 assert(!peace_agile::coordinate({NAN,0,0},{},{},.4f).valid);
 assert(!peace_agile::coordinate(goal,{0,NAN,0},{},.4f).valid);
 puts("PASS PEACE parity in roll/terminal/push, bounded AGILE authority, coupling withdrawal, disabled and invalid guards");
}
