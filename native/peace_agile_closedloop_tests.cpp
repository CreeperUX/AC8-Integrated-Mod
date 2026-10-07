#include "src/peace_agile_control.h"
#include <cassert>
#include <cstdio>
struct Metrics {float settle=20,level=20,t90=20,peak_error=0,crossing=0,path=0,final_error=0,final_bank=0;};
static flight::V rotate(flight::V v,flight::V omega){
 double n=std::sqrt(double(flight::dot(omega,omega)));if(n<1e-12)return v;
 auto axis=omega*float(1/n);double a=n*flight::rad,c=std::cos(a),s=std::sin(a);
 return v*float(c)+flight::cross(axis,v)*float(s)+axis*float(flight::dot(axis,v)*(1-c));
}
static Metrics fly(int policy,float bank,float gp,float gy,int plant,int hz=60,int scenario=0){
 std::array<shared_braking::Model,3> models{shared_braking::Model{1.2f,.18f,40,0,0},{1.2f,.12f,110,0,0},{.7f,.02f,8,0,0}};
 float scales[]{.75f,1.f,1.4f},taus[]{1.2f,1.f,.8f};
 for(auto& m:models){m.g0*=scales[plant];m.tau*=taus[plant];}
 auto truth=models; // Causal command delay, signed authority and rate saturation below are integrated independently.
 float negative=models[0].g0*.35f,dt=1.f/hz;auto body=flight::basis(0,0,bank);auto target=flight::basis(gp,gy,0).f;
 auto normal=flight::unit(flight::cross(body.f,target));std::array<float,3> rates{},previous{},limits{1,1,.85f},slews{12,16,6};
 adaptive_braking::History h;for(uint64_t t=1000;t<=2000;t+=5)h.add({t,0,0,0});flight::LevelBlend level;model_control::Reference roll_ref;float last_roll_goal=0;bool roll_seen=false;
 if(scenario==1)rates={-8,80,3};if(scenario==2)rates={12,-80,-2};
 float initial=std::acos(std::clamp(flight::dot(body.f,target),-1.f,1.f))/flight::rad;
 Metrics out;int pointing_stable=0,level_stable=0;
 for(int n=0;n<hz*16;++n){
  auto old_target=target;
  if(scenario==3){float tm=std::min(n*dt,4.f);target=flight::basis(gp+3*std::sin(.5f*tm),gy+4*std::sin(.4f*tm),0).f;normal=flight::unit(flight::cross({1,0,0},target));}
  uint64_t now=2000+uint64_t(std::llround(n*dt*1000));auto goal=flight::V{flight::dot(body.f,target),flight::dot(body.r,target),flight::dot(body.u,target)};std::array<float,3> cmd{};
  float error=std::atan2(std::sqrt(flight::dot(flight::cross(body.f,target),flight::cross(body.f,target))),flight::dot(body.f,target))/flight::rad,bank_error=std::atan2(body.r.z,body.u.z)/flight::rad;

  float mix=level.step(error,dt),turn=std::clamp(std::atan2(goal.y,std::max(.12f,goal.z))/flight::rad,-90.f,90.f);
  std::array<float,3> errors{std::atan2(goal.z,goal.x)/flight::rad,bank_error*(1-mix)+turn*mix,std::atan2(goal.y,goal.x)/flight::rad};
  std::array<float,3> caps{45,70+70*mix,7},zones{.2f,1,.2f},lim{.85f,1,.7f},references{};
  auto target_motion=flight::cross(old_target,target)*(1/dt/flight::rad);references[0]=-flight::dot(target_motion,body.r);references[2]=flight::dot(target_motion,body.u);
  float roll_goal=std::remainder(-bank_error+errors[1],360.f);
  references[1]=roll_ref.update(roll_seen?std::remainder(roll_goal-last_roll_goal,360.f)/dt:0,std::abs(flight::pitch(body.f))<60,dt,caps[1]);last_roll_goal=roll_goal;roll_seen=true;
  auto context=peace_agile::coordinate(goal,rates,references,std::max(models[0].delay+.3f*models[0].tau,models[2].delay+.3f*models[2].tau));
  std::array<model_control::Plan,3> peace{},selected{};
  for(int i=0;i<3;++i){peace[i]=model_control::predict(models[i],errors[i],rates[i],previous[i],400,lim[i],caps[i],zones[i],h,now,i,references[i],1.5f,false);
   auto agile=model_control::predict(models[i],errors[i],rates[i],previous[i],400,lim[i],caps[i],zones[i],h,now,i,references[i],1.5f,true);
   selected[i]=policy==0?peace[i]:policy==1?agile:peace_agile::select(peace[i],agile,context,i,errors[i],1).plan;
  }
  if(policy==2){peace_agile::JointInput in;in.models=models;in.rates=rates;in.previous=previous;in.errors=errors;in.references=references;in.caps=caps;in.damping={1.5f,1.5f,1.5f};in.goal=goal;in.dt=dt;in.now=now;selected=peace_agile::choose(in,h,peace,selected,1).plans;}
  for(int i=0;i<3;++i){assert(selected[i].valid);cmd[i]=std::clamp(selected[i].command,previous[i]-slews[i]*dt,previous[i]+slews[i]*dt);}
  for(int i=0;i<3;++i){assert(std::isfinite(cmd[i])&&std::abs(cmd[i])<=limits[i]+1e-4f&&std::abs(cmd[i]-previous[i])<=slews[i]*dt+1e-4f);}
  h.add({now,cmd[0],cmd[1],cmd[2]});previous=cmd;
  int steps=int(std::ceil(dt/.001f));float ds=dt/steps;
  for(int j=0;j<steps;++j){std::array<float,3> travel{};for(int i=0;i<3;++i){float at=float(now)+j*ds*1000-truth[i].delay*1000;float u=h.at_axis(uint64_t(std::max(0.f,at)),i),gain=i==0&&u<0?negative:truth[i].g0,eq=gain*u;
    if(i==1)eq=std::clamp(eq,-62.f*scales[plant],62.f*scales[plant]);float decay=std::exp(-ds/truth[i].tau);travel[i]=eq*ds+(rates[i]-eq)*truth[i].tau*(1-decay);rates[i]=eq+(rates[i]-eq)*decay;}
   auto rot=body.f*(-travel[1])+body.r*(-travel[0])+body.u*travel[2];body.f=rotate(body.f,rot);body.r=rotate(body.r,rot);body.u=rotate(body.u,rot);
  }
  error=std::atan2(std::sqrt(flight::dot(flight::cross(body.f,target),flight::cross(body.f,target))),flight::dot(body.f,target))/flight::rad;bank_error=std::atan2(body.r.z,body.u.z)/flight::rad;out.peak_error=std::max(out.peak_error,error);
  float remaining=std::atan2(flight::dot(normal,flight::cross(body.f,target)),flight::dot(body.f,target))/flight::rad;
  out.crossing=std::max(out.crossing,-remaining);out.path=std::max(out.path,std::abs(std::asin(std::clamp(flight::dot(body.f,normal),-1.f,1.f))/flight::rad));out.final_error=error;out.final_bank=bank_error;
  if(error<.1f*initial&&out.t90==20)out.t90=(n+1)*dt;
  if(error<.3f&&std::hypot(rates[0],rates[2])<.5f){if(++pointing_stable>=hz/5&&out.settle==20)out.settle=(n+1)*dt-.2f;}else pointing_stable=0;
  if(std::abs(bank_error)<5&&std::abs(rates[1])<4){if(++level_stable>=hz/5&&out.level==20)out.level=(n+1)*dt-.2f;}else level_stable=0;
 }
 return out;
}

