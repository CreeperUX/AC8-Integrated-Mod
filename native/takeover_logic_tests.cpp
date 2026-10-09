// WAR keyboard takeover rule (manual_takeover.h) with a stock input filter of typical parameters (Typhoon class:
// inadd 1.5/3/1.2, indec 2.5/1.7/1.75). Public test: no extracted game data.
#include "src/manual_takeover.h"
#include <cassert>
#include <cstdio>
#include <cmath>
int main(){
 fe::Params p{};const double ia[3]={1.5,3,1.2},id[3]={2.5,1.7,1.75};
 for(int a=0;a<3;++a){p.inadd[a]=ia[a];p.indec[a]=id[a];}
 const double dt=1/60.;
 struct Run{int first=-1,seeds=0,seed_axis=-1;double seed_value=0,m_after=0;int released_after=-1;};
 // hold key on axis `axis` (+1) for `hold` s from t=0, then release; frames counted from the press
 auto run=[&](int axis,double hold,manual_takeover::Tuning k){
  manual_takeover::State s;Run r;
  for(int i=0;i<180;++i){
   int dir[3]{};if(i*dt<hold)dir[axis]=1;bool seed[3];double sv[3];
   const int owned=manual_takeover::step(p,s,dir,dt,k,seed,sv);
   for(int a=0;a<3;++a)if(seed[a]){++r.seeds;r.seed_axis=a;r.seed_value=sv[a];r.m_after=s.m[a];
    // one stock filter step from the seed lands exactly on the keyboard-only state
    const double one=a==1?fe::input_yaw(p,sv[a],1,dt):fe::input_axis(p,a,sv[a],1,dt);assert(std::abs(one-s.m[a])<1e-12);}
   if(owned&&r.first<0)r.first=i;
   if(i*dt>=hold&&!owned&&r.released_after<0&&r.first>=0)r.released_after=i-int(std::ceil(hold/dt));
  }
  return r;
 };
 const auto K=manual_takeover::war242;
 // 1. roll key: engages when the keyboard ramp reaches 0.3 (~0.14 s), roll only, one seed on roll with the ramp value
 {const double t_on=std::log(1/(1-.3*.75))/(1.5*1.2);auto r=run(2,1.0,K);
  assert(std::abs(r.first*dt-t_on)<=1.5*dt&&r.seeds==1&&r.seed_axis==2&&r.seed_value>.25&&r.seed_value<.3+1e-9);
  // hand-back after release once the ramp falls below 0.8 ((1-0.8)/1.75 ~ 0.11 s)
  assert(r.released_after>0&&r.released_after*dt<=.2/1.75+2*dt);}
 // 2. a tap shorter than the engage time stays with the instructor; the 2.4.1 rule needed ~0.51 s
 {auto r=run(2,.10,K);assert(r.first<0&&r.seeds==0);
  auto o=run(2,1.0,{.8,.8,false});const double t08=std::log(1/(1-.8*.75))/(1.5*1.2);assert(std::abs(o.first*dt-t08)<=1.5*dt&&o.seeds==0);}
 // 3. pitch key: all three channels, only pitch seeded; yaw key (linear stock ramp 3/s): ~0.1 s, all three
 {auto r=run(0,1.0,K);manual_takeover::State s;int dir[3]{1,0,0};bool seed[3];double sv[3];int owned=0;
  for(int i=0;i<60&&!owned;++i)owned=manual_takeover::step(p,s,dir,dt,K,seed,sv);
  assert(owned==7&&seed[0]&&!seed[1]&&!seed[2]&&r.seeds==1);
  auto y=run(1,1.0,K);assert(std::abs(y.first*dt-.1)<=1.5*dt&&y.seed_axis==1);}
 // 4. roll reversal while held (right -> left without releasing): stays with the keys while the ramp passes
 //    through zero, no new seed (the stock state simply follows the keys); 2.4.1-style rule would lapse to the instructor
 {manual_takeover::State s;bool seed[3];double sv[3];int lapse=0,seeds=0;
  for(int i=0;i<120;++i){int dir[3]{0,0,i<60?1:-1};const int o=manual_takeover::step(p,s,dir,dt,K,seed,sv);
   if(i>=30&&!o)++lapse;if(i>=30&&seed[2])++seeds;}
  assert(lapse==0&&seeds==0&&s.m[2]<-.8);
  manual_takeover::State w;int wl=0;
  for(int i=0;i<120;++i){int dir[3]{0,0,i<60?1:-1};if(i>=60&&!manual_takeover::step(p,w,dir,dt,{.8,.8,false},seed,sv))++wl;else if(i<60)manual_takeover::step(p,w,dir,dt,{.8,.8,false},seed,sv);}
  assert(wl>10);}
 // 5. after release, a new press of the same key engages again with a fresh seed
 {manual_takeover::State s;bool seed[3];double sv[3];int seeds=0;
  for(int i=0;i<150;++i){int dir[3]{0,0,(i<30||i>=90)?1:0};manual_takeover::step(p,s,dir,dt,K,seed,sv);seeds+=seed[2];}
  assert(seeds==2);}
 // 6. handover value (F/A-18E flight cases): keep an instructor command already further along the key, replace an
 //    opposing one with the ramp, ignore a ramp still on the other side after a quick re-press
 {using manual_takeover::seeded;
  assert(seeded(.97,.25,1)==.97);              // pitch: instructor already pulling harder - no dip
  assert(seeded(.44,.25,-1)==-.25);            // roll: instructor opposing - continue from the ramp
  assert(seeded(.13,.25,1)==.25);              // same direction but weaker - ramp value
  assert(seeded(-.17,-.58,1)==-.17);           // ramp still on the old side (quick re-press) - keep the stock state
  assert(seeded(-.6,.3,-1)==-.6);
  // F-22 flight: key direction mirrored onto the flight-engine sign (d=+1 while the key convention says -1); the ramp
  // along the key (+0.27) still replaces the slightly opposing stock state
  assert(seeded(-.06,.27,1)==.27);
  manual_takeover::State s;int dir[3]{0,0,-1};bool seed[3];double sv[3];int n=0;
  while(!manual_takeover::step(p,s,dir,dt,K,seed,sv)&&++n<60){}
  assert(seed[2]&&sv[2]>.25&&sv[2]<.3+1e-9&&s.m[2]<0);}   // seed value is measured along the key
 std::puts("PASS takeover: held key engages at 0.3 (roll ~0.14 s, yaw ~0.1 s) with a seed equal to the keyboard ramp, taps under that stay with the instructor, hand-back at 0.8 after release, pitch/yaw own all three, reversal while held keeps the keys, re-press seeds again");
}
