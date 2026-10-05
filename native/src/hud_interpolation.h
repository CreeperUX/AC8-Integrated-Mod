#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
// Display-only, bounded one-sample delay. Never predicts or changes flight input.
struct HudInterpolation {
 float ax=0,ay=0,bx=0,by=0;double start=0,period=16.667;uint64_t sequence=0;uintptr_t pawn=0;bool valid=false;
 void reset(){valid=false;}
 void step(uintptr_t id,uint64_t seq,double now,bool visible,float& x,float& y,bool smooth){
  if(!visible||!smooth||!std::isfinite(x)||!std::isfinite(y)){reset();return;}
  if(!valid||id!=pawn||now<start||now-start>100||std::hypot(x-bx,y-by)>300){
   ax=bx=x;ay=by=y;start=now;sequence=seq;pawn=id;valid=true;return;
  }
  if(seq!=sequence){
   period=std::clamp(now-start,6.0,40.0);ax=bx;ay=by;bx=x;by=y;sequence=seq;start=now;
  }
  const float a=float(std::clamp((now-start)/period,0.0,1.0));
  x=ax+(bx-ax)*a;y=ay+(by-ay)*a;
 }
};
