#pragma once
#include "online_learning.h"
adaptive_braking::FastRate fast_pitch_rate,fast_roll_rate;
adaptive_braking::History adaptive_history;
std::mutex adaptive_history_mutex;
uintptr_t adaptive_owner=0,learning_pawn=0;
int learning_plane=-1;
uint64_t adaptive_pause_until=0,adaptive_log_tick=0;
online_learning::Fleet learned_fleet;
online_learning::Aircraft* learning_current=nullptr;
void observe_adaptive_input(uintptr_t pawn,float pitch,float roll){
 std::unique_lock<std::mutex> lock(adaptive_history_mutex,std::try_to_lock);
 if(lock.owns_lock()){
  if(adaptive_owner!=pawn){adaptive_history.clear();adaptive_owner=pawn;}
  adaptive_history.add({GetTickCount64(),pitch,roll});
 }
}
void reset_model_assist(){
 reset_legacy_model_assist();fast_pitch_rate.reset();fast_roll_rate.reset();
 if(learning_current)learning_current->suspend(); // Retain fitted/validated banks.
 assist_gain_pitch=assist_gain_roll=1;assist_learn_weight_pitch=assist_learn_weight_roll=0;
}
void apply_model_assist(float ep,float er,float wp,float wr,float rawp,float rawr,float dt,float& pcmd,float& rcmd){
 int plane=assist_plane_type.load();auto now=GetTickCount64();auto pawn=aircraft.load();
 if(plane<0){reset_model_assist();return;}
 if(plane!=learning_plane||pawn!=learning_pawn||!learning_current){
  reset_model_assist();learning_current=&learned_fleet.get(plane,now);learning_current->suspend();
  learning_plane=plane;learning_pawn=pawn;
 }
 AssistInput input;adaptive_braking::History history;bool read=false,have_history=false;
 {std::unique_lock<std::mutex> lock(assist_input_mutex,std::try_to_lock);if(lock.owns_lock()){input=assist_input;read=true;}}
 {std::unique_lock<std::mutex> lock(adaptive_history_mutex,std::try_to_lock);if(lock.owns_lock()&&adaptive_owner==pawn){history=adaptive_history;have_history=true;}}
 if(read&&input.manual)adaptive_pause_until=now+300;
 bool allowed=model_assist_enabled.load()&&read&&have_history&&input.pawn==pawn&&now>=input.tick&&now-input.tick<100&&
  now>=adaptive_pause_until&&assist_speed_pawn.load()==pawn&&now>=assist_speed_tick.load()&&now-assist_speed_tick.load()<150&&
  !assist_environment_unsafe.load()&&assist_brake.load()<.15f&&!game_paused.load()&&!gaze_active.load();
 auto& p=learning_current->pitch;auto& r=learning_current->roll;
 float dp=0,dr=0;auto pm=shared_braking::pitch,rm=shared_braking::roll;
 if(allowed){
  float speed=assist_speed.load(),pr=fast_pitch_rate.step(rawp,wp,dt),rr=fast_roll_rate.step(rawr,wr,dt);
  auto oldp=p.promotions(),oldr=r.promotions();
  p.observe(pm,false,now,pr,speed,history);r.observe(rm,true,now,rr,speed,history);
  pm=p.effective(pm,speed,dt);rm=r.effective(rm,speed,dt);
  if(oldp!=p.promotions()||oldr!=r.promotions())log_line("MODEL_LEARN_PROMOTED plane=%d pitch=%u roll=%u (disjoint future24samples verified; gradual blending follows)",plane,p.promotions(),r.promotions());
  if(now>=400&&online_learning::continuous(history,now-400,now)){
   dp=p.correction(pm,ep,pr,input.p,pcmd,speed,.35f,12.f,.2f,config.model_assist_strength,.10f,1.f,dt);
   dr=r.correction(rm,er,rr,input.r,rcmd,speed,.25f,25.f,1.f,config.model_assist_strength,.12f,1.5f,dt);
  }
 }else reset_model_assist();
 assist_delta_pitch=dp;assist_delta_roll=dr;assist_profile=2;
 assist_conf_pitch=p.confidence;assist_conf_roll=r.confidence;assist_gain_pitch=p.gain_scale;assist_gain_roll=r.gain_scale;
 assist_learn_tau_pitch=pm.tau;assist_learn_tau_roll=rm.tau;assist_learn_weight_pitch=p.learned_weight;assist_learn_weight_roll=r.learned_weight;
 assist_learn_promotions_pitch=p.promotions();assist_learn_promotions_roll=r.promotions();
 pcmd=std::clamp(pcmd+dp,-.85f,.85f);rcmd=std::clamp(rcmd+dr,-1.f,1.f);
 if(now-adaptive_log_tick>=10000){adaptive_log_tick=now;
  int band=online_learning::nearest_band(assist_speed.load());auto& pb=p.banks[band];auto& rb=r.banks[band];
  log_line("MODEL_LEARN plane=%d gate=%d conf=(%.3f,%.3f) gain=(%.3f,%.3f) tau=(%.3f,%.3f) blend=(%.3f,%.3f) promoted=(%u,%u) speed=%.1f delta=(%.4f,%.4f)",plane,allowed?1:0,p.confidence,r.confidence,p.gain_scale,r.gain_scale,pm.tau,rm.tau,p.learned_weight,r.learned_weight,p.promotions(),r.promotions(),assist_speed.load(),dp,dr);
  log_line("MODEL_LEARN_STAGE plane=%d band=%d phase=(%s,%s) training=(%u,%u) validation=(%u,%u) rejected=(%u,%u)",plane,band,pb.validating?"validate":"train",rb.validating?"validate":"train",pb.stats.n,rb.stats.n,pb.validation_count,rb.validation_count,pb.rejections,rb.rejections);
 }
}
