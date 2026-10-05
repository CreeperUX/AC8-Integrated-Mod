#pragma once
#include "flight_math.h"
#include "hud_geometry.h"
#include <cstdint>
namespace hud_prediction {
struct Q {double w=1,x=0,y=0,z=0;};
inline Q mul(Q a,Q b){return {a.w*b.w-a.x*b.x-a.y*b.y-a.z*b.z,a.w*b.x+a.x*b.w+a.y*b.z-a.z*b.y,a.w*b.y-a.x*b.z+a.y*b.w+a.z*b.x,a.w*b.z+a.x*b.y-a.y*b.x+a.z*b.w};}
inline Q conjugate(Q q){return {q.w,-q.x,-q.y,-q.z};}
inline Q normalized(Q q){double n=std::sqrt(q.w*q.w+q.x*q.x+q.y*q.y+q.z*q.z);return n>1e-12?Q{q.w/n,q.x/n,q.y/n,q.z/n}:Q{};}
inline Q orientation(const flight::Basis& b){
 double m00=b.f.x,m01=b.r.x,m02=b.u.x,m10=b.f.y,m11=b.r.y,m12=b.u.y,m20=b.f.z,m21=b.r.z,m22=b.u.z;Q q;double s;
 if(m00+m11+m22>0){s=std::sqrt(m00+m11+m22+1)*2;q={s/4,(m21-m12)/s,(m02-m20)/s,(m10-m01)/s};}
 else if(m00>m11&&m00>m22){s=std::sqrt(1+m00-m11-m22)*2;q={(m21-m12)/s,s/4,(m01+m10)/s,(m02+m20)/s};}
 else if(m11>m22){s=std::sqrt(1+m11-m00-m22)*2;q={(m02-m20)/s,(m01+m10)/s,s/4,(m12+m21)/s};}
 else{s=std::sqrt(1+m22-m00-m11)*2;q={(m10-m01)/s,(m02+m20)/s,(m12+m21)/s,s/4};}
 return normalized(q);
}
inline flight::V rotated(Q q,flight::V v){Q out=mul(mul(q,{0,v.x,v.y,v.z}),conjugate(q));return {float(out.x),float(out.y),float(out.z)};}
inline flight::Basis basis(Q q){return {rotated(q,{1,0,0}),rotated(q,{0,1,0}),rotated(q,{0,0,1})};}
struct Sample {uintptr_t pawn=0,manager=0;uint64_t epoch=0,sequence=0;double milliseconds=0;float p=0,y=0,r=0,fov=62;};
struct Result {flight::Basis view;bool applied=false;float horizon_ms=0,shift_px=0;int reason=1;}; // 0 active,1 warming,2 disabled,3 stale,4 projection
class Predictor {
 Sample latest{};Q rotation{};flight::V velocity{};double interval=16.667;unsigned stable=0;bool have=false;
 void seed(const Sample& s){latest=s;rotation=orientation(flight::basis(s.p,s.y,s.r));velocity={};interval=16.667;stable=1;have=true;}
public:
 void reset(){have=false;stable=0;velocity={};}
 void observe(const Sample& s){
  if(!s.pawn||!s.manager||!s.sequence||!std::isfinite(s.milliseconds)||!std::isfinite(s.p)||!std::isfinite(s.y)||!std::isfinite(s.r)||!std::isfinite(s.fov)||s.fov<15||s.fov>150){reset();return;}
  if(!have||s.pawn!=latest.pawn||s.manager!=latest.manager||s.epoch!=latest.epoch){seed(s);return;}
  if(s.sequence==latest.sequence)return;
  double dt=s.milliseconds-latest.milliseconds;
  if(s.sequence<latest.sequence||dt<4||dt>50||dt>interval*2||std::abs(s.fov-latest.fov)>.05){seed(s);return;}
  Q next=orientation(flight::basis(s.p,s.y,s.r)),delta=mul(next,conjugate(rotation));
  if(delta.w<0)delta={-delta.w,-delta.x,-delta.y,-delta.z};
  const double n=std::sqrt(delta.x*delta.x+delta.y*delta.y+delta.z*delta.z),angle=2*std::atan2(n,std::max(0.0,delta.w));
  if(angle>.2){seed(s);return;}
  flight::V rate=n>1e-10?flight::V{float(delta.x/n*angle/dt),float(delta.y/n*angle/dt),float(delta.z/n*angle/dt)}:flight::V{};
  const float speed=std::sqrt(flight::dot(rate,rate)),previous=std::sqrt(flight::dot(velocity,velocity));
  if(speed>.005f||(stable>2&&previous>.0001f&&(flight::dot(rate,velocity)<0||std::sqrt(flight::dot(rate-velocity,rate-velocity))>std::max(.0001f,previous*.75f)))){seed(s);return;}
  latest=s;rotation=next;velocity=rate;interval=dt;++stable;
 }
 Result evaluate(double now,flight::V target,flight::V nose,flight::V offset,float width,float height,bool enabled)const{
  Result out{have?basis(rotation):flight::basis(0,0,0)};
  if(!enabled){out.reason=2;return out;}
  if(!have||stable<8)return out;
  double age=now-latest.milliseconds;if(age<0||age>std::min(100.0,interval*2)){out.reason=3;return out;}
  float horizon=float(std::min({age,8.0,interval*.5}));
  if(horizon<=0)return out;
  const float speed=std::sqrt(flight::dot(velocity,velocity));if(speed<1e-8)return out;
  auto project=[&](const flight::Basis& view,flight::V point,hud_geometry::Point& p){
   auto v=point*50000-offset;float z=flight::dot(v,view.f);if(z<500)return false;
   float focal=width*.5f/std::tan(latest.fov*.5f*flight::rad);p={width*.5f+focal*flight::dot(v,view.r)/z,height*.5f-focal*flight::dot(v,view.u)/z};
   return std::isfinite(p.x)&&std::isfinite(p.y)&&p.x>20&&p.x<width-20&&p.y>20&&p.y<height-20;
  };
  hud_geometry::Point raw{},raw_nose{};if(!project(out.view,target,raw)){out.reason=4;return out;}
  bool nose_visible=project(out.view,nose,raw_nose);
  auto predicted=[&](float h){double a=speed*h*.5;float k=float(std::sin(a)/speed);return basis(normalized(mul({std::cos(a),velocity.x*k,velocity.y*k,velocity.z*k},rotation)));};
  auto error=[&](float h,float& shift){auto candidate=predicted(h);hud_geometry::Point p{},n{};if(!project(candidate,target,p))return false;shift=std::hypot(p.x-raw.x,p.y-raw.y);
   if(nose_visible){if(!project(candidate,nose,n))return false;shift=std::max(shift,std::hypot(n.x-raw_nose.x,n.y-raw_nose.y));}return shift<=height*.0035f;
  };
  float shift=0;
  if(!error(horizon,shift)){float lo=0,hi=horizon;for(int i=0;i<14;++i){float mid=(lo+hi)*.5f;if(error(mid,shift))lo=mid;else hi=mid;}horizon=lo;}
  if(horizon<.01f||!error(horizon,shift)){out.reason=4;return out;}
  out.view=predicted(horizon);out.applied=true;out.reason=0;out.horizon_ms=horizon;out.shift_px=shift;return out;
 }
};
}
