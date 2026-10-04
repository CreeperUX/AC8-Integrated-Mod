#pragma once
#include "adaptive_braking.h"
adaptive_braking::FastRate fast_pitch_rate,fast_roll_rate;
adaptive_braking::Axis adaptive_pitch,adaptive_roll;
adaptive_braking::History adaptive_history;
std::mutex adaptive_history_mutex;
uintptr_t adaptive_owner=0;int adaptive_plane=-1;
uint64_t adaptive_pause_until=0,adaptive_log_tick=0;
void observe_adaptive_input(uintptr_t pawn,float pitch,float roll){
    std::unique_lock<std::mutex> lock(adaptive_history_mutex,std::try_to_lock);
    if(lock.owns_lock()){
        if(adaptive_owner!=pawn){adaptive_history.clear();adaptive_owner=pawn;}
        adaptive_history.add({GetTickCount64(),pitch,roll});
    }
}
void reset_adaptive_assist(){adaptive_pitch.reset();adaptive_roll.reset();fast_pitch_rate.reset();fast_roll_rate.reset();}
void reset_model_assist(){reset_legacy_model_assist();reset_adaptive_assist();}
void apply_model_assist(float ep,float er,float wp,float wr,float rawp,float rawr,float dt,float& pcmd,float& rcmd){
    const int plane=assist_plane_type.load();
    if(plane!=adaptive_plane){reset_model_assist();adaptive_plane=plane;}
    if(plane==11010){
        // Preserve the user's accepted Typhoon branch, including its original rate filter/gates.
        apply_legacy_model_assist(ep,er,wp,wr,dt,pcmd,rcmd);
        assist_profile=0;assist_conf_pitch=assist_conf_roll=1;assist_gain_pitch=assist_gain_roll=1;return;
    }
    const auto now=GetTickCount64();AssistInput input;adaptive_braking::History history;bool read=false,have_history=false;
    {std::unique_lock<std::mutex> lock(assist_input_mutex,std::try_to_lock);if(lock.owns_lock()){input=assist_input;read=true;}}
    {std::unique_lock<std::mutex> lock(adaptive_history_mutex,std::try_to_lock);if(lock.owns_lock()&&adaptive_owner==aircraft.load()){history=adaptive_history;have_history=true;}}
    if(read&&input.manual)adaptive_pause_until=now+300;
    bool allowed=plane>=0 && model_assist_enabled.load()&&read&&have_history&&input.pawn==aircraft.load()&&now-input.tick<100&&
        now>=adaptive_pause_until&&assist_speed_pawn.load()==aircraft.load()&&now-assist_speed_tick.load()<150&&
        !assist_environment_unsafe.load()&&assist_brake.load()<.15f&&!game_paused.load()&&!gaze_active.load();
    float dp=0,dr=0;
    if(allowed){
        float speed=assist_speed.load();float p=fast_pitch_rate.step(rawp,wp,dt),r=fast_roll_rate.step(rawr,wr,dt);
        adaptive_pitch.observe(shared_braking::pitch,false,now,p,speed,history);
        adaptive_roll.observe(shared_braking::roll,true,now,r,speed,history);
        if(now>=400&&history.covers(now-400)){
            dp=adaptive_pitch.correction(shared_braking::pitch,ep,p,input.p,pcmd,speed,.35f,12.f,.2f,config.model_assist_strength,.10f,1.f,dt);
            dr=adaptive_roll.correction(shared_braking::roll,er,r,input.r,rcmd,speed,.25f,25.f,1.f,config.model_assist_strength,.12f,1.5f,dt);
        }
    }else reset_adaptive_assist();
    assist_delta_pitch=dp;assist_delta_roll=dr;assist_profile=1;
    assist_conf_pitch=adaptive_pitch.confidence;assist_conf_roll=adaptive_roll.confidence;
    assist_gain_pitch=adaptive_pitch.gain_scale;assist_gain_roll=adaptive_roll.gain_scale;
    pcmd=std::clamp(pcmd+dp,-.85f,.85f);rcmd=std::clamp(rcmd+dr,-1.f,1.f);
    if(now-adaptive_log_tick>=10000){adaptive_log_tick=now;log_line("MODEL_ADAPT plane=%d gate=%d conf=(%.3f,%.3f) gain=(%.3f,%.3f) speed=%.1f delta=(%.4f,%.4f)",plane,allowed?1:0,adaptive_pitch.confidence,adaptive_roll.confidence,adaptive_pitch.gain_scale,adaptive_roll.gain_scale,assist_speed.load(),dp,dr);}
}
