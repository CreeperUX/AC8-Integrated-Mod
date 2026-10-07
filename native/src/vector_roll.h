#pragma once
#include "capture_roll.h"
// WAR-only guidance. Near alignment the lift-plane azimuth is ill-conditioned:
// tiny pitch/yaw residuals must not ask for an arbitrary half-turn.
namespace vector_roll {
struct Guidance {
 capture_roll::Guidance capture;
 bool seen=false;float previous_error=0;
 float raw_error=0,handoff=0,regularization=0;
 void reset(){*this={};}
 capture_roll::Output update(capture_roll::Input in){
  // Match PEACE's attitude intent: reaching the cursor region starts leveling
  // without waiting for pointing rates to become almost zero. Rate stopping
  // remains in the downstream controller, not in the attitude decision.
  if(in.active&&in.angle<=3.f&&in.target_jump<2.5f)capture.leveling=true;
  if(in.angle>=6.f)capture.leveling=false;
  auto out=capture.update(in);if(!out.valid){reset();return out;}
  regularization=out.leveling?0.f:capture_roll::smooth((5.f-in.angle)/2.5f);
  if(regularization>0){
   // Only the <=5-degree correction region uses the well-conditioned local
   // objective. Efficient positive-G roll/pull at larger angles is unchanged.
   float local=std::atan2(in.right,std::max(.087155743f,in.up))*57.295779513f;
   out.roll_error+=regularization*(local-out.roll_error);
  }
  // The lift-plane azimuth is ill-conditioned when the nose is nearly on the
  // cursor. If a fresh geometric branch differs by roughly half a turn from
  // the transported reference, preserve the continuous reference instead of
  // commanding an unnecessary full roll. A well-conditioned local azimuth is
  // preferred when it is available.
  if(!out.leveling&&seen&&in.target_jump<2.5f&&in.angle<=8.f&&in.pole_clearance>.08f){
   float transported=previous_error-in.roll_rate*in.dt;
   float delta=std::abs(std::remainder(out.roll_error-transported,360.f));
   if(delta>120.f||(std::abs(out.roll_error)>150.f&&std::abs(previous_error)<120.f)){
    float local=std::atan2(in.right,std::max(.15f,in.up))*57.295779513f;
    if(std::abs(local)<=90.f)out.roll_error=local;
    else out.roll_error=previous_error;
    regularization=std::max(regularization,.75f);
   }
  }
  raw_error=out.roll_error;
  bool pole=out.leveling&&in.pole_clearance<=.05f;
  // Bound reference motion on EVERY frame, including crossing the edge of
  // the local correction region. A one-time handoff offset added to a moving
  // raw reference can instead amplify a subsequent branch change.
  handoff=0;
  if(seen&&in.target_jump<2.5f&&!pole){
   float transported=previous_error-in.roll_rate*in.dt;
   float change=std::remainder(raw_error-transported,360.f);
   float step=std::clamp(change,-360.f*in.dt,360.f*in.dt);
   out.roll_error=transported+step;
   handoff=std::abs(change-step)>.01f?1.f:0.f;
  }
  seen=true;previous_error=out.roll_error;
  return out;
 }
};
}
