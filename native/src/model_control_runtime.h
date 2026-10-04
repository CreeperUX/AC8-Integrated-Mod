#pragma once
#include "model_control.h"
std::array<adaptive_braking::FastRate,3> learning_rates;
adaptive_braking::History adaptive_history;
std::mutex adaptive_history_mutex;
uintptr_t adaptive_owner=0,learning_pawn=0;
int learning_plane=-1;
uint64_t adaptive_pause_until=0,adaptive_log_tick=0;
online_learning::Fleet learned_fleet;
online_learning::Aircraft* learning_current=nullptr;
std::array<model_control::Controller,3> primary_controllers;
std::array<model_control::Quality,3> primary_quality;
std::array<model_control::Reference,3> target_references;
float previous_roll_goal=0;bool have_roll_goal=false;
void observe_adaptive_input(uintptr_t pawn,float pitch,float roll,float yaw=0){
 std::unique_lock<std::mutex> lock(adaptive_history_mutex,std::try_to_lock);
 if(lock.owns_lock()){
  if(adaptive_owner!=pawn){adaptive_history.clear();adaptive_owner=pawn;}
  adaptive_history.add({GetTickCount64(),pitch,roll,yaw});
 }
}
void reset_learning_transients(){
 reset_legacy_model_assist();for(auto& r:learning_rates)r.reset();for(auto& q:primary_quality)q.reset();
 if(learning_current)learning_current->suspend();
 assist_gain_pitch=assist_gain_roll=assist_gain_yaw=1;
 assist_learn_weight_pitch=assist_learn_weight_roll=assist_learn_weight_yaw=0;
}
void reset_model_assist(){reset_learning_transients();for(auto& c:primary_controllers)c.reset();for(auto& r:target_references)r.reset();have_roll_goal=false;}
void apply_model_control(float ep,float er,float ey,float wp,float wr,float wy,float rawp,float rawr,float rawy,
                         float dt,float& pcmd,float& rcmd,float& ycmd,float roll_limit,const std::array<float,3>& reference_rates={}){
 int plane=assist_plane_type.load();auto now=GetTickCount64();auto pawn=aircraft.load();
 if(plane<0){reset_model_assist();return;}
 if(plane!=learning_plane||pawn!=learning_pawn||!learning_current){
  reset_model_assist();learning_current=&learned_fleet.get(plane,now);learning_current->suspend();learning_plane=plane;learning_pawn=pawn;
 }
 AssistInput input;adaptive_braking::History history;bool read=false,have_history=false;
 {std::unique_lock<std::mutex> lock(assist_input_mutex,std::try_to_lock);if(lock.owns_lock()){input=assist_input;read=true;}}
 {std::unique_lock<std::mutex> lock(adaptive_history_mutex,std::try_to_lock);if(lock.owns_lock()&&adaptive_owner==pawn){history=adaptive_history;have_history=true;}}
 if(read&&input.manual)adaptive_pause_until=now+300;
 bool allowed=model_assist_enabled.load()&&read&&have_history&&input.pawn==pawn&&now>=input.tick&&now-input.tick<100&&
  now>=adaptive_pause_until&&assist_speed_pawn.load()==pawn&&now>=assist_speed_tick.load()&&now-assist_speed_tick.load()<150&&
  !assist_environment_unsafe.load()&&assist_brake.load()<.15f&&!game_paused.load()&&!gaze_active.load();
 std::array<online_learning::Axis*,3> axes{&learning_current->pitch,&learning_current->roll,&learning_current->yaw};
 std::array<shared_braking::Model,3> models{shared_braking::pitch,shared_braking::roll,shared_braking::yaw};
 std::array<float,3> filtered{wp,wr,wy},raw{rawp,rawr,rawy},errors{ep,er,ey},previous{input.p,input.r,input.y};
 std::array<float,3> limits{.85f,1.f,.7f},max_rates{45.f,roll_limit,7.f},zones{.2f,1.f,.2f},slews{6.f,8.f,3.f};
 std::array<float,3> commands{pcmd,rcmd,ycmd},aux{};
 float speed=assist_speed.load();bool continuous=now>=750&&online_learning::continuous(history,now-750,now);
 if(!allowed)reset_learning_transients();
 for(int i=0;i<3;++i){
  auto& axis=*axes[i];auto& controller=primary_controllers[i];
  if(allowed){
   float rate=learning_rates[i].step(raw[i],filtered[i],dt);unsigned old=axis.promotions();
   auto standard=models[i];axis.observe(models[i],i,now,rate,speed,history);models[i]=axis.effective(models[i],speed,dt);
   if(old!=axis.promotions()){primary_quality[i].reset();log_line("MODEL_LEARN_PROMOTED plane=%d axis=%d total=%u (later validation passed)",plane,i,axis.promotions());}
   primary_quality[i].observe(models[i],standard,i,now,rate,speed,history);
   model_control::Plan plan;
   if(continuous){
    float response=std::clamp(std::sqrt(axis.gain_scale/axis.tau_scale),.65f,1.35f);
    plan=model_control::predict(models[i],errors[i],rate,previous[i],speed,limits[i],max_rates[i]*response,zones[i],history,now,i,reference_rates[i]);
   }
   commands[i]=controller.apply(plan,commands[i],primary_quality[i].usable?axis.learned_weight*std::clamp(config.model_assist_strength/.2f,0.f,1.f):0.f,axis.confidence,speed,limits[i],slews[i],dt);
   // Avoid stacking the old full auxiliary correction on top of model-led control.
   if(continuous&&i<2){
    aux[i]=axis.correction(models[i],errors[i],rate,previous[i],commands[i],speed,i==0?.35f:.25f,i==0?12.f:25.f,zones[i],config.model_assist_strength,i==0?.10f:.12f,i==0?1.f:1.5f,dt)*(1-controller.weight);
    commands[i]=std::clamp(commands[i]+aux[i],-limits[i],limits[i]);
   }
  }else commands[i]=controller.fallback(commands[i],limits[i],slews[i]*1.5f,dt);
  assist_primary_weight[i]=controller.weight;assist_primary_command[i]=controller.last.command;assist_primary_delta[i]=controller.delta;
  assist_primary_stop[i]=controller.last.stopping_angle;assist_primary_horizon[i]=controller.last.horizon;assist_primary_ratecap[i]=controller.last.rate_limit;
  assist_learn_delay[i]=models[i].delay;
  assist_primary_checks[i]=primary_quality[i].count;assist_primary_model_rmse[i]=std::sqrt(primary_quality[i].learned_error);assist_primary_base_rmse[i]=std::sqrt(primary_quality[i].baseline_error);
  assist_reference_rate[i]=reference_rates[i];
 }
 pcmd=commands[0];rcmd=commands[1];ycmd=commands[2];assist_delta_pitch=aux[0];assist_delta_roll=aux[1];assist_profile=3;
 auto& p=*axes[0];auto& r=*axes[1];auto& y=*axes[2];
 assist_conf_pitch=p.confidence;assist_conf_roll=r.confidence;assist_conf_yaw=y.confidence;
 assist_gain_pitch=p.gain_scale;assist_gain_roll=r.gain_scale;assist_gain_yaw=y.gain_scale;
 assist_learn_tau_pitch=models[0].tau;assist_learn_tau_roll=models[1].tau;assist_learn_tau_yaw=models[2].tau;
 assist_learn_weight_pitch=p.learned_weight;assist_learn_weight_roll=r.learned_weight;assist_learn_weight_yaw=y.learned_weight;
 assist_learn_promotions_pitch=p.promotions();assist_learn_promotions_roll=r.promotions();assist_learn_promotions_yaw=y.promotions();
 if(now-adaptive_log_tick>=10000){adaptive_log_tick=now;
  for(int i=0;i<3;++i){auto& a=*axes[i];int band=online_learning::nearest_band(speed);auto& b=a.banks[band];
   log_line("MODEL_CONTROL plane=%d axis=%d gate=%d speed=%.1f conf=%.3f gain=%.3f tau=%.3f delay=%.3f learned=%.3f primary=%.3f delta=%.4f stop_deg=%.2f horizon=%.3f phase=%s fit=%u val=%u promoted=%u",plane,i,allowed?1:0,speed,a.confidence,a.gain_scale,models[i].tau,models[i].delay,a.learned_weight,primary_controllers[i].weight,primary_controllers[i].delta,primary_controllers[i].last.stopping_angle,primary_controllers[i].last.horizon,b.validating?"validate":"train",b.stats.n,b.validation_count,a.promotions());
   log_line("MODEL_PRIMARY_VALIDATION plane=%d axis=%d ready=%d checks=%u rmse_model=%.3f rmse_base=%.3f",plane,i,primary_quality[i].usable?1:0,primary_quality[i].count,std::sqrt(primary_quality[i].learned_error),std::sqrt(primary_quality[i].baseline_error));
  }
 }
}
