#pragma once
#include "capture_adaptation.h"
// Identify one coherent signed pitch tuple: K+, K-, tau, delay and trim.
// Fitting and later validation use actual timestamped commands, with both
// short and long endpoints. This avoids grafting separately fitted gains
// onto the time constant of a symmetric response model.
namespace vector_signed_dynamics {
constexpr std::array<float,8> taus{.28f,.40f,.60f,.85f,1.15f,1.55f,2.05f,2.7f};
constexpr std::array<float,8> delays{0,.04f,.08f,.12f,.18f,.24f,.30f,.38f};
struct Model {float positive=50,negative=18,tau=1,delay=.18f,bias=0;};
inline Model from(shared_braking::Model m,float positive,float negative){return {positive,negative,m.tau,m.delay,m.bias};}
inline std::array<float,4> basis(const capture_adaptation::Path& p,float tau){
 std::array<float,4> b{0,0,0,1};
 for(size_t j=0;j<p.size;++j){auto s=p.segments[j];float a=std::exp(-s.dt/tau);b[0]=a*b[0]+(1-a)*std::max(0.f,s.u);b[1]=a*b[1]+(1-a)*std::min(0.f,s.u);b[3]*=a;}
 b[2]=1-b[3];return b;
}
inline float prediction(Model m,uint64_t start,uint64_t end,float q,const adaptive_braking::History& h){auto p=capture_adaptation::path(h,start,end,m.delay,0);if(!p.valid)return NAN;auto b=vector_signed_dynamics::basis(p,m.tau);return b[3]*q+b[0]*m.positive+b[1]*m.negative+b[2]*m.bias;}
struct Regression {
 double xx[3][3]{},xy[3]{},yy=0;unsigned count=0;
 void add(const std::array<float,4>& b,float y,float scale){double x[3]{b[0]*scale,b[1]*scale,b[2]};for(int i=0;i<3;++i){xy[i]+=x[i]*y;for(int j=0;j<3;++j)xx[i][j]+=x[i]*x[j];}yy+=y*y;++count;}
 bool solve(Model& m,double& mse)const{
  double a[3][4]{};for(int i=0;i<3;++i){if(xx[i][i]<.002)return false;for(int j=0;j<3;++j)a[i][j]=xx[i][j]/std::sqrt(xx[i][i]*xx[j][j]);a[i][3]=xy[i]/std::sqrt(xx[i][i]);}
  // Scaled pivot floor rejects unidentifiable trim or one missing sign.
  for(int i=0;i<3;++i){int pivot=i;for(int j=i+1;j<3;++j)if(std::abs(a[j][i])>std::abs(a[pivot][i]))pivot=j;
   if(std::abs(a[pivot][i])<.015)return false;for(int k=0;k<4;++k)std::swap(a[i][k],a[pivot][k]);double v=a[i][i];for(int k=i;k<4;++k)a[i][k]/=v;
   for(int j=0;j<3;++j)if(j!=i){v=a[j][i];for(int k=i;k<4;++k)a[j][k]-=v*a[i][k];}}
  double w[3]{};for(int i=0;i<3;++i)w[i]=a[i][3]/std::sqrt(xx[i][i]);m.positive=float(w[0]);m.negative=float(w[1]);m.bias=float(w[2]);
  if(!std::isfinite(m.positive)||!std::isfinite(m.negative)||!std::isfinite(m.bias)||m.positive<5||m.positive>220||m.negative<3||m.negative>m.positive||std::abs(m.bias)>8)return false;
  double e=yy;for(int i=0;i<3;++i){e-=2*w[i]*xy[i];for(int j=0;j<3;++j)e+=w[i]*w[j]*xx[i][j];}mse=std::max(0.,e)/std::max(1u,count);return std::isfinite(mse);
 }
};
struct Bank {
 std::array<Regression,taus.size()*delays.size()> fits{};unsigned shorts=0,longs=0,checks[2]{},accepted=0,rejected=0;uint64_t epoch=0,validation_epoch=0;
 double proposed_error[2]{},reference_error[2]{};Model proposal{},selected{};bool validating=false,valid=false;
 float rate_mse=4,long_mse=4,mix=0;unsigned monitored=0;
 static float scale(float speed,int index){return shared_braking::gain(shared_braking::pitch,speed)/shared_braking::gain(shared_braking::pitch,capture_adaptation::centers[index]);}
 Model at(Model m,float speed,int index)const{float k=scale(speed,index);m.positive*=k;m.negative*=k;return m;}
 void clear(){fits={};shorts=longs=0;checks[0]=checks[1]=0;proposed_error[0]=proposed_error[1]=reference_error[0]=reference_error[1]=0;epoch=validation_epoch=0;validating=false;}
 void observe(uint64_t start,uint64_t end,float initial,float actual,float speed,const adaptive_braking::History& h,bool longer,bool allowed,Model baseline,int index){
  float ref=prediction(valid?at(selected,speed,index):baseline,start,end,initial,h);if(!std::isfinite(ref))return;
  if(valid){float e=std::min(900.f,(actual-ref)*(actual-ref));float& monitor=longer?long_mse:rate_mse;monitor+=.08f*(e-monitor);++monitored;}
  if(!allowed){clear();return;}if(epoch&&end-epoch>45000)clear();if(!epoch)epoch=start;
  if(validating){if(start<validation_epoch)return;float p=prediction(at(proposal,speed,index),start,end,initial,h);if(!std::isfinite(p))return;int k=longer?1:0;
   proposed_error[k]+=(actual-p)*(actual-p);reference_error[k]+=(actual-ref)*(actual-ref);++checks[k];
   if(checks[0]>=12&&checks[1]>=6){double a=proposed_error[0]/checks[0],b=proposed_error[1]/checks[1],ra=reference_error[0]/checks[0],rb=reference_error[1]/checks[1];
    if(a+b<.88*(ra+rb)&&a<=1.04*ra+.0025&&b<=1.04*rb+.0025&&a<9&&b<16){selected=proposal;valid=true;++accepted;rate_mse=float(a);long_mse=float(b);monitored=1;}else ++rejected;clear();}
   return;}
  size_t n=0;for(float tau:taus)for(float delay:delays){auto p=capture_adaptation::path(h,start,end,delay,0);auto& f=fits[n++];if(p.valid){auto b=vector_signed_dynamics::basis(p,tau);f.add(b,actual-b[3]*initial,scale(speed,index));}}
  if(longer)++longs;else ++shorts;
  if(shorts>=24&&longs>=10){double best=INFINITY;size_t i=0;for(float tau:taus)for(float delay:delays){Model m;m.tau=tau;m.delay=delay;double error=0;auto& fit=fits[i++];if(fit.count>=shorts+longs&&fit.solve(m,error)&&error<best){best=error;proposal=m;}}
   if(std::isfinite(best)){validating=true;validation_epoch=end;}else if(shorts>=100)clear();}
 }
 void step(float dt){float rms=std::sqrt(std::max(rate_mse,long_mse));float want=valid&&monitored>=6?std::clamp((4.5f-rms)/1.5f,0.f,1.f):0;mix+=std::clamp(want-mix,-dt,.35f*dt);}
};
struct Anchor {uint64_t tick=0;float rate=0,speed=0;bool allowed=false;int bank=0;Model model{};};
struct Axis {
 std::array<Bank,4> banks{};std::array<Anchor,2> anchors{};uint64_t last=0;Model serving{};bool seen=false;float weight=0,rmse=0;
 void suspend(){anchors={};last=0;seen=false;for(auto& b:banks){b.clear();b.mix=0;b.monitored=0;}}
 void observe(Model baseline,uint64_t now,float rate,float speed,const adaptive_braking::History& h,bool allowed,float dt){
  if(!std::isfinite(rate)||!std::isfinite(speed)||!std::isfinite(dt)||dt<=0||dt>.1f||speed<90||speed>1400){anchors={};return;}
  if(last&&(now<=last||now-last>100))anchors={};last=now;int index=capture_adaptation::band(speed);
  for(int j=0;j<2;++j){auto& a=anchors[j];if(!a.tick){a={now,rate,speed,allowed,index,baseline};continue;}
   a.allowed=a.allowed&&allowed;uint64_t span=j?600:240;if(now-a.tick<span)continue;
   if(now-a.tick<=span+100&&a.bank==index&&std::abs(a.speed-speed)<20)banks[index].observe(a.tick,now,a.rate,rate,a.speed,h,j!=0,a.allowed,a.model,index);
   a={now,rate,speed,allowed,index,baseline};}
  for(auto& b:banks)b.step(dt);
 }
 Model effective(Model fallback,float speed,float dt){
  if(!std::isfinite(dt)||dt<=0||dt>.1f||!std::isfinite(speed)||speed<90||speed>1400)return fallback;
  auto q=capture_adaptation::bracket(speed);auto& a=banks[q.lo];auto& b=banks[q.hi];weight=(1-q.t)*a.mix+q.t*b.mix;
  Model x=a.at(a.selected,speed,q.lo),y=b.at(b.selected,speed,q.hi),target=fallback;
  auto blend=[&](float base,float left,float right){return base+(1-q.t)*a.mix*(left-base)+q.t*b.mix*(right-base);};
  target.positive=blend(fallback.positive,x.positive,y.positive);target.negative=blend(fallback.negative,x.negative,y.negative);target.tau=std::exp(blend(std::log(fallback.tau),std::log(x.tau),std::log(y.tau)));target.delay=blend(fallback.delay,x.delay,y.delay);target.bias=blend(fallback.bias,x.bias,y.bias);
  // Promoting or withdrawing a tuple must not jump the inverse controller.
  if(!seen){serving=target;seen=true;}else{auto approach=[&](float from,float to,float cap){return from+std::clamp(to-from,-cap*dt,cap*dt);};serving.positive=std::exp(approach(std::log(serving.positive),std::log(target.positive),.45f));serving.negative=std::exp(approach(std::log(serving.negative),std::log(target.negative),.45f));serving.tau=std::exp(approach(std::log(serving.tau),std::log(target.tau),.35f));serving.delay=approach(serving.delay,target.delay,.06f);serving.bias=approach(serving.bias,target.bias,2.f);}
  rmse=std::sqrt((1-q.t)*std::max(a.rate_mse,a.long_mse)+q.t*std::max(b.rate_mse,b.long_mse));return serving;
 }
 unsigned promotions()const{unsigned n=0;for(auto& b:banks)n+=b.accepted;return n;}
};
}

