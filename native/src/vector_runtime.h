#pragma once
#include "vector_control.h"
#include "vector_adaptation.h"
#include "vector_pitch_authority.h"
#include "vector_signed_dynamics.h"
struct SignedPitchAircraft{int id=-1;vector_pitch_authority::Axis authority;vector_signed_dynamics::Axis dynamics;};
std::array<SignedPitchAircraft,64> signed_pitch_fleet;SignedPitchAircraft* signed_pitch_current=nullptr;
vector_adaptation::Fleet vector_fleet;vector_adaptation::Aircraft* vector_aircraft=nullptr;
vector_control::Controller vector_controller;
control_modes::Blend vector_blend{0};
std::array<capture_rate::Endpoint,3> vector_endpoints;
uint64_t vector_log_tick=0;
void reset_vector_policy(){
 if(signed_pitch_current){signed_pitch_current->authority.suspend();signed_pitch_current->dynamics.suspend();}
 vector_controller.reset();vector_blend.reset(control_mode.load()==3);vector_mode_weight=vector_blend.value;
 for(auto& e:vector_endpoints)e.reset();for(auto& d:vector_coordination_diagnostics)d=0;for(auto& d:vector_performance_diagnostics)d=0;for(auto& d:vector_diagnostics)d=0;
 if(vector_aircraft)for(auto& a:vector_aircraft->axes)a.anchors={};
}
void apply_vector_policy(uint64_t now,int plane,float dt,float speed,bool data_ready,bool learn_ready,unsigned manual,
 const std::array<shared_braking::Model,3>& standards,const std::array<shared_braking::Model,3>& shared,
 const std::array<float,3>& raw,const std::array<float,3>& previous,std::array<float,3>& commands,
 const std::array<float,3>& authority,const std::array<float,3>& references,flight::V goal,flight::V world_up,
 const flight::Basis& body,float target_jump,const adaptive_braking::History& history){
 for(auto& d:vector_coordination_diagnostics)d=0;for(auto& d:vector_performance_diagnostics)d=0;for(auto& d:vector_diagnostics)d=0;for(auto& d:vector_decision_diagnostics)d=0;
 if(!vector_aircraft)return;
 float mix=vector_blend.step(control_mode.load()==3,dt);vector_mode_weight=mix;
 std::array<shared_braking::Model,3> priors=standards,models=standards;
 for(auto& m:priors)m.bias=0;
 std::array<float,3> rates{};std::array<bool,3> eligible{},qualified{};
 std::array<vector_adaptation::SharedCandidate,3> candidates{};
 for(int i=0;i<3;++i){rates[i]=vector_endpoints[i].step(raw[i],dt,priors[i],speed);
  qualified[i]=vector_endpoints[i].qualified&&vector_endpoints[i].noise<std::max(i==0?.3f:i==1?1.f:.12f,.04f*shared_braking::gain(priors[i],speed));
  eligible[i]=learn_ready&&qualified[i];
  auto& serving=serving_current->axes[i];auto candidate=shared[i];candidate=capture_adaptation::frozen(candidate,speed);candidate.bias=0;
  candidates[i]={candidate,serving.mix,data_ready&&serving.mix>.55f&&serving.serving_check.usable};
 }
 std::array<unsigned,3> accepted{};for(int i=0;i<3;++i)accepted[i]=vector_aircraft->axes[i].promotions();
 if(data_ready)vector_aircraft->observe(priors,now,rates,speed,history,eligible,candidates,std::atan2(world_up.y,world_up.z)/flight::rad);
 vector_control::Input input;input.rates=rates;input.previous=previous;input.goal=goal;input.world_up=world_up;input.body=body;
 input.limits={1,1,.85f};input.reference=references;input.reference[1]=0;input.target_jump=target_jump;
 input.history=&history;input.now=now;input.dt=dt;input.speed=speed;input.eligible=data_ready&&!manual;input.learn_allowed=learn_ready;input.local_feedback_allowed=data_ready&&!manual;input.rate_qualified=qualified;
 for(int i=0;i<3;++i){auto& a=vector_aircraft->axes[i];
  if(data_ready)models[i]=a.effective(priors[i],speed,dt);else{a.anchors={};models[i]=a.current(priors[i],speed);}
  if(data_ready)a.served(now,models[i]);auto risk=a.uncertainty(speed,rates[i],.4f);
  float noise=vector_endpoints[i].noise;
  if(!vector_endpoints[i].qualified){noise=std::max(noise,std::abs(rates[i]-raw[i]));risk.gain_fraction=std::max(risk.gain_fraction,.25f);risk.tau_fraction=std::max(risk.tau_fraction,.35f);risk.delay_seconds=std::max(risk.delay_seconds,.05f);}
  input.envelopes[i]={risk.gain_fraction,risk.tau_fraction,risk.delay_seconds,std::max(risk.bias_rate,noise),risk.trust};
  if(a.promotions()!=accepted[i])log_line("VECTOR_MODEL_DEPLOYED plane=%d axis=%d accepted=%u rejected=%u",plane,i,a.promotions(),a.rejections());
 }
 if(!signed_pitch_current||signed_pitch_current->id!=plane){auto found=std::find_if(signed_pitch_fleet.begin(),signed_pitch_fleet.end(),[&](const auto& p){return p.id==plane;});if(found==signed_pitch_fleet.end())found=std::find_if(signed_pitch_fleet.begin(),signed_pitch_fleet.end(),[](const auto& p){return p.id<0;});if(found==signed_pitch_fleet.end())found=signed_pitch_fleet.begin();if(found->id!=plane){*found={};found->id=plane;}signed_pitch_current=&*found;}
 signed_pitch_current->authority.observe(models[0],now,rates[0],speed,history,data_ready&&eligible[0],dt);
 input.pitch_positive_gain=signed_pitch_current->authority.positive;input.pitch_negative_gain=signed_pitch_current->authority.negative;
 auto& signed_dynamics=signed_pitch_current->dynamics;
 auto signed_baseline=vector_signed_dynamics::from(models[0],input.pitch_positive_gain,input.pitch_negative_gain);
 float signed_blend=0;
 if(data_ready){signed_dynamics.observe(signed_baseline,now,rates[0],speed,history,eligible[0],dt);
  auto coherent=signed_dynamics.effective(signed_baseline,speed,dt);
  // Identification continues through the maneuver, but the signed tuple only
  // serves the near-target stop/terminal loop. Large-angle pursuit keeps the
  // established WAR response so a newly promoted tuple cannot slow a pull-in.
  float theta=std::atan2(std::sqrt(goal.y*goal.y+goal.z*goal.z),goal.x)/flight::rad;
  auto smooth=[](float x){x=std::clamp(x,0.f,1.f);return x*x*(3-2*x);};
  signed_blend=signed_dynamics.weight>0?smooth((10.f-theta)/4.f):0.f;
   input.signed_model_weight=signed_blend*signed_dynamics.weight;
  auto base=models[0];
  models[0]={base.tau+signed_blend*(coherent.tau-base.tau),base.delay+signed_blend*(coherent.delay-base.delay),base.g0+signed_blend*(coherent.positive-base.g0),base.g1*(1-signed_blend),base.bias+signed_blend*(coherent.bias-base.bias)};
  input.pitch_positive_gain=signed_pitch_current->authority.positive+signed_blend*(coherent.positive-signed_pitch_current->authority.positive);
  input.pitch_negative_gain=signed_pitch_current->authority.negative+signed_blend*(coherent.negative-signed_pitch_current->authority.negative);
 }else signed_dynamics.anchors={};
 input.models=models;vector_control::Result result;
 if(mix>0&&data_ready&&!manual)result=vector_controller.plan(input);else vector_controller.reset();
 if(result.valid)for(int i=0;i<3;++i){
  float command=commands[i]+mix*(result.command[i]-commands[i]);
  if(result.plans[i].priority&&mix>.5f)command=result.command[i];
  command=std::clamp(command,std::max(-input.limits[i],previous[i]-result.slew[i]*dt),std::min(input.limits[i],previous[i]+result.slew[i]*dt));
  commands[i]=command;auto& primary=primary_controllers[i];primary.actual=command;primary.initialized=true;primary.last=result.plans[i].model;primary.last.command=command;
  primary.weight=std::max(primary.weight,mix);assist_primary_weight[i]=primary.weight;
  primary.delta=command-previous[i];assist_primary_command[i]=command;assist_primary_delta[i]=primary.delta;
  assist_primary_stop[i]=result.plans[i].stop_distance;assist_primary_horizon[i]=result.plans[i].model.horizon;assist_primary_ratecap[i]=result.plans[i].model.rate_limit;
 }
 for(int i=0;i<3;++i){auto& a=vector_aircraft->axes[i];auto d=a.diagnostics(speed,now);auto risk=a.uncertainty(speed,rates[i],.4f);
  std::array<float,32> values{models[i].g0,models[i].tau,models[i].delay,result.valid?result.effective_bias[i]:models[i].bias,a.model_weight,risk.rate_rms,risk.trust,
   float(d.fit_count),float(d.validation_count),float(d.long_count),float(d.status),float(d.candidate_source),float(d.proposals),float(d.accepted),float(d.rejected),float(d.expired),float(d.ambiguous_skips),float(d.innovation_skips),float(d.attempt_age_ms),
   d.fast_bias_mix,float(d.fast_bias_fit),float(d.fast_bias_validation),float(d.fast_bias_accepted),float(d.fast_bias_rejected),rates[i],vector_endpoints[i].qualified?1.f:0.f,vector_endpoints[i].noise,
   result.desired_rate[i],result.plans[i].stop_distance,result.plans[i].priority?1.f:0.f,commands[i],result.slew[i]};
  if(data_ready)for(size_t j=0;j<values.size();++j)vector_diagnostics[i*32+j]=values[j];
  if(result.valid&&mix>=.999f){assist_learn_delay[i]=models[i].delay;assist_learn_tau_pitch=i==0?models[i].tau:assist_learn_tau_pitch.load();
   if(i==1)assist_learn_tau_roll=models[i].tau;if(i==2)assist_learn_tau_yaw=models[i].tau;
   assist_model_source[i]=a.model_weight>.99f?1:a.model_weight>.001f?2:0;assist_deployed_models[i]=a.promotions();assist_candidate_weight[i]=a.model_weight;assist_primary_model_rmse[i]=risk.rate_rms;
   assist_pursuit[i]=result.plans[i].model.pursuit;assist_effective_damping[i]=result.plans[i].model.damping;assist_input_limit[i]=input.limits[i];
  }
  if(now-vector_log_tick>=10000)log_line("VECTOR_LEARNING plane=%d axis=%d K=%.3f tau=%.3f D=%.3f total_bias=%.3f model_mix=%.3f trust=%.3f rmse=%.3f stage=%u fit=%u short=%u long=%u candidate_source=%u proposals=%u accepted=%u rejected=%u expired=%u ambiguous=%u innovation=%u age_ms=%llu bias_mix=%.3f bias_fit=%u bias_checks=%u bias_accepted=%u bias_rejected=%u",plane,i,models[i].g0,models[i].tau,models[i].delay,result.valid?result.effective_bias[i]:models[i].bias,a.model_weight,risk.trust,risk.rate_rms,d.status,d.fit_count,d.validation_count,d.long_count,d.candidate_source,d.proposals,d.accepted,d.rejected,d.expired,d.ambiguous_skips,d.innovation_skips,d.attempt_age_ms,d.fast_bias_mix,d.fast_bias_fit,d.fast_bias_validation,d.fast_bias_accepted,d.fast_bias_rejected);
 }
 if(result.valid&&mix>=.999f){assist_gain_pitch=models[0].g0/shared_braking::gain(priors[0],speed);assist_gain_roll=models[1].g0/shared_braking::gain(priors[1],speed);assist_gain_yaw=models[2].g0/shared_braking::gain(priors[2],speed);
  assist_conf_pitch=vector_aircraft->axes[0].uncertainty(speed,rates[0],.4f).trust;assist_conf_roll=vector_aircraft->axes[1].uncertainty(speed,rates[1],.4f).trust;assist_conf_yaw=vector_aircraft->axes[2].uncertainty(speed,rates[2],.4f).trust;
  assist_learn_weight_pitch=vector_aircraft->axes[0].model_weight;assist_learn_weight_roll=vector_aircraft->axes[1].model_weight;assist_learn_weight_yaw=vector_aircraft->axes[2].model_weight;
  assist_learn_promotions_pitch=vector_aircraft->axes[0].promotions();assist_learn_promotions_roll=vector_aircraft->axes[1].promotions();assist_learn_promotions_yaw=vector_aircraft->axes[2].promotions();
 }
 std::array<float,14> common{mix,float(result.phase),result.theta,result.along_rate,result.transverse_rate,result.path_error,result.common_rate,result.joint_delta,result.predicted_path_before,result.predicted_path_after,result.roll_error,result.roll_rate_cap,
  result.plans[0].target_stop_guard||result.plans[2].target_stop_guard?1.f:0.f,result.valid?1.f:0.f};
 if(data_ready)for(size_t i=0;i<common.size();++i)vector_diagnostics[96+i]=common[i];
 if(data_ready){vector_diagnostics[110]=result.terminal_mix;vector_diagnostics[111]=result.roll_terminal_mix;}
 if(data_ready){std::array<float,14> decision{float(int(result.maneuver.mode)),result.maneuver.roll_error,result.maneuver.pull_time,result.maneuver.push_time,result.maneuver.pull_cost,result.maneuver.push_cost,result.maneuver.alignment,result.maneuver.preparing?1.f:0.f,result.positive_gain,result.negative_gain,float(signed_pitch_current->authority.positive_samples),float(signed_pitch_current->authority.negative_samples),result.maneuver.push_weight,1.f};for(size_t i=0;i<decision.size();++i)vector_decision_diagnostics[i]=decision[i];}
 if(data_ready&&result.valid){std::array<float,6> performance{result.envelope_mix,result.pitch_capacity,result.pitch_stop_cap,result.pitch_reachable_high,result.requested_rate,8.f};for(size_t i=0;i<performance.size();++i)vector_performance_diagnostics[i]=performance[i];}
 if(data_ready&&result.valid){std::array<float,40> coordination{models[0].bias,models[2].bias,result.local_bias_weight[0],result.local_bias_weight[2],float(result.joint_axes),result.pending_error[0],result.pending_error[2],float(vector_controller.local_bias[0].accepted),float(vector_controller.local_bias[2].accepted),float(vector_controller.local_bias[0].rejected),float(vector_controller.local_bias[2].rejected),learn_ready?1.f:0.f,input.local_feedback_allowed?1.f:0.f,float(vector_controller.local_bias[0].status),float(vector_controller.local_bias[2].status),result.terminal_retained,result.level_rate_scale,result.terminal_audit_fraction[0],result.terminal_audit_fraction[2],assist_environment_unsafe.load()?1.f:0.f,signed_dynamics.weight,signed_dynamics.rmse,float(signed_dynamics.promotions()),float(signed_dynamics.banks[capture_adaptation::band(speed)].rejected),float(signed_dynamics.banks[capture_adaptation::band(speed)].shorts),float(signed_dynamics.banks[capture_adaptation::band(speed)].longs),float(signed_dynamics.banks[capture_adaptation::band(speed)].checks[0]),float(signed_dynamics.banks[capture_adaptation::band(speed)].checks[1]),signed_blend,result.prediction_weight[0],result.prediction_weight[2],result.response_weight[0],result.response_weight[2],result.calibrated_terminal,result.terminal_stop_allowance,result.roll_raw_error,result.roll_handoff,result.roll_regularization,result.roll_pending_error,result.roll_audit_fraction};for(size_t i=0;i<coordination.size();++i)vector_coordination_diagnostics[i]=coordination[i];}
 if(now-vector_log_tick>=10000){vector_log_tick=now;log_line("VECTOR_MANEUVER plane=%d mode=%d roll_goal=%.2f pull_time=%.2f push_time=%.2f cost_pull=%.3f cost_push=%.3f alignment=%.3f preparing=%d Kpositive=%.3f Knegative=%.3f",plane,int(result.maneuver.mode),result.maneuver.roll_error,result.maneuver.pull_time,result.maneuver.push_time,result.maneuver.pull_cost,result.maneuver.push_cost,result.maneuver.alignment,result.maneuver.preparing?1:0,result.positive_gain,result.negative_gain);log_line("VECTOR_CONTROL plane=%d mode=%d weight=%.3f valid=%d angle=%.3f common_rate=%.3f along=%.3f side=%.3f path_error=%.3f joint_delta=%.3f phase=%d emergency=(%d,%d,%d)",plane,control_mode.load(),mix,result.valid?1:0,result.theta,result.common_rate,result.along_rate,result.transverse_rate,result.path_error,result.joint_delta,result.phase,result.plans[0].priority?1:0,result.plans[1].priority?1:0,result.plans[2].priority?1:0);}
}

