#pragma once
// WAR keyboard takeover: the War Thunder ownership rule applied to AC8's stock input filter.
// m[a] is the keyboard-only command state per flight-engine axis (0 pitch, 1 yaw, 2 roll), simulated with the stock
// filter from the held keys. WT compares its ramped keyboard value with 0.8 and, once the player owns a channel, the
// channel carries that ramped value. AC8's stock ramp is slower than WT's (inadd 1.2-3/s: 0.27-0.51 s to reach 0.8),
// so 2.4.2 engages at a lower level while a key is held (on) and still hands back at 0.8 after release (off), and the
// stock command state continues from m at the handover instead of re-ramping from the instructor's last command.
#include "fe_model.h"
#include <algorithm>
#include <cmath>
namespace manual_takeover {
// engaged[a]: a key on axis a engaged (ramp reached `on`) and is still held - a reversal while held keeps ownership
// instead of handing the axis to the instructor while the ramp passes through zero.
struct State{double m[3]{};int owned=0;bool engaged[3]{};};
struct Tuning{double on=.8,off=.8;bool seed=false,hold=false;};   // defaults = 2.4.1 behaviour; the runtime passes its own
inline constexpr Tuning war242{.3,.8,true,true};     // engage 0.3 while held, hand back at 0.8, continue from m, hold through reversals
inline int bit(int a){return a==0?1:a==1?2:4;}
// Command state to continue from at the handover, key direction d (+1/-1): whichever of the current stock state and
// the keyboard ramp is further along the key - never pulls back a command the instructor already had in the key's
// direction (F/A-18E flight: a pitch takeover cut the pull 0.97 -> 0.25), never uses a ramp still on the other side
// after a quick re-press (roll -0.17 -> +0.58), and replaces an opposing instructor command with the ramp value.
inline double seeded(double S,double m_prev,double d){return d*std::max(d*S,d*m_prev);}
// dir: held-key direction per axis (-1/0/+1). Returns the ownership mask (1 pitch, 2 yaw, 4 roll; pitch or yaw owned
// -> all three, roll alone -> roll only). seed[a]: axis a has just passed to a held key; seed_value[a] is the
// keyboard-only state before this frame, so one stock filter step from it lands exactly on m[a].
inline int step(const fe::Params& p,State& s,const int dir[3],double dt,const Tuning& k,bool seed[3],double seed_value[3]){
 for(int a=0;a<3;++a){
  seed_value[a]=s.m[a];seed[a]=false;
  s.m[a]=a==1?fe::input_yaw(p,s.m[a],dir[a],dt):fe::input_axis(p,a,s.m[a],dir[a],dt);
 }
 bool act[3];
 for(int a=0;a<3;++a){s.engaged[a]=dir[a]!=0&&((k.hold&&s.engaged[a])||std::abs(s.m[a])>=k.on);act[a]=s.engaged[a]||std::abs(s.m[a])>=k.off;}
 const int owned=(act[0]||act[1])?7:(act[2]?4:0),fresh=owned&~s.owned;
 if(k.seed)for(int a=0;a<3;++a)seed[a]=(fresh&bit(a))&&dir[a]!=0;
 s.owned=owned;return owned;
}
}
