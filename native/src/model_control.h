#pragma once
#include "online_learning.h"
// Closed-form, constrained one-move model prediction. It is not a multi-step
// optimizer or a reconstruction of the game's aerodynamics.
namespace model_control {
// Causal reference-motion estimate. A single mouse jump is a new point target,
// not evidence that the target will keep moving. Stop carrying motion after25ms.
struct Reference {
 float rate=0,previous=0,idle=0;unsigned streak=0;
 void reset(){*this=Reference{};}
 float update(float sample,bool moving,float dt,float limit){
  if(!std::isfinite(sample)||!std::isfinite(dt)||dt<=0||dt>.1){reset();return 0;}
  if(moving&&std::abs(sample)>.05f){
   idle=0;
   if(streak&&sample*previous<0){streak=1;rate=0;}else streak=std::min(4u,streak+1);
   previous=sample;
   if(streak>=3)rate+=(std::clamp(sample,-limit,limit)-rate)*(1-std::exp(-dt/.025f));
  }else{
   idle+=dt;rate*=std::exp(-dt/.025f);
   if(idle>=.025f)reset();
  }
  return std::clamp(rate,-limit,limit);
 }
};
inline float forward_error(float error,float travel,float reference,float delay){return error+reference*delay-travel;}
// Learned parameter deployment requires longer-horizon validation. Full-model
// control can use the standard model while this independent check runs.
struct Quality {
 uint64_t anchor=0;float rate=0,speed=0;shared_braking::Model learned{},baseline{};
 unsigned count=0;float learned_error=0,baseline_error=0;bool usable=false;
 shared_braking::Model tested_model{};uint64_t evaluated_tick=0;
 void reset(){*this=Quality{};}
 void observe(shared_braking::Model model,shared_braking::Model standard,int axis,uint64_t now,float current,float velocity,const adaptive_braking::History& history){
  if(!std::isfinite(current)||!std::isfinite(velocity)){reset();return;}
  if(anchor&&now>=anchor&&now-anchor<600)return;
  if(anchor){
   float span=float(now-anchor)/1000;uint64_t longest=uint64_t(std::max(learned.delay,baseline.delay)*1000);
   if(span<=.8f&&anchor>=longest&&std::abs(speed-velocity)<40&&online_learning::continuous(history,anchor-longest,now)){
    auto forecast=[&](shared_braking::Model m){float predicted=rate,d=std::exp(-span/(32*m.tau)),k=shared_braking::gain(m,std::clamp(speed,130.f,660.f));
     for(int i=0;i<32;++i){auto t=anchor+uint64_t((i+.5f)*span*1000/32)-uint64_t(m.delay*1000);predicted=d*predicted+(1-d)*k*history.at_axis(t,axis);}return predicted;};
    float e=std::pow(forecast(learned)-current,2),b=std::pow(forecast(baseline)-current,2);
    if(!count){learned_error=e;baseline_error=b;}else{learned_error+=.2f*(e-learned_error);baseline_error+=.2f*(b-baseline_error);}
    count=std::min(1000u,count+1);float tolerance=axis==0?8.f:axis==1?25.f:1.5f;
    usable=count>=6&&baseline_error>.0025f&&learned_error<.95f*baseline_error&&learned_error<tolerance*tolerance;
    tested_model=learned;evaluated_tick=now;
   }else{count=0;usable=false;}
  }
  anchor=now;rate=current;speed=velocity;learned=model;baseline=standard;
 }
};
struct Plan {bool valid=false;float command=0,stopping_angle=0,horizon=0,rate_limit=0,pursuit=0,damping=1,input_limit=0;};
inline Plan predict(shared_braking::Model m,float error,float rate,float previous,float speed,
                    float limit,float max_rate,float zone,const adaptive_braking::History& history,
                    uint64_t now,int axis,float reference_rate=0,float damping_scale=1,bool agile=false){
 Plan result;
 if(!std::isfinite(error)||!std::isfinite(rate)||!std::isfinite(previous)||!std::isfinite(speed)||
    !std::isfinite(reference_rate)||!std::isfinite(damping_scale)||!std::isfinite(m.tau)||!std::isfinite(m.delay)||m.tau<.2f||m.tau>3.f||m.delay<0||m.delay>.5f||limit<=0||max_rate<=0)return result;
 reference_rate=std::clamp(reference_rate,-max_rate,max_rate);
 float k=shared_braking::gain(m,std::clamp(speed,130.f,660.f));
 if(!std::isfinite(k)||k<1||k>600)return result;
 auto delay_ms=uint64_t(m.delay*1000);
 if(now<delay_ms||!online_learning::continuous(history,now-delay_ms,now))return result;
 // Propagate commands already in the estimated delay pipeline. No future input
 // is used, and no unvalidated trim/bias is commanded at zero error and rate.
 float w=rate,travel=0,step=m.delay/20,decay=std::exp(-step/m.tau);
 for(int i=0;i<20;++i){
  auto t=now-delay_ms+uint64_t((i+.5f)*m.delay*50);
  float input=std::clamp(history.at_axis(t,axis),-1.f,1.f),eq=k*input;
  travel+=eq*step+(w-eq)*m.tau*(1-decay);w=decay*w+(1-decay)*eq;
 }
 float advanced_error=forward_error(error,travel,reference_rate,m.delay);
 float remaining=std::copysign(std::max(0.f,std::abs(advanced_error)-zone),advanced_error);
 float brake_rate=k*limit,stop_time=m.tau*std::log1p(std::abs(w)/brake_rate);
 result.stopping_angle=std::max(0.f,std::abs(travel)+m.tau*(std::abs(w)-brake_rate*std::log1p(std::abs(w)/brake_rate)));
 float attainable_fraction=.85f;
 if(agile){
  // Use the original braking authority to decide whether there is room to
  // accelerate. Uncertain model response and rate-estimation lag get a buffer.
  float closing=std::max(0.f,std::copysign(1.f,advanced_error)*(w-reference_rate));
  float stop=m.tau*(closing-brake_rate*std::log1p(closing/brake_rate));
  float guard=2.5f*std::max(0.f,stop)+.20f*closing+zone;
  float margin=axis==0?4.f:axis==1?10.f:1.5f;
  float width=axis==0?15.f:axis==1?40.f:6.f;
  float distance=std::min(std::abs(error),std::abs(advanced_error));
  result.pursuit=online_learning::smooth((distance-guard-margin)/width);
  float p=result.pursuit;
  damping_scale+=(1.25f-damping_scale)*p;
  limit+=(axis==0?.15f:axis==1?0.f:.15f)*p;
  max_rate*=1+(axis==0?1.f/3.f:axis==1?.25f:2.f/7.f)*p;
  attainable_fraction+=.10f*p;
 }
 result.damping=damping_scale;result.input_limit=limit;
 // Delay is already propagated above. A very long control horizon would make
 // moving-target tracking sluggish even with reference-velocity feedforward.
 float horizon=std::clamp(.25f*m.tau+.25f*stop_time,.2f,.65f);
 float terminal=std::clamp(.15f*m.tau+.25f*m.delay,.10f,.35f);
 terminal*=std::clamp(damping_scale,1.f,3.f);
 float a=std::exp(-horizon/m.tau),wa=m.tau*(1-a),ua=k*(horizon-wa),uw=k*(1-a);
 float future_error=remaining+reference_rate*horizon-wa*w,future_rate=a*w;
 float slew_cost=std::pow(.15f*ua,2),effort_cost=std::pow(.03f*ua,2);
 float denom=ua*ua+terminal*terminal*uw*uw+slew_cost+effort_cost;
 if(!std::isfinite(denom)||denom<1e-6f)return result;
 float command=(ua*future_error-terminal*terminal*uw*(future_rate-reference_rate)+slew_cost*previous)/denom;
 float rate_limit=std::min(max_rate,attainable_fraction*k*limit);
 float lo=std::max(-limit,(-rate_limit-future_rate)/uw),hi=std::min(limit,(rate_limit-future_rate)/uw);
 command=lo<=hi?std::clamp(command,lo,hi):-std::copysign(limit,w);
 result.valid=std::isfinite(command);result.command=std::clamp(command,-limit,limit);
 result.horizon=horizon;result.rate_limit=rate_limit;return result;
}
struct Controller {
 float weight=0,delta=0;Plan last{};
 float actual=0;bool initialized=false;
 void reset(){*this=Controller{};}
 float fallback(float baseline,float limit,float slew,float dt){
  weight=std::max(0.f,weight-1.5f*dt);delta+=std::clamp(-delta,-slew*dt,slew*dt);
  float result=std::clamp(baseline+delta,-limit,limit);delta=result-baseline;last={};return result;
 }
 void follow_manual(float input,float baseline,float limit){actual=std::clamp(input,-limit,limit);initialized=true;weight=0;delta=actual-baseline;last={};}
 float fallback_full(float baseline,float limit,float slew,float dt){
  if(!std::isfinite(baseline)||!std::isfinite(dt)||dt<=0||dt>.1){reset();return std::isfinite(baseline)?std::clamp(baseline,-limit,limit):0;}
  if(!initialized){actual=std::clamp(baseline,-limit,limit);initialized=true;}
  weight=std::max(0.f,weight-2.f*dt);actual+=std::clamp(baseline-actual,-slew*dt,slew*dt);
  actual=std::clamp(actual,-limit,limit);delta=actual-baseline;last={};return actual;
 }
 float apply_full(const Plan& plan,float baseline,float limit,float slew,float dt){
  if(!plan.valid||!std::isfinite(plan.command))return fallback_full(baseline,limit,slew,dt);
  if(!std::isfinite(baseline)||!std::isfinite(dt)||dt<=0||dt>.1){reset();return std::isfinite(baseline)?std::clamp(baseline,-limit,limit):0;}
  if(!initialized){actual=std::clamp(baseline,-limit,limit);initialized=true;}
  weight=std::min(1.f,weight+2.f*dt);
  float desired=baseline+weight*(plan.command-baseline);
  actual+=std::clamp(desired-actual,-slew*dt,slew*dt);actual=std::clamp(actual,-limit,limit);
  delta=actual-baseline;last=plan;return actual;
 }
 float apply(const Plan& plan,float baseline,float learned,float confidence,float speed,float limit,float slew,float dt){
  if(!std::isfinite(baseline)||!std::isfinite(learned)||!std::isfinite(confidence)||!std::isfinite(speed)||!std::isfinite(dt)||dt<=0||dt>.1){reset();return std::isfinite(baseline)?baseline:0.f;}
  if(!plan.valid)return fallback(baseline,limit,slew,dt);
  float trust=online_learning::smooth((confidence-.25f)/.5f);
  float wanted=.85f*std::clamp(learned/.9f,0.f,1.f)*trust*online_learning::speed_weight(speed);
  weight+=std::clamp(wanted-weight,-1.5f*dt,.25f*dt);weight=std::clamp(weight,0.f,.85f);
  float desired=weight*(plan.command-baseline);
  delta+=std::clamp(desired-delta,-slew*dt,slew*dt);
  float command=std::clamp(baseline+delta,-limit,limit);delta=command-baseline;last=plan;return command;
 }
};
}
