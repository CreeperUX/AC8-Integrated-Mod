#pragma once
#include "flight_math.h"
#include "shared_braking.h"
#include "adaptive_braking.h"
#include <array>
// Local-body pose during the delay queue. Other axes may respond sooner;
// retain their last actual source command beyond their known queue.
namespace vector_pending_pose {
struct Result {flight::Basis body{{1,0,0},{0,1,0},{0,0,1}};std::array<float,3> rates{};};
inline flight::V rotate(flight::V v,flight::V omega){
 float n=std::sqrt(flight::dot(omega,omega));if(n<1e-8f)return v;
 return flight::rotate(v,omega*(1/n),n*flight::rad);
}
inline Result predict(const std::array<shared_braking::Model,3>& models,
 const std::array<float,3>& rates,const std::array<float,3>& previous,
 float positive,float negative,const adaptive_braking::History& history,uint64_t now,float duration,float speed){
 Result out;out.rates=rates;
 double time=0;
 while(time+1e-8<duration){
  double end=std::min(double(duration),time+.01);
  for(const auto& model:models)if(model.delay>time+1e-7&&model.delay<end)end=model.delay;
  // Split at actual source events shifted by each actuator's delay.
  for(int axis=0;axis<3;++axis)for(size_t j=history.size;j>0;--j){auto tick=history.values[(history.begin+j-1)%history.values.size()].tick;
   if(double(tick)<double(now)-models[axis].delay*1000-1)break;
   double boundary=(double(tick)-double(now))/1000+models[axis].delay;
   if(boundary>time+1e-7&&boundary<end)end=boundary;
  }
  float dt=float(end-time);std::array<float,3> travel{};
  for(int axis=0;axis<3;++axis){auto m=models[axis];double source=double(now)+(.5*(time+end)-m.delay)*1000;
   float u=source>=now?previous[axis]:history.at_axis(uint64_t(std::max(0.,source)),axis);
   float k=axis==0?(u>=0?positive:negative):shared_braking::gain(m,speed),eq=k*u+m.bias,one=-std::expm1(-dt/m.tau);
   travel[axis]=eq*dt+(out.rates[axis]-eq)*m.tau*one;out.rates[axis]+=one*(eq-out.rates[axis]);
  }
  auto omega=out.body.f*(-travel[1])+out.body.r*(-travel[0])+out.body.u*travel[2];
  out.body.f=rotate(out.body.f,omega);out.body.r=rotate(out.body.r,omega);out.body.u=rotate(out.body.u,omega);time=end;
 }
 return out;
}
}
