#include "src/mouse_aim.cpp"
#include <cassert>
#include <cstdio>
int main(){
    aircraft=65536;
    double v[7]={65536,30000,0,0,0,2147483013,0};
    assert(accept_control_observation(v)==1);
    assert(assist_plane_type==2147483013&&assist_speed==300&&assist_speed_pawn==65536);
    // Full int32 identity without truncation; reject fractional/overflow IDs.
    v[5]=2147483647;assert(accept_control_observation(v)==1);
    v[5]=2147483648.;assert(accept_control_observation(v)==-4);
    v[5]=1.5;assert(accept_control_observation(v)==-4);
    v[5]=-1;assert(accept_control_observation(v)==-4);
    v[5]=2147483013;v[0]=65544;assert(accept_control_observation(v)==-2);
    v[0]=65536;v[1]=NAN;assert(accept_control_observation(v)==-3);
    v[1]=30000;v[6]=2;assert(accept_control_observation(v)==-5);v[6]=0;
    // A missing brake disables learning, not standard model control.
    v[4]=-999;assert(accept_control_observation(v)==1&&assist_environment_unsafe);
    v[4]=0;assert(accept_control_observation(v)==1&&!assist_environment_unsafe);
    game_paused=false;gaze_active=false;model_assist_enabled=true;
    assist_input={};assist_input.pawn=65536;assist_input.tick=GetTickCount64();
    observe_adaptive_input(65536,0,0,0);
    float p=0,r=0,y=0;
    // No recorder calls, no valid position, no simulation clock required.
    for(int i=0;i<60;++i){
        accept_control_observation(v);assist_input.tick=GetTickCount64();
        adaptive_owner=65536;adaptive_history.clear();
        auto now=GetTickCount64();
        for(auto t=now-900;t<=now;t+=5)adaptive_history.add({t,p,r,y});
        apply_model_control(5,8,2,0,0,0,0,0,0,.01f,p,r,y,140);
    }
    assert(learning_plane==2147483013&&assist_profile==6);
    for(int i=0;i<3;++i)assert(assist_primary_weight[i]>.99f&&assist_model_source[i]==0);
    puts("PASS mission identity: three-axis standard model active without recorder; invalid metadata rejected.");
}
