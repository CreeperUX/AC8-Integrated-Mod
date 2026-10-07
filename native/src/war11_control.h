#pragma once
#include "war10_control.h"
// WAR v11: v10 feasible rate guidance, made robust to the unknown push/pull
// authority ratio and tuned on identified real-flight plants (analysis/fit.py).
// Roll keeps the PEACE plan (roll_damping is an off-by-default research knob).
namespace war11 {
struct Params {
 // Tuned 2026-10-06 (analysis/tune.py runs 1-6; final = run 5 with the per-axis
 // endpoint fix: normalized cost 0.721 vs v10 1.0 and v9 2.62 over 9 plants:
 // identified F-15C, F-15C +80 ms, shared prior, slow, agile, low speed, weak push,
 // two Codex proxies). down_theta/cap_pitch_down are pinned to the v10 policy.
 float stop_scale=1.3104f,coast=0.1760f,deadband=0.0175f,capture_time=0.2429f;
 float brake_pitch_up=0.1681f,brake_pitch_down=0.3000f,brake_yaw=0.9500f;
 float cap_scale_pitch=1.3333333f,cap_scale_yaw=1.2857143f;
 float cap_pitch_up=0.9906f,cap_pitch_down=0.3000f,cap_yaw=0.7484f;
 // Push authority is aircraft dependent (identified 0.4..1.0 of pull, proxies
 // down to 0.18). The delay propagation must never under-predict travel toward
 // the target: pushing toward a target below assumes strong push, while a push
 // that only brakes an upward approach assumes weaker push.
 float prop_negative_down=0.8099f,prop_negative_up=0.5271f,inv_negative_down=0.6295f,inv_negative_up=0.2500f;
 float accel_base=0.1204f,accel_tau=0.1299f,accel_min=0.1660f,accel_max=0.3414f;
 float path_margin=.04f,path_floor=.12f,path_theta=6.f;
 float down_theta=12.0000f;
 float roll_damping=-1.f; // <0: keep the runtime's PEACE roll plan unchanged
 // Fraction of PEACE's remaining roll applied when sizing the feasible path
 // rate: PEACE rotates a lateral target into the strong pitch plane, so the
 // weak yaw axis must not cap the rate of a far lateral pursuit (0 = v10).
 float post_roll=1.0000f;
 float own_delay_endpoint=1.f; // 1: per-axis effective rate (v11 fix); 0: v10 behavior
 // v11.1 target-plane planning (root-cause fix for the off-axis/height loss in
 // roll+pitch turns). Pitch/yaw commands are corrected so that the PREDICTED
 // nose path -- integrated in one rotating frame with command delay, response
 // lag, slew limits and PEACE's simultaneous roll -- stays in the plane through
 // the nose and the target, while along-track progress of the base plan is kept.
 float plane_mpc=1.f;            // 0 = v11.0 behaviour
 float plane_cross_w=1.f;        // weight of predicted out-of-plane angle
 float plane_along_w=.5f;        // weight of keeping the base plan's along-track progress
 float plane_reg=.02f;           // regularization on the command change
 float plane_max=1.f;            // max |command change| per axis
 float plane_theta=3.f,plane_theta_full=6.f; // correction fades in between these pointing errors (terminal stays v11.0)
 float plane_pitch_eff=1.f;      // pitch effectiveness floor at fast roll used in the prediction (1 = none)
 float plane_no_push=1.f;
 // Anchored plane: the plane runs from where the maneuver started to the target,
 // so off-plane drift already accumulated (e.g. from prediction bias during
 // rolls) is seen as an error and corrected instead of being re-based away.
 float plane_anchor=1.f;          // 0 = plane through the current nose every frame (v11.1 first build)
 float anchor_jump=2.f;           // target step per frame (deg) that starts a new maneuver
 float anchor_max_off=6.f;        // nose this far off the anchored plane: geometry changed, re-anchor
 // WAR roll arrival. PEACE still decides WHICH bank to fly (turn bank, leveling);
 // only the tracking toward that bank uses the same feasible-rate path as pitch/yaw.
 // The flight logs show PEACE's cold roll plan (damping 2) coasting at ~0 command with
 // 20+ deg still to go. Roll response measured on all 16 AC8 aircraft (analysis/aircraft_models.py):
 // delay 0.08-0.16 s (pooled 0.121, prior 0.06), tau 0.85, gain 0.86 x prior.
 float roll_path=1.f;            // 0 = PEACE roll plan (v11.0/v11.1)
 float roll_tau=.85f,roll_delay=.121f,roll_scale=.86f;
 float roll_brake=.6f,roll_cap=.9f,roll_accel=.25f,roll_stop_scale=1.3f,roll_coast=.15f,roll_deadband=1.f;
 // v11.2 measured aircraft: with in.aircraft set, push assumptions come from the
 // measured push/pull ratio (AC8: 0.43-0.80, pooled 0.68) instead of the tuned
 // worst-case constants, and the roll path uses in.models[1] (aircraft table) as-is.
 float use_aircraft=1.f;          // 0 = v11.1 constants
 float push_brake_use=.45f;        // fraction of the measured push authority planned for braking (limits negative G)        // 1: the correction may reduce pull but never push beyond the base plan (no extra negative G)
};
inline peace_agile::JointSelection choose_base(const peace_agile::JointInput& in,const adaptive_braking::History& history,
 const std::array<model_control::Plan,3>& peace,const std::array<model_control::Plan,3>& boosted,float mix,war10::Diagnostics* diagnostic,const Params& k){
 auto out=peace_agile::choose(in,history,peace,boosted,mix);
 if(mix<=0||!out.checked)return out;
 auto goal=flight::unit(in.goal);float theta=peace_agile::angle({1,0,0},goal);
 auto roll=peace[1];
 if(k.roll_damping>0){auto r=model_control::predict(in.models[1],in.errors[1],in.rates[1],in.previous[1],in.speed,in.limits[1],in.caps[1],1.f,history,in.now,1,in.references[1],k.roll_damping,false);if(r.valid)roll=r;}
 if(goal.z<0&&theta>k.down_theta){
  if(k.roll_damping>0){auto plans=out.plans;plans[1]=roll;auto trial=peace_agile::preview(in,{plans[0].command,plans[1].command,plans[2].command},history);if(trial.valid){out.plans=plans;out.after=trial;}}
  return out;
 }
 // endpoint: rate along the common horizon (drives the predicted goal direction).
 // effective: rate when THIS axis's next command takes effect (its own delay).
 // v10 used the common-horizon rate for every axis; for a zero-delay axis that
 // feeds the previous command back through the inversion (period-2 chatter).
 std::array<float,3> endpoint=in.rates,effective=in.rates,gain{};
 // Only aircraft with measured data use their push ratio; unknown aircraft keep the
 // conservative v11.1 constants (robust down to push ratio 0.18).
 const bool measured=k.use_aircraft>0&&in.aircraft!=nullptr&&in.aircraft->known;
 const float rho=measured?std::clamp(in.aircraft->push,.15f,1.f):0.f;
 const float push_ratio=measured?rho:(goal.z<0?k.prop_negative_down:k.prop_negative_up);
 const float brake_up=measured?rho*k.push_brake_use:k.brake_pitch_up;
 const float inv_down=measured?rho:k.inv_negative_down,inv_up=measured?rho:k.inv_negative_up;
 float horizon=std::max(in.models[0].delay,in.models[2].delay),ds=horizon/20;
 auto future=goal;
 for(int i=0;i<3;++i)gain[i]=shared_braking::gain(in.models[i],std::clamp(in.speed,130.f,660.f));
 for(int j=0;j<20;++j){std::array<float,3> turn{};
  for(int i=0;i<3;++i){auto m=in.models[i];float pending=(j+.5f)*ds-m.delay;
   float u=pending<0?history.at_axis(in.now-uint64_t(-pending*1000),i):in.previous[i];
   float eq=gain[i]*u;if(i==0&&u<0)eq*=push_ratio;
   float decay=std::exp(-ds/m.tau);turn[i]=eq*ds+(endpoint[i]-eq)*m.tau*(1-decay);
   endpoint[i]=eq+(endpoint[i]-eq)*decay;
   if(k.own_delay_endpoint&&(j+1)*ds<=m.delay+1e-5f)effective[i]=endpoint[i];
  }
  future=peace_agile::turn(future,{turn[1],turn[0]-in.references[0]*ds,-turn[2]+in.references[2]*ds});
 }
 float length=peace_agile::angle({1,0,0},future),radial=std::hypot(future.y,future.z);
 if(radial<1e-6f)return out;
 float p=future.z/radial,y=future.y/radial;
 std::array<float,3> errors{p*length,in.errors[1],y*length};
 war10::Params t10;t10.stop_scale=k.stop_scale;t10.coast=k.coast;t10.deadband=k.deadband;t10.capture_time=k.capture_time;
 float speed=1000,slope=0;
 // Post-roll shares (positive roll error rolls right: target right -> above).
 float phi=std::clamp(in.errors[1],-180.f,180.f)*flight::rad*k.post_roll;
 float p_post=y*std::sin(phi)+p*std::cos(phi),y_post=y*std::cos(phi)-p*std::sin(phi);
 for(int i:{0,2}){
  float share=std::abs(i==0?p_post:y_post);if(share<.02f)continue;
  bool down=i==0&&p_post<0;
  float cap=std::min(in.caps[i]*(i==0?k.cap_scale_pitch:k.cap_scale_yaw),gain[i]*(i==0?(down?k.cap_pitch_down:k.cap_pitch_up):k.cap_yaw));
  auto t=war10::target(length,gain[i]/share,in.models[i].tau,cap/share,i==0?(down?k.brake_pitch_down:brake_up):k.brake_yaw,t10);
  if(t.rate<speed){speed=t.rate;slope=t.slope;}
 }
 if(!k.own_delay_endpoint)effective=endpoint;
 auto plans=out.plans;
 auto coordination=peace_agile::coordinate(goal,in.rates,in.references,std::max(in.models[0].delay+.3f*in.models[0].tau,in.models[2].delay+.3f*in.models[2].tau));
 float weight=std::clamp(mix,0.f,1.f);
 for(int i:{0,2}){
  float share=i==0?p:y,sign=std::copysign(1.f,share),w=sign*(effective[i]-in.references[i]);
  float desired=std::abs(share)*speed;
  float acceleration=(desired-w)/std::clamp(k.accel_base+k.accel_tau*in.models[i].tau,k.accel_min,k.accel_max)-std::min(std::max(0.f,w),desired)*slope;
  float transport=(i==0?y:-p)*speed*in.rates[1]*flight::rad;
  float equilibrium=in.references[i]+sign*w+in.models[i].tau*(sign*acceleration+transport);
  float kk=gain[i]*(i==0&&equilibrium<0?(p<0?inv_down:inv_up):1.f),limit=i==0?1.f:.85f;
  float u=std::clamp(equilibrium/kk,-limit,limit);
  plans[i].command+=weight*(u-plans[i].command);plans[i].input_limit=limit;plans[i].rate_limit=std::abs(share)*speed;
  plans[i].damping=1;plans[i].pursuit=weight;
  if(diagnostic){if(i==0)diagnostic->desired_pitch=share*speed;else diagnostic->desired_yaw=share*speed;}
 }
 plans[1]=roll;
 auto trial=peace_agile::preview(in,{plans[0].command,plans[1].command,plans[2].command},history);
 if(!trial.valid)return out;
 if(theta>k.path_theta&&trial.path>std::max(k.path_floor,out.before.path+k.path_margin))return out;
 out.plans=plans;out.choice=3;out.after=trial;
 if(diagnostic){diagnostic->along=coordination.along;diagnostic->cross=coordination.transverse;diagnostic->weight=weight;}
 return out;
}
// Predicted nose motion relative to the (moving) nose-target plane over the
// preview horizon. Same kinematics as peace_agile::preview: per-axis delayed
// first-order response from the issued history, slew-limited new command,
// all three rotations integrated in one rotating body frame.
struct PlaneTrack {bool valid=false;std::array<float,24> cross{},along{};};
// Maneuver anchor kept by the caller (runtime or bench) in WORLD coordinates.
struct PathAnchor {
 flight::V origin{1,0,0};bool valid=false;
 void reset(){valid=false;}
 // Returns the anchor expressed in the current body frame (x forward, y right, z up).
 flight::V update(const flight::Basis& body,const flight::V& target_world,float target_jump_deg,float theta,const Params& k){
  bool renew=!valid||k.plane_anchor<=0||target_jump_deg>k.anchor_jump||theta<=k.plane_theta;
  if(!renew){
   float back=std::acos(std::clamp(flight::dot(origin,body.f),-1.f,1.f))/flight::rad;
   auto n=flight::cross(origin,target_world);float nl=std::sqrt(flight::dot(n,n));
   float off=nl>1e-5f?std::abs(std::asin(std::clamp(flight::dot(body.f,n*(1/nl)),-1.f,1.f)))/flight::rad:0.f;
   renew=back>90.f||off>k.anchor_max_off||nl<1e-5f;
  }
  if(renew){origin=body.f;valid=true;}
  return {flight::dot(origin,body.f),flight::dot(origin,body.r),flight::dot(origin,body.u)};
 }
};
inline PlaneTrack plane_track(const peace_agile::JointInput& in,const std::array<float,3>& command,const adaptive_braking::History& history,float pitch_floor){
 PlaneTrack out;float horizon=.45f;std::array<float,3> gain{};
 auto models=in.models;
 for(int i=0;i<3;++i){auto m=models[i];gain[i]=shared_braking::gain(m,std::clamp(in.speed,130.f,660.f));
  if(!std::isfinite(gain[i])||gain[i]<1||!std::isfinite(command[i])||in.now<uint64_t(m.delay*1000))return out;
  horizon=std::max(horizon,m.delay+.35f*m.tau);}
 horizon=std::min(horizon,.85f);const float ds=horizon/24;
 flight::Basis b{{1,0,0},{0,1,0},{0,0,1}};auto target=flight::unit(in.goal);auto rates=in.rates;
 auto motion=flight::V{0,-in.references[0],in.references[2]};
 flight::V f0=flight::dot(in.path_origin,in.path_origin)>.5f?flight::unit(in.path_origin):flight::V{1,0,0};
 for(int s=0;s<24;++s){float t=(s+.5f)*ds;std::array<float,3> travel{};
  for(int i=0;i<3;++i){auto m=models[i];float pending=t-m.delay,u;
   if(pending<0)u=history.at_axis(in.now-uint64_t(-pending*1000),i);
   else u=std::clamp(command[i],in.previous[i]-in.slews[i]*(pending+in.dt),in.previous[i]+in.slews[i]*(pending+in.dt));
   float eq=gain[i]*u;
   if(i==0&&pitch_floor<1){float x=std::clamp((std::abs(rates[1])-4.f)/81.f,0.f,1.f);eq*=1-(1-pitch_floor)*x*x*(3-2*x);}
   float decay=std::exp(-ds/m.tau);travel[i]=eq*ds+(rates[i]-eq)*m.tau*(1-decay);rates[i]=eq+(rates[i]-eq)*decay;}
  auto rot=b.f*(-travel[1])+b.r*(-travel[0])+b.u*travel[2];
  b.f=peace_agile::turn(b.f,rot);b.r=peace_agile::turn(b.r,rot);b.u=peace_agile::turn(b.u,rot);
  target=peace_agile::turn(target,motion*ds);
  auto nrm=flight::cross(f0,target);float nl=std::sqrt(flight::dot(nrm,nrm));
  if(nl<1e-5f){out.cross[s]=0;out.along[s]=peace_agile::angle(f0,b.f);continue;}
  nrm=nrm*(1/nl);auto tdir=flight::unit(flight::cross(nrm,f0));   // in-plane direction from f0 toward target
  out.cross[s]=std::asin(std::clamp(flight::dot(b.f,nrm),-1.f,1.f))/flight::rad;
  out.along[s]=std::atan2(flight::dot(b.f,tdir),flight::dot(b.f,f0))/flight::rad;
 }
 out.valid=true;return out;
}
inline float plane_cost(const PlaneTrack& a,const PlaneTrack& ref,const Params& k,std::array<float,48>* res=nullptr){
 float c=0;
 for(int s=0;s<24;++s){float wc=std::sqrt(k.plane_cross_w*(s+1)/24.f),wa=std::sqrt(k.plane_along_w*(s+1)/24.f);
  float r0=wc*a.cross[s],r1=wa*(a.along[s]-ref.along[s]);c+=r0*r0+r1*r1;if(res){(*res)[s]=r0;(*res)[24+s]=r1;}}
 return c;
}
// Feasible-rate roll tracking toward PEACE's bank goal (same rate path as pitch/yaw).
inline model_control::Plan roll_path_plan(const peace_agile::JointInput& in,const adaptive_braking::History& history,const model_control::Plan& peace_roll,float mix,const Params& k){
 auto roll=peace_roll;
 auto m=in.models[1];float scale=1;
 if(!(k.use_aircraft>0&&in.aircraft)){m.tau=k.roll_tau;m.delay=k.roll_delay;scale=k.roll_scale;}   // otherwise in.models[1] is already the measured roll
 float K=shared_braking::gain(m,std::clamp(in.speed,130.f,660.f))*scale;
 if(!peace_roll.valid||!std::isfinite(K)||K<=1||in.now<uint64_t(m.delay*1000)||!std::isfinite(in.errors[1]))return roll;
 // commands already issued but not yet acting
 float w=in.rates[1],travel=0;const int n=12;const float ds=m.delay/n;
 for(int j=0;j<n;++j){float u=history.at_axis(in.now-uint64_t(m.delay*1000)+uint64_t((j+.5f)*ds*1000),1);
  float eq=K*u,d=std::exp(-ds/m.tau);travel+=eq*ds+(w-eq)*m.tau*(1-d);w=eq+(w-eq)*d;}
 float e=in.errors[1]+in.references[1]*m.delay-travel;
 war10::Params t;t.stop_scale=k.roll_stop_scale;t.coast=k.roll_coast;t.deadband=k.roll_deadband;t.capture_time=.2f;
 float cap=std::min(in.caps[1],K*k.roll_cap);
 auto tr=war10::target(std::abs(e),K,m.tau,cap,k.roll_brake,t);
 float desired=std::copysign(tr.rate,e),rel=w-in.references[1];
 float acc=(desired-rel)/std::max(.05f,k.roll_accel);
 float u=std::clamp((in.references[1]+rel+m.tau*acc)/K,-in.limits[1],in.limits[1]);
 roll.command+=k.roll_path*std::clamp(mix,0.f,1.f)*(u-roll.command);roll.damping=1;
 return roll;
}
inline peace_agile::JointSelection choose(const peace_agile::JointInput& in,const adaptive_braking::History& history,
 const std::array<model_control::Plan,3>& peace,const std::array<model_control::Plan,3>& boosted,float mix,war10::Diagnostics* diagnostic=nullptr,const Params& k=Params{}){
 auto out=choose_base(in,history,peace,boosted,mix,diagnostic,k);
 if(k.roll_path>0&&mix>0&&out.checked&&!(in.errors[1]==0&&in.rates[1]==0))out.plans[1]=roll_path_plan(in,history,out.plans[1],mix,k);
 if(k.plane_mpc<=0||mix<=0||!out.checked)return out;
 for(auto& pl:out.plans)if(!pl.valid)return out;
 float theta=peace_agile::angle({1,0,0},flight::unit(in.goal));
 if(theta<=k.plane_theta)return out;
 const float fade=[&]{float x=std::clamp((theta-k.plane_theta)/std::max(.1f,k.plane_theta_full-k.plane_theta),0.f,1.f);return x*x*(3-2*x);}();
 std::array<float,3> u0{out.plans[0].command,out.plans[1].command,out.plans[2].command};
 auto base=plane_track(in,u0,history,k.plane_pitch_eff);if(!base.valid)return out;
 std::array<float,48> r0{};float c0=plane_cost(base,base,k,&r0);
 // Finite-difference Jacobian of the residuals w.r.t. pitch and yaw commands.
 const float h=.05f;std::array<std::array<float,48>,2> J{};
 for(int a=0;a<2;++a){auto u=u0;int ax=a==0?0:2;u[ax]+=h;auto tr=plane_track(in,u,history,k.plane_pitch_eff);if(!tr.valid)return out;
  std::array<float,48> r{};plane_cost(tr,base,k,&r);for(int j=0;j<48;++j)J[a][j]=(r[j]-r0[j])/h;}
 float A00=k.plane_reg,A01=0,A11=k.plane_reg,b0=0,b1=0;
 for(int j=0;j<48;++j){A00+=J[0][j]*J[0][j];A01+=J[0][j]*J[1][j];A11+=J[1][j]*J[1][j];b0-=J[0][j]*r0[j];b1-=J[1][j]*r0[j];}
 float det=A00*A11-A01*A01;if(!std::isfinite(det)||det<1e-9f)return out;
 float dp=std::clamp((A11*b0-A01*b1)/det,-k.plane_max,k.plane_max),dy=std::clamp((A00*b1-A01*b0)/det,-k.plane_max,k.plane_max);
 float pitch_floor=k.plane_no_push>0?std::min(u0[0],0.f):-1.f;
 auto u=u0;u[0]=std::clamp(u0[0]+dp,pitch_floor,1.f);
 // If the pitch correction was bounded, re-solve yaw for the bounded pitch change.
 if(std::abs((u[0]-u0[0])-dp)>1e-4f&&A11>1e-9f)dy=std::clamp((b1-A01*(u[0]-u0[0]))/A11,-k.plane_max,k.plane_max);
 u[2]=std::clamp(u0[2]+dy,-.85f,.85f);
 auto tr=plane_track(in,u,history,k.plane_pitch_eff);if(!tr.valid)return out;
 float c1=plane_cost(tr,base,k);
 if(!(c1<c0))return out;
 out.plans[0].command=u0[0]+fade*(u[0]-u0[0]);out.plans[2].command=u0[2]+fade*(u[2]-u0[2]);
 if(diagnostic){diagnostic->plane=fade;diagnostic->cross=tr.cross[23];diagnostic->margin=c0-c1;}
 return out;
}
}
#define WAR11_PARAM_LIST P(P11,stop_scale)P(P11,coast)P(P11,deadband)P(P11,capture_time)P(P11,brake_pitch_up)P(P11,brake_pitch_down)P(P11,brake_yaw)\
 P(P11,cap_scale_pitch)P(P11,cap_scale_yaw)P(P11,cap_pitch_up)P(P11,cap_pitch_down)P(P11,cap_yaw)P(P11,prop_negative_down)P(P11,prop_negative_up)P(P11,inv_negative_down)P(P11,inv_negative_up)\
 P(P11,accel_base)P(P11,accel_tau)P(P11,accel_min)P(P11,accel_max)P(P11,path_margin)P(P11,path_floor)P(P11,path_theta)P(P11,down_theta)P(P11,roll_damping)P(P11,post_roll)P(P11,own_delay_endpoint)P(P11,plane_mpc)P(P11,plane_cross_w)P(P11,plane_along_w)P(P11,plane_reg)P(P11,plane_max)P(P11,plane_theta)P(P11,plane_theta_full)P(P11,plane_pitch_eff)P(P11,plane_no_push)P(P11,plane_anchor)P(P11,anchor_jump)P(P11,anchor_max_off)P(P11,roll_path)P(P11,roll_tau)P(P11,roll_delay)P(P11,roll_scale)P(P11,roll_brake)P(P11,roll_cap)P(P11,roll_accel)P(P11,roll_stop_scale)P(P11,roll_coast)P(P11,roll_deadband)P(P11,use_aircraft)P(P11,push_brake_use)
