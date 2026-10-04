#pragma once
#include "shared_braking.h"
struct AssistInput {uintptr_t pawn=0;uint64_t tick=0;float p=0,r=0;int manual=0;float y=0;};
std::mutex assist_input_mutex;
AssistInput assist_input;
shared_braking::Axis pitch_assist,roll_assist;
uint64_t assist_pause_until=0,assist_log_tick=0;
void observe_assist_input(uintptr_t pawn,float p,float r,int manual,float y=0){
    std::unique_lock<std::mutex> lock(assist_input_mutex,std::try_to_lock);
    if(lock.owns_lock())assist_input={pawn,GetTickCount64(),p,r,manual,y};
}
void reset_legacy_model_assist(){pitch_assist.reset();roll_assist.reset();assist_delta_pitch=0;assist_delta_roll=0;}
void apply_legacy_model_assist(float pitch_error,float roll_error,float pitch_rate,float roll_rate,float dt,float& pcmd,float& rcmd){
    const auto now=GetTickCount64();AssistInput input;bool read=false;
    {std::unique_lock<std::mutex> lock(assist_input_mutex,std::try_to_lock);if(lock.owns_lock()){input=assist_input;read=true;}}
    if(read && input.manual)assist_pause_until=now+300;
    bool allowed=model_assist_enabled.load() && read && input.pawn==aircraft.load() && now-input.tick<100 &&
        now>=assist_pause_until && assist_speed_pawn.load()==aircraft.load() && now-assist_speed_tick.load()<150 &&
        !assist_environment_unsafe.load() && assist_brake.load()<.15f && !game_paused.load() && !gaze_active.load();
    float dp=0,dr=0;
    if(allowed){
        float speed=assist_speed.load();
        dp=pitch_assist.correction(shared_braking::pitch,pitch_error,pitch_rate,input.p,pcmd,speed,.35f,12.f,.2f,config.model_assist_strength,.10f,1.0f,dt);
        dr=roll_assist.correction(shared_braking::roll,roll_error,roll_rate,input.r,rcmd,speed,.25f,25.f,1.f,config.model_assist_strength,.12f,1.5f,dt);
    }else reset_legacy_model_assist();
    assist_delta_pitch=dp;assist_delta_roll=dr;
    pcmd=std::clamp(pcmd+dp,-.85f,.85f);rcmd=std::clamp(rcmd+dr,-1.f,1.f);
    if(now-assist_log_tick>=10000){assist_log_tick=now;log_line("MODEL_ASSIST enabled=%d gate=%d speed=%.1f correction_pitch=%.4f correction_roll=%.4f strength=%.2f",model_assist_enabled.load()?1:0,allowed?1:0,assist_speed.load(),dp,dr,config.model_assist_strength);}
}
