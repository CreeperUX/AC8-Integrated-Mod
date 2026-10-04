#include "src/mouse_aim.cpp"
#include <cassert>
#include <cstdio>
const std::array<shared_braking::Model,3> priors{shared_braking::pitch,shared_braking::roll,shared_braking::yaw};
const std::array<float,3> caps{.85f,1.f,.7f},absolute_caps{1.f,1.f,.85f},rates{45.f,140.f,7.f},slews{12.f,16.f,6.f};
void add(adaptive_braking::History& h,uint64_t t,int axis,float u){adaptive_braking::Input v{t,0,0,0};if(axis==0)v.pitch=u;else if(axis==1)v.roll=u;else v.yaw=u;h.add(v);}
struct Metrics{float peak=0,late=0,t90=40;double error_area=0;};
Metrics scenario(bool agile,int axis,float gs,float ts,float ds,float initial,float initial_rate=0,bool learned=false){
 auto m=priors[axis],truth=m;truth.g0*=gs;truth.g1*=gs;truth.tau*=ts;truth.delay=std::max(0.f,truth.delay+ds);
 if(learned)m=truth;
 float error=initial,w=initial_rate,previous=0;model_control::Controller c;adaptive_braking::History h;
 for(uint64_t t=1000;t<=2000;t+=10)add(h,t,axis,0);Metrics out;
 for(uint64_t t=2010;t<42010;t+=10){
  float response=learned?std::clamp(std::sqrt(gs/ts),.65f,1.35f):1.f;
  auto p=model_control::predict(m,error,w,previous,600,caps[axis],rates[axis]*response,axis==1?1.f:.2f,h,t,axis,0,learned?1.5f:2.f,agile);assert(p.valid);
  float u=c.apply_full(p,0,agile?absolute_caps[axis]:caps[axis],slews[axis],.01f);
  assert(std::isfinite(u)&&std::abs(u)<=(agile?absolute_caps[axis]:caps[axis])+1e-5f);
  assert(std::abs(u-previous)<=slews[axis]*.01f+1e-5f);
  add(h,t,axis,u);float d=std::exp(-.01f/truth.tau);w=d*w+(1-d)*shared_braking::gain(truth,600)*h.at_axis(t-uint64_t(truth.delay*1000),axis);error-=w*.01f;previous=u;
  out.peak=std::max(out.peak,-error);out.error_area+=std::abs(error)*.01;
  if(std::abs(error)<=initial*.1f&&out.t90==40)out.t90=float(t-2000)/1000;
  if(t>32000)out.late=std::max(out.late,std::abs(error));
 }return out;
}
int main(){
 setvbuf(stdout,nullptr,_IONBF,0);
 adaptive_braking::History h;for(uint64_t t=1000;t<=2000;t+=10)h.add({t,0,0,0});
 for(int axis=0;axis<3;++axis){
  for(float e:{-1.f,0.f,1.f})for(float rate:{-30.f,0.f,30.f}){
   auto a=model_control::predict(priors[axis],e,rate,0,600,caps[axis],rates[axis],axis==1?1.f:.2f,h,2000,axis,0,2,false);
   auto b=model_control::predict(priors[axis],e,rate,0,600,caps[axis],rates[axis],axis==1?1.f:.2f,h,2000,axis,0,2,true);
   assert(a.valid&&b.valid&&b.pursuit==0&&std::abs(a.command-b.command)<1e-6f);
  }
  auto distant=model_control::predict(priors[axis],90,0,0,600,caps[axis],rates[axis],.2,h,2000,axis,0,2,true);
  assert(distant.pursuit==1&&std::abs(distant.input_limit-absolute_caps[axis])<1e-5&&distant.damping==1.25f);
  printf("axis%d far inputlimit%.3f ratecap%.3f damping%.2f\n",axis,distant.input_limit,distant.rate_limit,distant.damping);
 }
 for(bool learned:{false,true})for(int axis=0;axis<3;++axis){
  unsigned n=0,faster=0,worse=0;float oldPeak=0,newPeak=0,late=0,regret=0;double oldTime=0,newTime=0,oldArea=0,newArea=0;
  for(float g:{.65f,1.f,1.65f})for(float tau:{.5f,1.f,2.f})for(float delay:{-.03f,0.f,.06f})for(float w:{0.f,axis==0?30.f:axis==1?90.f:5.f}){
   auto a=scenario(false,axis,g,tau,delay,axis==0?60.f:axis==1?90.f:20.f,w,learned),b=scenario(true,axis,g,tau,delay,axis==0?60.f:axis==1?90.f:20.f,w,learned);
   ++n;faster+=b.t90<a.t90;oldTime+=a.t90;newTime+=b.t90;oldArea+=a.error_area;newArea+=b.error_area;oldPeak=std::max(oldPeak,a.peak);newPeak=std::max(newPeak,b.peak);late=std::max(late,b.late);regret=std::max(regret,b.peak-a.peak);worse+=b.peak>a.peak+.25f;
  }
  printf("learned%d axis%d cases%u faster%u mean_t90 %.4f->%.4f error_area %.4f->%.4f maxovershoot %.4f->%.4f worstIncrease%.4f regressions>.25=%u maxlate%.4f\n",learned,axis,n,faster,oldTime/n,newTime/n,oldArea/n,newArea/n,oldPeak,newPeak,regret,worse,late);
  assert(late<(axis==1?2.f:1.f)&&newTime<oldTime&&newArea<oldArea);
  assert(regret<(axis==0?1.f:axis==1?1.5f:.2f));
 }
 puts("PASS dynamic authority, unchanged near-target planner, command slew and324 scalar approach cases; not actualAC8 acceptance");
}

