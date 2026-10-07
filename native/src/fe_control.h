#pragma once
// WAR1 / WAR2: mouse-aim guidance on the exact AC8 flight-engine model (fe_model.h).
// Geometry and roll goal follow PEACE; actuation is exact: per-axis braking curves are
// simulated on the game's own rotation law every frame and inverted through the known
// rate and input stages. WAR1 drives the stock stick (input smoothing kept);
// WAR2 writes the smoothed command state directly. Yaw always writes the command state
// (the stock yaw path is on/off; the existing mod already overrides it the same way).
#include "fe_model.h"
#include "flight_math.h"
#include "aim_motion.h"
#include "model_control.h"
#include <vector>

namespace fe_control {

enum class Mode {Stick=1,Direct=2};

struct Tuning {
 double margin=.04;            // fraction of the remaining error kept as braking margin
 double tau_rate=.05;          // terminal rate tracking time constant of the inversion (s)
 double tau_cmd=.06;           // terminal stick -> command-state inversion time constant (s)
 double term[3]={.28,.40,.20}; // terminal linear time constants: pitch, yaw, roll (s)
 double term_zone[3]={.6,.4,2.0}; // deg: below this error the linear terminal law takes over (blend up to 2x)
 double stick_min=.06,state_min=.03; // near-zero actions snapped to exactly 0 for a resting goal
 double roll_ff=0;             // weight of the roll-goal derivative as feed-forward (0: none)
 double yaw_dead=.15;          // deg
 double push_full=15,push_none=30; // deg: full push allowed below, none above (roll to pull instead)
 double wt_gate=2;              // WT-style pull gating: 1 pitch only, 2 pitch and yaw, 0 off
 double joint3d=0;             // WT-style: pitch/yaw solved on the predicted stop attitude with all axes simulated
 double joint_roll_max=15;     // deg: only once the roll goal is nearly reached (its plan is then reliable)
 double joint_roll_plan=1;     // roll follows its own time-optimal plan in the stop simulation (0: roll brakes too)
 double motion_wt=1;           // motion-bank weight inside the WT geometry
 double sync=2;                // pitch follows yaw progress when yaw sets the arrival time (straight nose path); 2: also in stick mode (WAR)
 double sync_roll=10;          // deg: only once the roll goal is reached
 double sync_angle=8;          // small residuals: synchronize even while rolling (roll does not move the nose)
 double sync_max=10;           // deg: synchronize only small residuals (large ones are handled by roll + pull)
 double sync_floor=1;          // pitch never waits for a stalled/reversing yaw
 double path_lock=1;           // 1: straight nose path at all angles (faster of pitch/yaw follows the slower)
 double path_ratio=1.1;        // lock when one axis needs this much longer than the other
 double hold_level=2;          // 2: WT-style continuous hold->level blend; 1: hold, level on arrival (switch); 0: WT lambda to level
 double level_on=1.5,level_on_still=1,level_hyst=3; // deg: levelling zone (grows with mouse stillness), exit hysteresis
 double level_ff=3,level_ff_hyst=2; // deg/s: no levelling while the aim moves faster than this (exit at x hyst)
 double level_smooth=4.5,level_track=.12,level_done=1.5,level_keep=4;
 double level_still_t=.3,level_arrive=2.5,level_cancel=3; // s of true mouse stillness, deg nose-on, deg/s windowed aim speed that cancels
 // "Still" for levelling: windowed aim speed below max(level_slow, level_slow_k * yaw capability). A slow creep
 // (over-lead settling, drift tracking) that pitch/yaw follow without bank -- the motion bank's own threshold --
 // does not hold the levelling off; level_slow_k<=0 uses the estimator's fixed stillness.
 double level_slow=1.5,level_slow_k=.45;
 // Quick start (level_quiet_t > 0): level once the mouse has been idle level_quiet_t s and the windowed speed is
 // below the slow threshold (or idle level_still_t s), with the nose within level_arrive.
 double level_quiet_t=.12,level_cancel_dev=0;   // s; deg beyond level_arrive at which renewed mouse input cancels
 // Levelling profile 1: minimum-jerk (quintic) roll from the current bank/rate to wings-level over
 // T = max(1.875*bank/level_vmax, sqrt(5.77*bank/level_amax), level_tmin): symmetric bell, no tail, small banks
 // quick, peak rate/acceleration bounded. 0: triple-pole reference (v13.2/13.3, slow tail, ~6/w s for any bank).
 double level_rearm=5;          // deg: a completed levelling is re-planned once the bank leaves wings-level by more
 double level_profile=1,level_vmax=120,level_amax=300,level_tmin=.35,level_lead=.2;
 // level_smooth: smooth levelling reference w (rad/s, 0 off); level_track: tracking time constant (s); level_done (deg);
 // level_keep: extra deg the nose may stray (e.g. bank-assist pull) before a started levelling is cancelled
 double level_band=2,level_sine=180; // hold_level=2: blend band (deg); sine roll law: deg/s at 90 deg (0: proportional)
 double level_tau=.3,level_max=120; // s, deg/s: levelling roll response
 double motion_near=6,motion_far=15; // deg: motion bank fades out between these residuals (tracking aid only)
 double push_cost=30,push_cost_rho=120,push_hyst=15; // deg: pull/push choice by roll cost (0: WT threshold)
 double pull_over_push=.45;     // sin(off-axis) at which an ongoing push gives way to roll-over-and-pull
 double roll_pred_a0=20,roll_pred_a1=45; // deg: look-ahead full below a0, none above a1 (a1<=a0: no fade); Su-57 flight:
                                   // unfaded look-ahead tripled fast roll reversals with the cursor far off the nose
 double roll_pred_max=0;        // deg: cap on the look-ahead shift (0: none)
 double roll_pred=.45;          // s: roll decision looks this far ahead along the windowed aim motion
 double decide_win=0;          // decisions (look-ahead, push escape) from the windowed aim velocity
 double decide_lead=0;        // s: look-ahead along the aim motion for pull/push and roll-over decisions
 double mot_tau=.12;           // s: low-pass on the unclamped aim motion
 double motion_sus0=.35,motion_sus1=.7; // motion_win 4: s of continuous aim motion where the motion bank starts / is full
 double motion_win=0;          // motion bank from the windowed aim motion (1; 2 low-passed; 3,4 sustained only). 0: the
                               // Lua-tick reference -- a short correction flick must not bank in precision tracking
 double motion_raw=0;          // motion bank from the unclamped aim motion (ff is clamped to 7 deg/s in yaw)
 double alloc=0;               // 1: minimum-time roll allocation (replaces lambda/push/roll-over rules)
 double alloc_overlap=.5,alloc_hyst=.06,alloc_hyst_rel=.08; // pitch/yaw overlap with the roll; keep-choice margin (s, fraction)
 double alloc_pred=1;          // weight of the aim's own motion in the plan (relative closing speeds)
 double alloc_rev=.3;          // weight of the aim-reverses scenario (back-and-forth dragging)
 double alloc_push=1,alloc_push_free=4; // s/rad beyond alloc_push_free deg of push: prefer pulling
 double alloc_inv=.5,alloc_inv_deg=100; // s/rad beyond this bank: keep the sky up
 double alloc_commit=.15;      // s/rad of roll while the mouse moves (it may reverse)
 double alloc_change=.1;       // s/rad of change from the previous plan
 double push_escape=.5;        // >0: give up pushing when the aim runs toward the belly faster than this x push capability
 double level_tau_small=0;     // s: levelling no faster than bank/level_tau_small (0: off)
 double gate_f0=.6,gate_rfloor=.4; // pull gating: factor scale, roll-rate floor (rad/s); lower = pull waits for roll alignment
 double ff_win=0;              // 1: feed-forward from the windowed aim motion (tested: its ~0.1 s lag misdirects more; off)
 double ff_gain=1;            // target-motion feed-forward (WT uses none: 0)
 double roll_lead=.2;          // s: pitch/yaw aimed at the target's bearing after this much of the current roll (rate lag)
 double lam_dead=3;            // deg: no bank toward residuals smaller than this (pitch + yaw only)
 double lam0=.125,lam_still=.3; // WT roll-goal blend: lambda = rho/(lam0+lam_still*still)
 double motion_fast=.5;         // WT2: motion bank rolls at the far-target rate
 double geo_wt=2;              // 2: War Thunder decision layer, 1: WT lambda roll goal only, 0: PEACE geometry
 double roll_time_mult=.8;     // roll time multiplier (AC8's slower roll filter wants gentler near-target roll, HIL-tuned)
 double ff_tau=.10;            // s: low-pass on the target angular-velocity feed-forward
 double motion_tau=.35;        // s: slower filter deciding motion bank (only sustained target motion banks)
 double motion_on=.3,motion_full=1.2; // target lateral motion / yaw capability where motion bank starts / is full
};

struct Geometry {
 flight::V goal;          // target in body axes: x forward, y right, z up (unit)
 float bank_r=0,bank_u=1; // world-up components of body right / body up (r.z, u.z)
 float up_f=0;            // world-up component of body forward (f.z)
 float ff_pitch=0,ff_yaw=0; // target angular-velocity feed-forward, deg/s (+nose up, +nose right)
 float mot_up=0,mot_right=0; // unclamped aim angular velocity in body axes, deg/s (decisions only)
 // Windowed aim motion (aim_motion::Estimator; robust to bursty mouse input). aim_still < 0: unavailable.
 float aim_speed=0,aim_still=-1,aim_up=0,aim_right=0,aim_path=0,aim_moving=0,aim_quiet=-1;
};

struct Output {
 double stick[3]{};   // FE order: pitch, yaw, roll (stock stick axes)
 double S[3]{};       // command state to write (Direct mode; yaw in both modes)
 double rate_des[3]{};// desired internal rates (rad/s)
 double errors[3]{};  // pitch, yaw, roll errors (rad)
 double stop[3]{};    // predicted stop-from-current-state angles (rad)
 double angle=0;      // deg
 bool sync=false;     // pitch synchronized to yaw this tick
 double dbg[3][4]{};  // per axis: travel(go), travel(brk), goal travel, chosen x (before terminal blend)
 bool valid=false;
};

namespace detail {
constexpr double PI=3.14159265358979323846;
// One axis of the exact chain. Action x is the stick (Stick mode) or the command state (Direct).
struct Axis {
 const fe::Params* p;int a;double cap,dt,pud,bias;Mode mode;
 double applied(double r)const{return a==0?(r<0?r*pud:r)+bias:r;}
 double internal(double w)const{double r=w-bias;if(a==0&&r<0)r/=pud;return std::clamp(r,-cap,cap);}
 void tick(double& S,double& r,double x)const{
  S=mode==Mode::Stick?fe::input_axis(*p,a,S,x,dt):std::clamp(x,-1.0,1.0);
  r=fe::rate_axis(*p,a,r,S,cap,dt);
 }
 // First action of the brake policy toward relative rest.
 double brake_action(double S,double r,double side,double ff,double r_end)const{
  double S_end=r_end/cap;
  if(std::abs(S_end)<.02)S_end=0;   // near-zero hold would select the slow AddRotR decay
  if(mode==Mode::Stick)return side*(S-S_end)>1e-9?-side:(std::abs(S_end)<1e-6?0.0:fe::stick_inverse(S_end));
  double best=1e18,xb=S_end;
  for(double c:{-side,0.0,S_end}){const double rc=fe::rate_axis(*p,a,r,std::clamp(c,-1.0,1.0),cap,dt),rel=side*(applied(rc)-ff);if(rel>=0&&rel<best){best=rel;xb=c;}}
  return xb;
 }
 // Relative angle travelled (in the side direction) until the applied rate matches ff.
 double brake(double S,double r,double side,double ff,double r_end)const{
  double ang=0;
  for(int n=0;n<300;++n){
   if(side*(applied(r)-ff)<=0)break;
   tick(S,r,brake_action(S,r,side,ff,r_end));
   ang+=std::max(0.0,side*(applied(r)-ff))*dt;
  }
  return ang;
 }
};
}

class Controller {
public:
 // Minimum time to move d (>=0) along +, from signed rate v0, accel a, rate limit V, ending at rest.
 static double min_time(double d,double v0,double a,double V){
  double t=0;if(v0<0){t+=-v0/a;d+=v0*v0/(2*a);v0=0;}
  const double stop=v0*v0/(2*a);
  if(stop>=d){t+=v0/a;d=stop-d;v0=0;}
  const double vp=std::sqrt(a*d+v0*v0/2);
  if(vp<=V)return t+(vp-v0)/a+vp/a;
  return t+(V-v0)/a+V/a+(d-(V*V-v0*v0)/(2*a)-V*V/(2*a))/V;
 }
 double allocate(const fe::Params& p,const fe::Caps& c,const double r[3],flight::V goal,double bank,Mode mode,const Tuning& k){
  using detail::PI;
  const double lag_in=[&](int a){return mode==Mode::Stick?1/(1.5*std::max(double(p.inadd[a]),.1)):0.0;}(2);
  // Per-axis acceleration of the stock rate law r' = AddRotR*Kadd*clamp(|r/cap|+floor,0,1)*(S*cap-r), averaged
  // over a full-command transient: a ~ AddRotR*Kadd*(1+floor)/2*cap.
  auto acc=[&](int a,double cap){const double kadd[3]={1,1,2},fl[3]={.5,.8,.4};return std::max(double(p.add[a]),.1)*kadd[a]*(1+fl[a])/2*cap;};
  const double V=std::max(c.cap[2],1e-3),A=acc(2,V);
  const double cp=std::max(c.cap[0],1e-3),cy=std::max(c.cap[1],1e-3),pud=std::max(double(p.pud),.1);
  const double lag_p=mode==Mode::Stick?.5/(1.5*std::max(double(p.inadd[0]),.1)):0.0;
  // Relative motion: the aim moves (mup, mri deg/s in body axes). Closing speed on each axis is the aircraft's
  // capability minus the aim's speed away from the nose; a target escaping faster than an axis can follow
  // makes that plan useless (e.g. pushing after a target that keeps running toward the belly). Robustness for
  // back-and-forth dragging: plans are scored against the aim continuing and stopping (worst case), plus
  // alloc_rev of the aim reversing.
  const double gx=goal.x,gy=goal.y,gz=goal.z,theta=std::atan2(std::hypot(gy,gz),gx),psi=std::atan2(gy,gz);
  const double vm=std::hypot(mri,mup)*PI/180*k.alloc_pred,va=std::atan2(mri,mup);
  const double act=1-still;                    // mouse activity: commitment is risky while it moves
  auto time_for=[&](double phi,double tr,double vs){
   const double q=psi-phi,qv=va-phi;
   double up=theta*std::cos(q),lat=theta*std::sin(q);
   const double vu=vs*vm*std::cos(qv),vl=vs*vm*std::sin(qv);
   up+=vu*tr;lat+=vl*tr;                       // where the aim is once the roll is done
   const double cpe=up>=0?cp:cp*pud;
   auto chase=[&](double x,double v,double cap,int ax){
    if(std::abs(x)<1e-4)return 0.0;
    const double away=x>=0?v:-v,ce=cap-away;
    if(ce<.15*cap)return 3.0+std::abs(x)/(.15*cap);                    // cannot catch it on this axis
    return min_time(std::abs(x),0,acc(ax,std::min(ce,2*cap)),std::min(ce,2*cap));
   };
   const double tpp=chase(up,vu,cpe,0)+(std::abs(up)>1e-4?lag_p:0.0),tyy=chase(lat,vl,cy,1);
   const double tpy=std::max(tpp,tyy);
   // Pilot preference: pull rather than push except for small corrections.
   const double push=up<0?k.alloc_push*std::max(0.0,-up-k.alloc_push_free*PI/180):0.0;
   return tr+tpy-k.alloc_overlap*std::min(tr,tpy)+push;
  };
  auto cost=[&](double phi){
   const double tr=std::abs(phi)<1e-3?0.0:min_time(std::abs(phi),r[2]*(phi>=0?1:-1),A,V)+.5*lag_in;
   double j=time_for(phi,tr,0);
   if(vm>1e-3){j=std::max(j,time_for(phi,tr,1));if(k.alloc_rev>0)j+=k.alloc_rev*time_for(phi,tr,-1);}
   // Keep the sky up: penalise ending beyond alloc_inv_deg of bank.
   const double ba=std::abs(std::remainder(bank-phi,2*PI)),inv=std::max(0.0,ba-k.alloc_inv_deg*PI/180);
   j+=k.alloc_inv*inv+k.alloc_commit*act*std::abs(phi);
   if(alloc_have)j+=k.alloc_change*std::abs(std::remainder(bank-phi-alloc_target,2*PI));
   return j;
  };
  double best=0,bc=cost(0);
  for(int i=-36;i<=36;++i){const double ph=i*5*PI/180,cc=cost(ph);if(cc<bc){bc=cc;best=ph;}}
  // Refine around the best grid point.
  for(double st=2.5*PI/180;st>.1*PI/180;st/=2)for(double ph:{best-st,best+st}){const double cc=cost(ph);if(cc<bc){bc=cc;best=ph;}}
  if(alloc_have){const double keep=std::remainder(bank-alloc_target,2*PI),ck=cost(keep);
   if(ck<=bc*(1+k.alloc_hyst_rel)+k.alloc_hyst){best=keep;bc=ck;}}
  alloc_target=bank-best;alloc_have=true;alloc_cost=bc;return best;
 }
 bool level_active()const{return lvl_on;}   // diagnostics (bench dumps)
 void reset(){level.reset();roll_seen=false;last_roll_goal=0;roll_ref.reset();ffp=ffy=ffpm=ffym=0;mup=mri=mupm=mrim=0;still=0;slow_t=0;mwu=mwr=0;lq_T=lq_t=0;leveling=false;lvl_on=false;push_prev=false;alloc_have=false;}

