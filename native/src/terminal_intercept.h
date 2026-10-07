#pragma once
#include "model_control.h"
#include <array>
#include <algorithm>
#include <cmath>
#include <cstdint>

// Third-policy surrogate controller. This is a reachable stopping-position
// search for an identified first-order angular-rate response, not a copy of
// another game's flight dynamics. Bias is intentionally excluded, consistently
// with the serving model's existing primary controller.
namespace terminal_intercept {
constexpr double max_profile_seconds=4.0;
struct Segment { double duration=0,u0=0,u1=0; };
struct Step { double rate=0,travel=0,peak_rate=0; };

// q is rate RELATIVE to a locally constant target rate. travel is integral(q).
// Exact integration for an input changing linearly from u0 to u1 over dt.
inline Step ramp_step(double q,double u0,double u1,double dt,double k,double tau,double reference=0){
 Step out{q,0,std::abs(q+reference)};
 if(dt<=0)return out;
 const double a=std::exp(-dt/tau),one=-std::expm1(-dt/tau);
 const double eq=k*u0-reference,slope=k*(u1-u0)/dt;
 out.rate=a*q+one*eq+slope*(dt-tau*one);
 out.travel=tau*one*q+(dt-tau*one)*eq+
            (dt*dt*.5-tau*dt+tau*tau*one)*slope;
 out.peak_rate=std::max(out.peak_rate,std::abs(out.rate+reference));
 // A linear equilibrium can produce one internal angular-rate extremum.
 const double denominator=q-eq+slope*tau;
 if(std::abs(slope)>1e-12&&std::abs(denominator)>1e-12){
  const double ratio=slope*tau/denominator;
  if(ratio>0&&ratio<1){
   const double t=-tau*std::log(ratio);
   if(t>0&&t<dt){
    const double aa=std::exp(-t/tau),oo=-std::expm1(-t/tau);
    const double extremum=aa*q+oo*eq+slope*(t-tau*oo)+reference;
    out.peak_rate=std::max(out.peak_rate,std::abs(extremum));
   }
  }
 }
 return out;
}

struct Profile {
 bool valid=false,complete=false;
 std::array<Segment,6> segments{};
 size_t count=0;
 double duration=0,travel=0,absolute_travel=0,final_rate=0,peak_rate=0;
 double initial_input=0,final_input=0,reference=0,hold=0;
};
inline bool append(Profile& p,double& q,double u0,double u1,double dt,double k,double tau){
 if(dt<=1e-12)return true;
 if(!std::isfinite(dt)||dt<0||p.count>=p.segments.size())return false;
 const auto s=ramp_step(q,u0,u1,dt,k,tau,p.reference);
 if(!std::isfinite(s.rate)||!std::isfinite(s.travel)||!std::isfinite(s.peak_rate))return false;
 p.segments[p.count++]={dt,u0,u1};p.duration+=dt;p.travel+=s.travel;
 p.peak_rate=std::max(p.peak_rate,s.peak_rate);q=s.rate;return true;
}

// Future ISSUED-input profile, starting at previous. The plant sees input_at(p,
// t-delay). A caller that has already advanced the known delay queue can use
// the profile directly from that advanced state's time zero.
// Ramp/hold candidate, then the quickest slew-limited reverse pulse that can
// release to u_ff=reference/k with zero relative rate. Releasing only once q=0
// is too late: the release ramp itself would continue braking past the target.
inline Profile profile(shared_braking::Model m,double rate,double previous,double candidate,
 double speed,double limit,double slew,double hold,double reference=0){
 Profile p;
 const double k=shared_braking::gain(m,float(std::clamp(speed,130.0,660.0)));
 if(!std::isfinite(rate)||!std::isfinite(previous)||!std::isfinite(candidate)||
    !std::isfinite(speed)||!std::isfinite(limit)||!std::isfinite(slew)||!std::isfinite(hold)||
    !std::isfinite(reference)||!std::isfinite(m.tau)||m.tau<.2||m.tau>3||
    !std::isfinite(k)||k<1||k>600||limit<=0||limit>1.0001||slew<=0||slew>100||
    hold<=0||hold>.5||std::abs(rate)>600||std::abs(previous)>1.0001||
    std::abs(reference)>=k*limit-.0001)return p;
 previous=std::clamp(previous,-limit,limit);candidate=std::clamp(candidate,-limit,limit);
 const double tau=m.tau,ff=reference/k;
 p.initial_input=previous;p.final_input=ff;p.reference=reference;p.hold=hold;
 p.peak_rate=std::abs(rate);double q=rate-reference;
 const double candidate_ramp=std::min(hold,std::abs(candidate-previous)/slew);
 const double reached=previous+(candidate>previous?slew:-slew)*candidate_ramp;
 if(!append(p,q,previous,reached,candidate_ramp,k,tau)||
    !append(p,q,reached,reached,hold-candidate_ramp,k,tau))return {};

 auto triangle=[&](double brake){
  const double first=std::abs(brake-reached)/slew,last=std::abs(ff-brake)/slew;
  const auto a=ramp_step(q,reached,brake,first,k,tau,reference);
  const auto b=ramp_step(a.rate,brake,ff,last,k,tau,reference);
  return b.rate;
 };
 const double neutral_duration=std::abs(ff-reached)/slew;
 const auto neutral=ramp_step(q,reached,ff,neutral_duration,k,tau,reference);
 if(std::abs(neutral.rate)<1e-9){
  if(!append(p,q,reached,ff,neutral_duration,k,tau))return {};
 }else{
  const double sign=std::copysign(1.0,neutral.rate),full=-sign*limit;
  double brake=full,plateau=0;
  const double full_end=triangle(full);
  if(sign*full_end<0){
   // For tiny q a full reverse triangular pulse is already too strong. Find
   // its amplitude, rather than forcing an impossible negative plateau.
   double lo=0,hi=1;
   for(int i=0;i<28;++i){
    const double f=(lo+hi)*.5,b=ff+(full-ff)*f;
    if(sign*triangle(b)>0)lo=f;else hi=f;
   }
   brake=ff+(full-ff)*((lo+hi)*.5);
  }else{
   const double first=std::abs(full-reached)/slew,last=std::abs(ff-full)/slew;
   const auto at_brake=ramp_step(q,reached,full,first,k,tau,reference);
   const double eq=k*full-reference;
   // The release must start before q=0, so it finishes at q=0 and input=u_ff.
   const double release_q=last>1e-12?
    -eq*(tau/last*std::expm1(last/tau)-1):0;
   const double denominator=release_q-eq,numerator=at_brake.rate-eq;
   if(denominator==0||numerator/denominator<=0)return {};
   plateau=tau*std::log(numerator/denominator);
   if(!std::isfinite(plateau)||plateau<-.000001)return {};
   plateau=std::max(0.0,plateau);
  }
  if(!append(p,q,reached,brake,std::abs(brake-reached)/slew,k,tau)||
     !append(p,q,brake,brake,plateau,k,tau)||
     !append(p,q,brake,ff,std::abs(ff-brake)/slew,k,tau))return {};
 }
 p.final_rate=q+reference;p.absolute_travel=p.travel+reference*p.duration;
 p.complete=std::abs(q)<1e-5&&std::isfinite(p.travel)&&std::isfinite(p.final_rate)&&p.peak_rate<=600;
 p.valid=p.complete&&p.duration<=max_profile_seconds;
 return p;
}
// A bounded pursuit forecast for a target rate the plant cannot attain. This
// retains the ACTUAL requested target velocity; it never invents a q=0 stop.
inline Profile hold_profile(shared_braking::Model m,double rate,double previous,double candidate,
 double speed,double limit,double slew,double hold,double reference=0){
 Profile p;
 const double k=shared_braking::gain(m,float(std::clamp(speed,130.0,660.0)));
 if(!std::isfinite(rate)||!std::isfinite(previous)||!std::isfinite(candidate)||
    !std::isfinite(speed)||!std::isfinite(limit)||!std::isfinite(slew)||!std::isfinite(hold)||
    !std::isfinite(reference)||!std::isfinite(m.tau)||m.tau<.2||m.tau>3||
    !std::isfinite(k)||k<1||k>600||limit<=0||limit>1.0001||slew<=0||slew>100||
    hold<=0||hold>.5||std::abs(rate)>600||std::abs(previous)>1.0001)return p;
 previous=std::clamp(previous,-limit,limit);candidate=std::clamp(candidate,-limit,limit);
 p.initial_input=previous;p.reference=reference;p.hold=hold;p.peak_rate=std::abs(rate);
 double q=rate-reference;
 const double duration=std::min(hold,std::abs(candidate-previous)/slew);
 const double reached=previous+(candidate>previous?slew:-slew)*duration;
 if(!append(p,q,previous,reached,duration,k,m.tau)||
    !append(p,q,reached,reached,hold-duration,k,m.tau))return {};
 p.final_input=reached;p.final_rate=q+reference;
 p.absolute_travel=p.travel+reference*p.duration;
 p.valid=std::isfinite(p.travel)&&std::isfinite(p.final_rate)&&p.peak_rate<=600;
 return p;
}
inline double input_at(const Profile& p,double time){
 if(time<0)return p.initial_input;
 for(size_t i=0;i<p.count;++i){const auto& s=p.segments[i];
  if(time<s.duration)return s.u0+(s.u1-s.u0)*(time/s.duration);
  time-=s.duration;
 }
 return p.final_input;
}

struct DelayedState { bool valid=false;double rate=0,travel=0,peak_rate=0; };
// Integrate all actual issued-input transitions in the immutable delay queue.
// History uses zero-order-held commands, so no ramp is invented for the past.
inline DelayedState delayed(shared_braking::Model m,double rate,double speed,
 const adaptive_braking::History& history,uint64_t now,int axis){
 DelayedState out;out.rate=rate;out.peak_rate=std::abs(rate);
 const double k=shared_braking::gain(m,float(std::clamp(speed,130.0,660.0)));
 if(!std::isfinite(rate)||!std::isfinite(speed)||!std::isfinite(m.tau)||m.tau<.2||m.tau>3||
    !std::isfinite(m.delay)||m.delay<0||m.delay>.5||!std::isfinite(k)||k<1||k>600||axis<0||axis>2)return out;
 const double span=double(m.delay)*1000,begin=double(now)-span;
 if(begin<0||!online_learning::continuous(history,uint64_t(begin),now))return out;
 double cursor=begin;
 auto advance=[&](double until){
  if(until<=cursor)return true;
  const double u=std::clamp(double(history.at_axis(uint64_t(cursor),axis)),-1.0,1.0);
  const auto step=ramp_step(out.rate,u,u,(until-cursor)/1000,k,m.tau);
  out.rate=step.rate;out.travel+=step.travel;out.peak_rate=std::max(out.peak_rate,step.peak_rate);
  cursor=until;return std::isfinite(out.rate)&&std::isfinite(out.travel);
 };
 for(size_t i=0;i<history.size;++i){
  const double tick=double(history.values[(history.begin+i)%history.values.size()].tick);
  if(tick>cursor&&tick<double(now)&&!advance(tick))return {};
 }
 if(!advance(double(now)))return {};
 out.valid=true;return out;
}

// stop residual = target angle after its known delay motion - relative travel
// of a feasible candidate/stop sequence. Search only the current command; the
// game executes its first slew-limited frame and the plan is recomputed next
// frame. zone is a capture tolerance, never subtracted from the angle error.
inline model_control::Plan predict(shared_braking::Model m,float error,float rate,float previous,
 float speed,float limit,float max_rate,float zone,const adaptive_braking::History& history,
 uint64_t now,int axis,float reference,float slew,float dt){
 model_control::Plan result;
 if(!std::isfinite(error)||!std::isfinite(previous)||!std::isfinite(limit)||!std::isfinite(max_rate)||
    !std::isfinite(reference)||!std::isfinite(zone)||!std::isfinite(dt)||dt<=0||dt>.1f||
    limit<=0||limit>1.0001f||max_rate<=0||zone<0||zone>.1f||std::abs(previous)>1.0001f)return result;
 const double k=shared_braking::gain(m,std::clamp(speed,130.f,660.f));
 if(!std::isfinite(k)||k<1||k>600)return result;
 const auto advanced=delayed(m,rate,speed,history,now,axis);if(!advanced.valid)return result;
 const double hold=std::clamp(3.0*double(dt),.17,.24);
 const double remaining=double(error)+double(reference)*m.delay-advanced.travel;
 // Limit the candidate's equilibrium angular rate, while allowing the full
 // reverse-input authority for braking. A pre-existing rate/pipeline cannot
 // be undone retroactively and is retained in the reported peak rate.
 const double cap=std::min(double(limit),double(max_rate)/k);
 auto pursuit_only=[&](){
  auto evaluate_hold=[&](double candidate){return hold_profile(m,advanced.rate,previous,candidate,speed,limit,slew,hold,reference);};
  double lo=-cap,hi=cap;auto left=evaluate_hold(lo),right=evaluate_hold(hi);
  if(!left.valid||!right.valid||left.travel>right.travel+1e-7)return model_control::Plan{};
  double chosen=0;Profile selected;
  if(remaining<=left.travel){chosen=lo;selected=left;}
  else if(remaining>=right.travel){chosen=hi;selected=right;}
  else{
   for(int i=0;i<16;++i){double mid=(lo+hi)*.5;auto trial=evaluate_hold(mid);
    if(!trial.valid)return model_control::Plan{};
    if(trial.travel<remaining)lo=mid;else hi=mid;
   }
   chosen=(lo+hi)*.5;selected=evaluate_hold(chosen);
  }
  model_control::Plan out;out.valid=selected.valid;out.command=float(chosen);
  out.stopping_angle=0; // explicitly unavailable for an unreachable reference
  out.horizon=float(m.delay+hold);out.rate_limit=float(std::max(advanced.peak_rate,selected.peak_rate));
  out.pursuit=float(std::clamp(std::abs(chosen)/std::max(.0001,cap),0.0,1.0));
  out.damping=-1;out.input_limit=limit;return out;
 };
 if(std::abs(reference)>=k*limit-.0001||std::abs(reference)>max_rate+.0001)return pursuit_only();
 auto evaluate=[&](double candidate){return profile(m,advanced.rate,previous,candidate,speed,limit,slew,hold,reference);};
 double lo=-cap,hi=cap;auto left=evaluate(lo),right=evaluate(hi);
 if(!left.complete||!right.complete||left.travel>right.travel+1e-7)return pursuit_only();
 double chosen=0;Profile selected;
 if(remaining<=left.travel){chosen=lo;selected=left;}
 else if(remaining>=right.travel){chosen=hi;selected=right;}
 else{
  for(int i=0;i<16;++i){
   const double mid=(lo+hi)*.5;auto trial=evaluate(mid);
   if(!trial.complete)return pursuit_only();
   if(trial.travel<remaining)lo=mid;else hi=mid;
  }
  chosen=(lo+hi)*.5;selected=evaluate(chosen);if(!selected.complete)return pursuit_only();
 }
 // In the sub-degree capture band, source/measurement latency
 // error can turn a stopping-position inversion into a small limit cycle.
 // Blend into a weak CURRENT-relative-rate hold loop there. The already-issued
 // queue must also be considered: small present error alone is not arrival.
 auto smooth=[](double x){x=std::clamp(x,0.0,1.0);return x*x*(3-2*x);};
 const double q_current=double(rate)-reference,q_pending=advanced.rate-reference;
 const double micro_outer=axis==0?.36:.08,micro_inner=axis==0?.10:.025;
 const double launch_guard=smooth((.20-std::abs(double(error)))/.10)+
  (1-smooth((.20-std::abs(double(error)))/.10))*smooth((std::abs(q_current)+std::abs(q_pending))/.5);
 // A nearly motionless nose with a large already-issued motion is not a hold
 // state. Keep its predictive braking instead of waiting for rate feedback.
 const double incoming_guard=1-smooth((std::abs(q_pending)-3.0)/1.0)*
  (1-smooth((std::abs(q_current)-.5)/.5));
 const double micro=incoming_guard*launch_guard*smooth((micro_outer-std::abs(double(error)))/(micro_outer-micro_inner))*
                    smooth((10.0-std::abs(q_current))/4.0)*
                    smooth((12.0-std::abs(q_pending))/4.0);
 if(micro>0){
  const double local=std::clamp((double(reference)+4.0*error-1.8*q_current)/k,-cap,cap);
  chosen+=micro*(local-chosen);selected=evaluate(chosen);
  if(!selected.complete)return pursuit_only();
 }
 result.valid=true;result.command=float(chosen);
 const bool certified=selected.valid&&double(m.delay)+selected.duration<=max_profile_seconds;
 result.stopping_angle=certified?float(advanced.travel+selected.absolute_travel):0;
 result.horizon=float(std::min(max_profile_seconds,double(m.delay)+selected.duration));
 result.rate_limit=float(std::max(advanced.peak_rate,selected.peak_rate));
 result.pursuit=float(std::clamp(std::abs(chosen)/std::max(.0001,cap),0.0,1.0));
 result.damping=certified?0:-1;result.input_limit=limit;return result;
}
}
