#pragma once
#include "terminal_intercept.h"
#include <array>
#include <algorithm>
#include <cmath>

// CAPTURE only. Current commands are zero-order held by the source. Predict
// the same immediate, slew-clipped first command and the subsequent frame
// ladder. Uncertainty is supplied by the aircraft-local residual observer;
// these bounds are not a replacement for model identification.
namespace robust_intercept {
struct Envelope {
 float gain_fraction=.15f,tau_fraction=.20f,delay_seconds=.035f,bias_rate=0,confidence=.35f;
};
struct Result {
 model_control::Plan plan{};
 float issued_command=0,desired_command=0,stop_residual=0,worst_crossing=0,unavoidable_crossing=0;
 float uncertainty_margin=0,confidence=0;
 bool unavoidable=false,reference_unreachable=false,source_certified=false;
};
struct SourceTrace {
 std::array<float,512> inputs{};size_t count=0;
 float frame_dt=0,initial_input=0,final_input=0,duration=0;
 bool valid=false,overflow=false;
};
inline double input_at(const SourceTrace& source,double time){
 if(time<0)return source.initial_input;
 const size_t frame=source.frame_dt>0?size_t(time/source.frame_dt):source.count;
 return frame<source.count?source.inputs[frame]:source.final_input;
}
inline double smooth(double x){x=std::clamp(x,0.0,1.0);return x*x*(3-2*x);}
struct State {
 double q=0,travel=0,low=0,high=0,peak_rate=0,time=0;
};
struct Plant {
 double k=0,tau=0,delay=0,bias=0,reference=0,frame=0,decay=0,one=0;
};
inline void step(State& s,const Plant& p,double u,double dt){
 if(dt<=0)return;
 const double a=std::abs(dt-p.frame)<1e-10?p.decay:std::exp(-dt/p.tau);
 const double one=std::abs(dt-p.frame)<1e-10?p.one:-std::expm1(-dt/p.tau);
 const double eq=p.k*u+p.bias-p.reference,old=s.q;
 const double travel=eq*dt+(old-eq)*p.tau*one;
 s.q=a*old+one*eq;
 // Travel can peak between source frames while angular rate changes sign.
 if(old*s.q<0&&std::abs(eq)>1e-12){
  const double ratio=(old-eq)/(-eq);
  if(ratio>0){const double t=p.tau*std::log(ratio);
   if(t>0&&t<dt){const double at=eq*t+(old-eq)*p.tau*(-std::expm1(-t/p.tau));
    s.low=std::min(s.low,s.travel+at);s.high=std::max(s.high,s.travel+at);}
  }
 }
 s.travel+=travel;s.low=std::min(s.low,s.travel);s.high=std::max(s.high,s.travel);
 s.peak_rate=std::max(s.peak_rate,std::max(std::abs(old+p.reference),std::abs(s.q+p.reference)));
 s.time+=dt;
}
inline bool pending(State& out,const Plant& p,double rate,const adaptive_braking::History& history,uint64_t now,int axis){
 out={};out.q=rate-p.reference;out.peak_rate=std::abs(rate);
 const double begin=double(now)-1000*p.delay;
 if(begin<0||!online_learning::continuous(history,uint64_t(begin),now))return false;
 double cursor=begin;
 auto advance=[&](double until){
  if(until<=cursor)return;
  step(out,p,std::clamp(double(history.at_axis(uint64_t(cursor),axis)),-1.0,1.0),(until-cursor)/1000);
  cursor=until;
 };
 for(size_t i=0;i<history.size;++i){const double tick=double(history.values[(history.begin+i)%history.values.size()].tick);
  if(tick>cursor&&tick<double(now))advance(tick);
 }
 advance(double(now));return std::isfinite(out.q)&&std::isfinite(out.travel);
}
inline void held_step(State& state,const Plant& p,double u,SourceTrace* source=nullptr){
 if(source){if(source->count<source->inputs.size())source->inputs[source->count++]=float(u);else source->overflow=true;}
 step(state,p,u,p.frame);
}
inline void ladder(State& s,const Plant& p,double& u,double target,double slew,SourceTrace* source=nullptr){
 const double delta=slew*p.frame;
 for(int i=0;i<512&&std::abs(u-target)>1e-10;++i){
  u+=std::clamp(target-u,-delta,delta);held_step(s,p,u,source);
 }
}
struct Forecast {
 bool valid=false,complete=false;
 double travel=0,low=0,high=0,rate=0,peak_rate=0,duration=0,first=0;
};
// Build a sampled reverse/release pulse. A final residual rate is
// integrated to its equilibrium exactly rather than pretending that a sampled
// continuous-ramp profile finishes at zero rate.
inline Forecast forecast(const Plant& p,State state,double previous,double candidate,double limit,double slew,double hold,bool stop=true,SourceTrace* source=nullptr,double first=NAN){
 Forecast out;double u=std::clamp(previous,-limit,limit);
 if(source){*source={};source->frame_dt=float(p.frame);source->initial_input=float(u);}
 const int frames=std::max(1,int(std::ceil(hold/p.frame-1e-10)));
 const double delta=slew*p.frame;
 for(int n=0;n<frames;++n){
  const double wanted=n==0&&std::isfinite(first)?first:candidate;
  u+=std::clamp(wanted-u,-delta,delta);if(n==0)out.first=u;held_step(state,p,u,source);
 }
 const Plant& serving=p;
 const double ff=(serving.reference-serving.bias)/serving.k;
 if(!stop||std::abs(ff)>=limit-.000001){
  out.valid=!(source&&source->overflow);out.travel=state.travel;out.low=state.low;out.high=state.high;
  if(source){source->final_input=float(u);source->duration=float(source->count*p.frame);source->valid=out.valid;}
  out.rate=state.q+p.reference;out.peak_rate=state.peak_rate;out.duration=state.time;return out;
 }
 // A smooth serving-model reverse/release profile supplies the brake shape;
 // only its real source-grid samples are applied to the predicted plant.
 shared_braking::Model model{float(serving.tau),0,float(serving.k),0,0};
 const double adjusted_reference=serving.reference-serving.bias;
 const auto profile=terminal_intercept::profile(model,state.q+adjusted_reference,u,u,360,limit,slew,.00001,adjusted_reference);
 if(!profile.valid)return out;
 const int brake_frames=std::max(1,int(std::ceil(profile.duration/p.frame-1e-10)));
 for(int i=0;i<brake_frames;++i){
  const double wanted=terminal_intercept::input_at(profile,(i+1)*p.frame);
  u+=std::clamp(wanted-u,-delta,delta);held_step(state,p,u,source);
 }
 ladder(state,p,u,ff,slew,source);
 // A source-grid pulse can leave a small rate. Holding the physically required
 // feedforward input makes its remaining travel tau*q, with no false zero.
 const double tail=p.tau*state.q;
 state.travel+=tail;state.low=std::min(state.low,state.travel);state.high=std::max(state.high,state.travel);
 out.valid=std::isfinite(state.travel)&&state.time<=terminal_intercept::max_profile_seconds&&!(source&&source->overflow);
 out.complete=out.valid;out.travel=state.travel;out.low=state.low;out.high=state.high;
 if(source){source->final_input=float(u);source->duration=float(source->count*p.frame);source->valid=out.valid;}
 out.rate=p.reference;out.peak_rate=state.peak_rate;out.duration=state.time;return out;
}
// Predict a common nominal issued sequence in a perturbed plant for a bounded
// lookahead. Unknown bias is not granted an omniscient scenario-specific trim.
inline Forecast common_forecast(const Plant& p,State state,const SourceTrace& source,double tail=.20,double max_future=INFINITY){
 Forecast out;if(!source.valid)return out;
 double future=0;
 for(size_t i=0;i<source.count&&future<max_future;++i){
  const double duration=std::min(double(source.frame_dt),max_future-future);
  step(state,p,source.inputs[i],duration);future+=duration;
 }
 if(future>=source.duration-1e-6)step(state,p,source.final_input,std::min(tail,std::max(0.0,max_future-future)));
 out.valid=std::isfinite(state.travel);out.complete=false;out.travel=state.travel;out.low=state.low;out.high=state.high;
 out.rate=state.q+p.reference;out.peak_rate=state.peak_rate;out.duration=state.time;return out;
}
inline Result predict(shared_braking::Model m,float error,float rate,float previous,float speed,float limit,float max_rate,
 float zone,const adaptive_braking::History& history,uint64_t now,int axis,float reference,float slew,float dt,
 float bias=0,Envelope envelope={}){
 Result out;out.confidence=std::clamp(envelope.confidence,0.f,1.f);
 const double k=shared_braking::gain(m,std::clamp(speed,130.f,660.f));
 if(!std::isfinite(error)||!std::isfinite(rate)||!std::isfinite(previous)||!std::isfinite(reference)||
    !std::isfinite(bias)||!std::isfinite(dt)||!std::isfinite(slew)||!std::isfinite(k)||
    !std::isfinite(m.tau)||!std::isfinite(m.delay)||dt<=0||dt>.1f||slew<=0||slew>100||
    k<1||k>600||m.tau<.2f||m.tau>3||m.delay<0||m.delay>.5f||
    !std::isfinite(limit)||limit<=0||limit>1.0001f||!std::isfinite(max_rate)||max_rate<=0||
    !std::isfinite(zone)||zone<0||zone>.1f||std::abs(previous)>1.0001f||axis<0||axis>2)return out;
 const double sign=std::abs(error)>1e-7?std::copysign(1.0,error):std::copysign(1.0,rate-reference);
 const double distance=std::abs(double(error)),cap=std::min(double(limit),double(max_rate)/k);
 // Bounds affect the terminal approach, then relinquish the last few
 // hundredths of a degree to measured-rate feedback. A fixed open-loop
 // uncertainty envelope must not freeze the final correction indefinitely.
 const double terminal_weight=(1-smooth((distance-8.0)/17.0))*smooth((distance-.025)/.225);
 const double gain_bound=std::clamp(double(envelope.gain_fraction),0.0,.65)*terminal_weight;
 const double tau_bound=std::clamp(double(envelope.tau_fraction),0.0,.65)*terminal_weight;
 const double delay_bound=std::clamp(double(envelope.delay_seconds),0.0,.15)*terminal_weight;
 const double bias_bound=std::clamp(double(envelope.bias_rate),0.0,15.0)*terminal_weight;
 if(!std::isfinite(gain_bound)||!std::isfinite(tau_bound)||!std::isfinite(delay_bound)||!std::isfinite(bias_bound))return out;
 constexpr int scenario_count=7;
 const std::array<std::array<int,3>,scenario_count> corners{{{0,0,0},{1,-1,1},{-1,1,1},{1,1,1},{-1,-1,1},{1,-1,-1},{-1,1,-1}}};
 std::array<Plant,scenario_count> plants{};std::array<State,scenario_count> advanced{};
 const bool certain=gain_bound<1e-6&&tau_bound<1e-6&&delay_bound<1e-6&&bias_bound<1e-6;
 const int count=certain?1:scenario_count;
 for(int i=0;i<count;++i){auto& p=plants[i];
  p.k=k*(1+corners[i][0]*gain_bound);p.tau=std::clamp(double(m.tau)*(1+corners[i][1]*tau_bound),.2,3.0);
  p.delay=std::clamp(double(m.delay)+corners[i][2]*delay_bound,0.0,.5);
  p.bias=bias+(i?sign*bias_bound:0);p.reference=reference;p.frame=dt;
  p.decay=std::exp(-double(dt)/p.tau);p.one=-std::expm1(-double(dt)/p.tau);
  if(!pending(advanced[i],p,rate,history,now,axis))return out;
 }
 const double hold=std::clamp(3.0*double(dt),.17,.24);
 const bool reachable=std::abs(reference-bias)<k*limit-.000001&&std::abs(reference)<=max_rate+.000001;
 out.reference_unreachable=!reachable;
 struct Assessment {bool valid=false,complete=false;double reach=0,cross=0;Forecast nominal{};};
 auto evaluate=[&](double command,bool stopping){
  Assessment a;a.valid=true;a.complete=true;a.reach=-INFINITY;a.cross=-INFINITY;
  SourceTrace source;
  a.nominal=forecast(plants[0],advanced[0],previous,command,limit,slew,hold,stopping,&source);
  if(!a.nominal.valid){a.valid=false;return a;}
  // Uncertain open-loop plans are credible over a short causal window; after
  // it, actual feedback and aircraft-local identification replan every frame.
  // Do not use the hypothetical whole-stop bias drift to suppress progress.
  for(int i=0;i<count;++i){auto f=i?common_forecast(plants[i],advanced[i],source,stopping?.10:0,stopping?.35:hold):a.nominal;
   if(!f.valid){a.valid=false;return a;}
   if(i==0)a.complete=f.complete;
   const double peak=sign>0?f.high:-f.low;
   a.reach=std::max(a.reach,peak);a.cross=std::max(a.cross,peak-distance);
  }return a;
 };
 // Candidate remains a future desired input while issued_command is its
 // actual first frame. Search in desired-input space to retain pursuit when
 // the current slew constraint makes several candidates share a first frame.
 double lo=-cap,hi=cap;auto left=evaluate(lo,reachable),right=evaluate(hi,reachable);
 if(!left.valid||!right.valid)return out;
 Assessment selected;double chosen=0;
 if(distance<1e-7&&std::abs(rate-reference)<1e-7&&std::abs(advanced[0].travel)<1e-7&&
    std::abs(previous-(reference-bias)/k)<1e-7){chosen=(reference-bias)/k;selected=evaluate(chosen,reachable);}
 else if(!reachable){
  // Preserve an unattainable moving reference. This is a bounded pursuit
  // forecast, not a damping fallback or a fabricated reachable stop.
  auto travel=[&](const Assessment& a){return a.nominal.travel;};
  if(double(error)<=travel(left)){chosen=lo;selected=left;}
  else if(double(error)>=travel(right)){chosen=hi;selected=right;}
  else{for(int j=0;j<13;++j){const double mid=.5*(lo+hi);auto a=evaluate(mid,false);if(!a.valid)return out;if(a.nominal.travel<error)lo=mid;else hi=mid;}chosen=.5*(lo+hi);selected=evaluate(chosen,false);}
 }else{
  // Use maximum relative travel, including pipeline and release excursions.
  // An already-issued unavoidable crossing requests the strongest feasible
  // brake and reports its irreducible amount; no zero-overshoot claim is made.
  Assessment brake=sign>0?left:right;
  const double minimal_cross=std::max(0.0,brake.cross);
  const double nominal_minimum=std::max(0.0,(sign>0?brake.nominal.high:-brake.nominal.low)-distance);
  out.unavoidable_crossing=float(nominal_minimum);out.unavoidable=nominal_minimum>std::max(.005,double(zone)*.2);
  // First invert the nominal signed stop. A maximum-travel barrier alone has
  // flat regions caused by irrevocable pending motion; searching their edge
  // can accidentally command a large excursion away from the target.
  if(double(error)<=left.nominal.travel){chosen=lo;selected=left;}
  else if(double(error)>=right.nominal.travel){chosen=hi;selected=right;}
  else{
   for(int j=0;j<13;++j){double mid=.5*(lo+hi);auto a=evaluate(mid,true);if(!a.valid)return out;
    if(a.nominal.travel<error)lo=mid;else hi=mid;
   }
   chosen=.5*(lo+hi);selected=evaluate(chosen,true);
  }
  const double tolerance=std::min(double(zone)*.15,.004);
  const double permitted=distance+minimal_cross+tolerance;
  if(selected.reach>permitted){
   // Back off only from the nominal interception, retaining the least extra
   // inherited crossing and avoiding a full reverse hold for a tiny passage.
   lo=sign>0?-cap:chosen;hi=sign>0?chosen:cap;
   for(int j=0;j<12;++j){double mid=.5*(lo+hi);auto a=evaluate(mid,true);if(!a.valid)return out;
    if(sign>0){if(a.reach<=permitted)lo=mid;else hi=mid;}
    else{if(a.reach<=permitted)hi=mid;else lo=mid;}
   }
   chosen=sign>0?lo:hi;selected=evaluate(chosen,true);
  }
  // Under a wide cold-model envelope, strict fixed-plan non-crossing may be
  // incompatible with any useful nominal progress. Preserve a third of the
  // requested stop displacement, disclose the remaining scenario crossing,
  // and let measured feedback/model adaptation resolve that uncertainty.
  const double progress=.35*distance;
  if(sign*selected.nominal.travel<progress&&distance>.08){
   double low=-cap,high=cap;const double target=sign*progress;
   if(target>=left.nominal.travel&&target<=right.nominal.travel){
    for(int n=0;n<12;++n){double mid=.5*(low+high);auto a=evaluate(mid,true);if(!a.valid)return out;
     if(a.nominal.travel<target)low=mid;else high=mid;
    }
    chosen=.5*(low+high);selected=evaluate(chosen,true);
   }
  }

 }
 if(!selected.valid)return out;
 if(reachable){
  // The last source-grid quantum is smaller than model/rate noise. Blend
  // continuously to a measured relative-rate hold law there; retain stop
  // prediction outside that small band and for an unattainable reference.
  const double q=rate-reference,qp=advanced[0].q;
  const double outer=axis==0?.36:.12,inner=axis==0?.025:.015;
  const double micro=smooth((outer-distance)/(outer-inner))*smooth((2.5-std::abs(q))/1.0)*smooth((3.0-std::abs(qp))/1.0);
  if(micro>0){
   const double local=std::clamp((reference-bias+4.0*error-2.2*q)/k,-cap,cap);
   chosen+=micro*(local-chosen);selected=evaluate(chosen,true);
   if(!selected.valid)return out;
  }
 }
 out.plan.valid=true;out.plan.command=float(selected.nominal.first);out.issued_command=out.plan.command;out.desired_command=float(chosen);
 out.plan.horizon=float(std::min(terminal_intercept::max_profile_seconds,selected.nominal.duration));
 out.plan.stopping_angle=selected.complete?float(selected.nominal.travel+reference*selected.nominal.duration):0;
 out.plan.rate_limit=float(selected.nominal.peak_rate);out.plan.input_limit=limit;
 out.plan.pursuit=float(std::clamp(std::abs(chosen)/std::max(.0001,cap),0.0,1.0));out.plan.damping=selected.complete?0:-1;
 out.stop_residual=float(double(error)-selected.nominal.travel);out.worst_crossing=float(std::max(0.0,selected.cross));
 // This flag certifies only the named common-source finite lookahead, never
 // an unknown aircraft's whole future trajectory. Joint/input changes must
 // clear it and be evaluated again by the caller.
 out.source_certified=reachable&&out.worst_crossing<=.01f&&!out.unavoidable;
 const double nominal_peak=sign>0?selected.nominal.high:-selected.nominal.low;
 out.uncertainty_margin=float(std::max(0.0,selected.reach-nominal_peak));return out;
}
// Read-only post-output audit. Rebuild the actual issued first frame and its
// intended continuation; never re-optimize or change the caller's command.
inline Result assess_issued(shared_braking::Model m,float error,float rate,float previous,float issued,float desired,
 float speed,float limit,float max_rate,float zone,const adaptive_braking::History& history,uint64_t now,int axis,
 float reference,float slew,float dt,float bias=0,Envelope envelope={}){
 Result out;out.issued_command=issued;out.desired_command=desired;out.confidence=std::clamp(envelope.confidence,0.f,1.f);
 const double k=shared_braking::gain(m,std::clamp(speed,130.f,660.f));
 if(!std::isfinite(error)||!std::isfinite(rate)||!std::isfinite(previous)||!std::isfinite(issued)||!std::isfinite(desired)||
    !std::isfinite(reference)||!std::isfinite(bias)||!std::isfinite(k)||!std::isfinite(dt)||!std::isfinite(slew)||
    !std::isfinite(m.tau)||!std::isfinite(m.delay)||k<1||k>600||m.tau<.2f||m.tau>3||m.delay<0||m.delay>.5f||
    dt<=0||dt>.1f||slew<=0||slew>100||!std::isfinite(limit)||limit<=0||limit>1.0001f||
    !std::isfinite(max_rate)||max_rate<=0||!std::isfinite(zone)||zone<0||zone>.1f||axis<0||axis>2||
    std::abs(issued)>limit+1e-5||std::abs(issued-previous)>slew*dt+1e-5)return out;
 const double distance=std::abs(double(error)),sign=std::abs(error)>1e-7?std::copysign(1.0,error):std::copysign(1.0,rate-reference);
 const double terminal_weight=(1-smooth((distance-8.0)/17.0))*smooth((distance-.025)/.225);
 const double gb=std::clamp(double(envelope.gain_fraction),0.0,.65)*terminal_weight,tb=std::clamp(double(envelope.tau_fraction),0.0,.65)*terminal_weight;
 const double db=std::clamp(double(envelope.delay_seconds),0.0,.15)*terminal_weight,bb=std::clamp(double(envelope.bias_rate),0.0,15.0)*terminal_weight;
 if(!std::isfinite(gb)||!std::isfinite(tb)||!std::isfinite(db)||!std::isfinite(bb))return out;
 const std::array<std::array<int,3>,7> corners{{{0,0,0},{1,-1,1},{-1,1,1},{1,1,1},{-1,-1,1},{1,-1,-1},{-1,1,-1}}};
 double maximum=-INFINITY;Forecast nominal;SourceTrace source;
 const double hold=std::clamp(3.0*double(dt),.17,.24);
 out.reference_unreachable=std::abs(reference-bias)>=k*limit-.000001||std::abs(reference)>max_rate+.000001;
 for(int i=0;i<7;++i){
  Plant p;p.k=k*(1+corners[i][0]*gb);p.tau=std::clamp(double(m.tau)*(1+corners[i][1]*tb),.2,3.0);
  p.delay=std::clamp(double(m.delay)+corners[i][2]*db,0.0,.5);p.bias=bias+(i?sign*bb:0);p.reference=reference;p.frame=dt;
  p.decay=std::exp(-double(dt)/p.tau);p.one=-std::expm1(-double(dt)/p.tau);
  State advanced;if(!pending(advanced,p,rate,history,now,axis))return out;
  Forecast f;
  if(!i){f=forecast(p,advanced,previous,std::clamp(double(desired),-double(limit),double(limit)),limit,slew,hold,!out.reference_unreachable,&source,issued);nominal=f;}
  else f=common_forecast(p,advanced,source,out.reference_unreachable?0:.10,out.reference_unreachable?hold:.35);
  if(!f.valid)return out;
  maximum=std::max(maximum,sign>0?f.high:-f.low);
  // Pre-existing pending travel is irrevocable. A negative continuation is
  // not called unavoidable solely because a perturbed unknown bias predicts it.
  if(!i)out.unavoidable_crossing=float(std::max(0.0,(sign>0?advanced.high:-advanced.low)-distance));
 }
 out.plan.valid=true;out.plan.command=issued;out.plan.input_limit=limit;out.plan.damping=nominal.complete?0:-1;
 out.plan.horizon=float(nominal.duration);out.plan.stopping_angle=nominal.complete?float(nominal.travel+reference*nominal.duration):0;
 out.plan.rate_limit=float(nominal.peak_rate);out.stop_residual=float(error-nominal.travel);
 out.worst_crossing=float(std::max(0.0,maximum-distance));
 out.uncertainty_margin=float(std::max(0.0,maximum-(sign>0?nominal.high:-nominal.low)));
 out.unavoidable=out.unavoidable_crossing>std::max(.005f,zone*.2f);
 out.source_certified=!out.reference_unreachable&&!out.unavoidable&&out.worst_crossing<=.01f;return out;
}
}




