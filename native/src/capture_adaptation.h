#pragma once
#include "adaptive_braking.h"
#include <limits>

// CAPTURE-only identification. Raw body rates are endpoints; issued commands
// are integrated with their real timestamps and zero-order hold. No derivative
// fit, allocations, I/O, game access, aircraft labels or changes to shared models.
namespace capture_adaptation {
constexpr std::array<float,4> centers{180.f,400.f,700.f,1000.f};
constexpr std::array<float,7> tau_scales{.40f,.60f,.80f,1.f,1.30f,1.75f,2.40f};
constexpr std::array<float,9> delays{0,.03f,.06f,.10f,.14f,.18f,.24f,.32f,.40f};
constexpr size_t shapes=tau_scales.size()*delays.size();
constexpr uint64_t attempt_lifetime_ms=60000;
inline float smooth(float x){x=std::clamp(x,0.f,1.f);return x*x*(3-2*x);}
inline int band(float speed){return speed<290?0:speed<550?1:speed<850?2:3;}
inline bool sane(shared_braking::Model m,float speed){
 float k=shared_braking::gain(m,speed);
 return std::isfinite(k)&&k>=.5f&&k<=700&&std::isfinite(m.bias)&&std::abs(m.bias)<=100&&
  std::isfinite(m.tau)&&m.tau>=.15f&&m.tau<=3.5f&&std::isfinite(m.delay)&&m.delay>=0&&m.delay<=.4f;
}
inline shared_braking::Model frozen(shared_braking::Model m,float speed){m.g0=shared_braking::gain(m,speed);m.g1=0;return m;}
struct Bracket {int lo=0,hi=1;float t=0;};
inline Bracket bracket(float speed){
 int lo=speed<=400?0:speed<=700?1:2;
 return {lo,lo+1,smooth((speed-centers[lo])/(centers[lo+1]-centers[lo]))};
}
// Capture the exact delayed ZOH segments, including a command change at the
// left boundary. Gaps and insufficient coverage cannot become fit evidence.
struct Segment {float dt=0,u=0;};
// State observations are about 30 Hz, but actual command hooks may run at a
// much higher frame rate. Keep enough real ZOH transitions for a 1 s check.
struct Path {std::array<Segment,256> segments{};size_t size=0;bool valid=false;float mean=0;};
inline Path path(const adaptive_braking::History& h,uint64_t from,uint64_t until,float delay,int axis){
 Path out;if(axis<0||axis>2||until<=from||!std::isfinite(delay)||delay<0||delay>.4f)return out;
 const auto d=uint64_t(std::lround(delay*1000));if(from<d)return out;from-=d;until-=d;
 if(!h.covers(from))return out;
 uint64_t previous=0,cursor=from;float held=0;bool found=false;
 for(size_t i=0;i<h.size;++i){const auto& v=h.values[(h.begin+i)%h.values.size()];
  float u=axis==0?v.pitch:axis==1?v.roll:v.yaw;
  if(!std::isfinite(u)||std::abs(u)>1.001f)return {};
  if(found&&v.tick<previous)return {};
  if(v.tick<=from){previous=v.tick;held=u;found=true;continue;}
  if(!found||v.tick-previous>100)return {};
  const auto end=std::min(v.tick,until);
  if(end>cursor){if(out.size==out.segments.size())return {};float dt=float(end-cursor)/1000;
   out.segments[out.size++]={dt,held};out.mean+=held*dt;cursor=end;}
  if(cursor>=until){out.valid=true;break;}
  previous=v.tick;held=u;
 }
 if(!out.valid&&found&&until>=cursor&&until-previous<=100){
  if(until>cursor){if(out.size==out.segments.size())return {};float dt=float(until-cursor)/1000;
   out.segments[out.size++]={dt,held};out.mean+=held*dt;}
  out.valid=true;
 }
 if(out.valid)out.mean/=float(until-from)/1000;return out;
}
struct Basis {float decay=1,response=0;};
inline Basis basis(const Path& p,float tau){
 Basis b;for(size_t i=0;i<p.size;++i){float a=std::exp(-p.segments[i].dt/tau);
  b.decay*=a;b.response=a*b.response+(1-a)*p.segments[i].u;}return b;
}
struct Parameters {
 float k=1,tau=1,delay=0,bias=0,slope=0,center=455.740317f;
 shared_braking::Model at(float speed)const{return {tau,delay,std::clamp(k*(1+slope*(speed-center)),.5f,700.f),0,bias};}
};
inline Parameters parameters(shared_braking::Model m,float center){
 const float k=shared_braking::gain(m,center);
 return {k,m.tau,m.delay,m.bias,m.g1/(177.865169f*std::max(.5f,k)),center};
}
inline float prediction(shared_braking::Model m,int axis,uint64_t start,uint64_t end,float rate,
 const adaptive_braking::History& h){
 const auto p=path(h,start,end,m.delay,axis);if(!p.valid)return NAN;
 const auto b=basis(p,m.tau);return b.decay*rate+m.g0*b.response+m.bias*(1-b.decay);
}
struct Regression {
 double xx=0,xz=0,zz=0,xy=0,zy=0,yy=0;
 void add(float x,float z,float y){xx+=x*x;xz+=x*z;zz+=z*z;xy+=x*y;zy+=z*y;yy+=y*y;}
 bool solve(float& k,float& bias,double& error)const{
  const double det=xx*zz-xz*xz;
  if(xx<.001||zz<.001||det<.015*xx*zz)return false;
  k=float((xy*zz-zy*xz)/det);bias=float((zy*xx-xy*xz)/det);
  error=std::max(0.,yy-2*k*xy-2*bias*zy+k*k*xx+2*k*bias*xz+bias*bias*zz);
  return std::isfinite(k)&&std::isfinite(bias)&&std::isfinite(error);
 }
};
struct Uncertainty {
 float rate_rms=0,angle_guard=0,delay_margin=0,trust=0;
 // Bounded engineering envelopes inferred from residuals, not statistical
 // confidence intervals or independently identified parameter covariance.
 float gain_fraction=.20f,tau_fraction=.30f,delay_seconds=.05f,bias_rate=0;
 unsigned observations=0;
};
struct Bank {
 std::array<Regression,shapes> fit{};
 Parameters seed{},selected{},proposal{};
 unsigned fit_count=0,validation_count=0,long_count=0,promotions=0,rejections=0,observations=0,expired_fits=0;
 uint64_t fit_epoch=0,validation_epoch=0,last_promotion=0;
 double input_sum=0,input_sq=0,fit_base=0,candidate_error=0,base_error=0,incumbent_error=0;
 double long_candidate=0,long_base=0,long_incumbent=0;
 float blend=0,selected_mse=0,baseline_mse=0,rate_mse=0,long_mse=0;
 // Complete held-out diagnostics survive clear_fit. Rejection bits:
 // 1 short prior, 2 long prior, 4 short incumbent, 8 long incumbent,
 // 16 short absolute tolerance, 32 long absolute tolerance.
 unsigned last_validation_count=0,last_long_count=0,last_rejection_reason=0;
 double last_candidate_error=0,last_baseline_error=0,last_long_candidate=0,last_long_baseline=0;
 unsigned fresh=0,long_observations=0;bool accepted=false,validating=false;
 void clear_fit(){fit={};fit_count=validation_count=long_count=0;input_sum=input_sq=fit_base=0;
  candidate_error=base_error=incumbent_error=long_candidate=long_base=long_incumbent=0;validating=false;fit_epoch=validation_epoch=0;}
 void suspend(){clear_fit();blend=0;fresh=0;selected_mse=baseline_mse=0;}
 void expire(uint64_t now,shared_braking::Model prior){
  if(!fit_count&&!validating)return;
  // Completed windows remain valid across an ordinary sampling gap, but never
  // accumulate indefinitely or mix materially different prior shapes. The
  // whole fit/validation attempt has a 60 s wall-clock lifetime. Qualified
  // windows can be sparse during coordinated flight; this still bounds stale
  // evidence without weakening excitation or held-out quality requirements.
  bool stale=now<fit_epoch||now-fit_epoch>attempt_lifetime_ms||!std::isfinite(prior.tau)||
   std::abs(std::log(prior.tau/seed.tau))>std::log(1.25f)||std::abs(prior.delay-seed.delay)>.08f||
   std::abs(shared_braking::gain(prior,seed.center)/std::max(.5f,seed.k)-1)>.35f;
  if(stale){++expired_fits;clear_fit();}
 }
 void monitor(float actual,float expected,float baseline,bool long_endpoint,float selected=NAN){
  if(!std::isfinite(actual)||!std::isfinite(expected)||!std::isfinite(baseline))return;
  const float delta=actual-expected,error=std::min(40000.f,delta*delta);
  float& mse=long_endpoint?long_mse:rate_mse;unsigned& n=long_endpoint?long_observations:observations;
  mse=n?mse+.12f*(error-mse):error;n=std::min(100000u,n+1);
  if(!long_endpoint&&accepted&&std::isfinite(selected)){float sd=actual-selected,bd=actual-baseline,se=std::min(40000.f,sd*sd),be=std::min(40000.f,bd*bd);
   selected_mse=fresh?selected_mse+.12f*(se-selected_mse):se;baseline_mse=fresh?baseline_mse+.12f*(be-baseline_mse):be;
   fresh=std::min(100000u,fresh+1);}
 }
 void finish(float tolerance,uint64_t now){
  if(!validating||validation_count<16||long_count<4)return;
  last_validation_count=validation_count;last_long_count=long_count;
  last_candidate_error=candidate_error;last_baseline_error=base_error;last_long_candidate=long_candidate;last_long_baseline=long_base;
  last_rejection_reason=0;
  if(!(candidate_error<.90*base_error))last_rejection_reason|=1;
  if(!(long_candidate<.98*long_base))last_rejection_reason|=2;
  if(accepted&&!(candidate_error<.97*incumbent_error))last_rejection_reason|=4;
  if(accepted&&!(long_candidate<.99*long_incumbent))last_rejection_reason|=8;
  if(!(candidate_error/validation_count<tolerance*tolerance))last_rejection_reason|=16;
  if(!(long_candidate/long_count<2.25*tolerance*tolerance))last_rejection_reason|=32;
  if(!last_rejection_reason){
   selected=proposal;accepted=true;++promotions;last_promotion=now;fresh=0;selected_mse=baseline_mse=0;
  }else ++rejections;
  clear_fit();
 }
 void collect(shared_braking::Model prior,int axis,uint64_t start,uint64_t end,float initial,float actual,float speed,
  const adaptive_braking::History& h,float tolerance,int index,bool long_endpoint){
  auto base=frozen(prior,speed);float baseline=prediction(base,axis,start,end,initial,h);
  float incumbent=accepted?prediction(selected.at(speed),axis,start,end,initial,h):baseline;
  if(!std::isfinite(baseline)||!std::isfinite(incumbent))return;
  if(validating){
   // Even validation anchors must be newer than the final training endpoint.
   if(start<validation_epoch)return;
   float expected=prediction(proposal.at(speed),axis,start,end,initial,h);if(!std::isfinite(expected))return;
   if(long_endpoint){++long_count;long_candidate+=std::pow(actual-expected,2);long_base+=std::pow(actual-baseline,2);long_incumbent+=std::pow(actual-incumbent,2);}
   else{++validation_count;candidate_error+=std::pow(actual-expected,2);base_error+=std::pow(actual-baseline,2);incumbent_error+=std::pow(actual-incumbent,2);}
   finish(tolerance,end);return;
  }
  if(long_endpoint)return;
  if(!fit_count){seed=parameters(prior,centers[index]);fit_epoch=start;}
  const float normalized=std::clamp(1+seed.slope*(speed-seed.center),.3f,3.f);
  const auto pp=path(h,start,end,prior.delay,axis);if(!pp.valid)return;
  // All shapes share the same endpoint pair. Keep only fully covered windows.
  std::array<Regression,shapes> additions{};
  for(size_t d=0;d<delays.size();++d){auto p=path(h,start,end,delays[d],axis);if(!p.valid)return;
   for(size_t t=0;t<tau_scales.size();++t){float tau=std::clamp(seed.tau*tau_scales[t],.2f,3.f);auto b=basis(p,tau);
    additions[d*tau_scales.size()+t].add(normalized*b.response,1-b.decay,actual-b.decay*initial);}
  }
  for(size_t i=0;i<shapes;++i){auto& a=fit[i];const auto& b=additions[i];a.xx+=b.xx;a.xz+=b.xz;a.zz+=b.zz;a.xy+=b.xy;a.zy+=b.zy;a.yy+=b.yy;}
  ++fit_count;input_sum+=pp.mean;input_sq+=pp.mean*pp.mean;fit_base+=std::pow(actual-baseline,2);
  if(fit_count<28)return;
  const double variance=input_sq/fit_count-std::pow(input_sum/fit_count,2);
  double best=std::numeric_limits<double>::infinity();Parameters chosen{};bool found=false;
  if(variance>.008&&fit_base/fit_count>.0025){
   for(size_t i=0;i<shapes;++i){float k=0,bias=0;double error=0;if(!fit[i].solve(k,bias,error))continue;
    if(k<.35f*seed.k||k>2.5f*seed.k||std::abs(bias)>std::min(40.f,.22f*seed.k+4.f))continue;
    if(error<best){best=error;chosen={k,std::clamp(seed.tau*tau_scales[i%tau_scales.size()],.2f,3.f),delays[i/tau_scales.size()],bias,seed.slope,seed.center};found=true;}
   }
  }
  if(found&&best<.80*fit_base){proposal=chosen;validating=true;validation_epoch=end;validation_count=long_count=0;}
  else clear_fit();
 }
 void step(float dt,float tolerance){
  const bool reliable=accepted&&fresh>=6&&selected_mse<tolerance*tolerance&&selected_mse<=1.10f*baseline_mse+.0025f;
  const float desired=reliable?.98f:0.f;blend+=std::clamp(desired-blend,-.8f*dt,.25f*dt);blend=std::clamp(blend,0.f,.98f);
 }
 shared_braking::Model effective(shared_braking::Model prior,float speed)const{
  auto base=frozen(prior,speed);if(!accepted||blend<=0)return base;const auto learned=selected.at(speed);
  base.g0+=blend*(learned.g0-base.g0);base.tau=std::exp(std::log(base.tau)+blend*(std::log(learned.tau)-std::log(base.tau)));
  base.delay+=blend*(learned.delay-base.delay);base.bias+=blend*(learned.bias-base.bias);return base;
 }
};
struct Anchor {uint64_t tick=0;float rate=0,speed=0;bool eligible=true;int band=-1;shared_braking::Model served{};};
struct Axis {
 std::array<Bank,4> banks{};std::array<Anchor,3> anchors{};
 uint64_t previous_tick=0;float model_weight=0,effective_bias=0,model_rate_rms=0;
 int last_axis=0;float last_k=1;
 void reset(){*this=Axis{};}
 void suspend(){anchors={};previous_tick=0;model_weight=0;for(auto& b:banks)b.suspend();}
 unsigned promotions()const{unsigned n=0;for(const auto& b:banks)n+=b.promotions;return n;}
 unsigned rejections()const{unsigned n=0;for(const auto& b:banks)n+=b.rejections;return n;}
 static float tolerance(int axis,float k){return std::max(axis==0?1.5f:axis==1?5.f:.35f,.08f*k);}
 shared_braking::Model current(shared_braking::Model prior,float speed)const{
  if(!sane(prior,speed))return frozen(prior,speed);
  auto q=bracket(speed);auto a=banks[q.lo].effective(prior,speed),b=banks[q.hi].effective(prior,speed);
  return {std::exp(std::log(a.tau)+q.t*(std::log(b.tau)-std::log(a.tau))),a.delay+q.t*(b.delay-a.delay),
   a.g0+q.t*(b.g0-a.g0),0,a.bias+q.t*(b.bias-a.bias)};
 }
 shared_braking::Model effective(shared_braking::Model prior,float speed,float dt){
  if(!std::isfinite(dt)||dt<=0||dt>.1f||!std::isfinite(speed)||!sane(prior,speed))return frozen(prior,speed);
  last_k=shared_braking::gain(prior,speed);for(auto& b:banks)b.step(dt,tolerance(last_axis,last_k));
  const auto q=bracket(speed);model_weight=banks[q.lo].blend+q.t*(banks[q.hi].blend-banks[q.lo].blend);
  auto out=current(prior,speed);effective_bias=out.bias;model_rate_rms=uncertainty(speed,0,.5f).rate_rms;return out;
 }
 // The caller may add a cold total-trim component after effective(). Record
 // that actual predictor model for anchors created THIS observation only.
 // Identification and candidate validation still use their fixed zero-bias
 // prior and fit total bias independently; this hook changes no fit statistics.
 void served(uint64_t now,shared_braking::Model actual_model){
  for(auto& a:anchors)if(a.tick==now&&sane(actual_model,a.speed))a.served=frozen(actual_model,a.speed);
 }
 void observe(shared_braking::Model prior,int axis,uint64_t now,float raw_rate,float speed,const adaptive_braking::History& h,bool eligible){
  if(axis<0||axis>2||!std::isfinite(raw_rate)||std::abs(raw_rate)>650||!std::isfinite(speed)||speed<90||speed>1400||!sane(prior,speed)){
   anchors={};previous_tick=0;return;}
  if(previous_tick&&now<=previous_tick){anchors={};previous_tick=0;for(auto& b:banks)b.suspend();}
  else if(previous_tick&&now-previous_tick>100){
   // No endpoint may bridge a gap. Already completed, fully covered windows
   // need not be discarded: their bounded sufficient statistics are valid.
   anchors={};previous_tick=0;
  }
  for(auto& b:banks)b.expire(now,prior);
  previous_tick=now;last_axis=axis;last_k=shared_braking::gain(prior,speed);const int index=band(speed);
  constexpr std::array<uint64_t,3> spans{240,600,1000};
  for(size_t i=0;i<anchors.size();++i){auto& a=anchors[i];
   if(!a.tick){a={now,raw_rate,speed,eligible,index,current(prior,speed)};continue;}
   a.eligible=a.eligible&&eligible;if(now-a.tick<spans[i])continue;
   bool covered=now-a.tick<=spans[i]+100&&a.band==index&&std::abs(speed-a.speed)<std::max(25.f,.035f*speed);
   if(covered){auto& bank=banks[index];float expected=prediction(a.served,axis,a.tick,now,a.rate,h);
    auto base=frozen(prior,a.speed);float baseline=prediction(base,axis,a.tick,now,a.rate,h);
    // Prediction risk sees real braking/reversal endpoints even when fitting is
    // inhibited by manual input; those samples never enter parameter learning.
    float selected=bank.accepted?prediction(bank.selected.at(a.speed),axis,a.tick,now,a.rate,h):expected;
    bank.monitor(raw_rate,expected,baseline,i!=0,selected);
    if(a.eligible)bank.collect(prior,axis,a.tick,now,a.rate,raw_rate,a.speed,h,tolerance(axis,last_k),index,i!=0);
   }
   a={now,raw_rate,speed,eligible,index,current(prior,speed)};
  }
 }
 Uncertainty uncertainty(float speed,float rate,float horizon)const{
  Uncertainty out;if(!std::isfinite(speed)||!std::isfinite(rate)||!std::isfinite(horizon))return out;
  const auto q=bracket(speed);const auto& a=banks[q.lo];const auto& b=banks[q.hi];
  float floor=last_axis==0?.35f:last_axis==1?1.f:.12f;
  float ra=a.observations?std::sqrt(std::max(a.rate_mse,a.long_mse)):std::max(floor,.06f*last_k);
  float rb=b.observations?std::sqrt(std::max(b.rate_mse,b.long_mse)):std::max(floor,.06f*last_k);
  out.rate_rms=std::clamp(ra+q.t*(rb-ra),floor,60.f);
  out.trust=std::clamp((a.blend+q.t*(b.blend-a.blend))*(1-smooth(out.rate_rms/std::max(1.f,.16f*last_k))),0.f,1.f);
  out.observations=unsigned((1-q.t)*a.observations+q.t*b.observations);
  out.delay_margin=std::clamp(.35f*out.rate_rms/std::max(1.f,last_k),0.f,.08f);
  const float relative=out.rate_rms/std::max(1.f,last_k);
  // Quiet data cannot prove gain, lag or delay. Retain a cold-model envelope
  // until an excited fit has passed independent short and long validation.
  const float validated=(a.accepted?1.f:0.f)*(1-q.t)+(b.accepted?1.f:0.f)*q.t;
  out.gain_fraction=std::clamp(.14f-.10f*validated+1.2f*relative,.04f,.45f);
  out.tau_fraction=std::clamp(.25f-.17f*validated+2.0f*relative,.08f,.65f);
  out.delay_seconds=std::clamp(.035f-.027f*validated+out.delay_margin,.008f,.09f);
  out.bias_rate=std::clamp(.6f*out.rate_rms,.05f,8.f);
  out.angle_guard=std::clamp(.18f*out.rate_rms*std::clamp(horizon,.05f,1.5f)+out.delay_margin*std::abs(rate),.015f,2.f);
  return out;
 }
};
struct Aircraft {int id=-1;uint64_t touched=0;std::array<Axis,3> axes{};void suspend(){for(auto& a:axes)a.suspend();}};
struct Fleet {
 std::array<Aircraft,64> planes{};
 Aircraft& get(int id,uint64_t now){
  for(auto& p:planes)if(p.id==id&&id>=0){p.touched=now;return p;}
  auto it=std::min_element(planes.begin(),planes.end(),[](const auto& a,const auto& b){return a.touched<b.touched;});
  *it={};it->id=id;it->touched=now;return *it;
 }
 void begin_flight(){for(auto& p:planes)p.suspend();}
 void reset(){for(auto& p:planes)p=Aircraft{};}
};
}
