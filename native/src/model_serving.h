#pragma once
#include "model_control.h"
namespace model_serving {
struct Cell {bool stored=false,confirmed=false;shared_braking::Model model{};unsigned deployments=0;};
struct Axis {
 std::array<Cell,4> cells{};
 model_control::Quality candidate_check,serving_check;
 uint64_t candidate_seen=0,serving_seen=0;
 int band=-1;unsigned bad_windows=0;
 float mix=0,gain_scale=1,tau_scale=1,delay_offset=0;
 float target_gain=1,target_tau=1,target_delay=0;
 void new_flight(){
  candidate_check.reset();serving_check.reset();candidate_seen=serving_seen=0;band=-1;bad_windows=0;
  mix=0;gain_scale=tau_scale=target_gain=target_tau=1;delay_offset=target_delay=0;
  for(auto& c:cells)c.confirmed=false;
 }
 void interrupt(){candidate_check.anchor=0;serving_check.anchor=0;}
 unsigned deployments()const{unsigned n=0;for(auto& c:cells)n+=c.deployments;return n;}
 void observe(shared_braking::Model standard,shared_braking::Model candidate,float candidate_weight,int axis,
              uint64_t now,float rate,float speed,const adaptive_braking::History& history){
  int current=online_learning::nearest_band(speed);
  if(current!=band){band=current;candidate_check.reset();serving_check.reset();candidate_seen=serving_seen=0;bad_windows=0;}
  auto& c=cells[band];
  if(c.stored){
   serving_check.observe(c.model,standard,axis,now,rate,speed,history);
   if(serving_check.evaluated_tick!=serving_seen){
    serving_seen=serving_check.evaluated_tick;
    if(serving_check.usable&&!c.confirmed){c.confirmed=true;candidate_check.reset();}
    float noise=axis==0?.8f:axis==1?2.f:.15f;
    bool worse=serving_check.count>=6&&serving_check.learned_error>1.25f*serving_check.baseline_error&&serving_check.learned_error>noise*noise;
    bad_windows=worse?bad_windows+1:0;
    if(bad_windows>=3){c.confirmed=false;bad_windows=0;candidate_check.reset();serving_check.reset();}
   }
  }
  // Test against the actual incumbent. A new candidate never evicts a working
  // deployed model merely because its own validation restarted.
  if(candidate_weight>.02f){
   candidate_check.observe(candidate,c.confirmed?c.model:standard,axis,now,rate,speed,history);
   if(candidate_check.usable&&candidate_check.evaluated_tick!=candidate_seen){
    candidate_seen=candidate_check.evaluated_tick;auto tested=candidate_check.tested_model;
    bool changed=!c.stored||std::abs(tested.g0-c.model.g0)>.001f||std::abs(tested.tau-c.model.tau)>.001f||std::abs(tested.delay-c.model.delay)>.001f;
    if(changed){c.model=tested;c.stored=c.confirmed=true;++c.deployments;serving_check.reset();candidate_check.reset();bad_windows=0;}
   }
  }else candidate_check.anchor=0;
 }
 shared_braking::Model effective(shared_braking::Model standard,float speed,float strength,float dt){
  auto& cell=cells[online_learning::nearest_band(speed)];bool use=cell.stored&&cell.confirmed;
  float desired=use?std::clamp(strength/.2f,0.f,1.f):0.f;
  if(use){target_gain=cell.model.g0/standard.g0;target_tau=cell.model.tau/standard.tau;target_delay=cell.model.delay-standard.delay;}
  mix+=std::clamp(desired-mix,-.75f*dt,.2f*dt);mix=std::clamp(mix,0.f,1.f);
  float g=1+mix*(target_gain-1),t=std::exp(mix*std::log(std::max(.2f,target_tau))),d=mix*target_delay;
  float slew=use?.15f:.5f;gain_scale+=std::clamp(g-gain_scale,-slew*dt,slew*dt);tau_scale+=std::clamp(t-tau_scale,-slew*dt,slew*dt);
  delay_offset+=std::clamp(d-delay_offset,-.05f*dt,.05f*dt);
  standard.g0*=gain_scale;standard.g1*=gain_scale;standard.tau*=tau_scale;standard.delay=std::clamp(standard.delay+delay_offset,0.f,.4f);return standard;
 }
 int source()const{return mix<=.001f&&std::abs(gain_scale-1)<.001f&&std::abs(tau_scale-1)<.001f&&std::abs(delay_offset)<.0001f?0:mix>.99f?1:2;}
 shared_braking::Model current_model(shared_braking::Model standard)const{standard.g0*=gain_scale;standard.g1*=gain_scale;standard.tau*=tau_scale;standard.delay=std::clamp(standard.delay+delay_offset,0.f,.4f);return standard;}
};
struct Aircraft {int id=-1;uint64_t touched=0;std::array<Axis,3> axes;void new_flight(){for(auto& a:axes)a.new_flight();}};
struct Fleet {
 std::array<Aircraft,64> planes{};
 Aircraft& get(int id,uint64_t now){for(auto& p:planes)if(p.id==id){p.touched=now;return p;}auto p=std::min_element(planes.begin(),planes.end(),[](auto& a,auto& b){return a.touched<b.touched;});*p={};p->id=id;p->touched=now;return *p;}
};
}
