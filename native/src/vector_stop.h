#pragma once
#include "robust_intercept.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>

// VECTOR only: a rate allocator's stopping budget, not an independent scalar
// position controller. A mouse target may stop instantly. Absolute angular
// momentum must remain stoppable at its current position even while the
// requested target reference is large. This necessarily trades moving-target
// lag for abrupt-stop safety on an aircraft with finite angular acceleration.
namespace vector_stop {
struct Envelope : robust_intercept::Envelope {
 float positive_gain=0,negative_gain=0;
 Envelope()=default;
 Envelope(float g,float t,float d,float b,float c):robust_intercept::Envelope{g,t,d,b,c}{}
 Envelope(const robust_intercept::Envelope& e):robust_intercept::Envelope(e){}
};
using State=robust_intercept::State;
struct Plant : robust_intercept::Plant {
 double positive_gain=0,negative_gain=0;
 Plant()=default;
 Plant(double K,double T,double D,double B,double R,double F,double A,double O):robust_intercept::Plant{K,T,D,B,R,F,A,O}{}
};
inline double actuator_gain(const Plant& p,double input){double k=input>=0?p.positive_gain:p.negative_gain;return k>0?k:p.k;}
inline double neutral_input(const Plant& p){double k=p.bias>=0?actuator_gain(p,-1):actuator_gain(p,1);return -p.bias/k;}
struct Budget {
 bool valid=false,priority=false,unavoidable=false,moving_away=false;
 float safe_rate=0,stop_distance=0,queued_rate=0,queued_travel=0;
 float uncertainty_margin=0,margin=0,direction=1,release_rate=0;
};
struct Plan {
 bool valid=false,priority=false,holding_brake=false,unavoidable=false,target_stop_guard=false;
 float command=0,desired_command=0,slew_used=0,safe_rate=0,stop_distance=0;
 float finite_crossing=0,queued_rate=0,queued_travel=0,release_rate=0;
 model_control::Plan model{};
};
struct Assessment {
 bool valid=false,acceptable=false;
 float stop_distance_before=0,stop_distance_after=0,additional_stop_distance=0;
 float finite_crossing_after=0,release_rate_after=0,queued_rate=0;
};
struct Reachable {bool valid=false;float lower=0,upper=0,steady_lower=0,steady_upper=0,horizon=0;};
inline double motion_direction(double error,double rate,double queued,double release){
 if(std::abs(rate)>.1)return std::copysign(1.0,rate);
 if(std::abs(queued)>.1)return std::copysign(1.0,queued);
 if(std::abs(release)>.1)return std::copysign(1.0,release);
 return std::copysign(1.0,error);
}
struct Piece {double duration=0,input=0,decay=1,one=0;};
struct Context {bool valid=false;Plant plant{};std::array<Piece,128> queue{};size_t count=0;};
inline bool valid_model(shared_braking::Model m,float speed,float limit,float slew,float dt){
 const float gain=shared_braking::gain(m,speed);
 return std::isfinite(gain)&&gain>=1&&gain<=600&&std::isfinite(speed)&&std::isfinite(m.tau)&&m.tau>=.2f&&m.tau<=3&&
  std::isfinite(m.delay)&&m.delay>=0&&m.delay<=.5f&&std::isfinite(limit)&&limit>0&&limit<=1.0001f&&
  std::isfinite(slew)&&slew>0&&slew<=100&&std::isfinite(dt)&&dt>0&&dt<=.1f;
}
inline void advance(State& state,const Plant& p,double input,double dt,double decay,double one){
 const double eq=actuator_gain(p,input)*input+p.bias,old=state.q;
 const double travel=eq*dt+(old-eq)*p.tau*one;
 const double next=decay*old+one*eq;
 if(old*next<0&&std::abs(eq)>1e-12){const double ratio=(old-eq)/(-eq);
  if(ratio>0){const double t=p.tau*std::log(ratio);
   if(t>0&&t<dt){const double part=eq*t+(old-eq)*p.tau*(-std::expm1(-t/p.tau));
    state.low=std::min(state.low,state.travel+part);state.high=std::max(state.high,state.travel+part);}
  }
 }
 state.q=next;state.travel+=travel;state.low=std::min(state.low,state.travel);state.high=std::max(state.high,state.travel);
 state.peak_rate=std::max(state.peak_rate,std::max(std::abs(old),std::abs(next)));state.time+=dt;
}
inline void advance(State& state,const Plant& p,double input,double dt){
 const bool frame=std::abs(dt-p.frame)<1e-10;
 advance(state,p,input,dt,frame?p.decay:std::exp(-dt/p.tau),frame?p.one:-std::expm1(-dt/p.tau));
}
inline Context context(Plant p,const adaptive_braking::History& history,uint64_t now,int axis){
 Context out;out.plant=p;
 const double begin=double(now)-p.delay*1000;
 if(axis<0||axis>2||begin<0||!online_learning::continuous(history,uint64_t(begin),now))return out;
 double cursor=begin;
 auto add=[&](double until){
  if(until<=cursor)return true;if(out.count>=out.queue.size())return false;
  const double dt=(until-cursor)/1000,input=std::clamp(double(history.at_axis(uint64_t(cursor),axis)),-1.0,1.0);
  out.queue[out.count++]={dt,input,std::exp(-dt/p.tau),-std::expm1(-dt/p.tau)};cursor=until;return true;
 };
 for(size_t i=0;i<history.size;++i){const double tick=double(history.values[(history.begin+i)%history.values.size()].tick);
  if(tick>cursor&&tick<double(now)&&!add(tick))return out;
 }
 if(!add(double(now)))return out;out.valid=true;return out;
}
inline State pending(const Context& c,double rate){
 State out;out.q=rate;out.peak_rate=std::abs(rate);
 for(size_t i=0;i<c.count;++i){const auto& part=c.queue[i];advance(out,c.plant,part.input,part.duration,part.decay,part.one);}return out;
}
struct Stop {bool valid=false;double peak=0,travel=0,rate=0,time=0,first=0;};
// A common FULL-authority reverse source ladder is used in every scenario.
// Stop at its first final zero-rate crossing only after the monotonic source
// ramp has reached full reverse authority. No perfect scenario-specific trim
// or brake release is invented. The bound concerns forward displacement, not
// the eventual opposite-direction motion if full reverse were held forever.
inline Stop full_stop(const Context& c,double rate,double previous,double direction,double limit,double slew){
 Stop out;if(!c.valid)return out;auto state=pending(c,rate);const auto& p=c.plant;
 const double full=-direction*limit,delta=slew*p.frame;double input=previous;
 bool first=true;
 for(int n=0;n<1024&&std::abs(input-full)>1e-10;++n){
  input+=std::clamp(full-input,-delta,delta);if(first){out.first=input;first=false;}advance(state,p,input,p.frame);
 }
 if(first)out.first=input;
 const double eq=actuator_gain(p,full)*full+p.bias;
 if(direction*eq>=-.000001)return out;
 if(direction*state.q>0){
  const double ratio=(state.q-eq)/(-eq);if(ratio<=0||!std::isfinite(ratio))return out;
  const double time=p.tau*std::log(ratio);if(time<0||time>8)return out;advance(state,p,full,time);
 }
 out.valid=std::isfinite(state.travel)&&state.time<=8;out.peak=direction>0?state.high:-state.low;
 out.travel=state.travel;out.rate=state.q;out.time=state.time;return out;
}
inline double neutral_release(const Context& c,double rate,double previous,double limit,double slew){
 auto state=pending(c,rate);double input=previous;const auto& p=c.plant;
 const double ff=std::clamp(neutral_input(p),-limit,limit),delta=slew*p.frame;
 for(int i=0;i<1024&&std::abs(input-ff)>1e-10;++i){input+=std::clamp(ff-input,-delta,delta);advance(state,p,input,p.frame);}
 return state.q;
}
struct Cases {bool valid=false;std::array<Context,7> values{};size_t count=1;};
inline Cases cases(shared_braking::Model model,double direction,float speed,float dt,const adaptive_braking::History& history,
 uint64_t now,int axis,Envelope envelope,float bias){
 Cases out;const double gain=shared_braking::gain(model,speed);
 const double gb=std::clamp(double(envelope.gain_fraction),0.0,.65),tb=std::clamp(double(envelope.tau_fraction),0.0,.65);
 const double db=std::clamp(double(envelope.delay_seconds),0.0,.15),bb=std::clamp(double(envelope.bias_rate),0.0,15.0);
 if(!std::isfinite(gb)||!std::isfinite(tb)||!std::isfinite(db)||!std::isfinite(bb))return out;
 const std::array<std::array<int,4>,7> corner{{{0,0,0,0},{-1,1,1,1},{1,-1,1,1},{-1,-1,1,-1},{1,1,1,-1},{-1,1,-1,-1},{1,-1,-1,-1}}};
 out.count=gb+tb+db+bb<1e-8?1:7;
 for(size_t i=0;i<out.count;++i){Plant p;p.k=gain*(1+corner[i][0]*gb);
  p.tau=std::clamp(double(model.tau)*(1+corner[i][1]*tb),.2,3.0);p.delay=std::clamp(double(model.delay)+corner[i][2]*db,0.0,.5);
  p.positive_gain=envelope.positive_gain>0?envelope.positive_gain*(1+corner[i][0]*gb):p.k;p.negative_gain=envelope.negative_gain>0?envelope.negative_gain*(1+corner[i][0]*gb):p.k;
  p.bias=bias+corner[i][3]*direction*bb;p.reference=0;p.frame=dt;p.decay=std::exp(-p.frame/p.tau);p.one=-std::expm1(-p.frame/p.tau);
  out.values[i]=context(p,history,now,axis);if(!out.values[i].valid)return out;
 }out.valid=true;return out;
}
inline Budget stop_budget(shared_braking::Model model,float error,float rate,float previous,float speed,float limit,float pursuit_rate,
 float slew,float dt,const adaptive_braking::History& history,uint64_t now,int axis,Envelope envelope={},float bias=0){
 Budget out;
 if(!valid_model(model,speed,limit,slew,dt)||!std::isfinite(error)||!std::isfinite(rate)||!std::isfinite(previous)||
    !std::isfinite(bias)||!std::isfinite(pursuit_rate)||pursuit_rate<=0||std::abs(previous)>limit+.00001f)return out;
 const double direction=std::abs(error)>1e-7?std::copysign(1.0,error):std::copysign(1.0,rate);
 const auto scenarios=cases(model,direction,speed,dt,history,now,axis,envelope,bias);if(!scenarios.valid)return out;
 out.direction=float(direction);const auto nominal=pending(scenarios.values[0],rate);
 out.queued_rate=float(nominal.q);out.queued_travel=float(nominal.travel);
 out.release_rate=float(neutral_release(scenarios.values[0],rate,previous,limit,slew));
 double physical_direction=motion_direction(error,rate,nominal.q,out.release_rate);
 double actual_peak=0,nominal_peak=0;
 for(size_t i=0;i<scenarios.count;++i){auto stop=full_stop(scenarios.values[i],rate,previous,physical_direction,limit,slew);if(!stop.valid)return out;
  actual_peak=std::max(actual_peak,stop.peak);if(!i)nominal_peak=stop.peak;
 }
 out.stop_distance=float(actual_peak);out.uncertainty_margin=float(std::max(0.0,actual_peak-nominal_peak));
 const double distance=std::abs(double(error));out.margin=float(std::min(.015,.05*distance));
 const double available=std::max(0.0,distance-out.margin);
 auto required=[&](double candidate_rate){double peak=0;
  for(size_t i=0;i<scenarios.count;++i){auto stop=full_stop(scenarios.values[i],direction*candidate_rate,previous,direction,limit,slew);if(!stop.valid)return double(INFINITY);peak=std::max(peak,stop.peak);}
  return peak;
 };
 double lo=0,hi=pursuit_rate;
 if(required(0)>available)out.safe_rate=0;
 else if(required(hi)<=available)out.safe_rate=float(hi);
 else{for(int i=0;i<13;++i){double mid=.5*(lo+hi);if(required(mid)<=available)lo=mid;else hi=mid;}out.safe_rate=float(lo);}
 out.moving_away=(error*rate<0&&std::abs(rate)>.25f)||(std::abs(rate)<=.25f&&direction*out.release_rate<-.25);
 const bool pending_away=out.moving_away&&physical_direction*out.release_rate>.15;
 out.priority=pending_away||(physical_direction==direction&&actual_peak>available+.03&&(direction*rate>.25||direction*nominal.q>.5));
 out.unavoidable=physical_direction==direction&&nominal_peak>distance+.005;
 out.valid=true;return out;
}
// Quiet-history convenience. The real controller should use stop_budget so
// already issued commands and their source cadence are included.
inline float attainable_rate(shared_braking::Model model,float distance,float speed,float limit,float pursuit_rate,float slew,float dt,Envelope envelope={},float bias=0){
 adaptive_braking::History quiet;for(uint64_t tick=1000;tick<=2000;tick+=50)quiet.add({tick,0,0,0});
 auto b=stop_budget(model,distance,0,0,speed,limit,pursuit_rate,slew,dt,quiet,2000,0,envelope,bias);return b.valid?b.safe_rate:0;
}
inline State rate_hold(const Context& c,double rate,double previous,double candidate,double limit,double slew,double hold){
 auto state=pending(c,rate);double input=std::clamp(previous,-limit,limit);const auto& p=c.plant;
 const int frames=std::max(1,int(std::ceil(hold/p.frame)));const double delta=slew*p.frame;
 for(int n=0;n<frames;++n){input+=std::clamp(candidate-input,-delta,delta);advance(state,p,input,p.frame);}return state;
}
// Absolute BODY rates at the nominal delay+held-source horizon. Target motion
// is not included. Both ends use identical real ZOH cadence/slew and full input
// authority; callers additionally intersect their pursuit/steady-rate budgets.
inline Reachable reachable_rates(shared_braking::Model model,float rate,float previous,float speed,float limit,float slew,float dt,
 const adaptive_braking::History& history,uint64_t now,int axis,float horizon=.18f,float bias=0,float positive_gain=0,float negative_gain=0){
 Reachable out;
 if(!valid_model(model,speed,limit,slew,dt)||!std::isfinite(rate)||!std::isfinite(previous)||std::abs(previous)>limit+1e-5||
    !std::isfinite(horizon)||horizon<=0||horizon>1||!std::isfinite(bias))return out;
 Plant p;p.k=shared_braking::gain(model,speed);p.positive_gain=positive_gain;p.negative_gain=negative_gain;p.tau=model.tau;p.delay=model.delay;p.bias=bias;p.frame=dt;
 p.decay=std::exp(-p.frame/p.tau);p.one=-std::expm1(-p.frame/p.tau);
 auto c=context(p,history,now,axis);if(!c.valid)return out;
 const auto lo=rate_hold(c,rate,previous,-limit,limit,slew,horizon),hi=rate_hold(c,rate,previous,limit,limit,slew,horizon);
 if(!std::isfinite(lo.q)||!std::isfinite(hi.q)||lo.q>hi.q+1e-8)return out;
 out.valid=true;out.lower=float(lo.q);out.upper=float(hi.q);out.steady_lower=float(-actuator_gain(p,-1)*limit+bias);out.steady_upper=float(actuator_gain(p,1)*limit+bias);out.horizon=float(lo.time);return out;
}
inline double release_command(const Cases& scenarios,double rate,double previous,double limit,double slew){
 const auto& nominal=scenarios.values[0].plant;const double delta=slew*nominal.frame;
 const double ff=std::clamp(neutral_input(nominal),-limit,limit),lower=std::max(-limit,previous-delta),upper=std::min(limit,previous+delta);
 auto end_rate=[&](double first){
  double low=INFINITY,high=-INFINITY;
  for(size_t i=0;i<scenarios.count;++i){const auto& c=scenarios.values[i];const auto& p=c.plant;
   auto state=pending(c,rate);double input=first;advance(state,p,input,p.frame);
   // Identical serving-model neutral source in every scenario. No hidden
   // scenario-specific trim is granted to an unobserved disturbance.
   for(int n=0;n<1024&&std::abs(input-ff)>1e-10;++n){input+=std::clamp(ff-input,-delta,delta);advance(state,p,input,p.frame);}
   low=std::min(low,state.q);high=std::max(high,state.q);
  }return .5*(low+high);
 };
 if(end_rate(lower)>=0)return -limit;if(end_rate(upper)<=0)return limit;
 double lo=lower,hi=upper;for(int n=0;n<16;++n){double mid=.5*(lo+hi);if(end_rate(mid)<0)lo=mid;else hi=mid;}
 return .5*(lo+hi);
}
inline Plan rate_plan(shared_braking::Model model,float error,float desired_rate,float reference_rate,float rate,float previous,
 float speed,float limit,float pursuit_rate,float slew,float dt,const adaptive_braking::History& history,uint64_t now,int axis,
 Envelope envelope={},float bias=0,float target_braking=0){
 Plan out;out.slew_used=slew;
 if(!std::isfinite(desired_rate)||!std::isfinite(reference_rate)||!std::isfinite(target_braking))return out;
 const auto budget=stop_budget(model,error,rate,previous,speed,limit,pursuit_rate,slew,dt,history,now,axis,envelope,bias);
 if(!budget.valid)return out;
 out.safe_rate=budget.safe_rate;out.stop_distance=budget.stop_distance;out.queued_rate=budget.queued_rate;
 out.queued_travel=budget.queued_travel;out.release_rate=budget.release_rate;out.unavoidable=budget.unavoidable;
 out.target_stop_guard=std::abs(reference_rate)>1.f;
 const double direction=budget.direction,physical_direction=motion_direction(error,rate,budget.queued_rate,budget.release_rate);
 // Emergency release tests the queued state PLUS its neutral-input slew tail.
 // Error passing through zero cannot by itself authorize releasing a nose
 // that is still predicted to fly away from the new target.
 out.priority=budget.priority;out.holding_brake=budget.moving_away&&physical_direction*budget.release_rate>.15;
 if(out.priority||out.holding_brake)out.slew_used=std::max(slew,axis==0?24.f:axis==2?12.f:32.f);
 auto scenarios=cases(model,direction,speed,dt,history,now,axis,envelope,bias);if(!scenarios.valid)return out;
 const auto& nominal=scenarios.values[0];const double gain=nominal.plant.k;
 double chosen=0;
 if(out.priority||out.holding_brake){
  // Full reverse is AVAILABLE, not an obligatory large pulse at sub-degree
  // rates. Solve its sampled release tail so a tiny risk does not launch the
  // aircraft in the opposite direction through the delayed command queue.
  chosen=release_command(scenarios,rate,previous,limit,out.slew_used);
 }
 else{
  // No scalar position cost here. Track the outer loop's already coordinated
  // absolute desired-rate budget. Full transient input may accelerate toward
  // that rate; a fictitious indefinitely held equilibrium cap is not applied.
  const double wanted=std::clamp(double(desired_rate),-double(pursuit_rate),double(pursuit_rate));
  const double hold=.18;
  double lo=-limit,hi=limit;
  auto response=[&](double command){return rate_hold(nominal,rate,previous,command,limit,out.slew_used,hold).q;};
  if(wanted<=response(lo))chosen=lo;else if(wanted>=response(hi))chosen=hi;
  else{for(int i=0;i<13;++i){double mid=.5*(lo+hi);if(response(mid)<wanted)lo=mid;else hi=mid;}chosen=.5*(lo+hi);}
 }
 // Being stoppable before a frame is insufficient: that frame's extra input
 // also consumes distance. Guard the actual issued first hold and subsequent
 // full braking, rather than waiting until next frame to discover the excess.
 auto first_risk=[&](double candidate,double active_slew){
  const double issued=previous+std::clamp(candidate-previous,-active_slew*dt,active_slew*dt);
  double maximum=0;
  for(size_t i=0;i<scenarios.count;++i){const auto& c=scenarios.values[i];auto state=pending(c,rate);advance(state,c.plant,issued,dt);
   Context advanced=c;advanced.count=0;advanced.plant.delay=0;
   auto future=full_stop(advanced,state.q,issued,physical_direction,limit,active_slew);
   if(!future.valid)return double(INFINITY);
   double peak=physical_direction>0?state.high:-state.low;
   peak=std::max(peak,physical_direction*state.travel+future.peak);maximum=std::max(maximum,peak);
  }return maximum;
 };
 double maximum=first_risk(chosen,out.slew_used);
 const double available=std::max(0.0,std::abs(double(error))-budget.margin);
 if(direction==physical_direction&&std::abs(error)>.03f&&maximum>available){
  out.priority=true;out.slew_used=std::max(slew,axis==0?24.f:axis==2?12.f:32.f);
  const double brake=-physical_direction*limit,minimal=first_risk(brake,out.slew_used);
  if(minimal>available){chosen=release_command(scenarios,rate,previous,limit,out.slew_used);}
  else{
   // Preserve as much of the coordinated outer-loop request as is reachable.
   double lo=physical_direction>0?brake:chosen,hi=physical_direction>0?chosen:brake;
   for(int n=0;n<12;++n){double mid=.5*(lo+hi);const double risk=first_risk(mid,out.slew_used);
    if(physical_direction>0){if(risk<=available)lo=mid;else hi=mid;}
    else{if(risk<=available)hi=mid;else lo=mid;}
   }chosen=physical_direction>0?lo:hi;
  }
  maximum=first_risk(chosen,out.slew_used);
 }
 const double first=previous+std::clamp(chosen-previous,-double(out.slew_used)*dt,double(out.slew_used)*dt);
 out.command=float(std::clamp(first,-double(limit),double(limit)));out.desired_command=float(chosen);
 if(!std::isfinite(maximum))return out;
 out.finite_crossing=float(std::max(0.0,maximum-std::abs(double(error))));
 out.valid=true;out.model.valid=true;out.model.command=out.command;out.model.input_limit=limit;
 out.model.horizon=float(model.delay+dt);out.model.rate_limit=float(std::abs(desired_rate));out.model.stopping_angle=float(maximum);
 out.model.pursuit=out.priority?0.f:1.f;out.model.damping=0;return out;
}
// Only compares the proposed REAL first source hold with the previous plan's
// first hold. Subsequent reverse source authority is identical in both cases.
// This does not certify a complete 3D trajectory or optimize another command.
inline Assessment assess_issued(shared_braking::Model model,float error,float rate,float previous,float issued,float baseline_issued,
 float speed,float limit,float pursuit_rate,float slew_used,float dt,const adaptive_braking::History& history,uint64_t now,int axis,
 Envelope envelope={},float bias=0){
 Assessment out;
 if(!valid_model(model,speed,limit,slew_used,dt)||!std::isfinite(error)||!std::isfinite(rate)||!std::isfinite(previous)||
    !std::isfinite(issued)||!std::isfinite(baseline_issued)||!std::isfinite(pursuit_rate)||pursuit_rate<=0||!std::isfinite(bias)||
    std::abs(issued)>limit+1e-5||std::abs(baseline_issued)>limit+1e-5||
    std::abs(issued-previous)>slew_used*dt+1e-5||std::abs(baseline_issued-previous)>slew_used*dt+1e-5)return out;
 const double goal_direction=std::copysign(1.0,error);
 auto scenarios=cases(model,goal_direction,speed,dt,history,now,axis,envelope,bias);if(!scenarios.valid)return out;
 const auto queued=pending(scenarios.values[0],rate);out.queued_rate=float(queued.q);
 const double release=neutral_release(scenarios.values[0],rate,previous,limit,slew_used);
 const double direction=motion_direction(error,rate,queued.q,release);
 auto inspect=[&](double first){double maximum=0;
  for(size_t i=0;i<scenarios.count;++i){const auto& c=scenarios.values[i];auto state=pending(c,rate);advance(state,c.plant,first,dt);
   Context future=c;future.count=0;future.plant.delay=0;
   auto stop=full_stop(future,state.q,first,direction,limit,slew_used);if(!stop.valid)return double(INFINITY);
   const double peak=std::max(direction>0?state.high:-state.low,direction*state.travel+stop.peak);
   maximum=std::max(maximum,peak);
   if(!i){const double ff=std::clamp(neutral_input(c.plant),-double(limit),double(limit));double input=first,delta=slew_used*c.plant.frame;
    for(int n=0;n<1024&&std::abs(input-ff)>1e-10;++n){input+=std::clamp(ff-input,-delta,delta);advance(state,c.plant,input,c.plant.frame);}
    out.release_rate_after=float(state.q);
   }
  }return maximum;
 };
 const double before=inspect(baseline_issued),after=inspect(issued);
 if(!std::isfinite(before)||!std::isfinite(after))return out;
 out.stop_distance_before=float(before);out.stop_distance_after=float(after);out.additional_stop_distance=float(after-before);
 out.finite_crossing_after=float(std::max(0.0,after-std::abs(double(error))));
 out.valid=true;out.acceptable=after<=before+.003;return out;
}
inline Assessment assess_issued(shared_braking::Model model,float error,float rate,float previous,const Plan& baseline,float issued,
 float speed,float limit,float pursuit_rate,float dt,const adaptive_braking::History& history,uint64_t now,int axis,
 Envelope envelope={},float bias=0){
 if(!baseline.valid)return {};
 return assess_issued(model,error,rate,previous,issued,baseline.command,speed,limit,pursuit_rate,baseline.slew_used,dt,history,now,axis,envelope,bias);
}
}
