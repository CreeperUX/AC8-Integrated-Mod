#include "robust_intercept.h"
#include <cassert>
#include <cstdio>
#include <chrono>
#include <vector>
struct Outcome {double final=0,peak=0,t03=10,t01=10,moving_error=0;unsigned invalid=0;};
static Outcome fly(shared_braking::Model model,shared_braking::Model truth,int axis,float initial,float dt,
 robust_intercept::Envelope envelope,bool moving=false,float initial_input=0,float initial_rate=0,float bias=0){
 const float limit=axis==2?.85f:1.f,slew=axis==2?6.f:12.f,cap=axis==2?7.f:45.f;
 float input=initial_input,rate=initial_rate,error=initial;
 adaptive_braking::History history;for(uint64_t t=1000;t<=2000;t+=8)history.add({t,axis==0?input:0,0,axis==2?input:0});
 Outcome out;double held03=0,held01=0;bool seen03=false,seen01=false;
 for(unsigned frame=0;frame<unsigned(std::ceil(8/dt));++frame){
  const double time=frame*double(dt),now_real=2000+time*1000;const uint64_t now=uint64_t(std::llround(now_real));
  float reference=moving&&time<4?2.f:0.f;
  auto plan=robust_intercept::predict(model,error,rate,input,360,limit,cap,.025f,history,now,axis,reference,slew,dt,bias,envelope);
  if(!plan.plan.valid){++out.invalid;break;}
  const float next=plan.issued_command;
  assert(std::isfinite(next)&&std::abs(next)<=limit+1e-5&&std::abs(next-input)<=slew*dt+1e-5);
  if(axis==0)history.add({now,next,0,0});else history.add({now,0,0,next});
  // Real source commands are immediately applied and held. Integrate every
  // delayed history change exactly, independent of the prediction code.
  double cursor=now_real,end=now_real+dt*1000;
  auto advance=[&](double until){if(until<=cursor)return;
   double seconds=(until-cursor)/1000;
   double u=history.at_axis(uint64_t(std::max(0.0,cursor-truth.delay*1000)),axis);
   double eq=shared_braking::gain(truth,360)*u+truth.bias;
   double a=std::exp(-seconds/truth.tau),travel=eq*seconds+(rate-eq)*truth.tau*(1-a);
   rate=float(a*rate+(1-a)*eq);error+=float(reference*seconds-travel);cursor=until;
  };
  for(size_t j=0;j<history.size;++j){double change=double(history.values[(history.begin+j)%history.values.size()].tick)+truth.delay*1000;
   if(change>cursor&&change<end)advance(change);
  }advance(end);input=next;
  out.peak=std::max(out.peak,std::max(0.0,-std::copysign(1.0,double(initial))*double(error)));out.final=std::abs(error);
  if(moving&&time>=2&&time<4)out.moving_error+=std::abs(error)*dt/2;
  if(std::abs(error)<.3&&std::abs(rate-reference)<.5){held03+=dt;if(held03>.1&&!seen03){out.t03=time-held03+dt;seen03=true;}}else{held03=0;seen03=false;out.t03=10;}
  if(std::abs(error)<.1&&std::abs(rate-reference)<.25){held01+=dt;if(held01>.1&&!seen01){out.t01=time-held01+dt;seen01=true;}}else{held01=0;seen01=false;out.t01=10;}
 }return out;
}
int main(){
 setvbuf(stdout,nullptr,_IONBF,0);
 robust_intercept::Envelope certain{0,0,0,0,1};unsigned cases=0;
 for(int axis:{0,2})for(float hz:{30.f,60.f,120.f})for(float initial:{.1f,.3f,1.f,3.f,-1.f}){
  auto m=axis==0?shared_braking::pitch:shared_braking::yaw;m.bias=0;
  auto o=fly(m,m,axis,initial,1/hz,certain);
  printf("NOMINAL axis=%d hz=%.0f e=%.2f over=%.5f final=%.6f t03=%.3f t01=%.3f invalid=%u\n",axis,hz,initial,o.peak,o.final,o.t03,o.t01,o.invalid);
  assert(!o.invalid&&o.final<.02&&o.peak<.07);
  if(std::abs(initial)==1)assert(o.t03<1.05&&o.t01<1.35);
  ++cases;
 }
 // Multiple aircraft response scales, not an F-35-only tuning surface.
 for(int axis:{0,2})for(float scale:{.55f,1.f,1.8f})for(float hz:{30.f,60.f,120.f}){
  auto m=axis==0?shared_braking::pitch:shared_braking::yaw;m.bias=0;m.g0*=scale;m.g1*=scale;
  for(float truth_gain:{.8f,1.f,1.2f})for(float truth_tau:{.75f,1.f,1.25f}){
   auto t=m;t.g0*=truth_gain;t.g1*=truth_gain;t.tau*=truth_tau;t.delay=std::clamp(t.delay+.035f,0.f,.5f);
   auto o=fly(m,t,axis,1,1/hz,{.23f,.30f,.045f,0,.6f});
   printf("MISMATCH axis=%d K=%.2f trueK=%.2f tau=%.2f hz=%.0f over=%.5f final=%.5f t03=%.3f invalid=%u\n",axis,scale,truth_gain,truth_tau,hz,o.peak,o.final,o.t03,o.invalid);
   // Frozen cold bounds prioritize a small excursion. The integrated runtime
   // additionally learns K/tau/delay; this isolated helper cannot promise the
   // warm-model response time while parameters are deliberately held wrong.
   assert(!o.invalid&&o.final<.05&&o.peak<.18&&o.t03<5);++cases;
  }
 }
 for(int axis:{0,2})for(float hz:{30.f,60.f,120.f}){
  auto m=axis==0?shared_braking::pitch:shared_braking::yaw;m.bias=0;
  auto moving=fly(m,m,axis,1,1/hz,certain,true);
  auto nonzero=fly(m,m,axis,3,1/hz,certain,false,.2f,1.f);
  auto truth=m;truth.bias=axis==0?2.f:.25f;
  auto trim=fly(m,truth,axis,1,1/hz,certain,false,0,0,truth.bias);
  printf("DYNAMIC axis=%d hz=%.0f moving=%.5f final=%.5f nonzero_over=%.5f trim_final=%.6f\n",axis,hz,moving.moving_error,moving.final,nonzero.peak,trim.final);
  assert(!moving.invalid&&moving.moving_error<.13&&moving.final<.04);
  assert(!nonzero.invalid&&nonzero.final<.04&&nonzero.peak<.07);
  assert(!trim.invalid&&trim.final<.02);cases+=3;
 }
 // Distinguish the cost of an indefinitely wrong cold model from control-law
 // latency. No fabricated learner is used: the warm case explicitly supplies
 // identified truth parameters and validated smaller residual bounds.
 for(float hz:{30.f,60.f,120.f}){
  auto old=shared_braking::pitch;old.bias=0;old.g0*=1.8f;old.g1*=1.8f;
  auto truth=old;truth.g0*=1.2f;truth.g1*=1.2f;truth.tau*=.75f;truth.delay+=.035f;
  auto cold=fly(old,truth,0,1,1/hz,{.23f,.30f,.045f,0,.6f});
  auto warm=fly(truth,truth,0,1,1/hz,{.05f,.08f,.012f,0,.95f});
  printf("IDENTIFIED hz=%.0f cold_t03=%.3f warm_t03=%.3f warm_over=%.5f warm_final=%.6f\n",hz,cold.t03,warm.t03,warm.peak,warm.final);
  assert(!warm.invalid&&warm.t03<1.2&&warm.peak<.1&&warm.final<.02);cases+=2;
 }
 adaptive_braking::History h;for(uint64_t t=1000;t<=2000;t+=10)h.add({t,0,0,0});
 robust_intercept::Plant source_plant{50,1,0,0,0,.02,std::exp(-.02),-std::expm1(-.02)};
 robust_intercept::SourceTrace trace;
 auto held=robust_intercept::forecast(source_plant,{},0,1,1,12,.04,false,&trace);
 auto independent=shared_braking::Model{1,0,50,0,0};float independent_rate=0;
 float expected=shared_braking::advance(independent,.24f,360,.02f,independent_rate);
 expected+=shared_braking::advance(independent,.48f,360,.02f,independent_rate);
 assert(held.valid&&trace.valid&&trace.count==2&&std::abs(trace.inputs[0]-.24)<1e-6&&std::abs(trace.inputs[1]-.48)<1e-6);
 assert(std::abs(held.travel-expected)<1e-6&&robust_intercept::input_at(trace,.019)==trace.inputs[0]&&robust_intercept::input_at(trace,.021)==trace.inputs[1]);
 robust_intercept::State pending;source_plant.delay=.18;source_plant.reference=2;
 assert(robust_intercept::pending(pending,source_plant,0,h,2000,0)&&std::abs(pending.travel+.36)<1e-6);
 auto zero=robust_intercept::predict(shared_braking::pitch,0,0,0,360,1,45,.025f,h,2000,0,0,12,.02f,0,certain);
 assert(zero.plan.valid&&std::abs(zero.issued_command)<.0002);
 auto selected=robust_intercept::predict(shared_braking::pitch,1,0,0,360,1,45,.025f,h,2000,0,0,12,.02f,0,certain);
 auto audited=robust_intercept::assess_issued(shared_braking::pitch,1,0,0,selected.issued_command,selected.desired_command,360,1,45,.025f,h,2000,0,0,12,.02f,0,certain);
 assert(audited.plan.valid&&audited.issued_command==selected.issued_command&&std::abs(audited.stop_residual-selected.stop_residual)<1e-6);
 auto changed=robust_intercept::assess_issued(shared_braking::pitch,1,0,0,selected.issued_command*.5f,selected.desired_command,360,1,45,.025f,h,2000,0,0,12,.02f,0,certain);
 assert(changed.plan.valid&&changed.issued_command==selected.issued_command*.5f&&std::abs(changed.stop_residual-audited.stop_residual)>1e-4);
 auto impossible_first=robust_intercept::assess_issued(shared_braking::pitch,1,0,0,.8f,.8f,360,1,45,.025f,h,2000,0,0,12,.02f,0,certain);
 assert(!impossible_first.plan.valid&&!impossible_first.source_certified);
 auto unreachable=robust_intercept::predict(shared_braking::yaw,2,0,0,360,.85f,7,.025f,h,2000,2,20,6,.02f,0,certain);
 assert(unreachable.plan.valid&&unreachable.reference_unreachable&&unreachable.issued_command>0&&unreachable.plan.damping==-1);
 for(uint64_t t=1000;t<=2000;t+=10)h.add({t,.8f,0,0});
 auto momentum=robust_intercept::predict(shared_braking::pitch,.1f,20,.8f,360,1,45,.025f,h,2000,0,0,12,.02f,0,certain);
 assert(momentum.plan.valid&&momentum.unavoidable&&momentum.unavoidable_crossing>.1&&momentum.issued_command<.8f);
 h.clear();const float ff=-2/shared_braking::gain(shared_braking::pitch,360);
 for(uint64_t tick=1000;tick<=2000;tick+=10)h.add({tick,ff,0,0});
 auto trimmed=robust_intercept::predict(shared_braking::pitch,0,0,ff,360,1,45,.025f,h,2000,0,0,12,.02f,2,certain);
 assert(trimmed.plan.valid&&std::abs(trimmed.issued_command-ff)<1e-6);
 h.clear();for(uint64_t t=1000;t<=2000;t+=10)h.add({t,0,0,0});
 auto begin=std::chrono::steady_clock::now();volatile float sum=0;constexpr int n=4000;
 for(int i=0;i<n;++i){auto p=robust_intercept::predict(shared_braking::pitch,1+.002f*(i%10),2,.1f,360,1,45,.025f,h,2000,0,0,12,1.f/60,0,{.2f,.25f,.04f,.2f,.6f});sum=sum+p.issued_command;}
 double us=std::chrono::duration<double,std::micro>(std::chrono::steady_clock::now()-begin).count()/n;
 printf("PASS %u scalar dynamic aircraft/rate cases; typical %.1fus/axis sum=%.3f\n",cases,us,float(sum));
 assert(us<200);
}
