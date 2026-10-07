#pragma once
#include "adaptive_braking.h"
namespace capture_rate {
// Pose differences measure the preceding frame's mean angular rate. A bounded
// causal half-frame extrapolation removes that latency without a 33 ms LPF.
// Identification and prediction must consume this same endpoint estimate.
struct Endpoint {
 float previous=0,older=0,previous_dt=0,value=0,noise=0;bool seen=false,twice=false,qualified=false;
 void reset(){*this={};}
 float step(float raw,float dt,shared_braking::Model model,float speed){
  if(!std::isfinite(raw)||!std::isfinite(dt)||dt<=0||dt>.1f){reset();return std::isfinite(raw)?raw:0;}
  value=raw;qualified=dt>=.003f&&dt<=.075f&&std::abs(raw)<=600;
  if(seen){
   float span=.5f*(dt+previous_dt);
   float physical=2.f*(std::abs(shared_braking::gain(model,speed))+std::abs(raw)+10.f)/std::max(.2f,model.tau);
   float observed=(raw-previous)/span;
   qualified=qualified&&std::abs(observed)<=physical*1.05f;
   float slope=std::clamp(observed,-physical,physical);
   if(twice){float curvature=std::abs(raw-2*previous+older)*.35f;
    noise+=(std::min(curvature,20.f)-noise)*(1-std::exp(-dt/.15f));}
   value+=.5f*dt*slope;
  }
  older=previous;twice=seen;previous=raw;previous_dt=dt;seen=true;return value;
 }
};
}
