#pragma once
#include "model_control.h"
namespace control_modes {
// Selectable modes: 0 PEACE and 3 WAR. WAR is the exact flight-engine controller (War Thunder structure on
// the reverse-engineered AC8 rotation law, stock stick input); WAR v11.1 is no longer selectable and only
// serves as WAR's automatic degraded mode (state/parameter/calibration failures). Legacy IDs: 1 -> PEACE;
// 2 (CAPTURE), 4 (WAR1), 5 (WAR2) -> WAR.
inline int normalize(int mode){mode=std::clamp(mode,0,5);return mode>=2?3:0;}
inline int next(int mode){return normalize(mode)==0?3:0;}
inline const char* name(int mode){return normalize(mode)==3?"WAR":"PEACE";}
inline const wchar_t* wname(int mode){return normalize(mode)==3?L"WAR":L"PEACE";}
inline bool exact(int mode){return normalize(mode)==3;}
struct KeyLatch {
 bool held=false;
 bool press(bool down,bool allowed){bool event=down&&!held&&allowed;held=down;return event;}
};
struct Blend {
 float value=1;
 void reset(bool agile){value=agile?1.f:0.f;}
 float step(bool agile,float dt){
  if(!std::isfinite(dt)||dt<=0||dt>.1f)return value;
  value+=std::clamp((agile?1.f:0.f)-value,-dt/.35f,dt/.35f);value=std::clamp(value,0.f,1.f);return value;
 }
};
inline model_control::Plan combine(const model_control::Plan& classic,const model_control::Plan& agile,float mix){
 if(mix<=0)return classic;if(mix>=1)return agile;
 if(!classic.valid||!agile.valid)return {};
 auto out=classic;auto lerp=[&](float a,float b){return a+(b-a)*mix;};
 out.command=lerp(classic.command,agile.command);out.stopping_angle=lerp(classic.stopping_angle,agile.stopping_angle);
 out.horizon=lerp(classic.horizon,agile.horizon);out.rate_limit=lerp(classic.rate_limit,agile.rate_limit);
 out.pursuit=lerp(classic.pursuit,agile.pursuit);out.damping=lerp(classic.damping,agile.damping);out.input_limit=lerp(classic.input_limit,agile.input_limit);return out;
}
}
