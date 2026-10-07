#pragma once
#include "capture_adaptation.h"

// VECTOR-only model bank. Geometry/reference transport belongs to the outer
// controller, not these physical body-rate response parameters. No future
// other-axis rates are used to make a candidate's prediction look better.
namespace vector_adaptation {
using capture_adaptation::Parameters;
using capture_adaptation::Uncertainty;
using capture_adaptation::centers;
using capture_adaptation::band;
using capture_adaptation::bracket;
using capture_adaptation::frozen;
using capture_adaptation::prediction;
using capture_adaptation::path;
using capture_adaptation::sane;
struct SharedCandidate {shared_braking::Model model{};float trust=0;bool valid=false;};
struct Diagnostics {
 unsigned fit_count=0,validation_count=0,long_count=0,status=0,source=0;
 unsigned proposals=0,accepted=0,rejected=0,expired=0,ambiguous_skips=0,innovation_skips=0;
 uint64_t attempt_age_ms=0;
 float fast_bias_mix=0,fast_total_bias=0,model_mix=0,trust=0;
 unsigned fast_bias_fit=0,fast_bias_validation=0,fast_bias_accepted=0,fast_bias_rejected=0;
 unsigned candidate_source=0;
};
// Simultaneous controls are allowed. The covariance check rejects an actuator
// gain that cannot be distinguished from an almost exact linear combination
// of the other controls; it imposes no fixed other-input magnitude gate.
struct Excitation {
 unsigned n=0;std::array<double,3> sum{};std::array<std::array<double,3>,3> product{};
 void add(const std::array<float,3>& u){++n;for(int i=0;i<3;++i){sum[i]+=u[i];for(int j=0;j<3;++j)product[i][j]+=u[i]*u[j];}}
 bool distinguishable(int axis)const{
  if(n<20)return false;std::array<std::array<double,3>,3> c{};
  for(int i=0;i<3;++i)for(int j=0;j<3;++j)c[i][j]=product[i][j]/n-sum[i]*sum[j]/(double(n)*n);
  int j=(axis+1)%3,k=(axis+2)%3;double variance=c[axis][axis];if(variance<.006)return false;
  const double det=c[j][j]*c[k][k]-c[j][k]*c[j][k];double explained=0;
  if(det>1e-8)explained=(c[axis][j]*c[axis][j]*c[k][k]+c[axis][k]*c[axis][k]*c[j][j]-2*c[axis][j]*c[axis][k]*c[j][k])/det;
  else for(int other:{j,k})if(c[other][other]>.001)explained=std::max(explained,c[axis][other]*c[axis][other]/c[other][other]);
  return variance-explained>std::max(.0015,.025*variance);
 }
};
struct Bank : capture_adaptation::Bank {
 Excitation excitation{};unsigned proposals=0,ambiguous_skips=0,innovation_skips=0,source=0,deployed_source=0;
 Parameters last_external{};bool external_seen=false;uint64_t last_offer=0;
 void clear_attempt(){clear_fit();excitation={};}
 void suspend(){capture_adaptation::Bank::suspend();excitation={};external_seen=false;last_offer=0;}
 void step(float dt,float tolerance){
  bool reliable=accepted&&fresh>=3&&selected_mse<tolerance*tolerance&&selected_mse<=1.10f*baseline_mse+.0025f;
  float desired=reliable?.98f:0;blend+=std::clamp(desired-blend,-1.2f*dt,.5f*dt);blend=std::clamp(blend,0.f,.98f);
 }
 void offer(shared_braking::Model prior,const SharedCandidate& candidate,int index,uint64_t now,float speed){
  if(!candidate.valid||candidate.trust<.25f||!std::isfinite(candidate.trust)||validating||fit_count>=8||!sane(candidate.model,speed)||!sane(prior,speed))return;
  auto p=capture_adaptation::parameters(candidate.model,centers[index]);auto ref=capture_adaptation::parameters(prior,centers[index]);
  if(p.k<.35f*ref.k||p.k>2.5f*ref.k||std::abs(p.bias)>std::min(40.f,.22f*ref.k+4.f))return;
  if(external_seen&&std::abs(p.k/last_external.k-1)<.05f&&std::abs(p.tau/last_external.tau-1)<.08f&&
     std::abs(p.delay-last_external.delay)<.015f&&std::abs(p.bias-last_external.bias)<.15f)return;
  if(last_offer&&now-last_offer<5000)return;
  clear_attempt();seed=ref;proposal=p;validating=true;fit_epoch=validation_epoch=now;
  source=2;++proposals;external_seen=true;last_external=p;last_offer=now;
 }
 void collect(shared_braking::Model prior,int axis,uint64_t start,uint64_t end,float initial,float actual,float speed,
  const adaptive_braking::History& history,float tolerance,int index,bool long_endpoint){
  float base=prediction(frozen(prior,speed),axis,start,end,initial,history);
  if(!std::isfinite(base))return;
  // A very large unexplained endpoint is a model/measurement anomaly, not
  // evidence for an extreme actuator gain. Monitoring still sees the sample.
  if(std::abs(actual-base)>std::max(6*tolerance,.30f*shared_braking::gain(prior,speed)+2.f)){
   ++innovation_skips;return;
  }
  const bool was_validating=validating;const unsigned old_fit=fit_count,old_promotions=promotions;
  std::array<float,3> means{};
  if(!was_validating&&!long_endpoint){
   if(!fit_count)excitation={};
   for(int j=0;j<3;++j){auto p=capture_adaptation::path(history,start,end,prior.delay,j);if(!p.valid)return;means[j]=p.mean;}
  }
  capture_adaptation::Bank::collect(prior,axis,start,end,initial,actual,speed,history,tolerance,index,long_endpoint);
  if(promotions!=old_promotions)deployed_source=source;
  if(!was_validating&&!long_endpoint&&(fit_count>old_fit||validating))excitation.add(means);
  if(validating&&!was_validating){
   if(!excitation.distinguishable(axis)){++ambiguous_skips;clear_attempt();}
   else {source=1;++proposals;}
  }
  if(!validating&&!fit_count)excitation={};
 }
};
struct Context {std::array<float,3> rates{},scales{50,180,8};float bank=NAN;};
inline bool compatible(const Context& a,const Context& b,int axis,float scale=1.f){
 for(int j=0;j<3;++j)if(j!=axis&&std::abs(a.rates[j]-b.rates[j])>scale*std::max(j==1?4.f:.3f,.08f*b.scales[j]))return false;
 if(std::isfinite(a.bank)&&std::isfinite(b.bank)&&std::abs(std::remainder(a.bank-b.bank,360.f))>10.f*scale)return false;
 return true;
}
struct FastBias {
 float total=0,proposal=0,weight=0,sum=0,min_value=INFINITY,max_value=-INFINITY;
 unsigned fit=0,validation=0,long_validation=0,accepted=0,rejected=0;bool validating=false,confirmed=false;
 double proposal_error=0,base_error=0,long_proposal_error=0,long_base_error=0;
 uint64_t last_good=0,proposal_tick=0;float context_speed=0,serving_speed=0;Context context{},serving_context{};
 void reset(){*this={};}
 void clear_attempt(){fit=validation=long_validation=0;sum=0;min_value=INFINITY;max_value=-INFINITY;validating=false;proposal_error=base_error=long_proposal_error=long_base_error=0;proposal_tick=0;}
 bool compatible_now(float speed,const Context& c,int axis)const{
  return std::abs(speed-context_speed)<std::max(10.f,.025f*context_speed)&&compatible(c,context,axis);
 }
 void observe(shared_braking::Model model,int axis,uint64_t from,uint64_t until,float initial,float actual,float speed,
  const adaptive_braking::History& h,const Context& before,const Context& after,bool allowed,bool longer,float tolerance){
  auto zero=model;zero.bias=0;float base=prediction(model,axis,from,until,initial,h),without=prediction(zero,axis,from,until,initial,h);
  if(!std::isfinite(base)||!std::isfinite(without))return;
  if(validating){
   if(from<proposal_tick)return;
   if(!allowed||!compatible_now(speed,after,axis)){clear_attempt();return;}
   auto proposed=model;proposed.bias=proposal;float forecast=prediction(proposed,axis,from,until,initial,h);if(!std::isfinite(forecast))return;
   if(longer){++long_validation;long_proposal_error+=std::pow(actual-forecast,2);long_base_error+=std::pow(actual-base,2);}
   else{++validation;proposal_error+=std::pow(actual-forecast,2);base_error+=std::pow(actual-base,2);}
   if(validation>=4&&long_validation>=1){
    if(proposal_error<.90*base_error&&long_proposal_error<.98*long_base_error&&
       proposal_error/validation<tolerance*tolerance&&long_proposal_error/long_validation<2.25*tolerance*tolerance){
     total=proposal;confirmed=true;last_good=until;serving_speed=speed;serving_context=after;++accepted;
    }else ++rejected;
    clear_attempt();
   }
   return;
  }
  if(longer||!allowed||!compatible(before,after,axis)){clear_attempt();return;}
  auto p=capture_adaptation::path(h,from,until,model.delay,axis);if(!p.valid)return;
  float low=2,high=-2;for(size_t i=0;i<p.size;++i){low=std::min(low,p.segments[i].u);high=std::max(high,p.segments[i].u);}
  if(std::abs(p.mean)>.45f||high-low>.22f){clear_attempt();return;}
  float one=1-std::exp(-float(until-from)/1000/model.tau);
  if(one<.02f)return;float estimate=(actual-without)/one;
  float bound=std::min(8.f,.15f*model.g0+.5f);
  if(!std::isfinite(estimate)||std::abs(estimate)>bound){clear_attempt();return;}
  if(fit&&!compatible_now(speed,after,axis)){clear_attempt();}
  if(!fit){context=after;context_speed=speed;}
  sum+=estimate;min_value=std::min(min_value,estimate);max_value=std::max(max_value,estimate);++fit;
  if(fit>=3){
   if(max_value-min_value<std::max(.3f,.025f*model.g0)){
    proposal=sum/fit;validating=true;proposal_tick=until;context=after;context_speed=speed;
    validation=long_validation=0;proposal_error=base_error=long_proposal_error=long_base_error=0;
   }else clear_attempt();
  }
 }
 void step(float speed,const Context& c,int axis,uint64_t now,float dt,bool allowed){
  bool fresh=confirmed&&last_good&&now>=last_good&&now-last_good<1800&&allowed&&
   std::abs(speed-serving_speed)<std::max(10.f,.025f*serving_speed)&&compatible(c,serving_context,axis);
  float desired=fresh?.85f:0;weight+=std::clamp(desired-weight,-4.f*dt,1.5f*dt);weight=std::clamp(weight,0.f,.85f);
  if(validating&&now-proposal_tick>2500)clear_attempt();
 }
};
struct Anchor {uint64_t tick=0;float rate=0,speed=0;bool eligible=true;int index=-1;shared_braking::Model served{},base{};Context context{};};
struct Axis {
 std::array<Bank,4> banks{};std::array<Anchor,3> anchors{};FastBias bias{};Context context{};
 uint64_t previous_tick=0;float model_weight=0,effective_bias=0,model_rate_rms=0,last_k=1;int last_axis=0;
 bool eligible_now=false;SharedCandidate offered{};
 void reset(){*this={};}
 void suspend(){anchors={};previous_tick=0;model_weight=0;bias.reset();for(auto& b:banks)b.suspend();}
 unsigned promotions()const{unsigned n=0;for(const auto& b:banks)n+=b.promotions;return n;}
 unsigned rejections()const{unsigned n=0;for(const auto& b:banks)n+=b.rejections;return n;}
 static float tolerance(int axis,float k){return std::max(axis==0?1.5f:axis==1?5.f:.35f,.08f*k);}
 shared_braking::Model base_model(shared_braking::Model prior,float speed)const{
  auto q=bracket(speed);auto a=banks[q.lo].effective(prior,speed),b=banks[q.hi].effective(prior,speed);
  return {std::exp(std::log(a.tau)+q.t*(std::log(b.tau)-std::log(a.tau))),a.delay+q.t*(b.delay-a.delay),a.g0+q.t*(b.g0-a.g0),0,a.bias+q.t*(b.bias-a.bias)};
 }
 shared_braking::Model current(shared_braking::Model prior,float speed)const{
  auto model=base_model(prior,speed);model.bias+=bias.weight*(bias.total-model.bias);return model;
 }
 shared_braking::Model effective(shared_braking::Model prior,float speed,float dt){
  if(!std::isfinite(dt)||dt<=0||dt>.1f||!sane(prior,speed))return frozen(prior,speed);
  last_k=shared_braking::gain(prior,speed);for(auto& b:banks)b.step(dt,tolerance(last_axis,last_k));
  auto q=bracket(speed);model_weight=banks[q.lo].blend+q.t*(banks[q.hi].blend-banks[q.lo].blend);
  bias.step(speed,context,last_axis,previous_tick,dt,eligible_now);
  auto model=current(prior,speed);effective_bias=model.bias;model_rate_rms=uncertainty(speed,0,.5f).rate_rms;return model;
 }
 void served(uint64_t now,shared_braking::Model model){for(auto& a:anchors)if(a.tick==now&&sane(model,a.speed))a.served=frozen(model,a.speed);}
 void observe(shared_braking::Model prior,int axis,uint64_t now,float rate,float speed,const adaptive_braking::History& history,bool eligible){
  if(axis<0||axis>2||!std::isfinite(rate)||std::abs(rate)>650||!std::isfinite(speed)||speed<90||speed>1400||!sane(prior,speed)){
   anchors={};eligible_now=false;return;
  }
  if(previous_tick&&now<=previous_tick)suspend();else if(previous_tick&&now-previous_tick>100)anchors={};
  previous_tick=now;last_axis=axis;last_k=shared_braking::gain(prior,speed);eligible_now=eligible;
  for(auto& b:banks){auto expired=b.expired_fits;b.expire(now,prior);if(expired!=b.expired_fits)b.excitation={};}
  int index=band(speed);banks[index].offer(prior,offered,index,now,speed);
  constexpr std::array<uint64_t,3> spans{180,600,1000};
  for(size_t i=0;i<anchors.size();++i){auto& a=anchors[i];
   if(!a.tick){a={now,rate,speed,eligible,index,current(prior,speed),base_model(prior,speed),context};continue;}
   a.eligible=a.eligible&&eligible;if(now-a.tick<spans[i])continue;
   bool covered=now-a.tick<=spans[i]+100&&a.index==index&&std::abs(speed-a.speed)<std::max(25.f,.035f*speed);
   if(covered){auto& b=banks[index];float expected=prediction(a.served,axis,a.tick,now,a.rate,history);
    float baseline=prediction(frozen(prior,a.speed),axis,a.tick,now,a.rate,history);
    float selected=b.accepted?prediction(b.selected.at(a.speed),axis,a.tick,now,a.rate,history):expected;
    b.monitor(rate,expected,baseline,i!=0,selected);
    if(a.eligible)b.collect(prior,axis,a.tick,now,a.rate,rate,a.speed,history,tolerance(axis,last_k),index,i!=0);
    bias.observe(a.base,axis,a.tick,now,a.rate,rate,a.speed,history,a.context,context,a.eligible,i!=0,tolerance(axis,last_k));
   }
   a={now,rate,speed,eligible,index,current(prior,speed),base_model(prior,speed),context};
  }
 }
 Uncertainty uncertainty(float speed,float rate,float horizon)const{
  Uncertainty out;if(!std::isfinite(speed)||!std::isfinite(rate)||!std::isfinite(horizon))return out;
  auto q=bracket(speed);auto& a=banks[q.lo];auto& b=banks[q.hi];float floor=last_axis==0?.35f:last_axis==1?1.f:.12f;
  float ar=a.observations?std::sqrt(std::max(a.rate_mse,a.long_mse)):std::max(floor,.06f*last_k),br=b.observations?std::sqrt(std::max(b.rate_mse,b.long_mse)):std::max(floor,.06f*last_k);
  out.rate_rms=std::clamp(ar+q.t*(br-ar),floor,60.f);float relative=out.rate_rms/std::max(1.f,last_k);
  float validated=(a.accepted?1.f:0.f)*(1-q.t)+(b.accepted?1.f:0.f)*q.t;
  out.trust=std::clamp(model_weight*(1-capture_adaptation::smooth(relative/.16f)),0.f,1.f);out.observations=unsigned((1-q.t)*a.observations+q.t*b.observations);
  out.delay_margin=std::clamp(.35f*relative,0.f,.08f);out.gain_fraction=std::clamp(.14f-.10f*validated+1.2f*relative,.04f,.45f);
  out.tau_fraction=std::clamp(.25f-.17f*validated+2*relative,.08f,.65f);out.delay_seconds=std::clamp(.035f-.027f*validated+out.delay_margin,.008f,.09f);
  out.bias_rate=std::clamp(.6f*out.rate_rms+(.25f*bias.weight*std::abs(bias.total-effective_bias)),.05f,8.f);
  out.angle_guard=std::clamp(.18f*out.rate_rms*std::clamp(horizon,.05f,1.5f)+out.delay_margin*std::abs(rate),.015f,2.f);return out;
 }
 Diagnostics diagnostics(float speed,uint64_t now)const{
  const auto& b=banks[band(speed)];auto risk=uncertainty(speed,0,.5f);
  return {b.fit_count,b.validation_count,b.long_count,b.validating?2u:b.accepted?3u:b.fit_count?1u:0u,b.deployed_source,b.proposals,promotions(),rejections(),b.expired_fits,b.ambiguous_skips,b.innovation_skips,
   b.fit_epoch&&now>=b.fit_epoch?now-b.fit_epoch:0,bias.weight,bias.total,model_weight,risk.trust,bias.fit,bias.validation,bias.accepted,bias.rejected,b.source};
 }
};
struct Aircraft {
 int id=-1;uint64_t touched=0;std::array<Axis,3> axes{};
 void suspend(){for(auto& a:axes)a.suspend();}
 void observe(const std::array<shared_braking::Model,3>& priors,uint64_t now,const std::array<float,3>& rates,float speed,
  const adaptive_braking::History& history,const std::array<bool,3>& eligible,const std::array<SharedCandidate,3>& shared={},float bank_angle=NAN){
  Context c;c.rates=rates;c.bank=bank_angle;for(int i=0;i<3;++i)c.scales[i]=std::max(1.f,shared_braking::gain(priors[i],speed));
  for(int i=0;i<3;++i){axes[i].context=c;axes[i].offered=shared[i];axes[i].observe(priors[i],i,now,rates[i],speed,history,eligible[i]);}
 }
};
struct Fleet {
 std::array<Aircraft,64> planes{};
 Aircraft& get(int id,uint64_t now){for(auto& p:planes)if(p.id==id&&id>=0){p.touched=now;return p;}
  auto p=std::min_element(planes.begin(),planes.end(),[](const auto& a,const auto& b){return a.touched<b.touched;});*p={};p->id=id;p->touched=now;return *p;}
 void begin_flight(){for(auto& p:planes)p.suspend();}
 void reset(){for(auto& p:planes)p={};}
};
}
