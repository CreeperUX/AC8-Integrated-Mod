#pragma once
#include "flight_math.h"
#include "capture_roll.h"
#include "vector_roll.h"
#include "robust_intercept.h"
#include "vector_stop.h"
#include "vector_maneuver.h"
#include "vector_local_bias.h"
#include "vector_pending_pose.h"
#include "vector_local_response.h"
#include <array>
#include <algorithm>
#include <cmath>
// WAR policy: one spatial pointing motion, jointly allocated to pitch/yaw.
// Dynamics remain an identified local response model, not full aerodynamics.
namespace vector_control {
inline float angle(flight::V a,flight::V b){return std::atan2(std::sqrt(flight::dot(flight::cross(a,b),flight::cross(a,b))),flight::dot(a,b))/flight::rad;}
inline flight::V world(const flight::Basis& b,flight::V local){return b.f*local.x+b.r*local.y+b.u*local.z;}
inline flight::V local(const flight::Basis& b,flight::V v){return {flight::dot(b.f,v),flight::dot(b.r,v),flight::dot(b.u,v)};}
inline flight::V rotate_rate(flight::V v,flight::V omega,float dt){
 float n=std::sqrt(flight::dot(omega,omega));if(n<1e-7f)return v;
 auto axis=omega*(1/n);float a=n*dt*flight::rad,c=std::cos(a),s=std::sin(a);
 return v*c+flight::cross(axis,v)*s+axis*(flight::dot(axis,v)*(1-c));
}
inline float plane_roll_error(flight::V goal){
 float error=std::atan2(goal.y,goal.z)/flight::rad;
 return error; // Main pointing maneuvers align the positive-lift plane.
}
// Shared by the issued controller and candidate rollout. The position term
// respects the coordinated rate budget even when the angle error is large.
inline float roll_arrival_torque(shared_braking::Model m,float error,float rate,float cap){
 float omega=std::clamp(4.f/std::sqrt(m.tau),1.5f,5.f),damping=2.2f*m.tau*omega-1.f;
 float position=std::clamp(m.tau*omega*omega*error,-(damping+1)*cap,(damping+1)*cap);
 return position-damping*rate-m.bias;
}
struct Input {
 std::array<shared_braking::Model,3> models{};
 std::array<vector_stop::Envelope,3> envelopes{};
 std::array<float,3> rates{},previous{},reference{},limits{1,1,.85f};
 flight::V goal{1,0,0},world_up{0,0,1};flight::Basis body{{1,0,0},{0,1,0},{0,0,1}};
 const adaptive_braking::History* history=nullptr;
 uint64_t now=0;float speed=400,dt=.016667f,target_jump=0,pitch_positive_gain=0,pitch_negative_gain=0;bool eligible=true;
 std::array<bool,3> rate_qualified{true,true,true};
 bool learn_allowed=true,local_feedback_allowed=true;
 float signed_model_weight=0;
};
struct Result {
 bool valid=false,leveling=false;int phase=0;vector_maneuver::Result maneuver{};float positive_gain=0,negative_gain=0;
 std::array<float,3> command{},desired_rate{},slew{12,16,6};
 std::array<vector_stop::Budget,3> budgets{};
 std::array<vector_stop::Plan,3> plans{};
 float theta=0,common_rate=0,along_rate=0,transverse_rate=0,path_error=0;
 float predicted_path_before=0,predicted_path_after=0,joint_delta=0,roll_error=0,roll_rate_cap=0;
 float terminal_mix=0,roll_terminal_mix=0;
 float roll_raw_error=0,roll_handoff=0,roll_regularization=0,roll_pending_error=0,roll_audit_fraction=1;
 float level_rate_scale=1,terminal_retained=0;std::array<float,3> terminal_audit_fraction{1,1,1};
 float envelope_mix=0,pitch_capacity=0,pitch_stop_cap=0,pitch_reachable_high=0,requested_rate=0;
 std::array<float,3> effective_bias{},local_bias_weight{};
 unsigned joint_axes=0;
 float joint_activity=0;
 std::array<float,3> pending_error{},prediction_weight{},response_weight{};
 float calibrated_terminal=0,terminal_stop_allowance=0;
};
struct Direction {
 float p=1,y=0,side_p=0,side_y=-1,theta=0;
};
inline Direction direction(flight::V goal){
 Direction d;float length=std::hypot(goal.y,goal.z);d.theta=std::atan2(length,goal.x)/flight::rad;
 if(length>1e-5f){d.p=goal.z/length;d.y=goal.y/length;}
 d.side_p=d.y;d.side_y=-d.p;return d;
}
inline float common_cap(float p,float y,float pitch_cap,float yaw_cap){
  float cap=600; // Numerical model-validity ceiling, not a shared-aircraft pursuit rate.
 if(std::abs(p)>1e-5f)cap=std::min(cap,pitch_cap/std::abs(p));
 if(std::abs(y)>1e-5f)cap=std::min(cap,yaw_cap/std::abs(y));return std::max(0.f,cap);
}
inline float envelope_weight(float theta){return vector_maneuver::smooth((theta-5.f)/10.f);}
inline float approach_request(float theta,float ability){
 // Keep a useful terminal angular velocity instead of letting the shared
 // ray collapse to a crawl near the cursor. The stop budget still clips this
 // request, so the higher request only spends authority that can be braked.
 float near_request=16.f*theta/(1+theta/8.f);
 float requested=near_request+envelope_weight(theta)*std::max(0.f,ability-near_request);
 return std::clamp(requested,0.f,std::max(0.f,ability));
}
struct Controller {
 vector_roll::Guidance roll;vector_maneuver::Planner maneuver;
 std::array<vector_local_bias::Observer,3> local_bias;
 std::array<vector_local_response::Observer,3> local_response;
 std::array<vector_local_response::PredictorCheck,3> prediction_check;
 flight::V target_world{},normal_world{};bool path_seen=false;
 bool terminal_retained=false;float level_rate_scale=1;
 void reset(){*this=Controller{};}
 Result plan(const Input& incoming){
  Input in=incoming;
  Result out;if(!in.history||!in.eligible||!std::isfinite(in.dt)||in.dt<=0||in.dt>.1f||
   !std::isfinite(in.signed_model_weight)||!std::isfinite(in.speed)||!std::isfinite(in.goal.x)||!std::isfinite(in.goal.y)||!std::isfinite(in.goal.z)||flight::dot(in.goal,in.goal)<.5f){reset();return out;}
  for(int i=0;i<3;++i)if(!std::isfinite(in.rates[i])||!std::isfinite(in.previous[i])||!std::isfinite(in.models[i].tau)||in.models[i].tau<.2f||in.models[i].tau>3.f)return out;
  float calibrated_terminal=vector_maneuver::smooth(in.signed_model_weight/.20f);
  out.calibrated_terminal=calibrated_terminal;out.terminal_stop_allowance=.05f*calibrated_terminal;
  auto goal=flight::unit(in.goal);auto d=direction(goal);out.theta=d.theta;
  auto goal_world=world(in.body,goal);
  if(!path_seen||angle(target_world,goal_world)>.02f){
   target_world=goal_world;normal_world=flight::cross(in.body.f,goal_world);
   float norm=std::sqrt(flight::dot(normal_world,normal_world));path_seen=norm>1e-4f;
   if(path_seen)normal_world=normal_world*(1/norm);
  }
  auto normal=path_seen?local(in.body,normal_world):flight::V{0,-d.p,d.y};
  out.path_error=std::asin(std::clamp(normal.x,-1.f,1.f))/flight::rad;
  out.along_rate=in.rates[0]*d.p+in.rates[2]*d.y;
  out.transverse_rate=in.rates[0]*d.y-in.rates[2]*d.p;
  auto response=[&](int axis,shared_braking::Model standard){float k=shared_braking::gain(in.models[axis],in.speed),base=shared_braking::gain(standard,in.speed);
   float ratio=std::sqrt(std::max(.05f,k/base*standard.tau/in.models[axis].tau));ratio=std::clamp(ratio,.75f,1.65f);
   return 1.f+std::clamp(in.envelopes[axis].confidence,0.f,1.f)*(ratio-1.f);};
  float standard_pitch=shared_braking::gain(shared_braking::pitch,in.speed);
  out.positive_gain=in.pitch_positive_gain>0?in.pitch_positive_gain:std::max(standard_pitch,in.models[0].g0);
  out.negative_gain=in.pitch_negative_gain>0?in.pitch_negative_gain:.35f*standard_pitch;
  in.envelopes[0].positive_gain=out.positive_gain;in.envelopes[0].negative_gain=out.negative_gain;
  if(in.target_jump>2.5f)for(auto& observer:local_bias)observer.reset();
  float motion=std::hypot(in.rates[0],in.rates[2]);
  float reference_motion=std::hypot(in.reference[0],in.reference[2]);
  bool near_context=in.local_feedback_allowed&&d.theta<=8.f&&motion<=12.f&&reference_motion<=8.f;
  float context_weight=vector_maneuver::smooth((8.f-d.theta)/3.f)*vector_maneuver::smooth((12.f-motion)/4.f)*vector_maneuver::smooth((8.f-reference_motion)/3.f);
  for(int axis:{0,2}){
   float k=shared_braking::gain(in.models[axis],in.speed);
   float positive=axis==0?out.positive_gain:k,negative=axis==0?out.negative_gain:k;
   float original_bias=in.models[axis].bias;
   bool qualified=near_context&&in.rate_qualified[axis]&&(axis==0||std::abs(in.rates[1])<15.f);
   float correction=local_bias[axis].step(in.models[axis],positive,negative,axis,in.now,in.rates[axis],in.speed,*in.history,qualified,in.dt,in.envelopes[axis].gain_fraction,in.envelopes[axis].tau_fraction,in.envelopes[axis].delay_seconds);
   float fade=qualified?context_weight*(axis==0?1.f:vector_maneuver::smooth((15.f-std::abs(in.rates[1]))/5.f)):0.f;
   in.models[axis].bias=original_bias+fade*(correction-original_bias);
   out.local_bias_weight[axis]=fade*local_bias[axis].weight;
   if(out.local_bias_weight[axis]>0)in.envelopes[axis].bias_rate=std::max(in.envelopes[axis].bias_rate,
    out.local_bias_weight[axis]*(local_bias[axis].uncertainty+.15f*(1-out.local_bias_weight[axis])*std::abs(local_bias[axis].total-original_bias)));
  }
  for(int axis:{0,2}){
   auto& observer=local_response[axis];if(in.target_jump>2.5f)observer.reset();
   float k=shared_braking::gain(in.models[axis],in.speed),original=in.models[axis].bias;
   bool allowed=(calibrated_terminal>0||std::abs(in.rates[1])<6.f)&&near_context&&d.theta<3.5f&&motion<8.f&&in.rate_qualified[axis]&&std::abs(in.previous[axis])<.5f;
   float corrected=observer.step(in.models[axis],axis==0?out.positive_gain:k,axis==0?out.negative_gain:k,axis,
    in.now,in.rates[axis],in.speed,*in.history,allowed,in.dt);
   float blend=vector_maneuver::smooth((3.5f-d.theta)/1.5f)*vector_maneuver::smooth((8.f-motion)/4.f);
   in.models[axis].bias=original+blend*(1-out.local_bias_weight[axis])*(corrected-original);
   out.response_weight[axis]=allowed?blend*(1-out.local_bias_weight[axis])*observer.weight:0.f;
  }
  std::array<float,3> prediction_weight{1,1,1};
  for(int axis:{0,2}){auto& check=prediction_check[axis];if(in.target_jump>2.5f)check={};float k=shared_braking::gain(in.models[axis],in.speed);
   prediction_weight[axis]=check.step(in.models[axis],axis==0?out.positive_gain:k,axis==0?out.negative_gain:k,axis,
    in.now,in.rates[axis],in.speed,*in.history,in.local_feedback_allowed&&in.rate_qualified[axis],in.dt);}
  out.prediction_weight=prediction_weight;
  for(int axis=0;axis<3;++axis)out.effective_bias[axis]=in.models[axis].bias;
  float old_positive_rate=std::min(80.f,.9f*out.positive_gain);
  vector_maneuver::Input mi;mi.goal=goal;mi.models=in.models;mi.speed=in.speed;mi.dt=in.dt;mi.theta=d.theta;mi.roll_rate=in.rates[1];mi.positive_rate=old_positive_rate+envelope_weight(d.theta)*(std::max(.5f,out.positive_gain*in.limits[0]+in.models[0].bias)-old_positive_rate);mi.negative_rate=std::min(25.f,.85f*out.negative_gain);mi.roll_rate_cap=140*response(1,shared_braking::roll);
  out.maneuver=maneuver.update(mi);
  capture_roll::Input ri;ri.dt=in.dt;ri.angle=d.theta;ri.up=goal.z;ri.right=goal.y;
  ri.turn_error=out.maneuver.roll_error;ri.level_error=std::atan2(in.world_up.y,in.world_up.z)/flight::rad;
  ri.pitch_rate=in.rates[0]-in.reference[0];ri.yaw_rate=in.rates[2]-in.reference[2];ri.roll_rate=in.rates[1];
  ri.target_rate=std::hypot(in.reference[0],in.reference[2]);ri.target_jump=in.target_jump;
  ri.pole_clearance=std::hypot(in.world_up.y,in.world_up.z);ri.pursuit_rate=140*response(1,shared_braking::roll);ri.active=true;
  auto guidance=roll.update(ri);out.leveling=guidance.leveling;out.roll_error=guidance.roll_error;out.roll_rate_cap=guidance.rate_limit;
  out.roll_raw_error=roll.raw_error;out.roll_handoff=roll.handoff;out.roll_regularization=roll.regularization;
  // Keep the horizontal goal, but reserve pointing authority when recovery
  // roll actually makes the nose retreat. Do not reduce pursuit roll globally.
  float relative_along=(in.rates[0]-in.reference[0])*d.p+(in.rates[2]-in.reference[2])*d.y;
  float retreat=vector_maneuver::smooth((-relative_along-.10f)/.9f);
  float lateral=vector_maneuver::smooth((std::abs(out.transverse_rate)-.5f)/1.5f);
  float pointing=vector_maneuver::smooth((d.theta-.12f)/.6f);
  float desired_level_scale=out.leveling?1.f-.72f*pointing*std::max(retreat,lateral):1.f;
  // PEACE keeps a strong, continuous recovery command when the aircraft is
  // badly banked. WAR may reserve some authority for pointing, but it must
  // never turn leveling into a near-zero command while the attitude error is
  // large. This uses the same level-error observable already supplied to the
  // capture scheduler; no terrain-relative altitude is invented here.
  float bank_magnitude=std::abs(std::remainder(ri.level_error,360.f));
  float recovery_floor=out.leveling?
   (.30f+.35f*vector_maneuver::smooth((bank_magnitude-35.f)/55.f)):0.f;
  if(out.leveling)desired_level_scale=std::max(desired_level_scale,recovery_floor);
  // Bound transverse travel while pitch/yaw momentum is still being caught.
  // 0.5 * pointing_rate * roll_rate(rad/s) * response_time^2 approximates
  // the path departure from rotating a not-yet-settled pointing velocity.
  if(out.leveling){float response_time=std::max(in.models[0].delay+.3f*in.models[0].tau,in.models[2].delay+.3f*in.models[2].tau);
   float relative=std::hypot(ri.pitch_rate,ri.yaw_rate),tube=.03f+.09f*calibrated_terminal;
   float coordinated=2*tube/(std::max(.05f,relative)*flight::rad*response_time*response_time);
   float coordinated_scale=std::clamp(coordinated/std::max(1.f,out.roll_rate_cap),.18f,1.f);
   desired_level_scale=std::min(desired_level_scale,coordinated_scale);
   desired_level_scale=std::max(desired_level_scale,recovery_floor);}
  level_rate_scale+=std::clamp(desired_level_scale-level_rate_scale,-3.f*in.dt,.90f*in.dt);
  if(!out.leveling)level_rate_scale=1;
  out.level_rate_scale=level_rate_scale;out.roll_rate_cap*=level_rate_scale;
  out.phase=guidance.leveling?(ri.pole_clearance<=.05f?4:3):guidance.stable_time>0?2:1;
  std::array<float,3> errors{std::atan2(goal.z,goal.x)/flight::rad,out.roll_error,std::atan2(goal.y,goal.x)/flight::rad};
  std::array<float,3> capacities{45*response(0,shared_braking::pitch),out.roll_rate_cap,7*response(2,shared_braking::yaw)};
  for(int i=0;i<3;++i)capacities[i]=std::min(capacities[i],std::max(.5f,.98f*(std::abs(shared_braking::gain(in.models[i],in.speed))*in.limits[i]-std::abs(in.models[i].bias))));
  // Large positive-G pursuit uses identified full-input capability. Keep the
  // v3 capability cap at <=5 degrees, transition continuously through 15.
  // The delayed stopping budget and issued-first-input audit remain binding.
  out.envelope_mix=envelope_weight(d.theta);
  float full_pitch=std::max(.5f,out.positive_gain*in.limits[0]+in.models[0].bias);
  capacities[0]+=out.envelope_mix*(full_pitch-capacities[0]);
  out.budgets[1]=vector_stop::stop_budget(in.models[1],errors[1],in.rates[1],in.previous[1],in.speed,in.limits[1],capacities[1],out.slew[1],in.dt,*in.history,in.now,1,in.envelopes[1],in.models[1].bias);
  if(!out.budgets[1].valid)return out;
  out.desired_rate[1]=std::clamp(4.f*out.roll_error,-std::min(capacities[1],out.budgets[1].safe_rate),std::min(capacities[1],out.budgets[1].safe_rate));
  out.plans[1]=vector_stop::rate_plan(in.models[1],errors[1],out.desired_rate[1],0,in.rates[1],in.previous[1],in.speed,in.limits[1],capacities[1],out.slew[1],in.dt,*in.history,in.now,1,in.envelopes[1],in.models[1].bias,0);
  if(!out.plans[1].valid)return out;
  out.command[1]=out.plans[1].command;out.slew[1]=out.plans[1].slew_used;
  // Damped roll arrival also serves lift-plane alignment. The former .18s
  // rate inverse could issue full alternating inputs at just a few degrees.
  // Predict through the actual command queue, then retain the scalar stop
  // audit; the final feedback is never allowed to bypass braking.
  out.roll_pending_error=out.roll_error-out.plans[1].queued_travel;
  out.roll_terminal_mix=1.f;
  if(out.roll_terminal_mix>0){auto m=in.models[1];float omega=std::clamp(4.f/std::sqrt(m.tau),1.5f,5.f);
   float damping=2.2f*m.tau*omega-1.f;
   float wanted=roll_arrival_torque(m,out.roll_pending_error,out.plans[1].queued_rate,capacities[1])/shared_braking::gain(m,in.speed);
   wanted=std::clamp(wanted,-in.limits[1],in.limits[1]);float baseline=out.command[1];float first=baseline+out.roll_terminal_mix*(wanted-baseline);
   float lo=std::max(-in.limits[1],in.previous[1]-out.slew[1]*in.dt),hi=std::min(in.limits[1],in.previous[1]+out.slew[1]*in.dt);first=std::clamp(first,lo,hi);
   auto permitted=[&](float command){auto audit=vector_stop::assess_issued(m,errors[1],in.rates[1],in.previous[1],out.plans[1],command,in.speed,in.limits[1],capacities[1],in.dt,*in.history,in.now,1,in.envelopes[1],m.bias);
    float available=std::max(0.f,std::abs(errors[1])-out.budgets[1].margin);
    float quiet_allowance=.05f*vector_maneuver::smooth((1.f-std::abs(errors[1]))/.5f)*vector_maneuver::smooth(2.f-std::max(std::abs(in.rates[1]),std::abs(out.plans[1].queued_rate)));
    return audit.valid&&audit.stop_distance_after<=std::max(available,audit.stop_distance_before)+.003f+quiet_allowance;};
   if(!permitted(first)){float a=0,b=1;for(int n=0;n<7;++n){float t=.5f*(a+b);if(permitted(baseline+t*(first-baseline)))a=t;else b=t;}first=baseline+a*(first-baseline);out.roll_audit_fraction=a;}
   out.command[1]=first;out.plans[1].command=first;out.plans[1].model.command=first;out.plans[1].model.damping=damping;
  }
  // Reproject the same world direction after predicted roll. This is frame
  // transport inside the prediction, never an invented moving mouse reference.
  float lead=std::clamp(std::max(in.models[0].delay,in.models[2].delay)+.12f,.08f,.35f);
  float future_roll=0,forecast_rate=in.rates[1];
  for(float time=0;time<lead-1e-6f;){float step=std::min(.01f,lead-time),at=time+.5f*step-in.models[1].delay;
   float u=at<0?in.history->at_axis(uint64_t(std::max(0.0,double(in.now)+at*1000)),1):out.command[1];
   float eq=shared_braking::gain(in.models[1],in.speed)*u+in.models[1].bias,a=std::exp(-step/in.models[1].tau);
   future_roll+=eq*step+(forecast_rate-eq)*in.models[1].tau*(1-a);forecast_rate=eq+(forecast_rate-eq)*a;time+=step;}

  auto future_basis=flight::Basis{{1,0,0},{0,1,0},{0,0,1}};
  future_basis.r=rotate_rate(future_basis.r,{-1,0,0},future_roll);future_basis.u=rotate_rate(future_basis.u,{-1,0,0},future_roll);
  auto ahead_goal=local(future_basis,goal);auto ahead=direction(ahead_goal);
  // Pitch/yaw input becomes effective after the body frame has rolled. Use
  // that same projected goal for allocation and each scalar stopping budget.
  // The joint world-space forecast remains responsible for path coordination.
  bool pull_pursuit=out.maneuver.mode==vector_maneuver::Mode::Pull&&d.theta>5.f;
  bool preparing=pull_pursuit&&out.maneuver.preparing;
  if(preparing){ahead.p=0;ahead.y=std::abs(goal.y)>1e-4f?std::copysign(1.f,goal.y):0;}
  else if(pull_pursuit&&ahead.p<0){ahead.p=0;}
  if(out.maneuver.mode==vector_maneuver::Mode::Push)capacities[0]=std::min(capacities[0],std::max(2.f,.85f*out.negative_gain));
  errors[0]=preparing?0.f:std::atan2(ahead_goal.z,ahead_goal.x)/flight::rad;errors[2]=std::atan2(ahead_goal.y,ahead_goal.x)/flight::rad;
  for(int i:{0,2})out.budgets[i]=vector_stop::stop_budget(in.models[i],errors[i],in.rates[i],in.previous[i],in.speed,in.limits[i],capacities[i],out.slew[i],in.dt,*in.history,in.now,i,in.envelopes[i],in.models[i].bias);
  for(const auto& budget:out.budgets)if(!budget.valid)return out;
  // A shared rate budget prevents independent axis clipping from bending the
  // pointing motion. Every budget includes the cursor-stopping scenario.
  float pitch_cap=std::min(capacities[0],out.budgets[0].safe_rate),yaw_cap=std::min(capacities[2],out.budgets[2].safe_rate);
  out.pitch_capacity=capacities[0];out.pitch_stop_cap=pitch_cap;
  // A nearly aligned axis can have a zero scalar stopping budget (queued
  // trim/noise exceeds its tiny residual). Requiring exact rate collinearity
  // then freezes the OTHER axis despite degrees of safe distance remaining.
  // Allow a bounded rate-projection error only on a minor, sub-0.2-degree
  // coordinate. The physical desired rate is still clipped to its original
  // stopping/reachability interval; no braking distance is added or waived.
  std::array<float,3> coordination_slack{};
  for(int axis:{0,2}){
   float component=std::abs(axis==0?ahead.p:ahead.y);
   float minor=vector_maneuver::smooth((.20f-component)/.10f);
   float horizon=std::max(.18f,in.models[axis].delay+.18f);
   coordination_slack[axis]=minor*std::min(.6f,std::max(0.f,.20f-std::abs(errors[axis]))/horizon);
  }
  float ability=common_cap(ahead.p,ahead.y,pitch_cap+coordination_slack[0],yaw_cap+coordination_slack[2]);
  float requested=approach_request(d.theta,ability);out.requested_rate=requested;
  out.common_rate=std::clamp(requested,0.f,ability);
  auto reference_omega=flight::V{0,-in.reference[0],in.reference[2]};
  float reference_p=preparing?0.f:-flight::dot(reference_omega,future_basis.r),reference_y=flight::dot(reference_omega,future_basis.u);
  float credit=std::min(1.f,common_cap(reference_p,reference_y,pitch_cap,yaw_cap));
  float side=std::clamp(-4.f*out.path_error,-6.f,6.f);
  float side_p=preparing?0.f:flight::dot(normal,future_basis.u),side_y=flight::dot(normal,future_basis.r);
  float side_scale=common_cap(side_p,side_y,capacities[0],capacities[2]);side=std::clamp(side,-side_scale,side_scale);
  // Solve one feasible alpha interval after adding the path-return motion.
  std::array<float,3> reachable_low{},reachable_high{};
  for(int axis:{0,2}){
   auto reachable=vector_stop::reachable_rates(in.models[axis],in.rates[axis],in.previous[axis],in.speed,in.limits[axis],out.slew[axis],in.dt,*in.history,in.now,axis,.18f,in.models[axis].bias,axis==0?out.positive_gain:0,axis==0?out.negative_gain:0);
   if(!reachable.valid)return out;
   if(axis==0)out.pitch_reachable_high=reachable.upper;
   float capacity=axis==0?pitch_cap:yaw_cap;
   reachable_low[axis]=std::max(-capacity,reachable.lower);reachable_high[axis]=std::min(capacity,reachable.upper);
  }
  struct Allocation{bool valid=false;float alpha=0,p=0,y=0;};
  auto allocate=[&](float correction){Allocation a;float offsets[3]{credit*reference_p+correction*side*side_p,0,credit*reference_y+correction*side*side_y};float low=0,high=out.common_rate;
   for(int axis:{0,2}){float component=axis==0?ahead.p:ahead.y,lo=reachable_low[axis],hi=reachable_high[axis];if(lo>hi)return a;
    lo-=coordination_slack[axis];hi+=coordination_slack[axis];
    if(std::abs(component)<1e-5f){if(offsets[axis]<lo-1e-5f||offsets[axis]>hi+1e-5f)return a;continue;}
    float left=(lo-offsets[axis])/component,right=(hi-offsets[axis])/component;if(left>right)std::swap(left,right);low=std::max(low,left);high=std::min(high,right);}
   if(high<low||high<0)return a;a.valid=true;a.alpha=std::clamp(out.common_rate,std::max(0.f,low),high);
   a.p=std::clamp(a.alpha*ahead.p+offsets[0],reachable_low[0],reachable_high[0]);
   a.y=std::clamp(a.alpha*ahead.y+offsets[2],reachable_low[2],reachable_high[2]);return a;};
  auto allocation=allocate(1);
  if(!allocation.valid){auto direct=allocate(0);
   if(direct.valid){float lo=0,hi=1;allocation=direct;for(int n=0;n<12;++n){float middle=.5f*(lo+hi);auto a=allocate(middle);if(a.valid){lo=middle;allocation=a;}else hi=middle;}}
   else{
    // An already-issued orthogonal momentum can make the common ray impossible.
    // Keep that axis' stop priority, while the other axis retains safe progress.
    allocation.alpha=0;
    for(int axis:{0,2}){float wanted=(axis==0?ahead.p:ahead.y)*out.common_rate+(axis==0?credit*reference_p:credit*reference_y),value=0;
     if(reachable_low[axis]<=reachable_high[axis]&&!out.budgets[axis].priority)value=std::clamp(wanted,reachable_low[axis],reachable_high[axis]);
     if(axis==0)allocation.p=value;else allocation.y=value;}
   }
  }
  out.common_rate=allocation.alpha;
  out.desired_rate[0]=allocation.p;out.desired_rate[2]=allocation.y;
  if(pull_pursuit)out.desired_rate[0]=std::max(0.f,out.desired_rate[0]);
  for(int i:{0,2}){out.plans[i]=vector_stop::rate_plan(in.models[i],errors[i],out.desired_rate[i],i==1?0.f:in.reference[i],in.rates[i],in.previous[i],in.speed,in.limits[i],capacities[i],out.slew[i],in.dt,*in.history,in.now,i,in.envelopes[i],in.models[i].bias,0);
   if(!out.plans[i].valid)return out;out.command[i]=out.plans[i].command;out.slew[i]=out.plans[i].slew_used;}
  // Quiet sub-degree motion needs measured feedback instead of repeated large
  // inverse-model pulses under a wide cold envelope. High angular momentum
  // and queued commands retain the full stopping policy above.
  auto smooth=[](float x){x=std::clamp(x,0.f,1.f);return x*x*(3-2*x);};
  float py=std::hypot(in.rates[0],in.rates[2]),pending_py=std::hypot(out.plans[0].queued_rate,out.plans[2].queued_rate);
  // Retain the local feedback through a small rebound. Target jumps and
  // renewed large/moving maneuvers immediately release this terminal context.
  if(in.target_jump>2.5f||d.theta>2.5f+1.5f*calibrated_terminal||std::hypot(in.reference[0],in.reference[2])>5.f)terminal_retained=false;
  if(d.theta<.8f&&py<3.f&&pending_py<4.f)terminal_retained=true;
  out.terminal_retained=terminal_retained?1.f:0.f;
  float terminal_radius=terminal_retained?3.2f+calibrated_terminal:1.2f+1.4f*calibrated_terminal;
  out.terminal_mix=smooth((terminal_radius-d.theta)/(terminal_radius-.35f))*smooth((3.f+2*calibrated_terminal-py)/(1.5f+.5f*calibrated_terminal))*smooth((4.f+2*calibrated_terminal-pending_py)/(2.f+calibrated_terminal))*smooth((3.f-std::hypot(in.reference[0],in.reference[2]))/1.f);
  // Basis rotation changes the pitch/yaw components of one world-space
  // pointing velocity. Feed this transport acceleration into the same
  // delayed actuator model, then enforce the issued stopping audit.
  if(std::abs(in.rates[1])>1.f)for(int i:{0,2}){
   float coupling=in.rates[1]*flight::rad*(i==0?in.rates[2]:-in.rates[0]);
   auto m=in.models[i];float torque=m.tau*coupling;
   float k=i==0?(out.command[i]>=0?out.positive_gain:out.negative_gain):shared_braking::gain(m,in.speed);
   float wanted=out.command[i]+torque/k;
   wanted=std::clamp(wanted,std::max(-in.limits[i],in.previous[i]-out.slew[i]*in.dt),std::min(in.limits[i],in.previous[i]+out.slew[i]*in.dt));
   auto audit=vector_stop::assess_issued(m,errors[i],in.rates[i],in.previous[i],out.plans[i],wanted,in.speed,in.limits[i],capacities[i],in.dt,*in.history,in.now,i,in.envelopes[i],m.bias);
   if(audit.valid&&audit.acceptable)out.command[i]=wanted;
  }
  if(out.terminal_mix>0)for(int i:{0,2}){float component=i==0?d.p:d.y,motion=in.reference[i],error=d.theta*component;
   auto m=in.models[i];float latency=m.tau/(m.tau+2*m.delay),damping=2.4f*latency,position=4.f*latency;
   float old_torque=motion+position*error-damping*(in.rates[i]-motion)-m.bias;
   // Predict the measured residual/rate at the end of the already-issued
   // delay queue. Damped local feedback can then close promptly
   // without braking merely because the current nose is near the cursor.
   // Reduce cold terminal bandwidth only when measured endpoints contradict
   // its predictions. Accurate models retain their responsive local loop.
   float cold_mismatch=(1-calibrated_terminal)*smooth((.5f-prediction_weight[i])/.35f);
   float omega=std::clamp((6.f-1.5f*cold_mismatch)/std::sqrt(m.tau),2.5f,10.f);
   bool spatial=in.rate_qualified[0]&&in.rate_qualified[1]&&in.rate_qualified[2];
   float future_error=error+motion*m.delay-out.plans[i].queued_travel,future_rate=out.plans[i].queued_rate;
   if(spatial){auto pose=vector_pending_pose::predict(in.models,in.rates,in.previous,out.positive_gain,out.negative_gain,*in.history,in.now,m.delay,in.speed);
    auto future_goal=local(pose.body,rotate_rate(goal,{0,-in.reference[0],in.reference[2]},m.delay));
    future_error=std::atan2(i==0?future_goal.z:future_goal.y,future_goal.x)/flight::rad;future_rate=pose.rates[i];
    auto reference_omega=flight::V{0,-in.reference[0],in.reference[2]};
    motion=i==0?-flight::dot(reference_omega,pose.body.r):flight::dot(reference_omega,pose.body.u);
   }
   out.pending_error[i]=future_error;
   float predictive_damping=(1.6f+.4f*calibrated_terminal+.4f*cold_mismatch)*m.tau*omega-1;
   float predicted_torque=motion+m.tau*omega*omega*future_error-predictive_damping*(future_rate-motion)-m.bias;
   float quiet_roll=(1-calibrated_terminal+calibrated_terminal*prediction_weight[i])*(spatial?1.f:smooth((45.f-std::abs(in.rates[1]))/20.f));
   float torque=old_torque+quiet_roll*(predicted_torque-old_torque);damping+=quiet_roll*(predictive_damping-damping);
   float gain=i==0?(torque>=0?out.positive_gain:out.negative_gain):shared_braking::gain(m,in.speed);float wanted=torque/gain;
   wanted=std::clamp(wanted,-in.limits[i],in.limits[i]);float first=out.command[i]+out.terminal_mix*(wanted-out.command[i]);
   first=std::clamp(first,std::max(-in.limits[i],in.previous[i]-out.slew[i]*in.dt),std::min(in.limits[i],in.previous[i]+out.slew[i]*in.dt));
   // Calibrated quiet control may use a 0.05-degree terminal tube. This
   // keeps a vanishing scalar coordinate from suppressing every correction
   // during roll. Large-angle/high-rate plans retain their original bounds.
   auto permitted=[&](float command){auto audit=vector_stop::assess_issued(in.models[i],errors[i],in.rates[i],in.previous[i],out.plans[i],command,in.speed,in.limits[i],capacities[i],in.dt,*in.history,in.now,i,in.envelopes[i],m.bias);
    float available=std::max(0.f,std::abs(errors[i])-out.budgets[i].margin);
    return audit.valid&&audit.stop_distance_after<=std::max(available,audit.stop_distance_before)+.003f+out.terminal_stop_allowance;};
   if(!permitted(first)){float lo=0,hi=1;for(int n=0;n<6;++n){float a=.5f*(lo+hi);if(permitted(out.command[i]+a*(first-out.command[i])))lo=a;else hi=a;}first=out.command[i]+lo*(first-out.command[i]);out.terminal_audit_fraction[i]=lo;}
   out.command[i]=first;
   out.plans[i].command=out.command[i];out.plans[i].model.command=out.command[i];out.plans[i].model.damping=damping;
  }
  // Model-based full-angle correction and first-command audit are separate
  // from the guidance. Priority braking may only be strengthened by search.
  // Joint corrections are useful when rotation or transverse motion couples
  // the axes. A pure single-axis arrival keeps its direct stopping solution.
  out.joint_activity=std::max({smooth((std::abs(in.rates[1])-1.f)/4.f),smooth((std::abs(out.roll_error)-1.f)/4.f),
   smooth((std::abs(out.transverse_rate)-.05f)/.35f),smooth((std::abs(out.path_error)-.02f)/.08f),
   smooth((std::min(std::abs(errors[0]),std::abs(errors[2]))-.05f)/.15f)});
  if(out.joint_activity>.001f&&(out.terminal_mix<=.001f||out.roll_terminal_mix<=.001f))refine(in,normal,goal,errors,capacities,out);
  // Pitch/yaw quiet feedback was screened against available stopping distance
  // above. Record risk for the final issued commands, including roll feedback.
  else for(int i=0;i<3;++i){auto audit=vector_stop::assess_issued(in.models[i],errors[i],in.rates[i],in.previous[i],out.plans[i],out.command[i],in.speed,in.limits[i],capacities[i],in.dt,*in.history,in.now,i,in.envelopes[i],in.models[i].bias);
   if(audit.valid){out.plans[i].stop_distance=audit.stop_distance_after;out.plans[i].finite_crossing=audit.finite_crossing_after;}}
  out.valid=true;return out;
 }
 void refine(const Input& in,flight::V normal,flight::V goal,const std::array<float,3>& errors,const std::array<float,3>& caps,Result& out){
  struct Trial {bool valid=false;float cost=INFINITY,peak_path=INFINITY;};
  constexpr int scenarios=3,max_frames=64;
  const float wanted_horizon=std::clamp(std::max({in.models[0].delay,in.models[1].delay,in.models[2].delay})+.22f,.30f,.55f);
  const float forecast_step=in.dt;
  const int frames=std::clamp(1+int(std::ceil((wanted_horizon-in.dt)/forecast_step)),4,max_frames);
  auto evaluate=[&](std::array<float,3> first)->Trial{
   std::array<std::array<float,3>,max_frames> source{};
   auto state=in.rates;flight::Basis b{{1,0,0},{0,1,0},{0,0,1}};auto target=goal;
   std::array<float,3> commanded=first;auto forecast_roll=roll;auto forecast_maneuver=maneuver;
   for(int n=0;n<frames;++n){float time=n?in.dt+(n-1)*forecast_step:0.f,step=n?forecast_step:in.dt;
    if(n){float elapsed=n==1?in.dt:forecast_step;auto fd=direction(local(b,target));float ability=common_cap(fd.p,fd.y,caps[0],caps[2]);float cr=std::min(ability,approach_request(fd.theta,ability));
     auto predicted_goal=local(b,target);
     vector_maneuver::Input mi;mi.goal=predicted_goal;mi.models=in.models;mi.speed=in.speed;mi.dt=elapsed;mi.theta=fd.theta;mi.roll_rate=state[1];mi.positive_rate=out.positive_gain;mi.negative_rate=out.negative_gain;mi.roll_rate_cap=caps[1];
     auto maneuver_goal=forecast_maneuver.update(mi);
     capture_roll::Input ri;ri.active=true;ri.dt=elapsed;ri.angle=fd.theta;ri.up=predicted_goal.z;ri.right=predicted_goal.y;ri.turn_error=maneuver_goal.roll_error;
     ri.level_error=std::atan2(flight::dot(b.r,in.world_up),flight::dot(b.u,in.world_up))/flight::rad;ri.pole_clearance=std::hypot(flight::dot(b.r,in.world_up),flight::dot(b.u,in.world_up));
     ri.pitch_rate=state[0];ri.yaw_rate=state[2];ri.roll_rate=state[1];ri.pursuit_rate=caps[1];
     float rr=forecast_roll.update(ri).roll_error;
     if(out.maneuver.mode==vector_maneuver::Mode::Pull&&fd.p<0)fd.p=0;
     std::array<float,3> wanted{cr*fd.p,std::clamp(4*rr,-caps[1],caps[1]),cr*fd.y};
     for(int i=0;i<3;++i){auto m=in.models[i];float a=std::exp(-.18f/m.tau),torque=(wanted[i]-state[i]*a-m.bias*(1-a))/(1-a),k=i==0?(torque>=0?out.positive_gain:out.negative_gain):shared_braking::gain(m,in.speed),u=torque/k;
      if(i==1){
       float eq=k*commanded[1]+m.bias,decay=std::exp(-m.delay/m.tau),qr=eq+(state[1]-eq)*decay;
       float travel=eq*m.delay+(state[1]-eq)*m.tau*(1-decay);
       float feedback=roll_arrival_torque(m,rr-travel,qr,caps[1])/k;
       u=feedback;}
      commanded[i]+=std::clamp(std::clamp(u,-in.limits[i],in.limits[i])-commanded[i],-out.slew[i]*elapsed,out.slew[i]*elapsed);}
    }
    source[n]=commanded;
    std::array<float,3> travel{};
    for(int i=0;i<3;++i){auto m=in.models[i];float at=time+.5f*step-m.delay,u=at<0?in.history->at_axis(in.now-uint64_t(-at*1000),i):source[std::min(n,at<in.dt?0:1+int((at-in.dt)/forecast_step))][i];float k=i==0?(u>=0?out.positive_gain:out.negative_gain):shared_braking::gain(m,in.speed);float eq=k*u+m.bias,a=std::exp(-step/m.tau);travel[i]=eq*step+(state[i]-eq)*m.tau*(1-a);state[i]=eq+(state[i]-eq)*a;}
    auto omega=b.f*(-travel[1])+b.r*(-travel[0])+b.u*travel[2];b.f=rotate_rate(b.f,omega,1);b.r=rotate_rate(b.r,omega,1);b.u=rotate_rate(b.u,omega,1);
    float reference_dt=std::max(0.f,1-time/.10f)*step;target=rotate_rate(target,{0,-in.reference[0],in.reference[2]},reference_dt);
   }
   Trial trial;trial.valid=true;trial.cost=0;trial.peak_path=0;
   for(int scenario=0;scenario<scenarios;++scenario){state=in.rates;b={{1,0,0},{0,1,0},{0,0,1}};target=goal;float cost=0,peak=0;
    for(int n=0;n<frames;++n){float time=n?in.dt+(n-1)*forecast_step:0.f,step=n?forecast_step:in.dt;std::array<float,3> travel{};
     for(int i=0;i<3;++i){auto m=in.models[i];auto env=in.envelopes[i];float sign=scenario==0?0.f:scenario==1?(i==1?1.f:-1.f):(i==1?-1.f:1.f);
      m.g0*=1+sign*std::min(.25f,env.gain_fraction);m.g1*=1+sign*std::min(.25f,env.gain_fraction);m.tau*=1-sign*std::min(.35f,env.tau_fraction);m.delay=std::clamp(m.delay-sign*std::min(.06f,env.delay_seconds),0.f,.45f);
      float at=time+.5f*step-m.delay,u=at<0?in.history->at_axis(in.now-uint64_t(-at*1000),i):source[std::min(n,at<in.dt?0:1+int((at-in.dt)/forecast_step))][i];float k=i==0?(u>=0?out.positive_gain:out.negative_gain)*(1+sign*std::min(.25f,env.gain_fraction)):shared_braking::gain(m,in.speed);float eq=k*u+m.bias,a=std::exp(-step/m.tau);
      travel[i]=eq*step+(state[i]-eq)*m.tau*(1-a);state[i]=eq+(state[i]-eq)*a;
     }
     auto omega=b.f*(-travel[1])+b.r*(-travel[0])+b.u*travel[2];b.f=rotate_rate(b.f,omega,1);b.r=rotate_rate(b.r,omega,1);b.u=rotate_rate(b.u,omega,1);
     if(scenario!=1){float reference_dt=std::max(0.f,1-time/.10f)*step;target=rotate_rate(target,{0,-in.reference[0],in.reference[2]},reference_dt);}
     float error=angle(b.f,target),path=std::asin(std::clamp(flight::dot(b.f,normal),-1.f,1.f))/flight::rad;
     float bank=std::atan2(flight::dot(b.r,in.world_up),flight::dot(b.u,in.world_up))/flight::rad;
     float clearance=std::hypot(flight::dot(b.r,in.world_up),flight::dot(b.u,in.world_up));
     float negative_maneuver=out.maneuver.mode==vector_maneuver::Mode::Pull&&out.theta>5?std::max(0.f,-state[0]):0;
     cost+=(4*negative_maneuver*negative_maneuver+error*error+6*path*path+(out.leveling&&clearance>.1f?.0015f*bank*bank:0))*step;peak=std::max(peak,std::abs(path));
     if(n==frames-1)cost+=3*error*error+4*path*path;
    }
    trial.cost+=cost*(scenario? .15f:1.f);trial.peak_path=std::max(trial.peak_path,peak);
   }
   for(int i=0;i<3;++i)trial.cost+=.02f*(first[i]-out.command[i])*(first[i]-out.command[i]);
   return trial;
  };
  auto baseline=out.command;auto best=evaluate(baseline);if(!best.valid||!std::isfinite(best.cost))return;
  out.predicted_path_before=out.predicted_path_after=best.peak_path;
  for(int pass=0;pass<2;++pass)for(int axis=0;axis<3;++axis)for(int sign:{-1,1}){
   if((axis==1?out.roll_terminal_mix:out.terminal_mix)>.001f)continue;
   out.joint_axes|=1u<<axis;
   auto candidate=out.command;float radius=out.joint_activity*(axis==1?.045f:.035f)*(pass?.5f:1.f);
   float lo=std::max(-in.limits[axis],in.previous[axis]-out.slew[axis]*in.dt),hi=std::min(in.limits[axis],in.previous[axis]+out.slew[axis]*in.dt);
   candidate[axis]=std::clamp(candidate[axis]+sign*radius,lo,hi);
   if(out.plans[axis].priority||out.plans[axis].holding_brake){float physical=std::abs(in.rates[axis])>.1f?in.rates[axis]:out.plans[axis].queued_rate;float brake_sign=std::copysign(1.f,physical);if(brake_sign*(candidate[axis]-out.command[axis])>0)continue;}
   if(std::abs(candidate[axis]-out.command[axis])<1e-7f)continue;
   auto audit=vector_stop::assess_issued(in.models[axis],errors[axis],in.rates[axis],in.previous[axis],out.plans[axis],candidate[axis],in.speed,in.limits[axis],caps[axis],in.dt,*in.history,in.now,axis,in.envelopes[axis],in.models[axis].bias);
   if(!audit.valid||!audit.acceptable)continue;
   auto trial=evaluate(candidate);if(trial.valid&&trial.cost<best.cost-1e-5f&&trial.peak_path<=best.peak_path+std::min(.15f,.003f*out.theta+.01f)){out.command=candidate;best=trial;out.predicted_path_after=trial.peak_path;}
  }
  for(int i=0;i<3;++i){out.joint_delta+=std::abs(out.command[i]-baseline[i]);auto audit=vector_stop::assess_issued(in.models[i],errors[i],in.rates[i],in.previous[i],out.plans[i],out.command[i],in.speed,in.limits[i],caps[i],in.dt,*in.history,in.now,i,in.envelopes[i],in.models[i].bias);
   if(!audit.valid||!audit.acceptable)out.command[i]=baseline[i];else {out.plans[i].stop_distance=audit.stop_distance_after;out.plans[i].finite_crossing=audit.finite_crossing_after;out.plans[i].command=out.command[i];out.plans[i].model.command=out.command[i];}}
 }
};
}