 Output step(const fe::Params& p,const double S[3],const double r[3],double speed,double dt,const Geometry& g,Mode mode,const Tuning& k=Tuning{}){
  Output out;
  if(!p.valid||!(dt>0)||dt>.1)return out;
  using detail::PI;
  const fe::Caps c=fe::caps(p,speed);
  const auto goal=flight::unit(g.goal);
  out.angle=std::acos(std::clamp(double(goal.x),-1.0,1.0))*180/PI;
  // PEACE geometry: bank until the target lies above, pull; level near the target.
  // Elevation of the target above the wing plane / azimuth off the vertical plane: continuous
  // everywhere (atan2(up, forward) flips sign when the target passes behind the wing line).
  const double wf=std::clamp((double(goal.x)-.3)/.4,0.0,1.0),fx=std::max(.02,double(goal.x));
  // Roll lead (WT carries the ongoing roll in its pitch/yaw prediction and pre-rotates the target by ra):
  // pitch/yaw take effect after a lag during which the body keeps rolling, so aim them at the target's
  // bearing in the body frame expected after roll_lead seconds of the current roll rate.
  double ly=goal.y,lz=goal.z;
  if(k.roll_lead!=0){const double a=k.roll_lead*r[2],ca=std::cos(a),sa=std::sin(a);ly=goal.y*ca-goal.z*sa;lz=goal.y*sa+goal.z*ca;}
  double pitch_err=wf*std::atan2(lz,fx)+(1-wf)*std::asin(std::clamp(lz,-1.0,1.0));
  const double yaw_err=wf*std::atan2(ly,fx)+(1-wf)*std::asin(std::clamp(ly,-1.0,1.0));
  // Target-motion feed-forward, low-passed (the Lua-side reference is noisy at small angles).
  {const double am2=1-std::exp(-dt/std::max(k.mot_tau,1e-3));mup+=(g.mot_up-mup)*am2;mri+=(g.mot_right-mri)*am2;}
  {const double am3=1-std::exp(-dt/std::max(k.motion_tau,1e-3));mupm+=(g.mot_up-mupm)*am3;mrim+=(g.mot_right-mrim)*am3;
   mwu+=(g.aim_up-mwu)*am3;mwr+=(g.aim_right-mwr)*am3;}   // windowed aim motion, sustained part (motion_win 2)
  // Target-motion feed-forward input: the windowed aim motion (smooth through bursty mouse input) when available,
  // else the Lua-tick reference (zero in ~45% of the frames of a bursty mouse move).
  const bool ffw=k.ff_win>0&&g.aim_still>=0;
  const double ffp_in=ffw?std::clamp(double(g.aim_up),-45.0,45.0):double(g.ff_pitch),ffy_in=ffw?std::clamp(double(g.aim_right),-c.cap[1]*180/PI,c.cap[1]*180/PI):double(g.ff_yaw);
  {const double al=1-std::exp(-dt/std::max(k.ff_tau,1e-3));ffp+=(ffp_in-ffp)*al;ffy+=(ffy_in-ffy)*al;
   const double am=1-std::exp(-dt/std::max(k.motion_tau,1e-3));ffpm+=(ffp_in-ffpm)*am;ffym+=(ffy_in-ffym)*am;}
  const double bank=std::atan2(double(g.bank_r),double(g.bank_u));
  double roll_err;
  wt_rate=-1;lvl_law=false;
  if(k.geo_wt>=2){
   // War Thunder-style decision layer:
   // 25 deg carrot, mouse stillness, level->target lambda blend, roll-over-and-pull / push, and a
   // proportional roll with T_roll = roll_time_mult*90/(P_max*coeff(off-axis)): fast far away, gentle near.
   {const double w_ang=std::hypot(double(g.ff_pitch),double(g.ff_yaw))*PI/180;   // aim angular speed, rad/s
    still=std::clamp(still+.4*dt-30*w_ang*dt,0.0,1.0);}
   flight::V d=goal;                                     // carrot: at most 25 deg from the nose
   // Motion-aware roll decision: the roll target is where the cursor will be roll_pred seconds ahead along its
   // (windowed) motion, so a cursor swept across the nose to the other side starts the reversal roll as it
   // crosses instead of after it has left the near zone; a still cursor is unaffected (no early roll in a
   // plain approach).
   if(k.roll_pred>0&&g.aim_still>=0){
    // Fade the look-ahead out far off the nose: with the cursor well off-axis (behind the wing line) its small
    // body-lateral components make the predicted bearing swing left/right with the cursor motion (roll wobble).
    const double fa=k.roll_pred_a1>k.roll_pred_a0?std::clamp((k.roll_pred_a1-out.angle)/(k.roll_pred_a1-k.roll_pred_a0),0.0,1.0):1.0;
    double sy=g.aim_right*k.roll_pred*fa,sz=g.aim_up*k.roll_pred*fa;const double sn=std::hypot(sy,sz);   // deg of look-ahead
    if(k.roll_pred_max>0&&sn>k.roll_pred_max){sy*=k.roll_pred_max/sn;sz*=k.roll_pred_max/sn;}
    d=flight::unit(flight::V{goal.x,float(goal.y+sy*PI/180),float(goal.z+sz*PI/180)});}
   {const double ca=std::clamp(double(d.x),-1.0,1.0);
    if(ca<std::cos(25*PI/180)){const double lat=std::hypot(double(d.y),double(d.z));
     const double sy=lat>1e-9?d.y/lat:0,sz=lat>1e-9?d.z/lat:1;
     d={float(std::cos(25*PI/180)),float(std::sin(25*PI/180)*sy),float(std::sin(25*PI/180)*sz)};}}
   const double gr=d.y,gu=d.z,rho=std::hypot(gr,gu);
   double tr=rho>1e-6?gr/rho:0,tu=rho>1e-6?gu/rho:1;
   double ur=g.bank_r,uu=g.bank_u;{const double n=std::hypot(ur,uu);if(n>1e-3){ur/=n;uu/=n;}else{ur=0;uu=1;}}
   if(k.hold_level>0){
    // WT-style: level inside a zone around the nose that grows with mouse stillness, at once
    // and proportionally. AC8 keeps the zone small (levelling far out rotates the pull/yaw directions
    // mid-approach) and holds the current bank outside it; hysteresis avoids toggling at the edge.
    const double on=k.level_on+k.level_on_still*still,fm=std::hypot(ffp,ffy);
    if(leveling){if(out.angle>on+k.level_hyst||fm>k.level_ff*k.level_ff_hyst)leveling=false;}
    else if(out.angle<on&&fm<k.level_ff)leveling=true;
    if(k.hold_level>=2){
     // WT-style continuous blend: the roll reference turns from "hold" to "level" smoothly as the nose
     // closes in (zone grows with stillness) and as the mouse slows down; no on/off switch.
     const double za=std::clamp((on+k.level_band-out.angle)/std::max(k.level_band,.1),0.0,1.0);
     const double zf=std::clamp((2*k.level_ff-fm)/std::max(k.level_ff,.1),0.0,1.0);
     level_w=za*za*(3-2*za)*zf*zf*(3-2*zf);leveling=level_w>0;
     double lerr=std::atan2(ur,uu)*level_w;
     if(k.level_smooth>0){
      // Smooth levelling: a triple-pole critically damped reference takes the bank from where it is (and the
      // current roll rate) to wings-level -- bell-shaped roll rate, peak ~0.27*w*bank, done in ~6/w s, no
      // slam to the rate cap, no plateau, no notchy tail. The roll follows the reference rate (feed-forward)
      // plus a tracking correction.
      const double q=std::atan2(ur,uu);                         // q: roll still needed to be wings-level
      // Once started, a levelling is carried through to wings-level (the bank-assist pull may push the nose a
      // few degrees out of the zone meanwhile); only real mouse motion or a clearly new offset cancels it.
      const bool still_gate=k.level_still_t>0&&g.aim_still>=0;
      const double slow_thr=std::max(k.level_slow,k.level_slow_k*c.cap[1]*180/PI);
      if(still_gate){slow_t=k.level_slow_k>0?(g.aim_speed<slow_thr?slow_t+dt:0):double(g.aim_still);}
      const bool quick=still_gate&&k.level_quiet_t>0&&g.aim_quiet>=0;
      // Idle: no mouse counts for level_quiet_t with the windowed speed already low; or (fallback, e.g. a noisy
      // sensor that never reports exactly zero) slow for level_still_t.
      const bool idle=(quick&&g.aim_quiet>=k.level_quiet_t&&(g.aim_speed<slow_thr||g.aim_quiet>=k.level_still_t))||slow_t>=k.level_still_t;
      // With the windowed aim estimate: level only once the nose has arrived and the mouse has truly been still
      // for level_still_t (longer than the ~0.1-0.2 s pauses inside a bursty mouse move); any real mouse
      // motion cancels it. Before that the bank is held (no partial pre-levelling during the approach).
      // Quick gate: a levelling also stops when the mouse moves again and the nose is no longer arrived (a new small
      // offset): continuing to roll while the nose closes on it bows the path (arc L-shape).
      if(lvl_on&&(still_gate?(quick?g.aim_quiet<.05&&(g.aim_speed>std::max(k.level_cancel,slow_thr)||(k.level_cancel_dev>0&&out.angle>k.level_arrive+k.level_cancel_dev))
                                     :slow_t<.05&&g.aim_speed>std::max(k.level_cancel,k.level_slow_k>0?slow_thr:0.0))||out.angle>k.level_arrive+k.level_keep
                            :(out.angle>on+k.level_band+k.level_keep||fm>k.level_ff*k.level_ff_hyst)))lvl_on=false;
      // The reference peaks at ~0.271*w*|bank|: slow it down for big banks so the roll rate stays bell-shaped
      // under the level_max ceiling instead of flattening against it.
      if(!lvl_on&&(still_gate?idle&&out.angle<k.level_arrive:level_w>.5)){lvl_on=true;lx=q;lx1=-r[2];lx2=0;lhold=q;lw=std::min(k.level_smooth,k.level_max*PI/180/(.271*std::max(std::abs(q),.05)));
       lvl_idle=still_gate;
       if(k.level_profile==1)plan_level(q,lx1,k);}
      const double w=lw;
      if(lvl_on){
       level_w=1;leveling=true;
       const double tgt=0;
       lvl_q=q;
       if(k.level_profile==1){
        // Reference bank and rate (rate taken level_lead s ahead: AC8's stick-to-roll-rate lag).
        lq_t+=dt;auto qx=[&](double s){s=std::clamp(s,0.0,lq_T);return lq_c[0]+s*(lq_c[1]+s*s*(lq_c[2]+s*(lq_c[3]+s*lq_c[4])));};
        auto qv=[&](double s){if(s>=lq_T)return 0.0;s=std::max(s,0.0);return lq_c[1]+s*s*(3*lq_c[2]+s*(4*lq_c[3]+s*5*lq_c[4]));};
        // Re-arm: a completed levelling whose bank was later moved away from wings-level (keyboard roll
        // takeover, F-18 flight: bank held at 94 deg for 15 s) is planned again from the current bank and rate.
        if(lq_done&&std::abs(q)>k.level_rearm*PI/180)plan_level(q,-r[2],k);
        if(lq_done){lx=q;lx1=0;}else{lx=qx(lq_t);lx1=qv(lq_t+k.level_lead);}lx2=0;
       }else{
       const double x3=w*w*w*(tgt-lx)-3*w*w*lx1-3*w*lx2;
       lx2+=x3*dt;lx1+=lx2*dt;lx+=lx1*dt;
       }
       if(std::abs(q)<k.level_done*PI/180&&std::abs(lx)<k.level_done*PI/180){lx=q;lx1=0;lx2=0;lq_done=true;}   // done: no chasing the last degree
       lerr=std::remainder(q-lx,2*PI);lvl_ff=-lx1;
      }
      else if(still_gate){lerr=0;level_w=0;leveling=false;}
     }
     ur=std::sin(lerr);uu=std::cos(lerr);
    }else if(!leveling){ur=0;uu=1;}
   }
   const flight::V W{g.up_f,g.bank_r,g.bank_u};
   const flight::V chord=flight::unit(flight::V{goal.x-1,goal.y,goal.z});
   // Decisions look decide_lead seconds ahead along the aim motion: a target running toward the belly is
   // rolled over for at once instead of being pushed after until it is 17 deg away.
   const double lead=k.decide_lead*PI/180;
   // decide_win: the decisions use the windowed aim velocity (continuous through bursty mouse input) instead of
   // the Lua-tick reference, which reads zero in the pauses of a bursty swipe across the nose.
   const bool dwin=k.decide_win>0&&g.aim_still>=0;
   const double dmu=dwin?double(g.aim_up):mup,dmr=dwin?double(g.aim_right):mri;
   const double pgy=goal.y+dmr*lead,pgz=goal.z+dmu*lead;
   const double rho_true=std::hypot(pgy,pgz);
   // Already pushing toward a target below: keep pushing until it is clearly far (no 17 deg flip to a 170 deg roll).
   roll_pull=pgz<0&&rho_true>(push_prev&&k.push_cost>0?k.pull_over_push:.3)&&flight::dot(chord,W)>-.7;
   // AC8 pushes slowly (PitchUpDownR): push only for nearby targets; far below, roll over and pull.
   if(k.push_cost>0){
    // Pull or push: compare the roll needed to put the target overhead vs. under the belly, pushing
    // penalised more the farther the target is; hysteresis keeps the choice. Continuous where WT's fixed
    // threshold flips the roll goal by ~180 deg (indecision when the mouse swings across the nose).
    bool want=false;
    if(!roll_pull&&pgz<0&&rho_true<=(push_prev?k.pull_over_push:.3)){
     const double pull=std::abs(std::atan2(tr,tu)),push=std::abs(std::atan2(-tr,-tu))+(k.push_cost+k.push_cost_rho*rho_true/.3)*PI/180;
     const double h=k.push_hyst*PI/180;
     want=push_prev?push<pull+h:push+h<pull;
     // A target running toward the belly faster than a push can close on it is rolled over for instead.
     if(want&&k.push_escape>0){const double cap_push=c.cap[0]*p.pud*180/PI,away=(dmu*goal.z+dmr*goal.y)/std::max(std::hypot(double(goal.y),double(goal.z)),1e-6);   // aim speed away from the nose, deg/s
      if(away>k.push_escape*cap_push)want=false;}
    }
    push_mode=push_prev=want;
   }else push_mode=!roll_pull&&goal.z<-.1&&rho_true<=.3;
   double lam;
   if(k.alloc>0){
    // Minimum-time allocation of the redundant roll: three controls, two-axis nose alignment. Choose the roll
    // change phi minimising (roll time) + (remaining pitch/yaw time), using the exact AC8 capabilities
    // (pull vs. slower push, weak yaw, roll acceleration and the stock input lag, the current roll rate);
    // keep the previous choice unless another is clearly faster. Replaces the lambda blend, the push
    // threshold and the roll-over rule with one cost: small offsets are fixed with pitch+yaw, large ones
    // rolled into the lift plane, near targets below pushed, far ones rolled over.
    const double phi=allocate(p,c,r,goal,bank,mode,k);
    const double after=std::cos(std::atan2(double(goal.y),double(goal.z))-phi);
    push_mode=after<0;roll_pull=false;push_prev=push_mode;
    lam=std::abs(phi)>.03?1.0:0.0;
    roll_err=phi*(1-level_w)+std::atan2(ur,uu);
   }else{
   if(push_mode){tr=-tr;tu=-tu;}
   lam=roll_pull?1.0:std::clamp((rho-std::sin(k.lam_dead*PI/180))/(k.lam0+k.lam_still*still),0.0,1.0);
   const double vr=ur+lam*(tr-ur),vu=uu+lam*(tu-uu);
   roll_err=std::atan2(vr,vu);
   }
   double wm=0;
   // A levelling started by the idle-mouse gate owns the roll: the filtered motion reference still decays for
   // ~0.4 s after the mouse stops and must not hold the bank (or bend the levelling) meanwhile.
   if(k.motion_wt>0&&!roll_pull&&(!push_mode||k.alloc>0)&&!(lvl_on&&lvl_idle)){   // sustained turns: lift along the target motion
    const bool win=k.motion_win>0&&g.aim_still>=0;
    const double mp_=win?(k.motion_win>=2?mwu:double(g.aim_up)):k.motion_raw>0?mupm:ffpm,my_=win?(k.motion_win>=2?mwr:double(g.aim_right)):k.motion_raw>0?mrim:ffym;   // windowed / unclamped / clamped aim motion
    double lat=std::hypot(mp_,my_);const double yawcap=c.cap[1]*180/PI;
    // Sustained motion only (motion_win 3: path speed over 0.5 s caps the magnitude; 4: ramp in with the
    // time the aim has kept moving): a lone correction flick is not a turning target.
    if(win&&k.motion_win==3)lat=std::min(lat,double(g.aim_path));
    if(win&&k.motion_win==4){const double s=std::clamp((double(g.aim_moving)-k.motion_sus0)/std::max(k.motion_sus1-k.motion_sus0,.01),0.0,1.0);lat*=s*s*(3-2*s);}
    const double x=std::clamp((lat/std::max(yawcap,.1)-k.motion_on)/(k.motion_full-k.motion_on),0.0,1.0);wm=x*x*(3-2*x)*k.motion_wt;
    // Tracking aid only: far off the nose the roll must bring the target overhead (not follow the mouse
    // motion), otherwise the goal flips between the two whenever the target crosses the wing line.
    if(k.motion_far>0){const double z=std::clamp((k.motion_far-out.angle)/(k.motion_far-k.motion_near),0.0,1.0);wm*=z*z*(3-2*z);}
    if(wm>0){const double psi=std::clamp(std::atan2(my_,std::max(mp_,.12*lat)),-PI/2,PI/2);roll_err=roll_err*(1-wm)+psi*wm;}
   }
   // WT rolls gently near the target. A fast-moving target needs its motion bank promptly (AC8 yaw is
   // weak, WT's residual would grow instead), so the motion-bank share rolls at the far-target rate.
   const double coeff=std::max(rho<=.17?.3:rho>=.4?1.5:.3+(1.5-.3)*(rho-.17)/.23,.3+1.2*std::min(wm,1.0)*k.motion_fast);
   const double pmax=c.cap[2]*180/PI;
   const double T_roll=std::max(k.roll_time_mult*90/(std::max(pmax,1.0)*coeff),.05);
   wt_rate=roll_err/T_roll;                              // desired roll rate (rad/s), proportional
   // Levelling: proportional (time constant level_tau) with a rate ceiling, like WT's roll PD on the
   // level error; no ramped goal, so it starts at once and settles without a long tail.
   // Applies whenever the roll goal is pure levelling (inside the no-bank zone), also while a small push
   // corrects the nose: switching laws with the push decision made the levelling roll pulse.
   if(k.hold_level>0&&leveling&&!roll_pull&&wm<.5&&lam<.05){
    const double lm=k.level_max*PI/180;
    // level_sine: WT's sine roll-error proxy (fast through 90 deg, easing in near level); else proportional.
    // Sine law: level_sine = rate at 90 deg (deg/s), capped at level_max; gain near level = level_sine (rad/s per rad).
    wt_rate=k.level_sine>0?std::clamp(k.level_sine*PI/180*(std::abs(roll_err)>=PI/2?std::copysign(1.0,roll_err):std::sin(roll_err)),-lm,lm):std::clamp(roll_err/std::max(k.level_tau,.05),-lm,lm);
    // Small banks level gently (time constant level_tau_small): no quick roll blips between small aim moves.
    if(k.level_tau_small>0&&std::abs(wt_rate)>std::abs(roll_err)/k.level_tau_small)wt_rate=roll_err/k.level_tau_small;
    if(k.level_smooth>0&&lvl_on){wt_rate=std::clamp(lvl_ff+roll_err/std::max(k.level_track,.03),-lm,lm);lvl_law=k.level_profile==1;}
   }
   if(roll_pull)pitch_err=std::max(pitch_err,0.0);
   else if(push_mode)pitch_err=std::min(pitch_err,0.0);
   else pitch_err=std::max(pitch_err,-.1);
   wt_rho=rho;
  }else if(k.geo_wt>0){
   // War Thunder-style mouse-aim geometry. In the body Y-Z plane:
   // u = level direction (world up), t = target direction, rho = sin(off-axis). Roll goal
   // v = normalize(u + lambda*(t-u)), lambda = rho/(0.125+0.3*still): small offsets get a proportional
   // bank (lift toward the target, straight approach), larger ones bank fully; the level zone grows
   // while the mouse is still. Continuous everywhere: no level/turn hysteresis jumps.
   const double moving=std::hypot(ffp,ffy)>1.0;
   still=moving?0.0:std::min(1.0,still+.4*dt);
   const double gr=goal.y,gu=goal.z,rho=std::hypot(gr,gu);
   double tr=rho>1e-6?gr/rho:0,tu=rho>1e-6?gu/rho:1;
   double ur=g.bank_r,uu=g.bank_u;{const double n=std::hypot(ur,uu);if(n>1e-3){ur/=n;uu/=n;}else{ur=0;uu=1;}}
   // Belly side: large offsets roll over and pull; small ones (target below the wing plane) push.
   const flight::V W{g.up_f,g.bank_r,g.bank_u};
   const flight::V chord=flight::unit(flight::V{goal.x-1,goal.y,goal.z});
   roll_pull=gu<0&&rho>.3&&flight::dot(chord,W)>-.7;
   push_mode=!roll_pull&&gu<-.1*1.0&&rho<=.3;
   if(push_mode){tr=-tr;tu=-tu;}
   const double lam=roll_pull?1.0:std::clamp(rho/(.125+.3*still),0.0,1.0);
   const double vr=ur+lam*(tr-ur),vu=uu+lam*(tu-uu);
   roll_err=std::atan2(vr,vu);
   // Motion bank (sustained turns): when the target moves faster than yaw can follow, blend the roll goal
   // toward aligning the pull direction with the (low-passed) target motion.
   if(k.motion_wt>0&&!roll_pull&&!push_mode){
    const double lat=std::hypot(ffpm,ffym),yawcap=c.cap[1]*180/PI;
    const double x=std::clamp((lat/std::max(yawcap,.1)-k.motion_on)/(k.motion_full-k.motion_on),0.0,1.0),wm=x*x*(3-2*x)*k.motion_wt;
    if(wm>0){const double psi=std::clamp(std::atan2(ffym,std::max(ffpm,.12*lat)),-PI/2,PI/2);roll_err=roll_err*(1-wm)+psi*wm;}
   }
   if(!push_mode&&pitch_err<0&&!roll_pull)pitch_err=std::max(pitch_err,-.1);   // no big push outside push mode
   if(roll_pull)pitch_err=std::max(pitch_err,0.0);
   if(push_mode)pitch_err=std::min(pitch_err,0.0);
  }else{
  // Push only for nearby targets below; far below, roll the target over the top and pull.
  bool force_roll=false;
  if(pitch_err<0){const double w=std::clamp((k.push_none-out.angle)/(k.push_none-k.push_full),0.0,1.0);pitch_err*=w;force_roll=w<1;}
  const float mix=level.step(float(out.angle),float(dt));
  double turn=std::clamp(std::atan2(double(goal.y),std::max(.12,double(goal.z))),-PI/2,PI/2);
  if(force_roll&&goal.z<0&&std::abs(turn)<PI/2){const double side=goal.y>=0?1:-1;const double w=std::clamp((out.angle-k.push_full)/(k.push_none-k.push_full),0.0,1.0);turn+=(side*PI/2-turn)*w;}
  // Motion bank: when the target moves faster than yaw can follow, align the pull (lift)
  // direction with the target's relative motion instead of levelling (sustained turns).
  double near_goal=bank;
  {const double fp=ffpm,fy=ffym,yawcap=c.cap[1]*180/PI;
   const double lat=std::hypot(fp,fy),x=std::clamp((lat/std::max(yawcap,.1)-k.motion_on)/(k.motion_full-k.motion_on),0.0,1.0),wm=x*x*(3-2*x);
   if(wm>0){const double psi=std::clamp(std::atan2(fy,std::max(fp,.12*lat)),-PI/2,PI/2);near_goal=bank*(1-wm)+psi*wm;}}
  roll_err=near_goal*(1-mix)+turn*mix;
  }
  // WT-style pull gating: until the roll has aligned the target
  // with the pull plane, shrink pitch/yaw demand; released while rolling fast (alignment imminent).
  if(k.wt_gate>0){
   const double rho=k.geo_wt>=2?wt_rho:std::hypot(double(goal.y),double(goal.z)),sr=std::sin(std::clamp(roll_err,-PI/2,PI/2));
   const double f0=k.gate_f0*(rho<=.15?2.0:rho>=.35?.06:2.0+(.06-2.0)*(rho-.15)/.2);
   const double sr2=std::abs(roll_err)>PI/2?1.0:sr*sr;
   const double R=std::max(std::abs(r[2]),k.gate_rfloor),gate=roll_pull&&k.geo_wt>0?.02:std::clamp(std::max(f0*R/std::max(sr2,1e-3),.02),0.0,1.0);
   pitch_err*=gate;yaw_gate=gate;
  }else yaw_gate=1;
  const double roll_goal=std::remainder(-bank+roll_err,2*PI);
  const float roll_rate_goal=roll_seen?float(std::remainder(roll_goal-last_roll_goal,2*PI)/dt*180/PI):0.f;
  last_roll_goal=roll_goal;roll_seen=true;
  const double ff_roll=roll_ref.update(roll_rate_goal,true,float(dt),float(c.cap[2]*180/PI))*PI/180;
  // Profiled levelling: the roll axis' time-optimal brake aims at wings-level (the whole remaining bank), not at
  // the reference point a few degrees ahead -- that held the roll ~0.5 s behind the reference.
  const double errors[3]={pitch_err,(std::abs(yaw_err)*180/PI<k.yaw_dead?0.0:yaw_err-std::copysign(k.yaw_dead*PI/180,yaw_err))*(k.wt_gate>1?yaw_gate:1.0),lvl_law?lvl_q:roll_err};
  // Roll goal motion is dominated by the goal's dependence on the aircraft's own attitude
  // (PEACE's max(.12, up) clamp), not by target motion: no roll feed-forward, re-solve each tick.
  const double ff[3]={k.ff_gain*ffp*PI/180,k.ff_gain*ffy*PI/180,k.roll_ff*ff_roll};
  const double assist=fe::bank_assist(c,g.bank_u);
  {const double cpe=std::max(c.cap[0]*(errors[0]<0?double(p.pud):1.0),1e-6);
   const double ty=std::abs(errors[1])/(.7*std::max(c.cap[1],1e-6))+.35,tp=std::abs(errors[0])/(.7*cpe)+.2;
   sync_ty=ty;sync_tp=tp;sync_yaw=false;
   if(k.path_lock>0){
    // Path lock: the nose's angular velocity must point at the target at every instant (straight nose path,
    // no L-shapes). The faster of pitch/yaw follows the slower one's progress in the ratio of their
    // errors, whatever the angle or roll state (rolling does not move the nose; roll_lead covers the lag).
    const bool both=std::abs(errors[0])*180/PI>.2&&std::abs(errors[1])*180/PI>.2;
    sync_active=both&&ty>k.path_ratio*tp;
    sync_yaw=both&&tp>k.path_ratio*ty;
   }else
   sync_active=k.sync>0&&(mode==Mode::Direct||k.sync>1)&&out.angle<k.sync_max&&(std::abs(errors[2])*180/PI<k.sync_roll||out.angle<k.sync_angle)&&std::abs(errors[1])*180/PI>.2&&ty>1.2*tp&&std::abs(errors[0])>1e-5&&!push_mode;}
  out.sync=sync_active;
  for(int a=0;a<3;++a){
   const Mode m=a==1?Mode::Direct:mode;
   const double cap=c.cap[a];
   xs[a]=0;axes[a]=detail::Axis{&p,a,cap,dt,1,0,m};terminal[a]=true;
   if(cap<=1e-6){out.S[a]=0;continue;}
   detail::Axis ax{&p,a,cap,dt,a==0?double(p.pud):1.0,a==0?assist:0.0,m};
   const double e=errors[a];
   // Internal rate whose applied rate matches the target's own motion (relative rest).
   const double r_end=ax.internal(ff[a]);
   out.errors[a]=e;out.rate_des[a]=r_end;
   // Time-optimal shooting: this tick's action x, then the brake policy; choose x so the
   // predicted relative travel to rest equals the remaining error.
   const double side=e>=0?1:-1,goal_travel=std::abs(e)*(1-k.margin);
   const double go=side;
   const double brk=ax.brake_action(S[a],r[a],side,ff[a],r_end);
   auto travel=[&](double x){double Sx=S[a],rx=r[a];ax.tick(Sx,rx,x);
    return std::max(0.0,side*(ax.applied(rx)-ff[a]))*dt+ax.brake(Sx,rx,side,ff[a],r_end);};
   double x;
   if(travel(go)<=goal_travel)x=go;
   else if(travel(brk)>=goal_travel)x=brk;
   else{double lo=brk,hi=go;for(int it=0;it<16;++it){const double mid=(lo+hi)/2;if(travel(mid)>goal_travel)hi=mid;else lo=mid;}x=lo;}
   // The game's rate law is discontinuous at a command state of exactly zero (fast DecRotR decay)
   // versus near zero (slow AddRotR approach). For a resting goal, a near-zero solution means
   // "coast slowly"; exact zero stops faster without overshoot, so prefer it.
   {const double z=m==Mode::Stick?k.stick_min:k.state_min;
    if(std::abs(x)<z&&std::abs(r_end)<1e-4&&x!=0.0&&travel(0.0)<=goal_travel)x=0.0;}
   out.dbg[a][0]=travel(go);out.dbg[a][1]=travel(brk);out.dbg[a][2]=goal_travel;out.dbg[a][3]=x;
   {double Sx=S[a],rx=r[a];ax.tick(Sx,rx,brk);out.stop[a]=ax.brake(Sx,rx,side,ff[a],r_end);}
   // Exact inversion: action that drives the internal rate toward rd (rate stage, then input stage).
   auto invert=[&](double rd)->double{
    const double want=r[a]+(rd-r[a])*std::min(1.0,dt/k.tau_rate);
    const double gS=fe::rate_gain(p,a,r[a],1,cap);
    double s1=gS*dt>1e-9?std::clamp((r[a]+(want-r[a])/(gS*dt))/cap,-1.0,1.0):0;
    if(std::abs(s1)<1e-6)s1=std::copysign(1e-6,want-r[a]+1e-12);
    const double r1=fe::rate_axis(p,a,r[a],s1,cap,dt),r0=fe::rate_axis(p,a,r[a],0,cap,dt);
    const double Sreq=std::abs(r0-want)<std::abs(r1-want)?0.0:s1;
    double xl=Sreq;
    if(m==Mode::Stick){
     const double ks=1.5*p.inadd[a]*dt,Swant=S[a]+(Sreq-S[a])*std::min(1.0,dt/k.tau_cmd);
     double u1=fe::stick_inverse(S[a]+(Swant-S[a])/std::max(ks,1e-9));
     if(std::abs(u1)<1e-6)u1=std::copysign(1e-6,Swant-S[a]+1e-12);
     const double S1=fe::input_axis(p,a,S[a],u1,dt),S0=fe::input_axis(p,a,S[a],0,dt);
     const bool zero=std::abs(Swant)<=p.indec[a]*dt*1.5||std::abs(S0-Swant)<std::abs(S1-Swant);
     xl=zero?0.0:std::clamp(u1,-1.0,1.0);
    }
    return xl;
   };
   // Terminal zone: linear rate tracking through the exact inversion (smooth fine aiming).
   const double eterm=k.term_zone[a]*detail::PI/180,ae=std::abs(e);
   if(ae<2*eterm){
    const double xl=invert(ax.internal(ff[a]+e/k.term[a]));
    const double w=std::clamp((ae-eterm)/eterm,0.0,1.0);
    x=w>=1?x:w<=0?xl:(m==Mode::Stick&&(xl==0.0||x==0.0)?(w>.5?x:xl):xl+(x-xl)*w);
   }
   // WT proportional roll (T_roll); the exact time-optimal action still limits it (never past the brake point).
   if(a==2&&wt_rate>-.5e9&&k.geo_wt>=2){
    const double xp=invert(ax.internal(std::clamp(wt_rate,-cap,cap)));
    // Profiled levelling: x is the time-optimal action toward wings-level itself (errors[2] = whole remaining
    // bank), so it only brakes near level (no overshoot); the quintic tracking xp shapes the smooth start.
    x=side*xp>side*x?x:xp;
   }
   // Pitch/yaw synchronization: once the roll goal is reached, yaw (slow in AC8) sets the arrival time;
   // pitch moves in proportion so the nose travels straight along the residual instead of closing the
   // pitch part first and the yaw part afterwards (no loss in arrival time).
   if(a==0&&sync_active){
    const double ey=errors[1],yrel=(r[1]-ff[1])*(ey>=0?1:-1);
    // Yaw's actual progress, floored by its planned pace (a reversing/starting yaw must not stall pitch).
    const double W=ff[0]+std::max(k.sync_floor>0?std::abs(ey)/std::max(sync_ty,.05):0.0,yrel)*e/std::abs(ey);
    const double xsync=invert(ax.internal(W));
    x=k.sync_floor>0&&side*xsync>side*x?x:xsync;
   }
   // Path lock, other direction: yaw follows pitch when pitch sets the arrival time.
   if(a==1&&sync_yaw){
    const double ep=errors[0];
    const double prel=(axes[0].applied(r[0])-ff[0])*(ep>=0?1:-1);
    const double W=ff[1]+std::max(std::abs(ep)/std::max(sync_tp,.05),prel)*e/std::abs(ep);
    const double xsync=invert(ax.internal(W));
    x=side*xsync>side*x?x:xsync;
   }
   xs[a]=x;axes[a]=ax;terminal[a]=ae<2*eterm;
  }
  if(k.joint3d>0&&std::abs(errors[2])*180/detail::PI<k.joint_roll_max){
   if(k.joint_roll_plan>0)plan_roll(S,r,errors[2],ff[2]);else roll_plan.clear();
   joint_refine(p,S,r,dt,g,c,errors,ff,k,out.angle);}
  for(int a=0;a<3;++a){
   const Mode m=a==1?Mode::Direct:mode;const double x=xs[a];
   if(c.cap[a]<=1e-6)continue;
   if(m==Mode::Direct){out.S[a]=std::clamp(x,-1.0,1.0);out.stick[a]=a==1?out.S[a]:fe::stick_inverse(out.S[a]);}
   else{out.stick[a]=std::clamp(x,-1.0,1.0);out.S[a]=fe::input_axis(p,a,S[a],out.stick[a],dt);}
  }
  out.valid=true;return out;
 }
private:
 double xs[3]{};detail::Axis axes[3]{};bool terminal[3]{};double yaw_gate=1;
 double lq_c[5]{},lq_T=0,lq_t=0,lvl_q=0;
 // Quintic from (q, rate v0, 0) to (0,0,0) over T: x(t)=q+v0 t+c3 t^3+c4 t^4+c5 t^5.
 void plan_level(double q,double v0,const Tuning& k){
  const double a=std::abs(q)*180/detail::PI;
  const double T=std::max({1.875*a/std::max(k.level_vmax,1.0),std::sqrt(5.77*a/std::max(k.level_amax,1.0)),k.level_tmin});
  lq_T=T;lq_t=0;lq_done=false;lq_c[0]=q;lq_c[1]=v0;
  lq_c[2]=(-20*q-12*v0*T)/(2*T*T*T);lq_c[3]=(30*q+16*v0*T)/(2*T*T*T*T);lq_c[4]=(-12*q-6*v0*T)/(2*T*T*T*T*T);
 }
bool lq_done=false,lvl_idle=false,lvl_law=false;
 double ffp=0,ffy=0,ffpm=0,ffym=0,still=0,slow_t=0,mwu=0,mwr=0,mup=0,mri=0,mupm=0,mrim=0,wt_rate=-1e9,wt_rho=0,sync_ty=1,sync_tp=1,level_w=0,lw=4,lx=0,lx1=0,lx2=0,lhold=0,lvl_ff=0,alloc_target=0,alloc_cost=0;bool alloc_have=false;bool lvl_on=false,leveling=false,push_prev=false,roll_pull=false,push_mode=false,sync_active=false,sync_yaw=false;
 std::vector<double> roll_plan;   // roll rate per tick of the roll's own time-optimal plan to its goal
 void plan_roll(const double S[3],const double r[3],double e,double ffr){
  roll_plan.clear();const auto& ax=axes[2];if(ax.cap<=1e-6)return;
  double Sx=S[2],rx=r[2],rem=e;
  ax.tick(Sx,rx,xs[2]);rem-=(ax.applied(rx)-ffr)*ax.dt;roll_plan.push_back(rx);
  for(int n=0;n<240;++n){
   const double side=rem>=0?1:-1;
   double Sg=Sx,rg=rx;ax.tick(Sg,rg,side);
   const double tr=std::max(0.0,side*(ax.applied(rg)-ffr))*ax.dt+ax.brake(Sg,rg,side,ffr,ax.internal(ffr));
   const double x=tr<=std::abs(rem)?side:ax.brake_action(Sx,rx,side*(ax.applied(rx)-ffr>=0?1:-1),ffr,ax.internal(ffr));
   ax.tick(Sx,rx,x);rem-=(ax.applied(rx)-ffr)*ax.dt;roll_plan.push_back(rx);
   if(std::abs(rem)<1e-3&&std::abs(ax.applied(rx)-ffr)<1e-3)break;
  }
 }
 // WT-style stop-attitude refinement: with every axis simulated on the exact model (this tick's
 // actions, then each axis braking to relative rest), find the pitch and yaw actions that leave
 // the target on the nose at the predicted stop attitude. Captures roll/pitch coupling.
 struct Errs {double p,y;};
 Errs stop_errors(const double S[3],const double r[3],double xp,double xy,double xr,const Geometry& g,const double ff[3],const fe::Caps& cc)const{
  double Sx[3]={S[0],S[1],S[2]},rx[3]={r[0],r[1],r[2]};
  flight::V T=flight::unit(g.goal),W{g.up_f,g.bank_r,g.bank_u};
  const flight::V Om{0.f,float(-ff[0]),float(ff[1])};   // target angular velocity (body, rad/s)
  const double dt=axes[0].dt;const double xs0[3]={xp,xy,xr};size_t step=0;
  auto advance=[&](){
   // Bank assist follows the predicted world-up component of body up.
   const double wp=(rx[0]<0?rx[0]*axes[0].pud:rx[0])+fe::bank_assist(cc,W.z),wy=rx[1];
   const double wr=roll_plan.empty()?rx[2]:step<roll_plan.size()?roll_plan[step]:0.0;++step;
   const flight::V w{float(-wr*dt),float(-wp*dt),float(wy*dt)};   // body rotation this tick
   const double n=std::sqrt(double(flight::dot(w,w)));
   auto rotv=[&](flight::V v){ if(n<1e-12)return v;const flight::V ax=w*float(1/n);const double ca=std::cos(-n),sa=std::sin(-n);
    return v*float(ca)+flight::cross(ax,v)*float(sa)+ax*float(flight::dot(ax,v)*(1-ca));};
   T=rotv(T);W=rotv(W);
   T=flight::unit(T+flight::cross(Om,T)*float(dt));
  };
  for(int a=0;a<3;++a)axes[a].tick(Sx[a],rx[a],xs0[a]);
  advance();
  for(int it=0;it<300;++it){
   bool moving=false;
   if(!roll_plan.empty()&&step<roll_plan.size())moving=true;
   for(int a=0;a<3;++a){const double rel=axes[a].applied(rx[a])-ff[a];if(std::abs(rel)<1e-4)continue;moving=true;
    const double side=rel>0?1:-1;axes[a].tick(Sx[a],rx[a],axes[a].brake_action(Sx[a],rx[a],side,ff[a],axes[a].internal(ff[a])));
    if(side*(axes[a].applied(rx[a])-ff[a])<0)rx[a]=axes[a].internal(ff[a]);}
   if(!moving)break;
   advance();
  }
  const double fx=std::max(.02,double(T.x));
  return {std::atan2(double(T.z),fx),std::atan2(double(T.y),fx)};
 }
 void joint_refine(const fe::Params& p,const double S[3],const double r[3],double dt,const Geometry& g,const fe::Caps& c,const double errors[3],const double ff[3],const Tuning& k,double angle){
  (void)p;(void)dt;
  if(angle>90)return;                       // behind the wing line: keep the per-axis geometry
  for(int pass=0;pass<3;++pass){
   const int a=pass==1?1:0;
   if(terminal[a]||axes[a].cap<=1e-6)continue;
   const double e=errors[a];if(std::abs(e)<1e-6)continue;
   if(a==0&&e<0&&std::abs(e)<std::abs(std::atan2(double(g.goal.z),std::max(.02,double(g.goal.x))))*.999)continue; // push gated: keep
   const double side=e>=0?1:-1,want=std::abs(e)*k.margin;
   auto f=[&](double x){const auto er=a==0?stop_errors(S,r,x,xs[1],xs[2],g,ff,c):stop_errors(S,r,xs[0],x,xs[2],g,ff,c);return side*(a==0?er.p:er.y);};
   const double go=side,brk=axes[a].brake_action(S[a],r[a],side,ff[a],axes[a].internal(ff[a]));
   double x;
   if(f(go)>=want)x=go;
   else if(f(brk)<=want)x=brk;
   else{double lo=brk,hi=go;for(int i=0;i<14;++i){const double mid=(lo+hi)/2;if(f(mid)<want)hi=mid;else lo=mid;}x=lo;}
   xs[a]=x;
  }
 }
 flight::LevelBlend level;bool roll_seen=false;double last_roll_goal=0;model_control::Reference roll_ref;
};

}
