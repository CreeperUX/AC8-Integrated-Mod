#pragma once
#include "flight_math.h"
#include "model_control.h"
#include "control_modes.h"
#include "terminal_intercept.h"
#include "capture_roll.h"
// Third policy only: convergence-aware leveling and bounded, slew-aware 3D search.
// This predicts attitude geometry, not aerodynamic sideslip or a physical envelope.
namespace capture_control {
inline float smooth(float x){x=std::clamp(x,0.f,1.f);return x*x*(3-2*x);}
// Local third-policy disturbance estimate. Only near-steady, low-input samples
// update it; it never modifies shared learned parameters or the old policies.
struct Trim {
 float value=0,previous=0,previous_input=0,steady=0,sum=0,filtered_input=0;bool seen=false,confirmed=false,input_seen=false;
 void reset(){*this=Trim{};}
 float step(shared_braking::Model m,float rate,float speed,float input,
  const adaptive_braking::History& history,uint64_t now,int axis,float dt,bool allowed,float filter_tau=1.f/30){
  if(!std::isfinite(rate)||!std::isfinite(input)||!std::isfinite(dt)||!std::isfinite(m.delay)||m.delay<0||m.delay>.5f||!std::isfinite(m.tau)||m.tau<.2f||m.tau>3.f||dt<=0||dt>.1f){reset();return 0;}
  float old_rate=previous,derivative=seen?(rate-previous)/dt:0,change=seen?std::abs(input-previous_input)/dt:1000;
  seen=true;previous=rate;previous_input=input;
  auto delay=uint64_t(m.delay*1000);float k=shared_braking::gain(m,std::clamp(speed,130.f,660.f));
  if(now<delay||!online_learning::continuous(history,now-delay,now)){steady=0;sum=0;return value;}
  float issued=history.at_axis(now-delay,axis),old_filtered=filtered_input;
  if(!input_seen){filtered_input=issued;old_filtered=issued;input_seen=true;}
  else filtered_input+=(issued-filtered_input)*(filter_tau>0?1-std::exp(-dt/filter_tau):1);
  if(!allowed||std::abs(derivative)>1.5f||change>.7f||std::abs(input)>.25f||!std::isfinite(k)){steady=0;sum=0;return value;}
  float residual=m.tau*derivative+.5f*(rate+old_rate)-k*.5f*(filtered_input+old_filtered);
  float bound=axis==0?5.f:1.f;
  if(!std::isfinite(residual)||std::abs(residual)>bound){steady=0;sum=0;return value;}
  steady+=dt;sum+=residual*dt;
  if(steady>.15f){if(!confirmed){value=sum/steady;confirmed=true;}else value+=(residual-value)*(1-std::exp(-dt/.25f));}
  value=std::clamp(value,-bound,bound);return value;
 }
};
using Schedule=capture_roll::Output;
inline Schedule schedule(capture_roll::Guidance& state,const capture_roll::Input& input){return state.update(input);}
inline model_control::Plan axis_plan(shared_braking::Model m,float error,float rate,float previous,float speed,
 float limit,float max_rate,float zone,const adaptive_braking::History& history,uint64_t now,int axis,float reference,float damping,float dt=.02f,float bias=0,bool leveling=false){
 auto normal=model_control::predict(m,error,rate,previous,speed,limit,max_rate,zone,history,now,axis,reference,damping,true);
 if(!normal.valid)return normal;
 if(axis==1){
  // A level-recovery speed budget must not be amplified as far-target pursuit.
  if(leveling)return model_control::predict(m,error,rate,previous,speed,limit,max_rate,.3f,history,now,axis,0,1.35f,false);
  return normal;
 }
 float weight=1-smooth((std::abs(error)-12.f)/12.f);
 if(weight<=0)return normal;
 auto intercept=terminal_intercept::predict(m,error,rate-bias,previous,speed,limit,max_rate,.025f,history,now,axis,reference-bias,axis==0?12.f:6.f,dt);
 if(!intercept.valid)return normal;
 if(intercept.damping==0){intercept.stopping_angle+=bias*intercept.horizon;intercept.rate_limit+=std::abs(bias);}
 return control_modes::combine(normal,intercept,weight);
}
inline flight::V rotate(flight::V v,flight::V omega,float dt){
 float n=std::sqrt(flight::dot(omega,omega));if(n<1e-7f)return v;
 auto axis=omega*(1/n);float a=n*dt*flight::rad,c=std::cos(a),s=std::sin(a);
 return v*c+flight::cross(axis,v)*s+axis*(flight::dot(axis,v)*(1-c));
}

struct Result {bool valid=false;std::array<float,3> command{};float before=0,after=0;};
inline Result refine(const std::array<shared_braking::Model,3>& models,const std::array<float,3>& rates,
 const std::array<float,3>& previous,const std::array<float,3>& baseline,const std::array<float,3>& limits,
 flight::V goal,float roll_error,const std::array<float,3>& references,float speed,
 const adaptive_braking::History& history,uint64_t now,float frame_dt,float mix,
 const std::array<float,3>& targets={NAN,NAN,NAN},const std::array<float,3>& bias={},flight::V world_up={0,0,1},bool leveling=false){
 Result out;out.command=baseline;
 if(!std::isfinite(frame_dt)||frame_dt<=0||frame_dt>.1f||!std::isfinite(speed)||!std::isfinite(roll_error)||
    !std::isfinite(goal.x)||!std::isfinite(goal.y)||!std::isfinite(goal.z)||!std::isfinite(world_up.x)||!std::isfinite(world_up.y)||!std::isfinite(world_up.z)||(leveling&&flight::dot(world_up,world_up)<.5f)||goal.x<.94f||mix<=0)return out;
 const float initial_clearance=std::hypot(world_up.y,world_up.z);
 if(leveling&&initial_clearance<=.05f)return out;
 float angle=std::acos(std::clamp(goal.x,-1.f,1.f))/flight::rad;
 if(angle<.36f&&!leveling)return out;
 mix*=1-smooth((angle-8.f)/12.f);if(mix<=0)return out;
 for(int i=0;i<3;++i)if(!std::isfinite(rates[i])||!std::isfinite(previous[i])||!std::isfinite(baseline[i])||!std::isfinite(references[i])||
  !std::isfinite(models[i].tau)||models[i].tau<.2f||models[i].tau>3.f||!std::isfinite(models[i].delay)||models[i].delay<0||models[i].delay>.5f||
  !std::isfinite(limits[i])||limits[i]<=0||!std::isfinite(shared_braking::gain(models[i],speed)))return out;
 std::array<float,3> slews{12,16,6},desired=targets;
 for(int i=0;i<3;++i)if(!std::isfinite(desired[i]))desired[i]=baseline[i];
 const float hold=std::clamp(3*frame_dt,.17f,.24f);
 std::array<terminal_intercept::DelayedState,3> delayed{};
 for(int i=0;i<3;++i){delayed[i]=terminal_intercept::delayed(models[i],rates[i]-bias[i],speed,history,now,i);if(!delayed[i].valid)return out;}
 struct Candidate {bool valid=false;std::array<float,3> first{};std::array<terminal_intercept::Profile,3> profile{};float end=0;};
 auto trajectory=[&](const std::array<float,3>& first){
  Candidate trial;trial.first=first;
  for(int i=0;i<3;++i){
   float k=shared_braking::gain(models[i],std::clamp(speed,130.f,660.f));
   
   float after=float(terminal_intercept::ramp_step(delayed[i].rate-(references[i]-bias[i]),previous[i],first[i],frame_dt,k,models[i].tau,references[i]-bias[i]).rate+references[i]-bias[i]);
   float wanted=std::clamp(desired[i]+first[i]-baseline[i],-limits[i],limits[i]);
   trial.profile[i]=terminal_intercept::profile(models[i],after,first[i],wanted,speed,limits[i],slews[i],hold-frame_dt,references[i]-bias[i]);
   if(!trial.profile[i].valid)return trial;
   trial.end=std::max(trial.end,models[i].delay+frame_dt+float(trial.profile[i].duration));
  }
  if(!std::isfinite(trial.end)||trial.end>4)return trial;
  trial.valid=true;return trial;
 };
 struct Score{float cost=INFINITY,peak=INFINITY,terminal=INFINITY;};
 const float current_bank=std::atan2(world_up.y,world_up.z)/flight::rad;
 const float target_bank=leveling?0.f:std::remainder(current_bank-roll_error,360.f);
 auto score=[&](const Candidate& trial,int scenario)->Score{
  if(!trial.valid)return {};
  auto plant=models;for(auto& m:plant){float g=scenario==1?.85f:scenario==2?1.15f:1;m.g0*=g;m.g1*=g;m.tau*=scenario==1?1.2f:scenario==2?.85f:1;}
  auto w=rates;flight::Basis b{{1,0,0},{0,1,0},{0,0,1}};auto target=flight::unit(goal);
  const int steps=std::max(16,int(std::ceil(trial.end/.02f)));float dt=trial.end/steps;
  std::array<float,3> decay{},gain{};for(int i=0;i<3;++i){decay[i]=std::exp(-dt/plant[i].tau);gain[i]=shared_braking::gain(plant[i],std::clamp(speed,130.f,660.f));}
  float cost=0,roll_travel=0,peak=0;
  for(int j=0;j<steps;++j){auto before=w;
   for(int i=0;i<3;++i){float t=(j+.5f)*dt-models[i].delay,u=0;
    if(t<0){auto delay=uint64_t(models[i].delay*1000);u=history.at_axis(now-delay+uint64_t((j+.5f)*dt*1000),i);}
    else if(t<frame_dt)u=previous[i]+(trial.first[i]-previous[i])*t/frame_dt;
    else u=float(terminal_intercept::input_at(trial.profile[i],t-frame_dt));
    w[i]=decay[i]*w[i]+(1-decay[i])*(gain[i]*u+bias[i]);
   }
   auto omega=b.f*(-.5f*(before[1]+w[1]))+b.r*(-.5f*(before[0]+w[0]))+b.u*(.5f*(before[2]+w[2]));
   b.f=capture_control::rotate(b.f,omega,dt);b.r=capture_control::rotate(b.r,omega,dt);b.u=capture_control::rotate(b.u,omega,dt);
   target=capture_control::rotate(target,{0,-references[0],references[2]},dt);
   float err=std::acos(std::clamp(flight::dot(b.f,target),-1.f,1.f))/flight::rad;
   peak=std::max(peak,err);
   float length=std::hypot(target.y,target.z),ay=length>1e-5f?target.y/length:0,az=length>1e-5f?target.z/length:0;
   float cross=(b.f.y*az-b.f.z*ay)/flight::rad;
   float crossing=std::max(0.f,(b.f.y-target.y)*ay+(b.f.z-target.z)*az)/flight::rad;
   cost+=(err*err+8*cross*cross+5*crossing*crossing)*dt;
   roll_travel+=.5f*(before[1]+w[1])*dt;
  }
  float terminal=std::acos(std::clamp(flight::dot(b.f,target),-1.f,1.f))/flight::rad;
  // Evaluate the planned stop, not constant-input drift or an arbitrary low final speed.
  float bank=std::atan2(flight::dot(b.r,world_up),flight::dot(b.u,world_up))/flight::rad;
  float bank_error=std::remainder(bank-target_bank,360.f);
  float final_clearance=std::hypot(flight::dot(b.r,world_up),flight::dot(b.u,world_up));
  float bank_weight=(leveling?.004f:.0001f)*smooth((std::min(initial_clearance,final_clearance)-.05f)/.15f);
  cost+=3*terminal*terminal+bank_weight*bank_error*bank_error;
  for(int i=0;i<3;++i)cost+=.03f*std::pow(trial.first[i]-baseline[i],2);
  return {cost,peak,terminal};
 };
 auto aggregate=[&](const Candidate& trial){return score(trial,0).cost+.15f*(score(trial,1).cost+score(trial,2).cost);};
 auto base=trajectory(baseline);float best=aggregate(base);if(!std::isfinite(best))return out;
 const auto base_score=score(base,0);out.before=out.after=base_score.cost;
 std::array<float,3> radius{.15f,.12f,.10f};
 if(std::abs(std::atan2(goal.z,goal.x)/flight::rad)<.36f)radius[0]=0;
 if(std::abs(std::atan2(goal.y,goal.x)/flight::rad)<.08f)radius[2]=0;
 for(int p=-1;p<=1;++p)for(int r=-1;r<=1;++r)for(int y=-1;y<=1;++y){
  auto first=baseline;std::array<int,3> direction{p,r,y};
  for(int i=0;i<3;++i){float lo=std::max(-limits[i],previous[i]-slews[i]*frame_dt),hi=std::min(limits[i],previous[i]+slews[i]*frame_dt);if(lo>hi)return out;first[i]=std::clamp(first[i]+direction[i]*radius[i]*mix,lo,hi);}
  auto trial=trajectory(first);float value=aggregate(trial);auto nominal_score=score(trial,0);float nominal=nominal_score.cost;
  // Level recovery may improve bank, but cannot buy it with a large pointing excursion.
  if(leveling&&(nominal_score.peak>std::max(angle+.75f,base_score.peak+.20f)||nominal_score.terminal>base_score.terminal+.25f))continue;
  if(std::isfinite(value)&&value<best-1e-6f&&nominal<=out.before+1e-5f){best=value;out.command=first;out.after=nominal;}
 }
 out.valid=true;return out;
}
}
