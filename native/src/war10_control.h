#pragma once
#include "peace_agile_control.h"
// WAR v10: feasible rate guidance. Models remain independently validated by
// PEACE's serving path; no new model is accepted by this stateless controller.
namespace war10 {
// Tunables. Defaults reproduce the Codex 22:41 prototype exactly.
struct Params {
 float stop_scale=1.25f,coast=.12f,deadband=.05f,capture_time=.20f;
 float brake_pitch_up=.22f,brake_pitch_down=.60f,brake_yaw=.65f;
 float cap_scale_pitch=1.3333333f,cap_scale_yaw=1.2857143f;
 float cap_pitch_up=.95f,cap_pitch_down=.30f,cap_yaw=.80f;
 float negative_pitch=.35f,negative_pitch_push=.70f;  // K-/K+ assumed by propagation and command inversion
 float accel_base=.22f,accel_tau=.10f,accel_min=.28f,accel_max=.45f;
 float path_margin=.04f,path_floor=.12f,path_theta=6.f;
 float down_theta=12.f;
};
struct RateTarget { float rate=0,slope=0,stop=0; };
inline float distance(float w,float gain,float tau){w=std::max(0.f,w);return tau*(w-gain*std::log1p(w/gain));}
inline RateTarget target(float error,float gain,float tau,float cap,float brake_fraction,const Params& k=Params{}){
 RateTarget o;float available=std::max(0.f,std::abs(error)-k.deadband),brake=gain*brake_fraction;
 auto stop=[&](float w){return k.stop_scale*distance(w,brake,tau)+k.coast*w;};
 float lo=0,hi=cap;
 for(int n=0;n<18;++n){float mid=(lo+hi)*.5f;if(stop(mid)>available)hi=mid;else lo=mid;}
 o.rate=lo;o.stop=stop(lo);
 o.slope=lo<cap-.001f?1/(k.stop_scale*tau*lo/(brake+lo)+k.coast):0;
 // Continuous capture at the origin (no fixed 5 degree PEACE lockout).
 float linear=available/k.capture_time;
 if(linear<o.rate){o.rate=linear;o.slope=1/k.capture_time;}
 return o;
}
struct Diagnostics {float desired_pitch=0,desired_yaw=0,along=0,cross=0,margin=0,weight=0,plane=0;};
inline peace_agile::JointSelection choose(const peace_agile::JointInput& in,const adaptive_braking::History& history,
 const std::array<model_control::Plan,3>& peace,const std::array<model_control::Plan,3>& boosted,float mix,Diagnostics* diagnostic=nullptr,const Params& k=Params{}){
 auto out=peace_agile::choose(in,history,peace,boosted,mix);
 if(mix<=0||!out.checked)return out;
 auto goal=flight::unit(in.goal);float theta=peace_agile::angle({1,0,0},goal);
 if(goal.z<0&&theta>k.down_theta)return out;
 std::array<float,3> endpoint=in.rates,travel{},gain{};
 float horizon=std::max(in.models[0].delay,in.models[2].delay),ds=horizon/20;
 auto future=goal;
 for(int i=0;i<3;++i)gain[i]=shared_braking::gain(in.models[i],std::clamp(in.speed,130.f,660.f));
 for(int j=0;j<20;++j){std::array<float,3> turn{};
  for(int i=0;i<3;++i){auto m=in.models[i];float pending=(j+.5f)*ds-m.delay;
   float u=pending<0?history.at_axis(in.now-uint64_t(-pending*1000),i):in.previous[i];
   float eq=gain[i]*u;if(i==0&&u<0)eq*=k.negative_pitch;
   float decay=std::exp(-ds/m.tau);turn[i]=eq*ds+(endpoint[i]-eq)*m.tau*(1-decay);
   travel[i]+=turn[i];endpoint[i]=eq+(endpoint[i]-eq)*decay;
  }
  future=peace_agile::turn(future,{turn[1],turn[0]-in.references[0]*ds,-turn[2]+in.references[2]*ds});
 }
 float length=peace_agile::angle({1,0,0},future),radial=std::hypot(future.y,future.z);
 if(radial<1e-6f)return out;
 float p=future.z/radial,y=future.y/radial;
 std::array<float,3> errors{p*length,in.errors[1],y*length};
 float speed=1000,slope=0;
 for(int i:{0,2}){
  float share=std::abs(i==0?p:y);if(share<.02f)continue;
  float cap=std::min(in.caps[i]*(i==0?k.cap_scale_pitch:k.cap_scale_yaw),gain[i]*(i==0?(errors[i]<0?k.cap_pitch_down:k.cap_pitch_up):k.cap_yaw));
  auto t=target(length,gain[i]/share,in.models[i].tau,cap/share,i==0?(errors[i]>0?k.brake_pitch_up:k.brake_pitch_down):k.brake_yaw,k);
  if(t.rate<speed){speed=t.rate;slope=t.slope;}
 }
 auto plans=out.plans;
 auto coordination=peace_agile::coordinate(goal,in.rates,in.references,std::max(in.models[0].delay+.3f*in.models[0].tau,in.models[2].delay+.3f*in.models[2].tau));
 float weight=std::clamp(mix,0.f,1.f);
 for(int i:{0,2}){
  float share=i==0?p:y,sign=std::copysign(1.f,share),w=sign*(endpoint[i]-in.references[i]);
  float desired=std::abs(share)*speed;
  float acceleration=(desired-w)/std::clamp(k.accel_base+k.accel_tau*in.models[i].tau,k.accel_min,k.accel_max)-std::min(std::max(0.f,w),desired)*slope;
  float transport=(i==0?y:-p)*speed*in.rates[1]*flight::rad;
  float equilibrium=in.references[i]+sign*w+in.models[i].tau*(sign*acceleration+transport);
  float kk=gain[i]*(i==0&&equilibrium<0?(p<0?k.negative_pitch_push:k.negative_pitch):1.f),limit=i==0?1.f:.85f;
  float u=std::clamp(equilibrium/kk,-limit,limit);
  plans[i].command+=weight*(u-plans[i].command);plans[i].input_limit=limit;plans[i].rate_limit=std::abs(share)*speed;
  plans[i].damping=1;plans[i].pursuit=weight;
  if(diagnostic){if(i==0)diagnostic->desired_pitch=share*speed;else diagnostic->desired_yaw=share*speed;}
 }
 plans[1]=peace[1];
 auto trial=peace_agile::preview(in,{plans[0].command,plans[1].command,plans[2].command},history);
 if(!trial.valid)return out;
 if(theta>k.path_theta&&trial.path>std::max(k.path_floor,out.before.path+k.path_margin))return out;
 out.plans=plans;out.choice=3;out.after=trial;
 if(diagnostic){diagnostic->along=coordination.along;diagnostic->cross=coordination.transverse;diagnostic->weight=weight;}
 return out;
}
}
