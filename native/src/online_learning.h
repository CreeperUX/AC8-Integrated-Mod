#pragma once
#include "adaptive_braking.h"
#include <limits>
// Small bounded parameter bank; no allocation, file I/O or game object access.
// Fit and validation use disjoint chronological blocks. Bias stays fixed.
namespace online_learning {
constexpr std::array<float,9> gains{.5f,.65f,.8f,.9f,1.f,1.15f,1.35f,1.65f,2.f};
constexpr std::array<float,9> taus{.35f,.5f,.7f,.85f,1.f,1.25f,1.6f,2.f,2.5f};
constexpr std::array<float,5> delay_offsets{-.09f,-.03f,0.f,.06f,.12f};
constexpr size_t shape_count=gains.size()*taus.size(),candidates=shape_count*delay_offsets.size(),baseline_index=2*shape_count+4*gains.size()+4;
constexpr std::array<float,4> centers{180.f,360.f,600.f,820.f};
inline int nearest_band(float speed){return speed<270?0:speed<480?1:speed<710?2:3;}
inline float speed_weight(float speed){return adaptive_braking::smooth((speed-90)/30)*(1-adaptive_braking::smooth((speed-900)/100));}
inline float smooth(float x){return adaptive_braking::smooth(x);}
inline bool continuous(const adaptive_braking::History& h,uint64_t from,uint64_t until){
 if(!h.covers(from)||until<from)return false;
 uint64_t previous=0;bool found=false;
 for(size_t i=0;i<h.size;++i){auto t=h.values[(h.begin+i)%h.values.size()].tick;
  if(t<=from){previous=t;found=true;continue;}
  if(!found||t-previous>100)return false;
  previous=t;if(t>=until)return true;
 }
 return found&&until>=previous&&until-previous<=100;
}
struct Statistics {
 unsigned n=0;double su=0,su2=0,sw=0,sw2=0,suw=0;
 void add(float u,float w){++n;su+=u;su2+=u*u;sw+=w;sw2+=w*w;suw+=u*w;}
 bool excited()const{
  if(n<48)return false;
  double vu=su2/n-std::pow(su/n,2),vw=sw2/n-std::pow(sw/n,2),cov=suw/n-su*sw/(double(n)*n);
  return vu>.01 && vw>.01 && cov*cov<.98*vu*vw;
 }
};
struct Bank {
 std::array<double,candidates> scores{};
 Statistics stats;
 bool validating=false,accepted=false;
 size_t proposal=baseline_index,selected=baseline_index;
 unsigned validation_count=0,promotions=0,rejections=0,fresh=0;
 double candidate_error=0,baseline_error=0,incumbent_error=0;
 float fresh_base=0,fresh_model=0,blend=0;
 void clear_fit(){scores.fill(0);stats={};validating=false;validation_count=0;candidate_error=baseline_error=incumbent_error=0;}
 void suspend(){fresh=0;fresh_base=fresh_model=0;blend=0;}
 void observe(const std::array<float,candidates>& predicted,float actual,float input,float normalized_rate,float tolerance){
  for(float p:predicted)if(!std::isfinite(p))return;
  if(!std::isfinite(actual)||!std::isfinite(input)||!std::isfinite(normalized_rate))return;
  float eb=std::pow(predicted[baseline_index]-actual,2),em=std::pow(predicted[selected]-actual,2);
  if(accepted){
   if(!fresh){fresh_base=eb;fresh_model=em;}else{fresh_base+=.1f*(eb-fresh_base);fresh_model+=.1f*(em-fresh_model);}
   fresh=std::min(1000u,fresh+1);
  }
  if(validating){
   candidate_error+=std::pow(predicted[proposal]-actual,2);baseline_error+=eb;incumbent_error+=em;
   if(++validation_count>=24){
    if(baseline_error/24>.01 && candidate_error<.85*baseline_error && candidate_error/24<tolerance*tolerance &&
       (!accepted||candidate_error<.98*incumbent_error)){
     accepted=true;selected=proposal;++promotions;suspend();
    }else ++rejections;
    clear_fit();
   }
   return;
  }
  stats.add(input,normalized_rate);
  for(size_t i=0;i<candidates;++i)scores[i]+=std::pow(predicted[i]-actual,2);
  if(stats.n>=48){
   size_t best=size_t(std::min_element(scores.begin(),scores.end())-scores.begin());
   if(stats.excited()&&best!=baseline_index&&scores[baseline_index]/stats.n>.01&&scores[best]<.85*scores[baseline_index]){
    proposal=best;validating=true;validation_count=0;candidate_error=baseline_error=incumbent_error=0;
   }else clear_fit();
  }
 }
 void step(float dt){
  float desired=accepted&&fresh>=12&&fresh_base>.01f&&fresh_model<.9f*fresh_base?.9f:0.f;
  blend+=std::clamp(desired-blend,-1.5f*dt,.1f*dt);
  blend=std::clamp(blend,0.f,.9f);
 }
 float gain()const{return 1+blend*(gains[selected%gains.size()]-1);}
 float tau()const{return std::exp(blend*std::log(taus[(selected/gains.size())%taus.size()]));}
 float delay_offset()const{return blend*delay_offsets[selected/shape_count];}
};
struct Axis {
 std::array<Bank,4> banks{};
 uint64_t anchor_tick=0;float anchor_rate=0,anchor_speed=0;
 float confidence=.35f,delta=0,gain_scale=1,tau_scale=1,delay_offset=0,learned_weight=0;
 int anchor_band=-1;
 void suspend(){anchor_tick=0;anchor_band=-1;confidence=.35f;delta=0;gain_scale=tau_scale=1;delay_offset=0;learned_weight=0;for(auto& b:banks)b.suspend();}
 unsigned promotions()const{unsigned n=0;for(auto& b:banks)n+=b.promotions;return n;}
 void observe(shared_braking::Model model,int axis,uint64_t now,float rate,float speed,const adaptive_braking::History& history){
  if(!std::isfinite(rate)||!std::isfinite(speed)||std::abs(rate)>600){suspend();return;}
  int band=nearest_band(speed);
  if(!anchor_tick){anchor_tick=now;anchor_rate=rate;anchor_speed=speed;anchor_band=band;return;}
  float span=float(now-anchor_tick)/1000;
  if(span<.2f)return;
  uint64_t longest=uint64_t(std::clamp(model.delay+.12f,0.f,.4f)*1000),shortest=uint64_t(std::clamp(model.delay-.09f,0.f,.4f)*1000);
  bool usable=span<=.3f&&band==anchor_band&&std::abs(speed-anchor_speed)<25&&now>=anchor_tick&&anchor_tick>=longest&&continuous(history,anchor_tick-longest,now-shortest);
  if(usable){
   std::array<float,20> inputs{};float mean=0;
   const float g=shared_braking::gain(model,std::clamp(anchor_speed,130.f,660.f));
   std::array<float,candidates> predictions{};
   for(size_t delay_index=0;delay_index<delay_offsets.size();++delay_index){
    uint64_t delay=uint64_t(std::clamp(model.delay+delay_offsets[delay_index],0.f,.4f)*1000);
    for(int i=0;i<20;++i){inputs[i]=history.at_axis(anchor_tick+uint64_t((i+.5f)*span*50)-delay,axis);if(delay_index==2)mean+=inputs[i]/20;}
    for(size_t t=0;t<taus.size();++t){
     float d=std::exp(-span/(20*model.tau*taus[t])),response=0;
     for(float u:inputs)response=d*response+(1-d)*u;
     float decay=std::exp(-span/(model.tau*taus[t]));
     for(size_t k=0;k<gains.size();++k)predictions[delay_index*shape_count+t*gains.size()+k]=decay*anchor_rate+g*gains[k]*response+model.bias*(1-decay);
    }
   }
   // Check effective (currently blended) model against a new observed endpoint.
   uint64_t effective_delay=uint64_t(std::clamp(model.delay+delay_offset,0.f,.4f)*1000);
   for(int i=0;i<20;++i)inputs[i]=history.at_axis(anchor_tick+uint64_t((i+.5f)*span*50)-effective_delay,axis);
   float d=std::exp(-span/(20*model.tau*tau_scale)),response=0;
   for(float u:inputs)response=d*response+(1-d)*u;
   float decay=std::exp(-span/(model.tau*tau_scale));
   float forecast=decay*anchor_rate+g*gain_scale*response+model.bias*(1-decay);
   float target=std::clamp(1-std::abs(rate-forecast)/std::max(axis==1?15.f:axis==2?1.f:5.f,.15f*g),0.f,1.f);
   confidence+=(target-confidence)*(1-std::exp(-span/(target<confidence?.4f:2.f)));
   if(speed>=110&&speed<=900)banks[band].observe(predictions,rate,mean,anchor_rate/(axis==1?100.f:axis==2?6.f:40.f),axis==1?15.f:axis==2?1.5f:5.f);
  }else confidence=std::min(confidence,.35f);
  anchor_tick=now;anchor_rate=rate;anchor_speed=speed;anchor_band=band;
 }
 shared_braking::Model effective(shared_braking::Model model,float speed,float dt){
  if(!std::isfinite(speed)||!std::isfinite(dt)||dt<=0||dt>.1)return model;
  int closest=nearest_band(speed);
  for(int i=0;i<4;++i){if(i==closest)banks[i].step(dt);else banks[i].blend=std::max(0.f,banks[i].blend-dt*.15f);}
  int lo=speed<=360?0:speed<=600?1:2,hi=lo+1;
  float t=std::clamp((speed-centers[lo])/(centers[hi]-centers[lo]),0.f,1.f);
  auto mix=[&](float a,float b){return a+(b-a)*t;};
  float gs=mix(banks[lo].gain(),banks[hi].gain()),ts=mix(banks[lo].tau(),banks[hi].tau()),ds=mix(banks[lo].delay_offset(),banks[hi].delay_offset());
  gain_scale+=std::clamp(gs-gain_scale,-.1f*dt,.1f*dt);tau_scale+=std::clamp(ts-tau_scale,-.1f*dt,.1f*dt);
  delay_offset+=std::clamp(ds-delay_offset,-.025f*dt,.025f*dt);
  learned_weight=mix(banks[lo].blend,banks[hi].blend);
  model.g0*=gain_scale;model.g1*=gain_scale;model.tau*=tau_scale;model.delay=std::clamp(model.delay+delay_offset,0.f,.4f);return model;
 }
 float correction(shared_braking::Model model,float error,float rate,float previous,float baseline,float speed,float horizon,float range,float zone,float strength,float cap,float slew,float dt){
  if(!std::isfinite(error)||!std::isfinite(rate)||!std::isfinite(previous)||!std::isfinite(baseline)||!std::isfinite(speed)||!std::isfinite(strength)||!std::isfinite(dt)||dt<=0||dt>.1){delta=0;return 0;}
  float max=cap*std::clamp(strength/.2f,0.f,1.75f),wanted=0;
  if(std::abs(error)<range&&std::abs(rate)>=1){
   float sign=std::copysign(1.f,rate);
   float predicted=sign*shared_braking::travel(model,rate,previous,baseline,std::clamp(speed,130.f,660.f),horizon);
   float remaining=std::max(0.f,sign*error-zone);
   float fraction=std::clamp(std::max(0.f,predicted-remaining)/std::max(1.f,std::abs(predicted)+remaining),0.f,1.f);
   float old_weight=adaptive_braking::speed_weight(speed);
   float coverage=old_weight+(speed_weight(speed)-old_weight)*std::clamp(learned_weight/.9f,0.f,1.f);
   wanted=-sign*max*(1-std::pow(std::abs(error)/range,2.f))*fraction*coverage*confidence;
  }
  if(delta*rate>0||std::abs(rate)<1)delta=0;
  delta+=std::clamp(wanted-delta,-slew*dt,slew*dt);delta=std::clamp(delta,-max,max);return delta;
 }
};
struct Aircraft {int id=-1;uint64_t touched=0;Axis pitch,roll,yaw;void suspend(){pitch.suspend();roll.suspend();yaw.suspend();}};
struct Fleet {
 std::array<Aircraft,64> planes{};
 Aircraft& get(int id,uint64_t now){
  for(auto& p:planes)if(p.id==id){p.touched=now;return p;}
  auto p=std::min_element(planes.begin(),planes.end(),[](const auto& a,const auto& b){return a.touched<b.touched;});
  *p=Aircraft{};p->id=id;p->touched=now;return *p;
 }
};
}
