#pragma once
// Robust mouse-aim motion estimate. Real mouse input arrives in bursts (~0.13 s moves separated by ~0.1 s
// pauses; measured on the JAS39E flight of v13.1), so a per-tick aim rate is zero almost half of the time
// while the pilot is still moving the cursor. Decisions that depend on "is the mouse moving / still" (the
// levelling gate, the motion bank) use the aim's world-frame displacement over a window instead, plus the
// time since the aim last moved by more than a small threshold.
#include "flight_math.h"
#include <array>
#include <cmath>

namespace aim_motion {
struct Estimator {
 static constexpr int N=256;      // >= long_window at 240 fps
 struct Sample {double t;flight::V a;};
 std::array<Sample,N> buf{};int head=0,count=0;
 double anchor_t=0;bool have=false;
 double window=.25,still_speed=1.5; // s; deg/s: the aim counts as still while its windowed speed stays below this
 double speed=0,still=0;            // deg/s over the window; s since the windowed speed was last above still_speed
 // Sustained motion: path length over long_window / long_window (deg/s; a lone 1-2 deg correction flick reads
 // low, continuous tracking reads its full speed) and the time the aim has been moving without a still spell.
 double long_window=.5,path_speed=0,moving=0,move_t=0;
 // Quiet: time since the aim last changed at all between two samples (an idle mouse leaves it exactly
 // unchanged; pauses inside a bursty move last ~0.1 s, 75% < 0.23 s).
 double quiet=0,quiet_t=0,quiet_eps=1e-3;   // s; deg per sample
 flight::V vel{};                   // world angular velocity of the aim direction (tangent vector, deg/s)
 void reset(){count=0;have=false;speed=0;still=0;vel={};path_speed=0;moving=0;quiet=0;}
 static double ang(flight::V a,flight::V b){
  const flight::V c=flight::cross(a,b);return std::atan2(std::sqrt(double(flight::dot(c,c))),double(flight::dot(a,b)))*180/3.14159265358979;
 }
 void update(double t,flight::V aim){
  aim=flight::unit(aim);
  if(count>0&&t<=buf[(head+N-1)%N].t-1e-6)reset();   // time went backwards: new flight
  if(count==0||ang(buf[(head+N-1)%N].a,aim)>quiet_eps)quiet_t=t;
  quiet=t-quiet_t;
  buf[head]={t,aim};head=(head+1)%N;if(count<N)++count;
  {double path=0;int i=1;
   for(;i<count;++i){const Sample& a=buf[(head+N-i)%N];const Sample& b=buf[(head+N-1-i)%N];if(t-b.t>long_window)break;path+=ang(b.a,a.a);}
   path_speed=path/long_window;}
  // Oldest sample within the window.
  Sample old=buf[(head+N-1)%N];
  for(int i=1;i<count;++i){const Sample& s=buf[(head+N-1-i)%N];if(t-s.t>window)break;old=s;}
  const double span=t-old.t;
  if(span>1e-3){speed=ang(old.a,aim)/span;vel=(aim-old.a)*float(180/3.14159265358979/span);}else{speed=0;vel={};}
  // A bursty move (~0.13 s bursts at ~20 deg/s, ~0.1 s pauses) keeps the windowed speed high through its
  // pauses; slow tracking of a drifting target (< still_speed) counts as still.
  if(!have){anchor_t=t;have=true;}
  if(speed>still_speed)anchor_t=t;
  still=t-anchor_t;
  if(speed<=still_speed)move_t=t;
  moving=t-move_t;
 }
};
}
