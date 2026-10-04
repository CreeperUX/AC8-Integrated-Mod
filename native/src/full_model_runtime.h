#pragma once
#include "model_serving.h"
#include "control_modes.h"
control_modes::Blend mode_blend;
std::array<adaptive_braking::FastRate,3> learning_rates;
adaptive_braking::History adaptive_history;std::mutex adaptive_history_mutex;
uintptr_t adaptive_owner=0,learning_pawn=0;int learning_plane=-1;
uint64_t adaptive_log_tick=0;std::array<uint64_t,3> learning_pause_until{};
std::atomic<bool> full_model_new_flight{true};
online_learning::Fleet learned_fleet;online_learning::Aircraft* learning_current=nullptr;
model_serving::Fleet serving_fleet;model_serving::Aircraft* serving_current=nullptr;
std::array<model_control::Controller,3> primary_controllers;
std::array<model_control::Reference,3> target_references;
float previous_roll_goal=0;bool have_roll_goal=false;
void observe_adaptive_input(uintptr_t pawn,float pitch,float roll,float yaw=0){
 if(!std::isfinite(pitch)||!std::isfinite(roll)||!std::isfinite(yaw))return;
 std::unique_lock<std::mutex> lock(adaptive_history_mutex,std::try_to_lock);
 if(lock.owns_lock()){if(adaptive_owner!=pawn){adaptive_history.clear();adaptive_owner=pawn;}adaptive_history.add({GetTickCount64(),pitch,roll,yaw});}
}
void interrupt_learning(){
 if(learning_current){learning_current->pitch.anchor_tick=0;learning_current->roll.anchor_tick=0;learning_current->yaw.anchor_tick=0;}
 if(serving_current)for(auto& s:serving_current->axes)s.interrupt();
}
void reset_model_assist(){
 mode_blend.reset(control_mode.load()!=0);control_mode_blend=mode_blend.value;
 reset_legacy_model_assist();for(auto& r:learning_rates)r.reset();for(auto& c:primary_controllers)c.reset();for(auto& r:target_references)r.reset();have_roll_goal=false;interrupt_learning();
}
void apply_model_control(float ep,float er,float ey,float wp,float wr,float wy,float rawp,float rawr,float rawy,
 float dt,float& pcmd,float& rcmd,float& ycmd,float roll_limit,const std::array<float,3>& references={}){
 auto now=GetTickCount64();auto pawn=aircraft.load();int plane=assist_plane_type.load();
 if(plane<0){reset_model_assist();return;}
 bool new_flight=full_model_new_flight.exchange(false);
 if(plane!=learning_plane||pawn!=learning_pawn||!learning_current||new_flight){
  reset_model_assist();learning_current=&learned_fleet.get(plane,now);learning_current->suspend();serving_current=&serving_fleet.get(plane,now);serving_current->new_flight();learning_plane=plane;learning_pawn=pawn;
 }
 AssistInput input;adaptive_braking::History history;bool read=false,have=false;
 {std::unique_lock<std::mutex> l(assist_input_mutex,std::try_to_lock);if(l.owns_lock()){input=assist_input;read=true;}}
 {std::unique_lock<std::mutex> l(adaptive_history_mutex,std::try_to_lock);if(l.owns_lock()&&adaptive_owner==pawn){history=adaptive_history;have=true;}}
 float speed=assist_speed.load();
 bool data_ready=model_assist_enabled.load()&&read&&have&&input.pawn==pawn&&now>=input.tick&&now-input.tick<100&&
  assist_speed_pawn.load()==pawn&&now>=assist_speed_tick.load()&&now-assist_speed_tick.load()<150&&
  std::isfinite(speed)&&std::isfinite(input.p)&&std::isfinite(input.r)&&std::isfinite(input.y)&&!game_paused.load()&&!gaze_active.load();
 std::array<int,3> manual_bits{1,4,2};
 for(int i=0;i<3;++i)if(read&&(input.manual&manual_bits[i]))learning_pause_until[i]=now+300;
 bool learn_ready=data_ready&&!input.manual&&!assist_environment_unsafe.load()&&assist_brake.load()<.15f;
 for(auto t:learning_pause_until)learn_ready=learn_ready&&now>=t;
 std::array<online_learning::Axis*,3> axes{&learning_current->pitch,&learning_current->roll,&learning_current->yaw};
 std::array<shared_braking::Model,3> standard{shared_braking::pitch,shared_braking::roll,shared_braking::yaw},models=standard;
 std::array<float,3> filtered{wp,wr,wy},raw{rawp,rawr,rawy},errors{ep,er,ey},previous{input.p,input.r,input.y},commands{pcmd,rcmd,ycmd};
 std::array<float,3> limits{.85f,1.f,.7f},max_rates{45.f,roll_limit,7.f},zones{.2f,1.f,.2f},slews{12.f,16.f,6.f};
 float blend=mode_blend.step(control_mode.load()!=0,dt);control_mode_blend=blend;
 std::array<float,3> authority{.85f+.15f*blend,1.f,.7f+.15f*blend};
 if(!learn_ready)interrupt_learning();
 for(int i=0;i<3;++i){
  auto& learner=*axes[i];auto& served=serving_current->axes[i];auto& control=primary_controllers[i];
  float rate=learning_rates[i].step(raw[i],filtered[i],dt);
  if(learn_ready){
   unsigned old=learner.promotions(),deployments=served.deployments();learner.observe(standard[i],i,now,rate,speed,history);
   auto candidate=learner.effective(standard[i],speed,dt);
   if(old!=learner.promotions()){served.candidate_check.reset();log_line("MODEL_CANDIDATE_PROMOTED plane=%d axis=%d total=%u; serving controller preserved",plane,i,learner.promotions());}
   served.observe(standard[i],candidate,learner.learned_weight,i,now,rate,speed,history);
   if(deployments!=served.deployments())log_line("MODEL_PARAMETERS_DEPLOYED plane=%d axis=%d total=%u",plane,i,served.deployments());
  }
  if(data_ready)models[i]=served.effective(standard[i],speed,config.model_assist_strength,dt);else models[i]=served.current_model(standard[i]);
  bool manual=read&&(input.manual&manual_bits[i]);model_control::Plan plan;
  if(data_ready&&!manual){
   float response=std::clamp(std::sqrt(served.gain_scale/served.tau_scale),.65f,1.35f);
   // Uncalibrated response needs extra terminal-rate damping. Keep a margin
   // even after validated parameters are blended in; never gate model control.
   float damping=2.f-.5f*served.mix;
   auto predict=[&](bool agile){return model_control::predict(models[i],errors[i],rate,previous[i],speed,limits[i],max_rates[i]*response,zones[i],history,now,i,references[i],damping,agile);};
   if(blend<=0)plan=predict(false);
   else if(blend>=1)plan=predict(true);
   else plan=control_modes::combine(predict(false),predict(true),blend);
   commands[i]=control.apply_full(plan,commands[i],authority[i],slews[i],dt);
  }else if(manual){control.follow_manual(previous[i],commands[i],authority[i]);commands[i]=std::clamp(previous[i],-authority[i],authority[i]);}
  else commands[i]=control.fallback_full(commands[i],authority[i],slews[i],dt);
  assist_pursuit[i]=plan.pursuit;assist_effective_damping[i]=plan.damping;assist_input_limit[i]=plan.input_limit;
  assist_primary_weight[i]=control.weight;assist_primary_command[i]=control.last.command;assist_primary_delta[i]=control.delta;
  assist_primary_stop[i]=control.last.stopping_angle;assist_primary_horizon[i]=control.last.horizon;assist_primary_ratecap[i]=control.last.rate_limit;assist_reference_rate[i]=references[i];assist_learn_delay[i]=models[i].delay;
  assist_primary_checks[i]=served.serving_check.count;assist_primary_model_rmse[i]=std::sqrt(served.serving_check.learned_error);assist_primary_base_rmse[i]=std::sqrt(served.serving_check.baseline_error);
  assist_candidate_weight[i]=learner.learned_weight;assist_model_source[i]=data_ready&&!manual&&plan.valid?served.source():-1;assist_deployed_models[i]=served.deployments();
 }
 pcmd=commands[0];rcmd=commands[1];ycmd=commands[2];assist_delta_pitch=assist_delta_roll=0;assist_profile=6;
 auto& p=serving_current->axes[0];auto& r=serving_current->axes[1];auto& y=serving_current->axes[2];
 assist_conf_pitch=axes[0]->confidence;assist_conf_roll=axes[1]->confidence;assist_conf_yaw=axes[2]->confidence;
 assist_gain_pitch=p.gain_scale;assist_gain_roll=r.gain_scale;assist_gain_yaw=y.gain_scale;
 assist_learn_tau_pitch=models[0].tau;assist_learn_tau_roll=models[1].tau;assist_learn_tau_yaw=models[2].tau;
 assist_learn_weight_pitch=p.mix;assist_learn_weight_roll=r.mix;assist_learn_weight_yaw=y.mix;
 assist_learn_promotions_pitch=axes[0]->promotions();assist_learn_promotions_roll=axes[1]->promotions();assist_learn_promotions_yaw=axes[2]->promotions();
 if(now-adaptive_log_tick>=10000){adaptive_log_tick=now;
  for(int i=0;i<3;++i){auto& served=serving_current->axes[i];auto& a=*axes[i];
   log_line("MODEL_FULL plane=%d axis=%d data=%d learning=%d source=%d primary=%.3f gain=%.3f tau=%.3f delay=%.3f learned_mix=%.3f candidate_mix=%.3f deployed=%u delta=%.4f",plane,i,data_ready?1:0,learn_ready?1:0,int(assist_model_source[i].load()),primary_controllers[i].weight,served.gain_scale,models[i].tau,models[i].delay,served.mix,a.learned_weight,served.deployments(),primary_controllers[i].delta);
   log_line("MODEL_PARAMETER_VALIDATION plane=%d axis=%d ready=%d checks=%u candidate_rmse=%.3f reference_rmse=%.3f",plane,i,served.candidate_check.usable?1:0,served.candidate_check.count,std::sqrt(served.candidate_check.learned_error),std::sqrt(served.candidate_check.baseline_error));
   log_line("MODEL_AGILITY plane=%d axis=%d pursuit=%.3f damping=%.3f plan_input_limit=%.3f ratecap=%.3f",plane,i,assist_pursuit[i].load(),assist_effective_damping[i].load(),assist_input_limit[i].load(),primary_controllers[i].last.rate_limit);
  }
 }
}
