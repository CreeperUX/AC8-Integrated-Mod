#pragma once
#include "model_serving.h"
#include "control_modes.h"
#include "peace_agile_control.h"
#include "war11_control.h"
#include "capture_control.h"
#include "capture_adaptation.h"
#include "capture_rate.h"
#include "robust_intercept.h"
#include "capture_joint.h"
capture_adaptation::Fleet capture_fleet;capture_adaptation::Aircraft* capture_aircraft=nullptr;
std::array<capture_rate::Endpoint,3> capture_endpoints;
control_modes::Blend capture_blend{0};
capture_roll::Guidance capture_roll_guidance;
std::array<capture_control::Trim,3> capture_trims;std::array<float,3> capture_bias{};
control_modes::Blend mode_blend{0};
std::array<adaptive_braking::FastRate,3> learning_rates;
adaptive_braking::History adaptive_history;std::mutex adaptive_history_mutex;
uintptr_t adaptive_owner=0,learning_pawn=0;int learning_plane=-1;
uint64_t adaptive_log_tick=0;std::array<uint64_t,3> learning_pause_until{};
std::atomic<bool> full_model_new_flight{true};
online_learning::Fleet learned_fleet;online_learning::Aircraft* learning_current=nullptr;
model_serving::Fleet serving_fleet;model_serving::Aircraft* serving_current=nullptr;
std::array<model_control::Controller,3> primary_controllers;
std::array<model_control::Reference,3> target_references;
war11::PathAnchor war_anchor; // WAR v11.1 anchored target plane (world frame)
float previous_roll_goal=0;bool have_roll_goal=false;
#include "vector_runtime.h"
void observe_adaptive_input(uintptr_t pawn,float pitch,float roll,float yaw=0){
 if(!std::isfinite(pitch)||!std::isfinite(roll)||!std::isfinite(yaw))return;
 std::unique_lock<std::mutex> lock(adaptive_history_mutex,std::try_to_lock);
 if(lock.owns_lock()){if(adaptive_owner!=pawn){adaptive_history.clear();adaptive_owner=pawn;}adaptive_history.add({GetTickCount64(),pitch,roll,yaw});}
}
void interrupt_learning(){
 if(learning_current){learning_current->pitch.anchor_tick=0;learning_current->roll.anchor_tick=0;learning_current->yaw.anchor_tick=0;}
 if(serving_current)for(auto& s:serving_current->axes)s.interrupt();
}
void reset_capture_roll_state(){
 capture_roll_guidance.reset();capture_level_phase=0;capture_level_weight=0;capture_cross_rate=0;
 capture_relative_pointing_rate=0;capture_roll_limit_cause=0;capture_joint_delta=0;capture_joint_roll_delta=0;
}
void reset_model_assist(){
 for(auto& d:heritage_diagnostics)d=0;
 reset_vector_policy();war_anchor.reset();
 capture_blend.reset(control_mode.load()==2);capture_mode_weight=capture_blend.value;reset_capture_roll_state();for(auto& t:capture_trims)t.reset();capture_bias={};for(auto& r:capture_endpoints)r.reset();if(capture_aircraft)capture_aircraft->axes[0].anchors=capture_aircraft->axes[1].anchors=capture_aircraft->axes[2].anchors={};for(auto& d:capture_model_diagnostics)d=0;
 mode_blend.reset(control_mode.load()!=0);control_mode_blend=mode_blend.value;
 reset_legacy_model_assist();for(auto& r:learning_rates)r.reset();for(auto& c:primary_controllers)c.reset();for(auto& r:target_references)r.reset();have_roll_goal=false;interrupt_learning();
}
void apply_model_control(float ep,float er,float ey,float wp,float wr,float wy,float rawp,float rawr,float rawy,
 float dt,float& pcmd,float& rcmd,float& ycmd,float roll_limit,const std::array<float,3>& references={},flight::V goal={1,0,0},flight::V world_up={0,0,1},flight::Basis body={{1,0,0},{0,1,0},{0,0,1}},float target_jump=0){
 auto now=GetTickCount64();auto pawn=aircraft.load();int plane=assist_plane_type.load();
 if(plane<0){reset_model_assist();return;}
 bool new_flight=full_model_new_flight.exchange(false);
 if(plane!=learning_plane||pawn!=learning_pawn||!learning_current||new_flight){
  reset_model_assist();learning_current=&learned_fleet.get(plane,now);learning_current->suspend();serving_current=&serving_fleet.get(plane,now);serving_current->new_flight();learning_plane=plane;learning_pawn=pawn;capture_aircraft=&capture_fleet.get(plane,now);capture_aircraft->suspend();vector_aircraft=&vector_fleet.get(plane,now);vector_aircraft->suspend();
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
 if(read&&input.manual)reset_capture_roll_state();
 bool learn_ready=data_ready&&!input.manual&&!assist_environment_unsafe.load()&&assist_brake.load()<.15f;
 for(auto t:learning_pause_until)learn_ready=learn_ready&&now>=t;
 std::array<online_learning::Axis*,3> axes{&learning_current->pitch,&learning_current->roll,&learning_current->yaw};
 std::array<shared_braking::Model,3> standard{shared_braking::pitch,shared_braking::roll,shared_braking::yaw},models=standard;
 std::array<float,3> filtered{wp,wr,wy},raw{rawp,rawr,rawy},errors{ep,er,ey},previous{input.p,input.r,input.y},commands{pcmd,rcmd,ycmd};
 std::array<float,3> limits{.85f,1.f,.7f},max_rates{45.f,roll_limit,7.f},zones{.2f,1.f,.2f},slews{12.f,16.f,6.f};
 std::array<float,3> capture_rates{},capture_targets{};
 std::array<shared_braking::Model,3> capture_models=standard,capture_priors=standard;
 for(auto& m:capture_priors)m.bias=0; // Fixed identification reference, never moving shared-serving parameters.
 std::array<robust_intercept::Result,3> capture_plans{};
 std::array<robust_intercept::Envelope,3> capture_envelopes{};
 for(auto& d:capture_model_diagnostics)d=0;
 float blend=mode_blend.step(control_mode.load()!=0,dt);control_mode_blend=blend;
 std::array<float,3> authority{.85f+.15f*blend,1.f,.7f+.15f*blend};
 // Snapshot the same independently validated PEACE models for the extra-
 // authority gate. The per-axis predictions below use the served model.
 float pointing_response=.1f;
 for(int axis:{0,2}){auto m=serving_current->axes[axis].current_model(standard[axis]);pointing_response=std::max(pointing_response,m.delay+.3f*m.tau);}
 auto coordination=data_ready&&!input.manual?peace_agile::coordinate(goal,filtered,references,pointing_response):peace_agile::Coordination{};
 std::array<float,3> boost{},planning_rates{},planning_caps{},planning_damping{};
 std::array<model_control::Plan,3> peace_plans{},plans{};
 std::array<float,3> sources{},reasons{};
 bool heritage_valid=data_ready&&!input.manual;
 // Reason bitmask: 1 disabled, 2 observation/history ownership, 4 input age,
 // 8 speed age/value, 16 pause/gaze, 32 manual, 64 invalid prediction.
 unsigned data_reason=(!model_assist_enabled.load()?1u:0u)|(!read||!have||input.pawn!=pawn?2u:0u)|
  (now<input.tick||now-input.tick>=100?4u:0u)|
  (assist_speed_pawn.load()!=pawn||now<assist_speed_tick.load()||now-assist_speed_tick.load()>=150||!std::isfinite(speed)?8u:0u)|
  (game_paused.load()||gaze_active.load()?16u:0u);
 if(!std::isfinite(input.p)||!std::isfinite(input.r)||!std::isfinite(input.y))data_reason|=2u;

 if(!learn_ready)interrupt_learning();
 for(int i=0;i<3;++i){
  auto& learner=*axes[i];auto& served=serving_current->axes[i];auto& control=primary_controllers[i];
  float rate=learning_rates[i].step(raw[i],filtered[i],dt);planning_rates[i]=rate;
  if(learn_ready){
   unsigned old=learner.promotions(),deployments=served.deployments();learner.observe(standard[i],i,now,rate,speed,history);
   auto candidate=learner.effective(standard[i],speed,dt);
   if(old!=learner.promotions()){served.candidate_check.reset();log_line("MODEL_CANDIDATE_PROMOTED plane=%d axis=%d total=%u; serving controller preserved",plane,i,learner.promotions());}
   served.observe(standard[i],candidate,learner.learned_weight,i,now,rate,speed,history);
   if(deployments!=served.deployments())log_line("MODEL_PARAMETERS_DEPLOYED plane=%d axis=%d total=%u",plane,i,served.deployments());
  }
  if(data_ready)models[i]=served.effective(standard[i],speed,config.model_assist_strength,dt);else models[i]=served.current_model(standard[i]);
  bool manual=read&&(input.manual&manual_bits[i]);model_control::Plan plan;
  float endpoint=capture_endpoints[i].step(raw[i],dt,capture_priors[i],speed);
  auto& adaptation=capture_aircraft->axes[i];
  if(data_ready){
   bool independent=learn_ready&&capture_endpoints[i].qualified&&capture_endpoints[i].noise<std::max(i==0?.3f:i==1?1.f:.12f,.04f*shared_braking::gain(capture_priors[i],speed));
   for(int j=0;j<3;++j)if(j!=i)independent=independent&&std::abs(previous[j])<=.35f;
   auto before=adaptation.promotions();
   adaptation.observe(capture_priors[i],i,now,endpoint,speed,history,independent);
   capture_models[i]=adaptation.effective(capture_priors[i],speed,dt);
   if(adaptation.promotions()!=before)log_line("CAPTURE_MODEL_DEPLOYED plane=%d axis=%d accepted=%u rejections=%u",plane,i,adaptation.promotions(),adaptation.rejections());
  }else {adaptation.anchors={};capture_models[i]=adaptation.current(capture_priors[i],speed);}
  auto risk=adaptation.uncertainty(speed,endpoint,.35f);
  float rate_noise=capture_endpoints[i].noise;
  if(!capture_endpoints[i].qualified){rate_noise=std::max(rate_noise,std::max(i==0?.5f:i==1?2.f:.2f,std::abs(endpoint-raw[i])));risk.gain_fraction=std::max(risk.gain_fraction,.25f);risk.tau_fraction=std::max(risk.tau_fraction,.35f);risk.delay_seconds=std::max(risk.delay_seconds,.05f);}
  risk.bias_rate=std::max(risk.bias_rate,rate_noise);
  risk.delay_seconds=std::min(.09f,risk.delay_seconds+rate_noise/std::max(1.f,capture_models[i].g0)*.15f);
  capture_envelopes[i]={risk.gain_fraction,risk.tau_fraction,risk.delay_seconds,risk.bias_rate,risk.trust};
  // Cold trim and learned TOTAL bias are blended, never added twice.
  if(data_ready&&i!=1){
   float trim=capture_trims[i].step(capture_models[i],endpoint,speed,previous[i],history,now,i,dt,learn_ready&&std::hypot(ep,ey)<1.5f&&std::abs(rawr)<3.f,0);
   capture_models[i].bias+=(1-adaptation.model_weight)*trim;
  }
  capture_bias[i]=capture_models[i].bias;
  if(data_ready)adaptation.served(now,capture_models[i]);
  capture_rates[i]=endpoint;
  if(data_ready&&!manual){
   float response=std::clamp(std::sqrt(served.gain_scale/served.tau_scale),.65f,1.35f);
   // Uncalibrated response needs extra terminal-rate damping. Keep a margin
   // even after validated parameters are blended in; never gate model control.
   float damping=2.f-.5f*served.mix;planning_caps[i]=max_rates[i]*response;planning_damping[i]=damping;
   auto predict=[&](bool agile){return model_control::predict(models[i],errors[i],rate,previous[i],speed,limits[i],max_rates[i]*response,zones[i],history,now,i,references[i],damping,agile);};
   if(control_mode.load()==2){ // Retired CAPTURE test/migration path.
    plan=blend<=0?predict(false):blend>=1?predict(true):control_modes::combine(predict(false),predict(true),blend);
   }else{
    auto peace=predict(false);peace_plans[i]=peace;
    auto choice=peace_agile::select(peace,i==1||blend<=0?peace:predict(true),coordination,i,errors[i],blend);
    plan=choice.plan;boost[i]=choice.boost;
    heritage_valid=heritage_valid&&plan.valid;
   }
   if(capture_mode_weight.load()>0){
    model_control::Plan fast;
    if(i==1){
     auto shifted=capture_models[i];shifted.bias=0;
     bool leveling=capture_roll_guidance.leveling;
     float reference=leveling?0.f:references[i];
     fast=model_control::predict(shifted,errors[i],endpoint-capture_bias[i],previous[i],speed,authority[i],max_rates[i],leveling?.3f:zones[i],history,now,i,reference-capture_bias[i],leveling?1.35f:damping,!leveling);
     capture_plans[i].plan=fast;capture_plans[i].desired_command=fast.command;
    }else{
     capture_plans[i]=robust_intercept::predict(capture_models[i],errors[i],endpoint,previous[i],speed,authority[i],max_rates[i],.025f,history,now,i,references[i],slews[i],dt,capture_bias[i],capture_envelopes[i]);
     fast=capture_plans[i].plan;
    }
    plan=control_modes::combine(plan,fast,capture_mode_weight.load());
   }
   capture_targets[i]=plan.command;
  }
  plans[i]=plan;
 }
 peace_agile::JointSelection joint;war10::Diagnostics war_rate{};bool anchored=false;
 if(control_mode.load()!=2&&blend>0&&capture_mode_weight.load()==0&&data_ready&&!input.manual){
  peace_agile::JointInput in;in.models=models;in.rates=planning_rates;in.previous=previous;
  in.errors=errors;in.references=references;in.caps=planning_caps;in.damping=planning_damping;
  in.goal=goal;in.speed=speed;in.dt=dt;in.now=now;
  // WAR planners use the measured AC8 aircraft response until the online learner has
  // deployed a validated model for that axis (PEACE plans above keep the shared prior).
  {const auto& craft=ac8_models::find(plane);in.aircraft=&craft;
   for(int i=0;i<3;++i)if(serving_current->axes[i].deployments()==0){
    const auto& ax=i==0?craft.pitch:i==1?craft.roll:craft.yaw;auto m=standard[i];
    m.tau=ax.tau;m.delay=ax.delay;m.g0*=ax.scale;m.g1*=ax.scale;in.models[i]=m;}}
  {auto g=flight::unit(goal);auto target_world=flight::unit(body.f*g.x+body.r*g.y+body.u*g.z);
   float theta=std::acos(std::clamp(g.x,-1.f,1.f))/flight::rad;
   in.path_origin=war_anchor.update(body,target_world,target_jump,theta,war11::Params{});anchored=true;}
  // WAR v11.1: feasible pitch/yaw rate path on top of the v9 PEACE/AGILE choice,
  // corrected to keep the predicted nose path in the nose-target plane.
  joint=war11::choose(in,history,peace_plans,plans,blend,&war_rate);plans=joint.plans;
  if(joint.choice!=1)boost={};
 }
 if(!anchored)war_anchor.reset(); // PEACE, manual, stale data: the next WAR maneuver starts a new plane
 // Apply the selected joint plan once, using the same continuity/slew path for
 // PEACE, WAR, invalid plans and stale observations. There is no VECTOR override.
 for(int i=0;i<3;++i){
  auto& learner=*axes[i];auto& served=serving_current->axes[i];auto& control=primary_controllers[i];auto plan=plans[i];
  bool manual=read&&(input.manual&manual_bits[i]);
  if(manual){control.follow_manual(previous[i],commands[i],authority[i]);commands[i]=std::clamp(previous[i],-authority[i],authority[i]);sources[i]=3;reasons[i]=32;}
  else if(data_ready&&plan.valid){commands[i]=control.apply_full(plan,commands[i],authority[i],slews[i],dt);sources[i]=i!=1&&joint.choice?2.f:1.f;}
  else{commands[i]=control.fallback_full(commands[i],authority[i],slews[i],dt);sources[i]=0;reasons[i]=float(data_reason|(data_ready?64u:0u));heritage_valid=false;}
  assist_pursuit[i]=plan.pursuit;assist_effective_damping[i]=plan.damping;assist_input_limit[i]=plan.input_limit;
  assist_primary_weight[i]=control.weight;assist_primary_command[i]=control.last.command;assist_primary_delta[i]=control.delta;
  assist_primary_stop[i]=control.last.stopping_angle;assist_primary_horizon[i]=control.last.horizon;assist_primary_ratecap[i]=control.last.rate_limit;assist_reference_rate[i]=references[i];assist_learn_delay[i]=models[i].delay;
  assist_primary_checks[i]=served.serving_check.count;assist_primary_model_rmse[i]=std::sqrt(served.serving_check.learned_error);assist_primary_base_rmse[i]=std::sqrt(served.serving_check.baseline_error);
  assist_candidate_weight[i]=learner.learned_weight;assist_model_source[i]=data_ready&&!manual&&plan.valid?served.source():-1;assist_deployed_models[i]=served.deployments();
 }
 capture_joint_delta=0;capture_joint_roll_delta=0;
 if(capture_mode_weight.load()>0&&data_ready&&!input.manual){
  auto measured=capture_rates;
  auto corrected=capture_joint::refine(capture_models,measured,previous,commands,authority,capture_plans,capture_envelopes,goal,errors[1],references,speed,history,now,dt,capture_mode_weight.load(),capture_bias,world_up,capture_roll_guidance.leveling);
  capture_model_diagnostics[45]=corrected.before_crossing;capture_model_diagnostics[46]=corrected.crossing;capture_model_diagnostics[47]=corrected.valid?1:0;
  if(corrected.valid)for(int i=0;i<3;++i){
   float change=corrected.command[i]-commands[i];capture_joint_delta=capture_joint_delta.load()+std::abs(change);if(i==1)capture_joint_roll_delta=change;commands[i]=corrected.command[i];
   capture_plans[i].desired_command=corrected.desired[i];capture_plans[i].issued_command=commands[i];
   auto& c=primary_controllers[i];c.actual=commands[i];c.delta=commands[i]-(i==0?pcmd:i==1?rcmd:ycmd);
   assist_primary_command[i]=commands[i];assist_primary_delta[i]=c.delta;
  }
 }
 for(int i=0;i<3;++i){
  auto& a=capture_aircraft->axes[i];auto risk=a.uncertainty(speed,capture_rates[i],.35f);auto& result=capture_plans[i];
  // A mode fade, cold-primary fade, fallback or joint change alters the real
  // first source input. Never publish a certificate from the pre-change plan.
  if(i!=1&&result.plan.valid&&data_ready&&!input.manual)
   result=robust_intercept::assess_issued(capture_models[i],errors[i],capture_rates[i],previous[i],commands[i],result.desired_command+(commands[i]-result.issued_command),speed,authority[i],max_rates[i],.025f,history,now,i,references[i],slews[i],dt,capture_bias[i],capture_envelopes[i]);
  if(capture_mode_weight.load()<.999f||primary_controllers[i].weight<.999f)result.source_certified=false;
  if(i!=1&&capture_mode_weight.load()>=.999f&&result.plan.valid){assist_primary_stop[i]=result.plan.stopping_angle;assist_primary_horizon[i]=result.plan.horizon;assist_primary_ratecap[i]=result.plan.rate_limit;}
  size_t d=size_t(i)*15;
  std::array<float,15> values{capture_models[i].g0,capture_models[i].tau,capture_models[i].delay,capture_bias[i],a.model_weight,std::hypot(risk.rate_rms,capture_endpoints[i].noise),risk.trust,capture_envelopes[i].gain_fraction,capture_envelopes[i].tau_fraction,capture_envelopes[i].delay_seconds,float(a.promotions()),result.stop_residual,result.worst_crossing,result.unavoidable_crossing,result.source_certified?1.f:0.f};
  if(data_ready)for(size_t j=0;j<values.size();++j)capture_model_diagnostics[d+j]=values[j];
  if(data_ready){capture_model_diagnostics[48+i*3]=capture_rates[i];capture_model_diagnostics[49+i*3]=capture_endpoints[i].qualified?1.f:0.f;capture_model_diagnostics[50+i*3]=capture_endpoints[i].noise;capture_model_diagnostics[57]=4;}
 }
 capture_trim_pitch=capture_bias[0];capture_trim_yaw=capture_bias[2];
 pcmd=commands[0];rcmd=commands[1];ycmd=commands[2];assist_delta_pitch=assist_delta_roll=0;assist_profile=6;
 auto& p=serving_current->axes[0];auto& r=serving_current->axes[1];auto& y=serving_current->axes[2];
 assist_conf_pitch=axes[0]->confidence;assist_conf_roll=axes[1]->confidence;assist_conf_yaw=axes[2]->confidence;
 assist_gain_pitch=p.gain_scale;assist_gain_roll=r.gain_scale;assist_gain_yaw=y.gain_scale;
 assist_learn_tau_pitch=models[0].tau;assist_learn_tau_roll=models[1].tau;assist_learn_tau_yaw=models[2].tau;
 assist_learn_weight_pitch=p.mix;assist_learn_weight_roll=r.mix;assist_learn_weight_yaw=y.mix;
 assist_learn_promotions_pitch=axes[0]->promotions();assist_learn_promotions_roll=axes[1]->promotions();assist_learn_promotions_yaw=axes[2]->promotions();
 // The selected primary plan above is the only command producer. Do not
 // replace it with the former VECTOR controller after the slew limiter.
 reset_vector_policy();vector_blend.reset(false);vector_mode_weight=0;
 for(auto& d:vector_decision_diagnostics)d=0;
 // v11.1 target-plane planning, in retired VECTOR decision columns (CSV layout unchanged):
 // vector_maneuver_mode = correction weight (0..1), vector_maneuver_roll_goal = predicted
 // off-plane angle at the preview horizon after correction (deg), vector_pull_completion_s = cost reduction.
 if(war_rate.plane>0){vector_decision_diagnostics[0]=war_rate.plane;vector_decision_diagnostics[1]=war_rate.cross;vector_decision_diagnostics[2]=war_rate.margin;}
 // Retired VECTOR performance columns carry the v11 rate path (CSV layout unchanged):
 // envelope_mix=rate-path weight, pitch_pursuit_cap/pitch_stop_cap=desired pitch/yaw
 // rate, pitch_reachable_high=predicted path after, requested_rate=|desired|, revision=11.
 if(joint.choice==3){vector_performance_diagnostics[0]=war_rate.weight;vector_performance_diagnostics[1]=war_rate.desired_pitch;
  vector_performance_diagnostics[2]=war_rate.desired_yaw;vector_performance_diagnostics[3]=joint.after.path;
  vector_performance_diagnostics[4]=std::hypot(war_rate.desired_pitch,war_rate.desired_yaw);}
 vector_performance_diagnostics[5]=11.2f;
 std::array<float,27> heritage{blend,heritage_valid?1.f:0.f,11.2f,coordination.weight,
  coordination.roll_weight,coordination.path_weight,coordination.theta,coordination.along,coordination.transverse,
  boost[0],boost[2],data_ready&&!(read&&input.manual&4)&&primary_controllers[1].last.valid?1.f:0.f,
  serving_current->axes[0].mix,serving_current->axes[1].mix,serving_current->axes[2].mix,
  float(joint.choice),joint.checked?1.f:0.f,joint.before.cost-joint.after.cost,joint.before.path,joint.after.path,joint.transport_deg,
  sources[0],sources[1],sources[2],reasons[0],reasons[1],reasons[2]};
 for(size_t i=0;i<heritage.size();++i)heritage_diagnostics[i]=heritage[i];
 pcmd=commands[0];rcmd=commands[1];ycmd=commands[2];
 if(now-adaptive_log_tick>=10000){adaptive_log_tick=now;
  log_line("CAPTURE_CONTROL mode=%d weight=%.3f track=%.3f level=%.3f cross_rate=%.3f joint_delta=%.4f trim_pitch=%.3f trim_yaw=%.3f",control_mode.load(),capture_mode_weight.load(),capture_track_weight.load(),capture_level_weight.load(),capture_cross_rate.load(),capture_joint_delta.load(),capture_bias[0],capture_bias[2]);
  for(int i=0;i<3;++i){auto& served=serving_current->axes[i];auto& a=*axes[i];
   auto& ca=capture_aircraft->axes[i];
   log_line("CAPTURE_ADAPTATION plane=%d axis=%d K=%.3f tau=%.3f delay=%.3f total_bias=%.3f mix=%.3f residual_rms=%.3f accepted=%u rejected=%u stop_residual=%.3f crossing=%.3f inherited_crossing=%.3f finite_source_check=%d",plane,i,capture_models[i].g0,capture_models[i].tau,capture_models[i].delay,capture_bias[i],ca.model_weight,ca.model_rate_rms,ca.promotions(),ca.rejections(),capture_plans[i].stop_residual,capture_plans[i].worst_crossing,capture_plans[i].unavoidable_crossing,capture_plans[i].source_certified?1:0);
   log_line("MODEL_FULL plane=%d axis=%d data=%d learning=%d source=%d primary=%.3f gain=%.3f tau=%.3f delay=%.3f learned_mix=%.3f candidate_mix=%.3f deployed=%u delta=%.4f",plane,i,data_ready?1:0,learn_ready?1:0,int(assist_model_source[i].load()),primary_controllers[i].weight,served.gain_scale,models[i].tau,models[i].delay,served.mix,a.learned_weight,served.deployments(),primary_controllers[i].delta);
   log_line("MODEL_PARAMETER_VALIDATION plane=%d axis=%d ready=%d checks=%u candidate_rmse=%.3f reference_rmse=%.3f",plane,i,served.candidate_check.usable?1:0,served.candidate_check.count,std::sqrt(served.candidate_check.learned_error),std::sqrt(served.candidate_check.baseline_error));
   log_line("MODEL_AGILITY plane=%d axis=%d pursuit=%.3f damping=%.3f plan_input_limit=%.3f ratecap=%.3f",plane,i,assist_pursuit[i].load(),assist_effective_damping[i].load(),assist_input_limit[i].load(),primary_controllers[i].last.rate_limit);
  }
 }
}
