#pragma once
// Exact AC8 player flight-engine rotation law (re/AC8-FlightEngine RE notes).
// Reconstructed from AceCombat8.exe: input shaping 0x4D3B680, rate caps 0x4D43FF0,
// rate dynamics 0x4D40AD0, attitude 0x4D429A0. Validated against in-game samples
// (one-step rate error ~0.02 deg/s). Axes: 0 = X pitch, 1 = Y yaw, 2 = Z roll.
// Units: rad, rad/s, m/s, seconds. Positive: nose up, nose right, right wing down.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace fe {

struct Params {
 float graph[10]{};            // SpeedGraph, m/s
 double speedrot[10][3]{};     // SpeedRot, rad/s (raw table, before CAPK)
 double rotgrav[10][3]{};      // RotGravR, rad/s
 double add[3]{},dec[3]{},inadd[3]{},indec[3]{};
 float pud=1;                  // PitchUpDownR: push-rate scale
 double mult_add[3]{1,1,1};    // runtime multipliers after the table (P+0x3F8..)
 double mult_cap[3]{1,1,1};    // (P+0x410..)
 bool valid=false;
};

constexpr double CAPK[3]={1.0/6.0,1.0/15.0,5.0/6.0};
constexpr double KADD[3]={1,1,2},KDEC[3]={3,1,3},FLOOR[3]={.5,.8,.4};

// Reads the converted runtime parameter block (pawn+0x2C28). Layout from the
// reflected FAceFlightEnginePlayerParameter plus the runtime tail.
inline bool from_memory(const unsigned char* P,Params& p){
 auto f=[&](size_t o){float v;std::memcpy(&v,P+o,4);return v;};
 auto d=[&](size_t o){double v;std::memcpy(&v,P+o,8);return v;};
 for(int i=0;i<10;++i){p.graph[i]=f(0x150+4*i);for(int a=0;a<3;++a){p.speedrot[i][a]=d(0x1A0+24*i+8*a);p.rotgrav[i][a]=d(0x290+24*i+8*a);}}
 for(int a=0;a<3;++a){p.add[a]=d(0xB8+8*a);p.dec[a]=d(0xD0+8*a);p.inadd[a]=d(0xE8+8*a);p.indec[a]=d(0x100+8*a);
  p.mult_add[a]=d(0x3F8+8*a);p.mult_cap[a]=d(0x410+8*a);}
 p.pud=f(0xB4);
 bool ok=std::isfinite(p.pud)&&p.pud>0&&p.pud<=2;
 for(int i=0;i<10&&ok;++i)ok=std::isfinite(p.graph[i])&&(i==0||p.graph[i]>=p.graph[i-1]);
 for(int a=0;a<3&&ok;++a)ok=p.add[a]>0&&p.add[a]<50&&p.dec[a]>0&&p.dec[a]<50&&p.inadd[a]>0&&p.inadd[a]<50&&p.indec[a]>0&&p.indec[a]<50&&
  p.mult_add[a]>0&&p.mult_add[a]<10&&p.mult_cap[a]>0&&p.mult_cap[a]<10&&p.speedrot[5][a]>0&&p.speedrot[5][a]<20;
 p.valid=ok;return ok;
}

struct Caps {double cap[3]{};double grav[2]{};};
// 0x4D43FF0: piecewise-linear interpolation over the speed graph.
inline Caps caps(const Params& p,double v){
 v=std::abs(v);int i=0;
 for(int k=0;k<10;++k){if(v>=p.graph[k])i=k;else break;}
 const int j=std::min(i+1,9);
 const double w=std::max(std::abs(double(p.graph[j])+(i==j?100.0:0.0)-p.graph[i]),1.0),fr=(v-p.graph[i])/w;
 Caps c;
 for(int a=0;a<3;++a)c.cap[a]=(p.speedrot[i][a]+(p.speedrot[j][a]-p.speedrot[i][a])*fr)*CAPK[a]*p.mult_cap[a];
 for(int a=0;a<2;++a)c.grav[a]=p.rotgrav[i][a]+(p.rotgrav[j][a]-p.rotgrav[i][a])*fr;
 return c;
}

// Stick curve of 0x4D3B680 and its inverse.
inline double stick_target(double u){return (u*std::abs(u)+u)*(2.0/3.0);}
inline double stick_inverse(double T){
 T=std::clamp(T,-4.0/3.0,4.0/3.0);const double s=T<0?-1:1,m=std::abs(T);
 return s*(-1+std::sqrt(1+6*m))/2;
}

// One frame of the stock input filter for pitch (a=0) or roll (a=2): stick u -> command state S.
inline double input_axis(const Params& p,int a,double S,double u,double dt,double k=0){
 if(std::abs(u)>1e-8)return std::clamp(S+(stick_target(u)-S)*1.5*p.inadd[a]*dt*(1-.7*k),-1.0,1.0);
 const double step=p.indec[a]*dt;
 return S>0?std::max(S-step,0.0):std::min(S+step,0.0);
}
// Stock yaw: any non-zero stick ramps S to full deflection.
inline double input_yaw(const Params& p,double S,double u,double dt){
 if(std::abs(u)>1e-8)return std::clamp(S+(u>0?1:-1)*p.inadd[1]*dt,-1.0,1.0);
 const double step=p.indec[1]*dt;
 return S>0?std::max(S-step,0.0):std::min(S+step,0.0);
}

// Effective acceleration gain of 0x4D40AD0 for the current state.
inline double rate_gain(const Params& p,int a,double r,double S,double cap){
 if(std::abs(S)>1e-8)return cap>1e-8?p.add[a]*p.mult_add[a]*KADD[a]*std::clamp(std::abs(r/cap)+FLOOR[a],0.0,1.0):0.0;
 return KDEC[a]*p.dec[a];
}
inline double rate_axis(const Params& p,int a,double r,double S,double cap,double dt){
 const double target=S*cap;const double e=double(float(target-r));
 return std::clamp(r+rate_gain(p,a,r,S,cap)*e*dt,-cap,cap);
}

// 0x4D429A0: body rotation actually applied, given the world-up component of the body-up axis.
// Bank assist ("RotGrav"): extra nose-up rate (acos(up_y)/pi)^2 * RotGrav.X when banked or inverted.
inline double bank_assist(const Caps& c,double up_y){
 const double th=std::acos(std::clamp(up_y,-1.0,1.0))/3.14159265358979;return th*th*c.grav[0];
}
inline double applied_pitch(const Params& p,double r,double assist){return (r<0?r*p.pud:r)+assist;}

}
