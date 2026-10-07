#pragma once
#include "capture_adaptation.h"
namespace vector_local_response {
// Compare predictions frozen at the beginning of each causal window with its
// later measured endpoint. Mismatch reduces terminal prediction authority;
// it does not rewrite aircraft performance or enlarge the stopping envelope.
inline bool valid(shared_braking::Model m,float positive,float negative,int axis,float rate,float speed,float dt){
 return axis>=0&&axis<3&&std::isfinite(rate)&&std::isfinite(speed)&&speed>0&&std::isfinite(dt)&&dt>0&&dt<=.1f&&
  std::isfinite(m.tau)&&m.tau>=.2f&&m.tau<=3&&std::isfinite(m.delay)&&m.delay>=0&&m.delay<=.5f&&
  std::isfinite(m.bias)&&std::isfinite(positive)&&positive>=1&&std::isfinite(negative)&&negative>=1;
}
struct PredictorCheck {
 uint64_t anchor=0,last=0;float initial=0,positive0=0,negative0=0,speed0=0,mse=0;shared_braking::Model seed{};unsigned checks=0;
 float weight=1;
 float step(shared_braking::Model m,float positive,float negative,int axis,uint64_t now,float rate,float speed,const adaptive_braking::History& h,bool allowed,float dt){
  if(!valid(m,positive,negative,axis,rate,speed,dt)){*this={};return 0;}
  if(!allowed){anchor=0;return weight;}
  if(last&&(now<=last||now-last>100))*this={};last=now;
  if(anchor&&now-anchor>=120){auto path=now-anchor<=220&&std::abs(speed-speed0)<25?capture_adaptation::path(h,anchor,now,seed.delay,axis):capture_adaptation::Path{};
   if(path.valid){double predicted=initial;for(size_t j=0;j<path.size;++j){auto p=path.segments[j];double a=std::exp(-p.dt/seed.tau);predicted=a*predicted+(1-a)*((p.u>=0?positive0:negative0)*p.u+seed.bias);}
    float error=rate-float(predicted);mse+=.3f*(error*error-mse);++checks;
   }anchor=0;
  }
  if(!anchor){anchor=now;initial=rate;seed=m;positive0=positive;negative0=negative;speed0=speed;}
  float tolerance=.12f+.08f*std::abs(rate),target=checks>=2?tolerance*tolerance/(tolerance*tolerance+mse):1.f;
  weight+=std::clamp(target-weight,-6*dt,1*dt);return weight;
 }
};
// A bounded short-lived equivalent disturbance, local to quiet input. It is
// filtered/rate-limited and never committed to the aircraft model.
struct Observer {
 uint64_t anchor=0,last=0;float initial=0,speed0=0,positive0=0,negative0=0,estimate=0,weight=0;
 shared_braking::Model seed{};bool initialized=false;unsigned accepted=0,rejected=0;
 void reset(){*this={};}
 float step(shared_braking::Model m,float positive,float negative,int axis,uint64_t now,float rate,float speed,
   const adaptive_braking::History& history,bool eligible,float dt){
  if(!valid(m,positive,negative,axis,rate,speed,dt)){reset();return m.bias;}
  if(!eligible){anchor=0;weight=std::max(0.f,weight-3*dt);return m.bias;}
  if(last&&(now<=last||now-last>100)){reset();}last=now;
  if(!initialized){estimate=m.bias;initialized=true;}
  if(anchor&&now-anchor>=120){
   auto path=now-anchor<=220&&std::abs(speed-speed0)<25?capture_adaptation::path(history,anchor,now,seed.delay,axis):capture_adaptation::Path{};
   if(path.valid){double q=initial,a=1;for(size_t j=0;j<path.size;++j){auto part=path.segments[j];double decay=std::exp(-part.dt/seed.tau);q=decay*q+(1-decay)*(part.u>=0?positive0:negative0)*part.u;a*=decay;}
    float observed=float((rate-q)/std::max(.015,1-a));float bound=axis==0?10.f:3.f;
    if(std::isfinite(observed)&&std::abs(observed)<bound){float elapsed=float(now-anchor)/1000;
     float alpha=1-std::exp(-elapsed/.24f);estimate+=std::clamp(alpha*(observed-estimate),-6.f*elapsed,6.f*elapsed);
     weight=std::min(1.f,weight+elapsed*3);++accepted;
    }else {++rejected;weight=std::max(0.f,weight-.3f);}
   }else {++rejected;weight=std::max(0.f,weight-.3f);}anchor=0;
  }
  if(!anchor){anchor=now;initial=rate;speed0=speed;positive0=positive;negative0=negative;seed=m;}
  return m.bias+weight*(estimate-m.bias);
 }
};
}