int main(){setvbuf(stdout,nullptr,_IONBF,0);
 for(int plant:{0,1,2})for(int hz:{30,60,120})for(auto goal:{std::array<float,3>{0,1,0}, {0,30,0},{90,2,1},{-90,2,-1},{60,20,15},{-60,20,-15},{0,-3,3},{0,-3,-3},{0,-5,5},{0,-5,-5},{0,-6,6},{0,-6,-6},{0,-8,8},{0,-8,-8},{0,8,8},{0,8,-8},{30,-6,4},{-30,-6,-4}}){
  auto w=fly(2,goal[0],goal[1],goal[2],plant,hz),p=fly(0,goal[0],goal[1],goal[2],plant,hz),a=fly(1,goal[0],goal[1],goal[2],plant,hz);
  printf("HERITAGE plant=%d hz=%d bank=%.0f gp=%.0f gy=%.0f peace_t90=%.3f war_t90=%.3f peace_time=%.3f agile_time=%.3f war_time=%.3f peace_level=%.3f war_level=%.3f peace_path=%.3f agile_path=%.3f war_path=%.3f peace_cross=%.3f agile_cross=%.3f war_cross=%.3f final=%.3f bank_final=%.3f\n",plant,hz,goal[0],goal[1],goal[2],p.t90,w.t90,p.settle,a.settle,w.settle,p.level,w.level,p.path,a.path,w.path,p.crossing,a.crossing,w.crossing,w.final_error,w.final_bank);
  assert(w.final_error<.5f&&std::abs(w.final_bank)<5.f);
  assert(w.path<=p.path+.35f&&w.crossing<=p.crossing+.35f);
  if(goal[1]>=0&&goal[1]<=2)assert(std::abs(w.settle-p.settle)<1e-5f&&std::abs(w.level-p.level)<1e-5f);
  if(goal[0]==0&&goal[1]>=20)assert(w.t90<p.t90);
 }
 for(int plant:{0,1,2})for(int scenario:{1,2,3})for(int hz:{30,60,120}){
  auto w=fly(2,25,15,12,plant,hz,scenario),p=fly(0,25,15,12,plant,hz,scenario);
  printf("DYNAMIC plant=%d scenario=%d hz=%d peace_time=%.3f war_time=%.3f peace_path=%.3f war_path=%.3f peace_cross=%.3f war_cross=%.3f final=%.3f bank_final=%.3f\n",plant,scenario,hz,p.settle,w.settle,p.path,w.path,p.crossing,w.crossing,w.final_error,w.final_bank);
  assert(w.final_error<.5f&&std::abs(w.final_bank)<5.f);
  assert(w.path<=p.path+.35f&&w.crossing<=p.crossing+.35f);
 }
 puts("PASS 189 independently integrated 3-axis signed/saturated cases: PEACE near/roll parity, bounded path/overshoot and faster aligned pursuit");
}
