#pragma once
#include "vector_adaptation.h"
// Independent signed pitch capability estimates for maneuver selection. Pure
// delayed-input windows identify each sign; later windows validate it. These
// estimates never turn a negative input used for braking into a forbidden input.
namespace vector_pitch_authority {
struct Side {
 float gain=0,proposal=0,xx=0,xy=0,candidate_error=0,base_error=0,mix=0;
 unsigned fit=0,validation=0,accepted=0,rejected=0;uint64_t epoch=0;
 shared_braking::Model seed{};bool validating=false,valid=false;
 void clear(){xx=xy=0;fit=validation=0;candidate_error=base_error=0;validating=false;epoch=0;}
 void observe(shared_braking::Model model,float initial,float actual,uint64_t start,uint64_t end,const adaptive_braking::History& h,int sign){
  if(fit&&std::abs(std::log(model.tau/seed.tau))>std::log(1.25f)){clear();}
  if(epoch&&end-epoch>45000)clear();
  if(!fit&&!validating){seed=model;epoch=start;}
  auto path=capture_adaptation::path(h,start,end,seed.delay,0);if(!path.valid)return;
  for(size_t i=0;i<path.size;++i)if(sign*path.segments[i].u<.12f)return;
  auto b=capture_adaptation::basis(path,seed.tau);float target=actual-b.decay*initial-seed.bias*(1-b.decay);
  if(validating){if(start<epoch)return;float reference=valid?gain:model.g0;
   candidate_error+=std::pow(target-proposal*b.response,2);base_error+=std::pow(target-reference*b.response,2);++validation;
   if(validation>=8){if(candidate_error<.92f*base_error&&candidate_error/validation<16){gain=proposal;valid=true;++accepted;}else ++rejected;clear();}
   return;
  }
  xx+=b.response*b.response;xy+=b.response*target;++fit;
  if(fit>=12&&xx>.005f){proposal=xy/xx;
   if(std::isfinite(proposal)&&proposal>5&&proposal<200){validating=true;epoch=end;candidate_error=base_error=0;validation=0;}else clear();}
 }
 float current(float fallback,float dt){float wanted=valid?.95f:0;mix+=std::clamp(wanted-mix,-dt,dt*.5f);return fallback+mix*((valid?gain:fallback)-fallback);}
};
struct Bank {Side positive,negative;};
struct Axis {
 std::array<Bank,4> banks{};uint64_t anchor=0;float initial=0,speed0=0;int bank0=0;
 float positive=50,negative=18;unsigned positive_samples=0,negative_samples=0;
 void suspend(){anchor=0;for(auto& b:banks){b.positive.clear();b.negative.clear();b.positive.mix=b.negative.mix=0;}}
 void observe(shared_braking::Model model,uint64_t now,float rate,float speed,const adaptive_braking::History& h,bool allowed,float dt){
  int index=capture_adaptation::band(speed);
  if(!std::isfinite(speed)||!std::isfinite(dt)||dt<=0||dt>.1f||!capture_adaptation::sane(model,speed))return;
  // Suspending identification must not freeze the serving capability at an
  // earlier airspeed. Clear unfinished fits, but keep evaluating validated
  // banks and the speed-dependent fallback on every usable control update.
  if(!allowed){anchor=0;for(auto& b:banks){b.positive.clear();b.negative.clear();}}
  if(allowed&&anchor&&now>anchor&&now-anchor>=240){
   if(now-anchor<=340&&bank0==index&&std::abs(speed-speed0)<25){banks[index].positive.observe(model,initial,rate,anchor,now,h,1);banks[index].negative.observe(model,initial,rate,anchor,now,h,-1);}
   anchor=0;
  }
  if(allowed&&!anchor){anchor=now;initial=rate;speed0=speed;bank0=index;}
  float baseline=shared_braking::gain(shared_braking::pitch,speed);
  auto q=capture_adaptation::bracket(speed);float p0=banks[q.lo].positive.current(std::max(model.g0,baseline),dt),p1=banks[q.hi].positive.current(std::max(model.g0,baseline),dt);
  float n0=banks[q.lo].negative.current(.35f*baseline,dt),n1=banks[q.hi].negative.current(.35f*baseline,dt);
  positive=std::max(5.f,p0+q.t*(p1-p0));negative=std::clamp(n0+q.t*(n1-n0),3.f,positive);
  positive_samples=banks[index].positive.fit;negative_samples=banks[index].negative.fit;
 }
};
}
