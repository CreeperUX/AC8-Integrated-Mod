#pragma once
#include "model_control.h"
#include "flight_math.h"
#include "ac8_aircraft_models.h"

// WAR v9 retains PEACE roll guidance/leveling and uses its validated models.
// Optional pitch/yaw authority is accepted only after a common-frame preview.
namespace peace_agile {
inline float smooth(float x){x=std::clamp(x,0.f,1.f);return x*x*(3-2*x);}
struct Coordination {
 bool valid=false;
 float theta=0,along=0,transverse=0,roll_weight=0,path_weight=0,weight=0;
};
inline Coordination coordinate(flight::V goal,const std::array<float,3>& rates,
 const std::array<float,3>& reference,float response_time){
 Coordination o;
 if(!std::isfinite(goal.x)||!std::isfinite(goal.y)||!std::isfinite(goal.z)||
    flight::dot(goal,goal)<.5f||!std::isfinite(response_time)||response_time<=0)return o;
 for(int i=0;i<3;++i)if(!std::isfinite(rates[i])||!std::isfinite(reference[i]))return o;
 goal=flight::unit(goal);float length=std::hypot(goal.y,goal.z);
 o.theta=std::atan2(length,goal.x)/flight::rad;
 float p=length>1e-5f?goal.z/length:1.f,y=length>1e-5f?goal.y/length:0.f;
 float q=rates[0]-reference[0],r=rates[2]-reference[2];
 o.along=q*p+r*y;o.transverse=q*y-r*p;
 // Angle through which the pointing axes rotate during their response.
 float rotation=std::abs(rates[1])*flight::rad*std::clamp(response_time,.1f,1.5f);
 o.roll_weight=1-smooth((rotation-.12f)/.40f);
 float side_budget=1.f+.20f*std::max(0.f,o.along);
 o.path_weight=1-smooth((std::abs(o.transverse)-side_budget)/(2.f+side_budget));
 o.weight=o.roll_weight*o.path_weight*smooth((o.theta-5.f)/10.f);
 o.valid=true;return o;
}
struct Selection {model_control::Plan plan;float boost=0;};
inline Selection select(const model_control::Plan& peace,const model_control::Plan& agile,
 const Coordination& coordination,int axis,float error,float mode_mix){
 Selection out;out.plan=peace;
 // Roll, terminal corrections, uncertain observations and downward pitch pursuit
 // keep PEACE. AGILE's own stopping-distance test still controls its pursuit.
 if(axis==1||!peace.valid||!agile.valid||!coordination.valid||
    !std::isfinite(error)||!std::isfinite(mode_mix)||axis<0||axis>2||
    (axis==0&&error<=0)||error*(agile.command-peace.command)<=0)return out;
 out.boost=std::clamp(mode_mix,0.f,1.f)*coordination.weight;
 auto blend=[&](float a,float b){return a+out.boost*(b-a);};
 out.plan.command=blend(peace.command,agile.command);
 out.plan.stopping_angle=blend(peace.stopping_angle,agile.stopping_angle);
 out.plan.horizon=blend(peace.horizon,agile.horizon);
 out.plan.rate_limit=blend(peace.rate_limit,agile.rate_limit);
 out.plan.pursuit=blend(peace.pursuit,agile.pursuit);
 out.plan.damping=blend(peace.damping,agile.damping);
 out.plan.input_limit=blend(peace.input_limit,agile.input_limit);
 return out;
}
}

