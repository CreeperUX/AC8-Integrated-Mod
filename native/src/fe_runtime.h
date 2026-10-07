#pragma once
// Runtime glue for WAR1 / WAR2 (control modes 4 / 5): exact AC8 flight-engine state read from the
// player pawn inside the input hook (re/AC8-FlightEngine notes). Layout verified on the shipped
// AceCombat8.exe; every read is guarded and validated, and any failure falls back to WAR (v11).
#include "fe_control.h"
#include <share.h>

namespace fe_runtime {

constexpr uintptr_t OFF_FE=0x2268,OFF_S=0x22A0,OFF_R=0x2300,OFF_P=0x2C28;
constexpr uintptr_t OFF_QSEL=OFF_FE+0x868,OFF_Q1=OFF_FE+0x8F8,OFF_Q2=OFF_FE+0x920;
constexpr size_t P_BYTES=0x430;

struct Quat {double x=0,y=0,z=0,w=1;};
inline flight::V rot(const Quat& q,flight::V v){   // q * v * q^-1 (game's own convention, 0x4D2FEC0)
 const double ux=q.x,uy=q.y,uz=q.z,vx=v.x,vy=v.y,vz=v.z;
 const double tx=2*(uy*vz-uz*vy),ty=2*(uz*vx-ux*vz),tz=2*(ux*vy-uy*vx);
 return {float(vx+q.w*tx+(uy*tz-uz*ty)),float(vy+q.w*ty+(uz*tx-ux*tz)),float(vz+q.w*tz+(ux*ty-uy*tx))};
}
inline Quat conj(const Quat& q){return {-q.x,-q.y,-q.z,q.w};}

// Frame correspondence (verified in flight 2026-10-07: the calibration bank error was exactly 2x bank
// with X taken as the right wing): FE body X is the LEFT wing, Y up, Z forward. Mod/UE body axes are
// (f, r, u) with r the right wing, so FE (X,Y,Z) = (-r, u, f). The controller works in the mod frame
// (+roll = right wing down, +yaw = nose right); FE roll and yaw command states and rates are mirrored.
// This matches the mod's long-standing roll_sign = yaw_sign = -1 for the stick axes.
inline flight::V fe_from_mod(flight::V m){return {-m.y,m.z,m.x};}   // (f,r,u) -> (X,Y,Z)
inline flight::V mod_from_fe(flight::V v){return {v.z,-v.x,v.y};}   // (X,Y,Z) -> (f,r,u)
constexpr double MIRROR[3]={1,-1,-1};                                 // pitch, yaw, roll: FE <-> mod

// Raw reads (POD only, SEH-safe).
struct Raw {double S[3];double R[3];float speed;float q[4];unsigned char qsel;};
inline bool read_raw(uintptr_t pawn,Raw& out){
 __try{
  std::memcpy(out.S,reinterpret_cast<const void*>(pawn+OFF_S),24);
  std::memcpy(out.R,reinterpret_cast<const void*>(pawn+OFF_R+0x178),24);
  std::memcpy(&out.speed,reinterpret_cast<const void*>(pawn+OFF_R+0x18),4);
  out.qsel=*reinterpret_cast<const unsigned char*>(pawn+OFF_QSEL);
  std::memcpy(out.q,reinterpret_cast<const void*>(pawn+(out.qsel?OFF_Q1:OFF_Q2)),16);
 }__except(EXCEPTION_EXECUTE_HANDLER){return false;}
 return true;
}
inline bool read_params_raw(uintptr_t pawn,unsigned char* buf){
 __try{std::memcpy(buf,reinterpret_cast<const void*>(pawn+OFF_P),P_BYTES);}__except(EXCEPTION_EXECUTE_HANDLER){return false;}
 return true;
}
inline bool quat_of(const Raw& r,Quat& q){
 q={r.q[0],r.q[1],r.q[2],r.q[3]};const double n=std::sqrt(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w);
 if(!std::isfinite(n)||std::abs(n-1)>.01)return false;q.x/=n;q.y/=n;q.z/=n;q.w/=n;return true;
}
inline bool read_quat(uintptr_t pawn,Quat& q){Raw r;return read_raw(pawn,r)&&quat_of(r,q);}

// Lua-tick hand-off: target in body axes at the instant the UE pose was sampled, plus the
// flight-engine attitude at that same instant. The hook re-expresses the target in the current
// body frame with the attitude change since, so no world-frame mapping between UE and the
// flight engine is assumed.
struct Goal {uintptr_t pawn=0;Quat q;flight::V body{1,0,0};float ff_pitch=0,ff_yaw=0,mot_up=0,mot_right=0;uint64_t tick=0;bool valid=false;};
inline Goal goal;inline std::mutex goal_mutex;
inline std::atomic<float> calib_pitch_err{0},calib_bank_err{0};inline std::atomic<int> calib_bad{0};

// Called on the Lua tick with the UE body basis used for aiming. Also checks that the flight-engine
// attitude agrees with the UE pose (pitch and bank), which validates the axis correspondence.
inline void publish_goal(uintptr_t pawn,const flight::Basis& b,flight::V aim,float ff_pitch,float ff_yaw,uint64_t tick,float mot_up=0,float mot_right=0){
 Quat q;
 if(!pawn||!read_quat(pawn,q)){std::lock_guard<std::mutex> l(goal_mutex);goal.valid=false;return;}
 // FE body axes: X left wing, Y up, Z forward; FE world up is +Y.
 const auto fwd=rot(q,{0,0,1}),left=rot(q,{1,0,0}),up=rot(q,{0,1,0});
 const float pe=std::abs(std::asin(std::clamp(fwd.y,-1.f,1.f))-std::asin(std::clamp(b.f.z,-1.f,1.f)))/flight::rad;
 const float be=std::abs(std::remainder(std::atan2(-left.y,up.y)-std::atan2(b.r.z,b.u.z),2*3.14159265f))/flight::rad;
 calib_pitch_err=pe;calib_bank_err=std::abs(fwd.y)<.9f?be:0.f;
 if(pe>3||(std::abs(fwd.y)<.9f&&be>5))calib_bad=std::min(calib_bad.load()+1,1000);else calib_bad=std::max(calib_bad.load()-5,0);
 std::lock_guard<std::mutex> l(goal_mutex);
 goal.pawn=pawn;goal.q=q;goal.body={flight::dot(aim,b.f),flight::dot(aim,b.r),flight::dot(aim,b.u)};
 goal.ff_pitch=ff_pitch;goal.ff_yaw=ff_yaw;goal.mot_up=mot_up;goal.mot_right=mot_right;goal.tick=tick;goal.valid=true;
}
inline void invalidate(){std::lock_guard<std::mutex> l(goal_mutex);goal.valid=false;}

// Per-frame trace (bounded queue, drained by the existing logger thread).
struct Record {uint64_t t;int mode;float dt,speed;float goal[3],bank_r,bank_u,ff[2];double S[3],R[3],stick[3],Sout[3],err[3],stop[3];float angle,calib_p,calib_b,aim_speed,aim_still;};
inline std::mutex trace_mutex;inline std::array<Record,4096> trace_queue;inline size_t trace_head=0,trace_size=0;
inline std::atomic<uint64_t> trace_dropped{0};inline FILE* trace_file=nullptr;inline bool trace_failed=false;inline uint64_t trace_rows=0;
inline void trace_push(const Record& r){
 std::unique_lock<std::mutex> lock(trace_mutex,std::try_to_lock);
 if(!lock.owns_lock()||trace_size==trace_queue.size()){++trace_dropped;return;}
 trace_queue[(trace_head+trace_size)%trace_queue.size()]=r;++trace_size;
}
inline void trace_drain(const wchar_t* module_folder){
 if(trace_failed)return;
 std::array<Record,256> batch;size_t count=0;
 {std::unique_lock<std::mutex> lock(trace_mutex,std::try_to_lock);if(!lock.owns_lock())return;
  count=std::min(batch.size(),trace_size);
  for(size_t i=0;i<count;++i)batch[i]=trace_queue[(trace_head+i)%trace_queue.size()];
  trace_head=(trace_head+count)%trace_queue.size();trace_size-=count;}
 if(!count)return;
 if(!trace_file){
  wchar_t folder[MAX_PATH]{},path[MAX_PATH]{};swprintf_s(folder,L"%s\\..\\Logs",module_folder);CreateDirectoryW(folder,nullptr);
  for(int n=1;n<10000;++n){swprintf_s(path,L"%s\\FE-%05d.csv",folder,n);if(GetFileAttributesW(path)!=INVALID_FILE_ATTRIBUTES)continue;trace_file=_wfsopen(path,L"w",_SH_DENYWR);break;}
  if(!trace_file){trace_failed=true;return;}
  fputs("t_us,mode,dt,speed,goal_f,goal_r,goal_u,bank_r,bank_u,ff_pitch,ff_yaw,S_p,S_y,S_r,R_p,R_y,R_r,u_p,u_y,u_r,Sout_p,Sout_y,Sout_r,e_p,e_y,e_r,stop_p,stop_y,stop_r,angle,calib_pitch,calib_bank,aim_speed,aim_still\n",trace_file);
 }
 for(size_t i=0;i<count&&trace_rows<900000;++i,++trace_rows){const auto& r=batch[i];
  fprintf(trace_file,"%llu,%d,%.6g,%.6g,%.6g,%.6g,%.6g,%.6g,%.6g,%.6g,%.6g,%.7g,%.7g,%.7g,%.7g,%.7g,%.7g,%.6g,%.6g,%.6g,%.7g,%.7g,%.7g,%.6g,%.6g,%.6g,%.6g,%.6g,%.6g,%.5g,%.4g,%.4g,%.4g,%.4g\n",
   (unsigned long long)r.t,r.mode,r.dt,r.speed,r.goal[0],r.goal[1],r.goal[2],r.bank_r,r.bank_u,r.ff[0],r.ff[1],r.S[0],r.S[1],r.S[2],r.R[0],r.R[1],r.R[2],
   r.stick[0],r.stick[1],r.stick[2],r.Sout[0],r.Sout[1],r.Sout[2],r.err[0],r.err[1],r.err[2],r.stop[0],r.stop[1],r.stop[2],r.angle,r.calib_p,r.calib_b,r.aim_speed,r.aim_still);}
 fflush(trace_file);
}

// Controller state owned by the game thread (input hook).
struct Runtime {
 fe_control::Controller controller;fe_control::Tuning tuning;fe::Params params;uintptr_t params_pawn=0;uint64_t params_frame=0;
 uintptr_t pawn=0;int mode=0;uint64_t frames=0,used=0,fallbacks=0;int last_reason=0;
};
inline Runtime rt;

// War Thunder-style manual override: ownership is decided every frame on the
// *processed* manual input (after the keyboard ramp), threshold 0.8, no hysteresis or timer. Manual pitch
// or yaw releases all three channels; manual roll alone releases roll only. Below the threshold the
// instructor overrides the keys completely. AC8's equivalent of WT's keyboard ramp is the stock input
// filter, so the keyboard-only command state is simulated with the exact input model.
struct Manual {double m[3]{};uintptr_t pawn=0;};
inline Manual manual;
// dir: held-key direction per FE axis (0 pitch, 1 yaw, 2 roll), -1/0/+1. Returns release mask: 1 pitch, 2 yaw, 4 roll.
inline int manual_release(uintptr_t pawn,const unsigned char* context,const int dir[3],double threshold=.8){
 if(pawn!=manual.pawn){manual=Manual{};manual.pawn=pawn;}
 float dt=0;std::memcpy(&dt,context+0x20,4);
 if(!(dt>0&&dt<.1f)||!rt.params.valid)return (dir[0]||dir[1]?7:0)|(dir[2]?4:0);   // no model: keys win at once
 for(int a=0;a<3;++a)manual.m[a]=a==1?fe::input_yaw(rt.params,manual.m[a],dir[a],dt):fe::input_axis(rt.params,a,manual.m[a],dir[a],dt);
 if(std::abs(manual.m[0])>=threshold||std::abs(manual.m[1])>=threshold)return 7;
 return std::abs(manual.m[2])>=threshold?4:0;
}
inline aim_motion::Estimator aim_est;inline uintptr_t aim_est_pawn=0;
struct Command {double stick[3]{};double S[3]{};bool valid=false;int reason=0;};
// reason codes: 1 params, 2 state, 3 goal, 4 calibration, 5 dt, 6 controller
inline Command compute(uintptr_t pawn,const unsigned char* context,int mode,uint64_t now_us){
 Command c;++rt.frames;
 if(pawn!=rt.pawn||mode!=rt.mode){rt.controller.reset();rt.pawn=pawn;rt.mode=mode;}
 if(pawn!=rt.params_pawn||rt.frames-rt.params_frame>600||!rt.params.valid){
  unsigned char buf[P_BYTES];
  if(!read_params_raw(pawn,buf)||!fe::from_memory(buf,rt.params)){c.reason=1;rt.last_reason=1;++rt.fallbacks;rt.params.valid=false;return c;}
  rt.params_pawn=pawn;rt.params_frame=rt.frames;
 }
 float dt=0;std::memcpy(&dt,context+0x20,4);
 if(!(dt>0&&dt<.1f)){c.reason=5;rt.last_reason=5;++rt.fallbacks;return c;}
 Raw raw;Quat q;
 if(!read_raw(pawn,raw)||!quat_of(raw,q)||!std::isfinite(raw.speed)||raw.speed<0||raw.speed>2000){c.reason=2;rt.last_reason=2;++rt.fallbacks;return c;}
 for(int a=0;a<3;++a)if(!std::isfinite(raw.S[a])||std::abs(raw.S[a])>1.001||!std::isfinite(raw.R[a])||std::abs(raw.R[a])>20){c.reason=2;rt.last_reason=2;++rt.fallbacks;return c;}
 if(calib_bad.load()>120){c.reason=4;rt.last_reason=4;++rt.fallbacks;return c;}
 Goal g;{std::lock_guard<std::mutex> l(goal_mutex);g=goal;}
 if(!g.valid||g.pawn!=pawn){c.reason=3;rt.last_reason=3;++rt.fallbacks;return c;}
 // Target in the current FE body frame via the attitude change since the Lua sample.
 const auto world=rot(g.q,fe_from_mod(g.body));const auto now_b=mod_from_fe(rot(conj(q),world));
 const auto up_r=-rot(q,{1,0,0}).y,up_u=rot(q,{0,1,0}).y,up_f=rot(q,{0,0,1}).y;
 fe_control::Geometry geo;geo.goal=now_b;geo.bank_r=up_r;geo.bank_u=up_u;geo.up_f=up_f;geo.ff_pitch=g.ff_pitch;geo.ff_yaw=g.ff_yaw;geo.mot_up=g.mot_up;geo.mot_right=g.mot_right;
 // Windowed aim motion in the world (FE) frame, expressed in the current mod body axes.
 if(pawn!=aim_est_pawn){aim_est.reset();aim_est_pawn=pawn;}
 aim_est.update(double(now_us)*1e-6,world);
 {const auto vb=mod_from_fe(rot(conj(q),aim_est.vel));geo.aim_speed=float(aim_est.speed);geo.aim_still=float(aim_est.still);geo.aim_up=vb.z;geo.aim_right=vb.y;geo.aim_path=float(aim_est.path_speed);geo.aim_moving=float(aim_est.moving);geo.aim_quiet=float(aim_est.quiet);}
 double Sm[3],Rm[3];for(int a=0;a<3;++a){Sm[a]=raw.S[a]*MIRROR[a];Rm[a]=raw.R[a]*MIRROR[a];}
 const auto o=rt.controller.step(rt.params,Sm,Rm,raw.speed,dt,geo,mode==5?fe_control::Mode::Direct:fe_control::Mode::Stick,rt.tuning);
 if(!o.valid){c.reason=6;rt.last_reason=6;++rt.fallbacks;return c;}
 for(int a=0;a<3;++a){c.stick[a]=o.stick[a]*MIRROR[a];c.S[a]=o.S[a]*MIRROR[a];}   // back to FE signs
 c.valid=true;rt.last_reason=0;++rt.used;
 Record r{};r.t=now_us;r.mode=mode;r.dt=dt;r.speed=raw.speed;r.goal[0]=geo.goal.x;r.goal[1]=geo.goal.y;r.goal[2]=geo.goal.z;r.bank_r=up_r;r.bank_u=up_u;r.ff[0]=g.ff_pitch;r.ff[1]=g.ff_yaw;
 for(int a=0;a<3;++a){r.S[a]=raw.S[a];r.R[a]=raw.R[a];r.stick[a]=c.stick[a];r.Sout[a]=c.S[a];r.err[a]=o.errors[a];r.stop[a]=o.stop[a];}
 r.angle=float(o.angle);r.calib_p=calib_pitch_err.load();r.calib_b=calib_bank_err.load();r.aim_speed=geo.aim_speed;r.aim_still=geo.aim_still;trace_push(r);
 return c;
}

}
