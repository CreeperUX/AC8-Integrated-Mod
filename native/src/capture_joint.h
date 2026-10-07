#pragma once
#include "flight_math.h"
#include "robust_intercept.h"
#include <array>
#include <cmath>
#include <algorithm>

// CAPTURE-only local attitude correction. Every scenario receives the same
// actually issued nominal source sequence; parameter uncertainty never grants
// a scenario its own ideal brake/trim. This is a bounded geometric lookahead,
// not a complete aerodynamic model or an infinite-horizon guarantee.
namespace capture_joint {
struct Result {
 bool valid=false;
 std::array<float,3> command{},desired{};
 float before=0,after=0,crossing=0,before_crossing=0;
};
struct Rotation {
 flight::V axis{};float cosine=1,sine=0,one=0;
 explicit Rotation(flight::V degrees){
  const float n=std::sqrt(flight::dot(degrees,degrees));if(n<1e-9f)return;
  axis=degrees*(1/n);const float a=n*flight::rad,a2=a*a;
  if(std::abs(a)<.2f){sine=a*(1-a2/6+a2*a2/120);cosine=1-a2*.5f+a2*a2/24-a2*a2*a2/720;}
  else{sine=std::sin(a);cosine=std::cos(a);}
  one=1-cosine;
 }
 flight::V apply(flight::V v)const{return v*cosine+flight::cross(axis,v)*sine+axis*(flight::dot(axis,v)*one);}
};
struct Event {double time=0,input=0;};
struct Timeline {
 std::array<Event,256> events{};size_t count=0;bool valid=true;
 void add(double time,double input){
  if(time<0)return;
  if(count&&std::abs(events[count-1].time-time)<1e-9){events[count-1].input=input;return;}
  if(count>=events.size()){valid=false;return;}
  events[count++]={time,input};
 }
};
inline Timeline timeline(const robust_intercept::Plant& p,const robust_intercept::SourceTrace& source,
 const adaptive_braking::History& history,uint64_t now,int axis,double horizon){
 Timeline out;
 const double begin=double(now)-p.delay*1000;
 if(begin<0||!source.valid||!online_learning::continuous(history,uint64_t(begin),now)){out.valid=false;return out;}
 out.add(0,history.at_axis(uint64_t(begin),axis));
 for(size_t j=0;j<history.size;++j){const auto& value=history.values[(history.begin+j)%history.values.size()];
  const double at=(double(value.tick)-double(now))/1000+p.delay;
  if(at>0&&value.tick<now&&at<horizon)out.add(at,axis==0?value.pitch:axis==1?value.roll:value.yaw);
 }
 for(size_t j=0;j<source.count;++j){const double at=p.delay+j*double(source.frame_dt);if(at>=horizon)break;out.add(at,source.inputs[j]);}
 const double end=p.delay+source.duration;if(end<horizon)out.add(end,source.final_input);
 return out;
}
struct Score {
 bool valid=false;
 double cost=INFINITY,pointing=INFINITY,crossing=INFINITY,peak=INFINITY,bank=INFINITY;
};
inline Result refine(const std::array<shared_braking::Model,3>& models,const std::array<float,3>& rates,
 const std::array<float,3>& previous,const std::array<float,3>& baseline,const std::array<float,3>& limits,
 const std::array<robust_intercept::Result,3>& plans,const std::array<robust_intercept::Envelope,3>& envelopes,
 flight::V goal,float roll_error,const std::array<float,3>& references,float speed,
 const adaptive_braking::History& history,uint64_t now,float frame_dt,float mix,
 const std::array<float,3>& bias={},flight::V world_up={0,0,1},bool leveling=false){
 Result out;out.command=baseline;
 if(!std::isfinite(frame_dt)||frame_dt<=0||frame_dt>.1f||!std::isfinite(speed)||!std::isfinite(roll_error)||
    !std::isfinite(mix)||mix<=0||!std::isfinite(goal.x)||!std::isfinite(goal.y)||!std::isfinite(goal.z)||goal.x<.90f||
    !std::isfinite(world_up.x)||!std::isfinite(world_up.y)||!std::isfinite(world_up.z)||flight::dot(goal,goal)<.5f)return out;
 goal=flight::unit(goal);world_up=flight::unit(world_up);
 const float angle=std::acos(std::clamp(goal.x,-1.f,1.f))/flight::rad;
 mix*=float(1-robust_intercept::smooth((angle-10)/15));if(mix<=0)return out;
 const std::array<float,3> slews{12,16,6};std::array<double,3> gain{},desired{};
 for(int i=0;i<3;++i){gain[i]=shared_braking::gain(models[i],std::clamp(speed,130.f,660.f));
  if(!std::isfinite(gain[i])||gain[i]<1||gain[i]>600||!std::isfinite(models[i].tau)||models[i].tau<.2f||models[i].tau>3||
     !std::isfinite(models[i].delay)||models[i].delay<0||models[i].delay>.5f||!std::isfinite(rates[i])||std::abs(rates[i])>600||
     !std::isfinite(previous[i])||!std::isfinite(baseline[i])||!std::isfinite(limits[i])||limits[i]<=0||limits[i]>1.0001||
     !std::isfinite(references[i])||!std::isfinite(bias[i])||std::abs(baseline[i])>limits[i]+1e-5f||
     std::abs(baseline[i]-previous[i])>slews[i]*frame_dt+1e-5f)return out;
  desired[i]=plans[i].plan.valid&&std::isfinite(plans[i].desired_command)?plans[i].desired_command:baseline[i];
  out.desired[i]=float(desired[i]);
 }
 double tangent=std::hypot(double(goal.y),double(goal.z));double ay=0,az=0;
 if(tangent>1e-7){ay=goal.y/tangent;az=goal.z/tangent;}
 else{tangent=std::hypot(double(rates[2]-references[2]),double(rates[0]-references[0]));
  if(tangent>1e-7){ay=(rates[2]-references[2])/tangent;az=(rates[0]-references[0])/tangent;}}
 const double initial_bank=std::atan2(world_up.y,world_up.z)/flight::rad;
 const double target_bank=leveling?0:std::remainder(initial_bank-roll_error,360.0);
 const double clearance=std::hypot(double(world_up.y),double(world_up.z));
 const double bank_weight=(leveling?.003:.0001)*robust_intercept::smooth((clearance-.05)/.15);
 const double horizon=std::clamp(.38+.6*double(std::max({models[0].delay,models[1].delay,models[2].delay})),.35,.60);
 constexpr size_t scenarios=5;
 const std::array<std::array<int,3>,scenarios> corners{{{0,0,0},{1,-1,1},{-1,1,1},{1,1,-1},{-1,-1,1}}};
 std::array<std::array<robust_intercept::Plant,3>,scenarios> plants{};
 for(size_t scenario=0;scenario<scenarios;++scenario)for(int i=0;i<3;++i){
  const auto& env=envelopes[i];const double gb=std::clamp(double(env.gain_fraction),0.0,.65),tb=std::clamp(double(env.tau_fraction),0.0,.65);
  const double db=std::clamp(double(env.delay_seconds),0.0,.15),bb=std::clamp(double(env.bias_rate),0.0,15.0);
  if(!std::isfinite(gb)||!std::isfinite(tb)||!std::isfinite(db)||!std::isfinite(bb))return out;
  auto& p=plants[scenario][i];p.k=gain[i]*(1+corners[scenario][0]*gb);
  p.tau=std::clamp(double(models[i].tau)*(1+corners[scenario][1]*tb),.2,3.0);
  p.delay=std::clamp(double(models[i].delay)+corners[scenario][2]*db,0.0,.5);
  const double closing=i==0?goal.z:i==2?goal.y:roll_error;
  p.bias=bias[i]+(scenario?std::copysign(bb,closing):0);p.reference=references[i];p.frame=frame_dt;
  p.decay=std::exp(-p.frame/p.tau);p.one=-std::expm1(-p.frame/p.tau);
 }
 struct Candidate {bool valid=false;std::array<float,3> first{},desired{};std::array<robust_intercept::SourceTrace,3> source{};};
 auto build=[&](const std::array<float,3>& first){
  Candidate candidate;candidate.first=first;
  for(int i=0;i<3;++i){const auto& p=plants[0][i];robust_intercept::State advanced;
   if(!robust_intercept::pending(advanced,p,rates[i],history,now,i))return candidate;
   const double wanted=std::clamp(desired[i]+first[i]-baseline[i],-double(limits[i]),double(limits[i]));candidate.desired[i]=float(wanted);
   const bool stop=std::abs(references[i]-bias[i])<gain[i]*limits[i]-.000001;
   auto f=robust_intercept::forecast(p,advanced,previous[i],wanted,limits[i],slews[i],std::clamp(3.0*double(frame_dt),.17,.24),stop,&candidate.source[i],first[i]);
   if(!f.valid)return candidate;
  }candidate.valid=true;return candidate;
 };
 auto score=[&](const Candidate& candidate,size_t scenario){
  Score score;if(!candidate.valid)return score;
  std::array<Timeline,3> inputs{};std::array<size_t,3> index{};
  for(int i=0;i<3;++i){inputs[i]=timeline(plants[scenario][i],candidate.source[i],history,now,i,horizon);if(!inputs[i].valid)return score;}
  std::array<double,3> w{rates[0],rates[1],rates[2]};flight::Basis b{{1,0,0},{0,1,0},{0,0,1}};auto target=goal;
  double time=0,pointing=0,peak=angle,crossing=0;
  for(int n=0;n<1024&&time<horizon-1e-9;++n){
   double dt=std::min(.02,horizon-time);
   for(int i=0;i<3;++i){auto& timeline=inputs[i];while(index[i]+1<timeline.count&&timeline.events[index[i]+1].time<=time+1e-9)++index[i];
    if(index[i]+1<timeline.count)dt=std::min(dt,timeline.events[index[i]+1].time-time);
   }
   if(dt<1e-10)return score;
   std::array<double,3> travel{};
   for(int i=0;i<3;++i){const auto& p=plants[scenario][i];const double input=inputs[i].events[index[i]].input,eq=p.k*input+p.bias;
    const double one=-std::expm1(-dt/p.tau);travel[i]=eq*dt+(w[i]-eq)*p.tau*one;w[i]+=one*(eq-w[i]);
   }
   Rotation attitude(b.f*float(-travel[1])+b.r*float(-travel[0])+b.u*float(travel[2]));
   b.f=attitude.apply(b.f);b.r=attitude.apply(b.r);b.u=attitude.apply(b.u);
   target=Rotation({0,float(-references[0]*dt),float(references[2]*dt)}).apply(target);
   const double dx=double(b.f.x)-target.x,dy=double(b.f.y)-target.y,dz=double(b.f.z)-target.z;
   const double error2=(dx*dx+dy*dy+dz*dz)/(flight::rad*flight::rad);
   const double cross=std::max(0.0,(dy*ay+dz*az)/flight::rad),off=(dy*az-dz*ay)/flight::rad;
   crossing=std::max(crossing,cross);peak=std::max(peak,std::sqrt(error2));
   pointing+=(error2+8*off*off+8*cross*cross)*dt;time+=dt;
  }
  const double ex=double(b.f.x)-target.x,ey=double(b.f.y)-target.y,ez=double(b.f.z)-target.z;
  const double end_error2=(ex*ex+ey*ey+ez*ez)/(flight::rad*flight::rad);
  const double bank=std::atan2(flight::dot(b.r,world_up),flight::dot(b.u,world_up))/flight::rad;
  const double bank_error=std::remainder(bank-target_bank,360.0);
  double effort=0;for(int i=0;i<3;++i)effort+=.01*std::pow(candidate.first[i]-baseline[i],2);
  score.valid=true;score.pointing=pointing+2*end_error2;score.cost=score.pointing+bank_weight*bank_error*bank_error+effort;
  score.crossing=crossing;score.peak=peak;score.bank=std::abs(bank_error);return score;
 };
 auto evaluate=[&](const Candidate& candidate){std::array<Score,scenarios> scores{};for(size_t i=0;i<scenarios;++i)scores[i]=score(candidate,i);return scores;};
 auto base=build(baseline);if(!base.valid)return out;auto best_scores=evaluate(base);for(auto& score:best_scores)if(!score.valid)return out;
 const auto base_scores=best_scores;double best=best_scores[0].cost;
 for(size_t i=1;i<scenarios;++i)best+=.1*best_scores[i].cost;
 double initial_cross=0;for(auto& score:base_scores)initial_cross=std::max(initial_cross,score.crossing);
 out.valid=true;out.before=out.after=float(base_scores[0].cost);out.before_crossing=out.crossing=float(initial_cross);
 std::array<float,3> selected=baseline;const std::array<float,3> radius{.045f,.065f,.025f};
 for(int pass=0;pass<2;++pass)for(int axis=0;axis<3;++axis){
  for(int direction:{-1,1}){auto first=selected;
   const float lo=std::max(-limits[axis],previous[axis]-slews[axis]*frame_dt),hi=std::min(limits[axis],previous[axis]+slews[axis]*frame_dt);
   first[axis]=std::clamp(first[axis]+direction*radius[axis]*mix*(pass?.5f:1.f),lo,hi);
   if(std::abs(first[axis]-selected[axis])<1e-7)continue;
   auto candidate=build(first);if(!candidate.valid)continue;auto scores=evaluate(candidate);bool acceptable=true;
   for(size_t i=0;i<scenarios;++i)if(!scores[i].valid||scores[i].crossing>base_scores[i].crossing+.005)acceptable=false;
   if(scores[0].cost>=best_scores[0].cost-1e-8||scores[0].pointing>base_scores[0].pointing+std::max(1e-5,.02*base_scores[0].pointing)||scores[0].peak>base_scores[0].peak+.02)acceptable=false;
   if(leveling&&clearance>.15&&scores[0].bank>base_scores[0].bank+.2)acceptable=false;
   if(!acceptable)continue;double aggregate=scores[0].cost;for(size_t i=1;i<scenarios;++i)aggregate+=.1*scores[i].cost;
   if(aggregate<best-1e-8){best=aggregate;best_scores=scores;selected=first;out.command=first;out.desired=candidate.desired;out.after=float(scores[0].cost);
    double cross=0;for(auto& score:scores)cross=std::max(cross,score.crossing);out.crossing=float(cross);
   }
  }
 }
 return out;
}
}
