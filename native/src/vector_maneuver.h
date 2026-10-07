#pragma once
#include "flight_math.h"
#include "shared_braking.h"
#include <array>
#include <cmath>
#include <algorithm>
// Strategic pointing decision. Positive lift and negative lift are not
// interchangeable. Small push corrections remain available; major turns
// orient the lift plane and pull, while braking always retains full authority.
namespace vector_maneuver {
enum class Mode {Pull=1,Push=2};
struct Input {
 flight::V goal{1,0,0};std::array<shared_braking::Model,3> models{};
 float speed=400,dt=.016667f,theta=0,roll_rate=0;
 float positive_rate=45,negative_rate=15,roll_rate_cap=140;
};
struct Result {
 Mode mode=Mode::Pull;float roll_error=0,pull_time=0,push_time=0,pull_cost=0,push_cost=0;
 float alignment=0,push_weight=0;bool preparing=false;
};
inline float smooth(float x){x=std::clamp(x,0.f,1.f);return x*x*(3-2*x);}
inline float response_time(float angle,float rate,shared_braking::Model model){
 if(angle<=.02f)return 0;
 // A bounded inverse of the step-rate integral includes finite acceleration
 // and input delay. Both maneuver candidates use the same completion metric.
 float low=0,high=std::max(1.f,angle/std::max(1.f,rate)+3*model.tau);
 for(int n=0;n<14;++n){float t=.5f*(low+high),travel=rate*(t-model.tau*(1-std::exp(-t/model.tau)));
  if(travel<angle)low=t;else high=t;}
 return model.delay+.5f*(low+high);
}
struct Planner {
 int turn_direction=0;Mode previous=Mode::Pull;float push_confirm=0;
 void reset(){*this={};}
 Result update(const Input& in){
  Result out;float length=std::hypot(in.goal.y,in.goal.z),full=length>1e-6f?std::atan2(in.goal.y,in.goal.z)/flight::rad:0;
  float direct=full;if(direct>90)direct-=180;if(direct< -90)direct+=180;
  float positive=std::clamp(in.positive_rate,2.f,600.f),negative=std::clamp(in.negative_rate,1.f,positive),roll_cap=std::clamp(in.roll_rate_cap,20.f,220.f);
  out.pull_time=response_time(std::abs(full),roll_cap,in.models[1])*.75f+response_time(in.theta,positive,in.models[0]);
  out.push_time=response_time(std::abs(direct),roll_cap,in.models[1])*.75f+response_time(in.theta,negative,in.models[0]);
  // Normalized tracking, completion time and sustained negative-load exposure.
  // Path is a cost, not an absolute priority that vetoes efficient roll/pull.
  float roll_transient=std::abs(full)/180.f;
  out.pull_cost=out.pull_time+.18f*roll_transient*std::min(1.f,in.theta/30.f);
  out.push_cost=out.push_time+.8f*in.theta/std::max(2.f,negative)+.12f;
  bool small_push=in.theta<=5.f&&in.goal.z<0&&out.push_cost+.12f<out.pull_cost;
  push_confirm=small_push?std::min(.15f,push_confirm+in.dt):0;
  out.mode=small_push&&(previous==Mode::Push||push_confirm>=.04f)?Mode::Push:Mode::Pull;
  previous=out.mode;
  float chosen=out.mode==Mode::Push?direct:full;
  if(out.mode==Mode::Pull&&std::abs(chosen)>=150.f){
   if(!turn_direction)turn_direction=std::abs(in.roll_rate)>5.f?(in.roll_rate<0?-1:1):(chosen<0?-1:1);
   if(chosen*turn_direction<0)chosen+=360.f*turn_direction;
  }else turn_direction=0;
  out.roll_error=chosen;
  float up=length>1e-5f?in.goal.z/length:1;
  out.alignment=out.mode==Mode::Push?1.f:smooth((up+.02f)/.32f);
  out.preparing=out.mode==Mode::Pull&&up<.15f&&in.theta>2.f;
  out.push_weight=out.mode==Mode::Push?smooth((5.f-in.theta)/2.f):0;
  return out;
 }
};
}
