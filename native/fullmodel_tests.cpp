#include "src/mouse_aim.cpp"
#include <cassert>
#include <cstdio>
const std::array<shared_braking::Model,3> bases{shared_braking::pitch,shared_braking::roll,shared_braking::yaw};
const std::array<float,3> caps{.85f,1.f,.7f},rates{45.f,140.f,7.f},slews{12.f,16.f,6.f};
void add(adaptive_braking::History& h,uint64_t t,int axis,float u){adaptive_braking::Input v{t,0,0,0};if(axis==0)v.pitch=u;else if(axis==1)v.roll=u;else v.yaw=u;h.add(v);}
void serving_test(int axis){
 auto standard=bases[axis],truth=standard;truth.g0*=1.5f;truth.g1*=1.5f;truth.tau*=.7f;truth.bias=0;
 model_serving::Axis s;adaptive_braking::History h;float w=0;
 for(uint64_t t=1000;t<31000;t+=10){
  float u=.6f*std::sin(float(t)*.002f);add(h,t,axis,u);float d=std::exp(-.01f/truth.tau);
  w=d*w+(1-d)*shared_braking::gain(truth,360)*h.at_axis(t-uint64_t(truth.delay*1000),axis);
  s.observe(standard,truth,.9f,axis,t,w,360,h);s.effective(standard,360,.2f,.01f);
 }
 auto& cell=s.cells[1];assert(cell.stored&&cell.confirmed&&cell.deployments>0&&s.mix>.99f);
 assert(std::abs(s.current_model(standard).g0-truth.g0)<.01f);
 unsigned deployed=cell.deployments;s.candidate_check.reset();s.interrupt();
 s.effective(standard,360,.2f,.01f);assert(cell.confirmed&&s.mix>.99f&&cell.deployments==deployed);
 // A new, wrong candidate cannot evict the already tested model.
 auto bad=standard;bad.g0*=.5f;bad.g1*=.5f;bad.tau*=2.f;
 for(uint64_t t=31000;t<61000;t+=10){
  float u=.6f*std::sin(float(t)*.002f);add(h,t,axis,u);float d=std::exp(-.01f/truth.tau);
  w=d*w+(1-d)*shared_braking::gain(truth,360)*h.at_axis(t-uint64_t(truth.delay*1000),axis);
  s.observe(standard,bad,.9f,axis,t,w,360,h);s.effective(standard,360,.2f,.01f);
 }
 assert(cell.confirmed&&cell.deployments==deployed);
 // Change the true plant back: deployed model must fail monitoring and fade.
 for(uint64_t t=61000;t<101000;t+=10){
  float u=.6f*std::sin(float(t)*.002f);add(h,t,axis,u);float d=std::exp(-.01f/standard.tau);
  w=d*w+(1-d)*shared_braking::gain(standard,360)*h.at_axis(t-uint64_t(standard.delay*1000),axis);
  s.observe(standard,bad,0,axis,t,w,360,h);s.effective(standard,360,.2f,.01f);
 }
 assert(!cell.confirmed&&s.mix==0&&s.source()==0);
 s.new_flight();assert(cell.stored&&!cell.confirmed&&s.source()==0);
 printf("axis%d serving: deploy, retain across candidate reset/rejection, reject degraded serving, standard on new flight PASS\n",axis);
}
struct Metrics{float overshoot=0,final_error=0,late_peak=0;};
Metrics cold_proxy(int axis,float gs,float ts,float ds,float damping=1){
 auto m=bases[axis],truth=m;truth.g0*=gs;truth.g1*=gs;truth.tau*=ts;truth.delay=std::max(0.f,truth.delay+ds);
 float error=axis==1?40.f:axis==0?20.f:8.f,w=0,previous=0;model_control::Controller c;adaptive_braking::History h;
 for(uint64_t t=1000;t<=2000;t+=10)add(h,t,axis,0);
 Metrics out;
 for(uint64_t t=2010;t<42010;t+=10){
  auto plan=model_control::predict(m,error,w,previous,360,caps[axis],rates[axis],axis==1?1.f:.2f,h,t,axis,0,damping);assert(plan.valid);
  float u=c.apply_full(plan,0,caps[axis],slews[axis],.01f);assert(std::isfinite(u)&&std::abs(u)<=caps[axis]+1e-5f);
  assert(std::abs(u-previous)<=slews[axis]*.01f+.00001f);if(t>2600)assert(c.weight==1);
  add(h,t,axis,u);float d=std::exp(-.01f/truth.tau);
  w=d*w+(1-d)*shared_braking::gain(truth,360)*h.at_axis(t-uint64_t(truth.delay*1000),axis);error-=w*.01f;previous=u;
  out.overshoot=std::max(out.overshoot,-error);if(t>32000)out.late_peak=std::max(out.late_peak,std::abs(error));
 }
 out.final_error=error;return out;
}
void full_runtime(){
 aircraft=65536;assist_speed_pawn=65536;assist_speed=360;assist_brake=0;assist_environment_unsafe=false;assist_plane_type=14010;
 game_paused=false;gaze_active=false;model_assist_enabled=true;config.model_assist_strength=.2f;
 auto refresh=[](unsigned mask){auto now=GetTickCount64();assist_speed_tick=now;adaptive_owner=65536;adaptive_history.clear();for(auto t=now-900;t<=now;t+=5)adaptive_history.add({t,.1f,.2f,.3f});observe_assist_input(65536,.1f,.2f,mask,.3f);};
 for(int step=0;step<100;++step){refresh(0);float p=.2f,r=.2f,y=.2f;apply_model_control(2,2,2,10,10,1,10,10,1,.01f,p,r,y,140);assert(assist_profile==6);}
 for(int i=0;i<3;++i)assert(primary_controllers[i].weight==1&&assist_model_source[i]==0&&assist_deployed_models[i]==0);
 // Brake/cloud conditions suspend calibration, not standard-model control.
 assist_environment_unsafe=true;assist_brake=1;refresh(0);float p=.2f,r=.2f,y=.2f;
 apply_model_control(2,2,2,10,10,1,10,10,1,.01f,p,r,y,140);for(auto& c:primary_controllers)assert(c.weight==1);
 refresh(2);apply_model_control(2,2,2,10,10,1,10,10,1,.01f,p,r,y,140);
 assert(primary_controllers[2].weight==0&&primary_controllers[0].weight==1&&primary_controllers[1].weight==1&&std::abs(y-.3f)<1e-6);
 refresh(0);assist_speed_tick=GetTickCount64()-1000;apply_model_control(2,2,2,10,10,1,10,10,1,.01f,p,r,y,140);
 for(int i=0;i<3;++i)assert(assist_model_source[i]==-1);assert(primary_controllers[0].weight<1);
 full_model_new_flight=true;refresh(0);apply_model_control(2,2,2,10,10,1,10,10,1,.01f,p,r,y,140);assert(primary_controllers[0].weight<.021f);
 puts("Runtime cold standard3axes, yaw primary100%, peraxis manual, calibration gates, stale-data fallback and same-pawn restart PASS");
}
void learn_and_serve(int axis){
 auto standard=bases[axis],truth=standard;truth.g0*=1.65f;truth.g1*=1.65f;truth.tau*=1.6f;truth.delay+=.06f;truth.bias=0;
 online_learning::Axis learner;model_serving::Axis served;model_control::Controller controller;adaptive_braking::History h;model_control::Reference reference;
 float angle=0,rate=0,previous=0,last_target=0;unsigned active=0;
 for(uint64_t tick=1000;tick<241000;tick+=10){
  float t=float(tick-1000)/1000,scale=axis==0?35.f:axis==1?65.f:7.f,target=scale*(std::sin(t*.4f)+.43f*std::sin(t*.8f));
  auto old=learner.promotions();learner.observe(standard,axis,tick,rate,360,h);auto candidate=learner.effective(standard,360,.01f);
  if(old!=learner.promotions())served.candidate_check.reset();served.observe(standard,candidate,learner.learned_weight,axis,tick,rate,360,h);
  auto model=served.effective(standard,360,.2f,.01f);float target_rate=reference.update((target-last_target)/.01f,std::abs(target-last_target)>.0001f,.01f,rates[axis]);last_target=target;
  float response=std::clamp(std::sqrt(served.gain_scale/served.tau_scale),.65f,1.35f);
  auto plan=model_control::predict(model,target-angle,rate,previous,360,caps[axis],rates[axis]*response,axis==1?1.f:.2f,h,tick,axis,target_rate,2.f-.5f*served.mix,true);
  float u=controller.apply_full(plan,0,axis==2?.85f:1.f,slews[axis],.01f);if(tick>3000)assert(controller.weight==1);active+=served.mix>.01f;
  add(h,tick,axis,u);float d=std::exp(-.01f/truth.tau);rate=d*rate+(1-d)*shared_braking::gain(truth,360)*h.at_axis(tick-uint64_t(truth.delay*1000),axis);angle+=rate*.01f;previous=u;
 }
 printf("axis%d actual closedloop learning promotions%u deployments%u learned_control_frames%u\n",axis,learner.promotions(),served.deployments(),active);
 assert(learner.promotions()>0&&served.deployments()>0&&active>100);
}
int main(){
 setvbuf(stdout,nullptr,_IONBF,0);
 for(int i=0;i<3;++i){serving_test(i);learn_and_serve(i);}
 model_control::Controller c;model_control::Plan plan;plan.valid=true;plan.command=.5f;
 for(int i=0;i<60;++i)c.apply_full(plan,-.5f,.7f,6,.01f);assert(c.weight==1&&std::abs(c.actual-.5f)<1e-5);
 assert(std::abs(c.apply_full(plan,.7f,.7f,6,.01f)-.5f)<1e-5); // baseline cannot leak at full authority
 assert(c.apply_full(plan,8,.7f,6,NAN)==.7f);assert(c.fallback_full(NAN,.7f,6,.01f)==0);
 for(float damping:{2.f})for(int axis=0;axis<3;++axis){float worst=0,late=0;int failures=0,count=0;
  for(float g:{.65f,1.f,1.65f})for(float t:{.5f,1.f,2.f})for(float d:{-.03f,0.f,.06f}){
   auto m=cold_proxy(axis,g,t,d,damping);worst=std::max(worst,m.overshoot);late=std::max(late,m.late_peak);bool fail=m.late_peak>(axis==1?2.f:1.f);failures+=fail;++count;if(fail)printf("  mismatch gain%.2f tau%.2f delay%+.2f late%.4f\n",g,t,d,m.late_peak);
  }
  printf("damping%.2f axis%d cold standard mismatched proxies=%d maxovershoot=%.4f maxlateerror=%.4f unsettled=%d (simulation only)\n",damping,axis,count,worst,late,failures);
  assert(failures==0);
 }
 full_runtime();puts("PASS full-model tests; actual AC8 flight remains unverified");
}
