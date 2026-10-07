#pragma once
#include "capture_adaptation.h"
// Short-lived terminal response correction, using the SAME signed actuator
// gains as the issued-command predictor. It does not integrate pointing error
// or rewrite the aircraft's long-term model. A later disjoint window must
// validate each candidate before it can influence control.
namespace vector_local_bias {
struct Observer {
 uint64_t anchor=0,fresh=0;float initial=0,speed0=0,positive0=0,negative0=0;
 shared_braking::Model seed{};float proposal=0,total=0,weight=0;
 bool pending=false,confirmed=false;unsigned accepted=0,rejected=0,status=0;
 // status: 1 disabled, 2 collecting, 3 context/history, 4 residual bound,
 // 5 explained by model uncertainty, 6 validation rejected, 7 accepted.
 float uncertainty=0,serving_speed=0,serving_tau=1,serving_delay=0;
 float gain_bound0=0,tau_bound0=0,delay_bound0=0;
 void reset(){*this={};}
 float step(shared_braking::Model model,float positive,float negative,int axis,
            uint64_t now,float rate,float speed,const adaptive_braking::History& history,
            bool eligible,float dt,float gain_bound=0,float tau_bound=0,float delay_bound=0){
  if(!std::isfinite(dt)||dt<=0||dt>.1f||!std::isfinite(rate)||!std::isfinite(speed)||
     !std::isfinite(positive)||!std::isfinite(negative)||positive<1||negative<1||
     !std::isfinite(model.bias)||!std::isfinite(model.tau)||model.tau<.2f||model.tau>3.f||
     !std::isfinite(model.delay)||model.delay<0||model.delay>.4f||axis<0||axis>2||
     !std::isfinite(gain_bound)||!std::isfinite(tau_bound)||!std::isfinite(delay_bound)){reset();return model.bias;}
  float bound=axis==0?10.f:3.f;
  if(!eligible){status=1;anchor=0;pending=false;weight=std::max(0.f,weight-4*dt);return model.bias;}
  if(status<2)status=2;
  if(anchor&&now<=anchor){reset();}
  if(anchor&&now-anchor>=180){
   bool compatible=now-anchor<=260&&std::abs(speed-speed0)<25&&
     std::abs(model.tau/seed.tau-1)<.20f&&std::abs(model.delay-seed.delay)<.04f&&
     std::abs(positive/positive0-1)<.20f&&std::abs(negative/negative0-1)<.20f;
   auto path=compatible?capture_adaptation::path(history,anchor,now,seed.delay,axis):capture_adaptation::Path{};
   if(path.valid){
    float predicted=initial,decay=1;
    for(size_t j=0;j<path.size;++j){auto s=path.segments[j];float a=std::exp(-s.dt/seed.tau);
     float k=s.u>=0?positive0:negative0;predicted=a*predicted+(1-a)*k*s.u;decay*=a;}
    float one=1-decay,observed=(rate-predicted)/std::max(.01f,one);
    if(std::isfinite(observed)&&std::abs(observed)<=bound&&one>.03f){
     float base_error=std::abs(rate-predicted-seed.bias*one);
     float candidate_error=std::abs(rate-predicted-proposal*one);
     // Do not mistake an allowed gain/tau/delay error for a constant offset.
     // In particular, a weak actuator's braking residual must not become a
     // falsely strong bias correction on the next opposite-sign command.
     float lower=predicted+seed.bias*one,upper=lower;bool covered=true;
     for(int kg:{-1,1})for(int kt:{-1,1})for(int kd:{-1,1}){
      float delay=std::clamp(seed.delay+kd*delay_bound0,0.f,.5f),tau=std::clamp(seed.tau*(1+kt*tau_bound0),.2f,3.f);
      uint64_t lag=uint64_t(std::lround(delay*1000));
      auto uncertain=anchor>=lag?capture_adaptation::path(history,anchor-lag,now-lag,0,axis):capture_adaptation::Path{};
      if(!uncertain.valid){covered=false;continue;}float q=initial;
      for(size_t j=0;j<uncertain.size;++j){auto s=uncertain.segments[j];float a=std::exp(-s.dt/tau);
       float k=(s.u>=0?positive0:negative0)*(1+kg*gain_bound0);q=a*q+(1-a)*(k*s.u+seed.bias);}
      lower=std::min(lower,q);upper=std::max(upper,q);
     }
     bool unexplained=covered&&(rate<lower-.03f||rate>upper+.03f);
     if(pending&&unexplained&&base_error>.06f&&std::abs(proposal-seed.bias)>.35f&&candidate_error<=.85f*base_error+.025f&&
        std::abs(observed-proposal)<=std::max(axis==0?3.5f:1.f,.25f*std::abs(proposal))){
      total=.5f*(proposal+observed);uncertainty=.25f*std::abs(observed-proposal);
      serving_speed=speed;serving_tau=model.tau;serving_delay=model.delay;confirmed=true;fresh=now;++accepted;status=7;
     }else if(pending){status=unexplained?6:5;++rejected;if(candidate_error>base_error+.05f)confirmed=false;}
     proposal=observed;pending=true;
    }else {status=4;pending=false;}
   }else {status=3;pending=false;}
   anchor=0;
  }
  if(!anchor){anchor=now;initial=rate;speed0=speed;seed=model;positive0=positive;negative0=negative;
   gain_bound0=std::clamp(gain_bound,0.f,.65f);tau_bound0=std::clamp(tau_bound,0.f,.65f);delay_bound0=std::clamp(delay_bound,0.f,.15f);}
  bool usable=confirmed&&now>=fresh&&now-fresh<=900&&std::abs(speed-serving_speed)<25&&
   std::abs(model.tau/serving_tau-1)<.20f&&std::abs(model.delay-serving_delay)<.04f;
  float target=usable?1.f:0.f;weight+=std::clamp(target-weight,-4*dt,4*dt);
  return model.bias+weight*(total-model.bias);
 }
};
}