namespace peace_agile {
// Kinematic coupling only: independent first-order actuator responses are
// integrated in ONE rotating frame. No unmeasured aerodynamic cross terms.
struct Preview {
 bool valid=false;float cost=0,path=0,crossing=0,stop=0,roll_travel=0,error=0;
};
struct JointInput {
 std::array<shared_braking::Model,3> models;
 std::array<float,3> rates{},previous{},errors{},references{},caps{},damping{},limits{.85f,1,.7f},slews{12,16,6};
 flight::V goal{1,0,0};float speed=400,dt=.02f;uint64_t now=0;
 // Body-frame direction where the current pointing maneuver started (WAR v11.1
 // anchored target plane). {1,0,0} = the current nose (no anchor).
 flight::V path_origin{1,0,0};
 // Measured AC8 aircraft characteristics (push/pull ratio etc.); nullptr = unknown.
 const ac8_models::Aircraft* aircraft=nullptr;
};
inline flight::V turn(flight::V v,flight::V degrees){
 float angle=std::sqrt(flight::dot(degrees,degrees));
 return angle>1e-7f?flight::rotate(v,degrees*(1/angle),angle*flight::rad):v;
}
inline float angle(flight::V a,flight::V b){return std::atan2(std::sqrt(flight::dot(flight::cross(a,b),flight::cross(a,b))),flight::dot(a,b))/flight::rad;}
inline Preview preview(const JointInput& in,const std::array<float,3>& command,const adaptive_braking::History& history){
 Preview out;float horizon=.45f;
 if(!std::isfinite(in.dt)||in.dt<=0||in.dt>.1f||!std::isfinite(in.speed)||!std::isfinite(flight::dot(in.goal,in.goal))||flight::dot(in.goal,in.goal)<.5f)return out;
 std::array<float,3> gain{};
 for(int i=0;i<3;++i){auto m=in.models[i];gain[i]=shared_braking::gain(m,std::clamp(in.speed,130.f,660.f));
  if(!std::isfinite(gain[i])||gain[i]<1||gain[i]>600||!std::isfinite(m.tau)||m.tau<.2f||m.tau>3||!std::isfinite(m.delay)||m.delay<0||m.delay>.5f||!std::isfinite(command[i])||!std::isfinite(in.rates[i])||!std::isfinite(in.references[i])||!std::isfinite(in.previous[i]))return out;
  if(in.now<uint64_t(m.delay*1000)||!online_learning::continuous(history,in.now-uint64_t(m.delay*1000),in.now))return out;
  horizon=std::max(horizon,m.delay+.35f*m.tau);
 }
 horizon=std::min(horizon,.85f);const float ds=horizon/24;
 flight::Basis b{{1,0,0},{0,1,0},{0,0,1}};auto target=flight::unit(in.goal);auto rates=in.rates;
 auto target_motion=flight::V{0,-in.references[0],in.references[2]};
 float path_sum=0,negative_exposure=0;
 for(int step=0;step<24;++step){float t=(step+.5f)*ds;std::array<float,3> travel{};
  for(int i=0;i<3;++i){auto m=in.models[i];float pending=t-m.delay,u;
   if(pending<0)u=history.at_axis(in.now-uint64_t(-pending*1000),i);
   else u=std::clamp(command[i],in.previous[i]-in.slews[i]*(pending+in.dt),in.previous[i]+in.slews[i]*(pending+in.dt));
   float eq=gain[i]*u,decay=std::exp(-ds/m.tau);
   travel[i]=eq*ds+(rates[i]-eq)*m.tau*(1-decay);rates[i]=eq+(rates[i]-eq)*decay;
   if(i==0)negative_exposure+=ds*std::max(0.f,-u); // Input exposure, NOT measured negative G.
  }
  auto rotation=b.f*(-travel[1])+b.r*(-travel[0])+b.u*travel[2];
  b.f=turn(b.f,rotation);b.r=turn(b.r,rotation);b.u=turn(b.u,rotation);
  target=turn(target,target_motion*ds);out.roll_travel+=travel[1];
  auto normal=flight::unit(flight::cross({1,0,0},target));
  float path=std::abs(std::asin(std::clamp(flight::dot(b.f,normal),-1.f,1.f))/flight::rad);
  float remaining=std::atan2(flight::dot(normal,flight::cross(b.f,target)),flight::dot(b.f,target))/flight::rad;
  out.path=std::max(out.path,path);out.crossing=std::max(out.crossing,-remaining);path_sum+=path*path/24;
 }
 out.error=angle(b.f,target);
 // Penalize residual pointing motion using a conservative stopping-distance
 // estimate. The same command delay and roll recovery burden apply to all candidates.
 auto relative=b.r*(-rates[0])+b.u*rates[2]-target_motion;
 float closing=std::max(0.f,flight::dot(relative,flight::unit(flight::cross(b.f,target))));
 auto to=flight::unit(flight::cross(b.f,target));
 float pitch_share=std::abs(flight::dot(to,b.r)),yaw_share=std::abs(flight::dot(to,b.u));
 float brake=std::max(1.f,1/std::max(.001f,std::hypot(pitch_share/(gain[0]*in.limits[0]),yaw_share/(gain[2]*in.limits[2]))));
 float tau=std::max(in.models[0].tau,in.models[2].tau),delay=std::max(in.models[0].delay,in.models[2].delay);
 out.stop=closing*delay+tau*(closing-brake*std::log1p(closing/brake));
 float excess=std::max(0.f,1.5f*out.stop-out.error);
 float roll_stop=in.models[1].tau*(std::abs(rates[1])-gain[1]*std::log1p(std::abs(rates[1])/gain[1]));
 out.cost=out.error*out.error+2*path_sum+4*excess*excess+6*out.crossing*out.crossing+.02f*(std::abs(out.roll_travel)+roll_stop)+.5f*negative_exposure;
 out.valid=std::isfinite(out.cost);return out;
}
struct JointSelection {
 std::array<model_control::Plan,3> plans{};Preview before{},after{};
 int choice=0;bool checked=false;float transport_deg=0;
};
inline JointSelection choose(const JointInput& in,const adaptive_braking::History& history,
 const std::array<model_control::Plan,3>& peace,const std::array<model_control::Plan,3>& boosted,float mix){
 JointSelection out;out.plans=peace;
 for(auto p:peace)if(!p.valid)return out;
 auto commands=[](const auto& plans){return std::array<float,3>{plans[0].command,plans[1].command,plans[2].command};};
 out.before=preview(in,commands(peace),history);out.after=out.before;out.checked=out.before.valid;
 float theta=angle({1,0,0},flight::unit(in.goal));
 if(!out.checked||mix<=0||theta<=5)return out;
 auto consider=[&](const std::array<model_control::Plan,3>& plans,int choice){
  for(auto p:plans)if(!p.valid)return;
  auto trial=preview(in,commands(plans),history);
  if(trial.valid&&trial.cost<out.after.cost-.002f&&trial.path<=out.before.path+.05f&&trial.crossing<=out.before.crossing+.02f&&std::max(0.f,1.5f*trial.stop-trial.error)<=std::max(0.f,1.5f*out.before.stop-out.before.error)+.02f){out.plans=plans;out.after=trial;out.choice=choice;}
 };
 consider(boosted,1);
 // Mid-horizon transport anticipates the rotation of pitch/yaw axes during
 // PEACE's roll. The preview independently checks whether it actually helps.
 float rotation=std::clamp(.5f*out.before.roll_travel,-15.f,15.f)*flight::rad;
 out.transport_deg=rotation/flight::rad;
 float c=std::cos(rotation),s=std::sin(rotation);auto goal=flight::unit(in.goal);
 auto transported=flight::V{goal.x,goal.y*c-goal.z*s,goal.y*s+goal.z*c};
 float fraction=std::clamp(mix,0.f,1.f)*smooth((theta-5)/10.f)*.5f;
 auto corrected=out.plans;
 for(int axis:{0,2}){
  float future=std::atan2(axis==0?transported.z:transported.y,std::max(.02f,transported.x))/flight::rad;
  float error=in.errors[axis]+std::clamp(future-in.errors[axis],-8.f,8.f)*fraction;
  // Do not turn a local downward correction into an opposite-sign pull.
  if(error*in.errors[axis]<0)error=0;
  auto ref=in.references;float reference=axis==0?ref[0]*c+ref[2]*s:ref[2]*c-ref[0]*s;
  corrected[axis]=model_control::predict(in.models[axis],error,in.rates[axis],in.previous[axis],in.speed,in.limits[axis],in.caps[axis],.2f,history,in.now,axis,ref[axis]+fraction*(reference-ref[axis]),in.damping[axis],false);
 }
 consider(corrected,2);return out;
}
}
