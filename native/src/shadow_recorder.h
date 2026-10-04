#pragma once
#include <share.h>
// Observation only. Bounded queue; all disk access is on existing logger thread.
struct ShadowRecord {
    char kind='S';uint64_t t=0,epoch=0;uintptr_t pawn=0;int plane=-1,manual=0;
    double pitch=0,yaw=0,roll=0,vx=0,vy=0,vz=0,x=0,y=0,z=0;
    double up=0,uy=0,ur=0,throttle=-999,brake=-999,input_age=-1,dt=0;
    double wp=0,wy=0,wr=0,aimp=0,aimy=0,sim_seconds=-1,assist_on=0,assist_p=0,assist_r=0,profile=0,conf_p=0,conf_r=0,gain_p=1,gain_r=1;
    double tau_p=1,tau_r=1,weight_p=0,weight_r=0,promotions_p=0,promotions_r=0;
    std::array<double,58> model_control{};
};
struct ShadowInput {uintptr_t pawn=0;uint64_t t=0;float p=0,y=0,r=0;int manual=0;};
std::mutex shadow_mutex,shadow_input_mutex;
std::array<ShadowRecord,2048> shadow_queue;
size_t shadow_head=0,shadow_size=0;
std::atomic<uint64_t> shadow_dropped{0},shadow_epoch{0};
std::atomic<uint64_t> shadow_first_capture{0};
std::atomic<bool> shadow_armed{false},shadow_limit_noticed{false};
std::atomic<bool> shadow_new_flight{true};
uintptr_t shadow_window_pawn=0;int shadow_window_plane=-1;
constexpr uint64_t shadow_session_row_limit=720000;
uint64_t shadow_origin=0,shadow_last_state=0;
uintptr_t shadow_last_pawn=0;
ShadowInput shadow_input;
FILE* shadow_file=nullptr;uint64_t shadow_rows=0,shadow_report_tick=0;
bool shadow_file_failed=false;
uint64_t shadow_now(){return perf_us(perf_clock()-shadow_origin);}
void shadow_start(){shadow_origin=perf_clock();shadow_armed=true;log_line("SHADOW_ARMED first15minutes per aircraft/flight; session cap720000rows; online learning telemetry enabled");}
bool shadow_allowed(){
    if(!shadow_armed.load())return false;
    auto first=shadow_first_capture.load();
    if(first && shadow_now()-first>900000000ull){
        if(!shadow_limit_noticed.exchange(true))log_line("SHADOW_LIMIT current flight15minute window ended; next aircraft/flight re-arms automatically; control continues");
        return false;
    }
    return true;
}
void shadow_enqueue(const ShadowRecord& r){
    if(!shadow_allowed())return;
    uint64_t first=0;shadow_first_capture.compare_exchange_strong(first,r.t?r.t:1);
    std::unique_lock<std::mutex> lock(shadow_mutex,std::try_to_lock);
    if(!lock.owns_lock()||shadow_size==shadow_queue.size()){++shadow_dropped;return;}
    shadow_queue[(shadow_head+shadow_size)%shadow_queue.size()]=r;++shadow_size;
}
void shadow_capture_input(uintptr_t pawn,float p,float y,float r,int manual){
    if(!shadow_allowed()||!std::isfinite(p)||!std::isfinite(y)||!std::isfinite(r))return;
    ShadowInput input{pawn,shadow_now(),p,y,r,manual};bool emit=false;
    {
        std::unique_lock<std::mutex> lock(shadow_input_mutex,std::try_to_lock);
        if(!lock.owns_lock()){++shadow_dropped;return;}
        emit=shadow_input.pawn!=pawn || shadow_input.manual!=manual || std::abs(shadow_input.p-p)>0.0001f ||
             std::abs(shadow_input.y-y)>0.0001f || std::abs(shadow_input.r-r)>0.0001f || input.t-shadow_input.t>100000;
        shadow_input=input;
    }
    if(emit){ShadowRecord row;row.kind='I';row.t=input.t;row.epoch=shadow_epoch.load();row.pawn=pawn;
        row.up=p;row.uy=y;row.ur=r;row.manual=manual;shadow_enqueue(row);}
}
void shadow_capture_state(const double (&v)[13]){
    if(!shadow_armed.load())return;
    const auto pawn=static_cast<uintptr_t>(v[0]);auto now=shadow_now();
    const int plane=int(v[9]);
    // Detect transitions BEFORE checking the expired window. Otherwise a new
    // aircraft is never observed once the previous flight consumed 15 minutes.
    if(shadow_new_flight.exchange(false)||pawn!=shadow_window_pawn||plane!=shadow_window_plane){
        shadow_window_pawn=pawn;shadow_window_plane=plane;
        shadow_first_capture.store(now?now:1);shadow_limit_noticed=false;
        shadow_last_pawn=pawn;shadow_last_state=0;++shadow_epoch;
        log_line("SHADOW_WINDOW plane=%d pawn=%llX epoch=%llu restarted15minutes",plane,static_cast<unsigned long long>(pawn),shadow_epoch.load());
    }
    if(!shadow_allowed())return;
    if(shadow_last_pawn!=pawn || (shadow_last_state && now-shadow_last_state>250000))++shadow_epoch;
    shadow_last_pawn=pawn;shadow_last_state=now;
    ShadowRecord row;row.t=now;row.epoch=shadow_epoch.load();row.pawn=pawn;row.plane=int(v[9]);
    row.x=v[1];row.y=v[2];row.z=v[3];row.vx=v[4];row.vy=v[5];row.vz=v[6];row.throttle=v[7];row.brake=v[8];row.dt=v[10];row.sim_seconds=v[11];row.assist_on=model_assist_enabled.load()?1:0;row.assist_p=assist_delta_pitch.load();row.assist_r=assist_delta_roll.load();row.profile=assist_profile.load();row.conf_p=assist_conf_pitch.load();row.conf_r=assist_conf_roll.load();row.gain_p=assist_gain_pitch.load();row.gain_r=assist_gain_roll.load();
    row.tau_p=assist_learn_tau_pitch.load();row.tau_r=assist_learn_tau_roll.load();row.weight_p=assist_learn_weight_pitch.load();row.weight_r=assist_learn_weight_roll.load();row.promotions_p=assist_learn_promotions_pitch.load();row.promotions_r=assist_learn_promotions_roll.load();
    row.model_control[0]=assist_conf_yaw.load();row.model_control[1]=assist_gain_yaw.load();row.model_control[2]=assist_learn_tau_yaw.load();row.model_control[3]=assist_learn_weight_yaw.load();row.model_control[4]=assist_learn_promotions_yaw.load();
    for(int i=0;i<3;++i){row.model_control[5+i]=assist_learn_delay[i].load();row.model_control[8+i]=assist_primary_weight[i].load();row.model_control[11+i]=assist_primary_command[i].load();row.model_control[14+i]=assist_primary_delta[i].load();row.model_control[17+i]=assist_primary_stop[i].load();row.model_control[20+i]=assist_primary_horizon[i].load();row.model_control[23+i]=assist_primary_ratecap[i].load();}
    for(int i=0;i<3;++i){row.model_control[26+i]=assist_primary_checks[i].load();row.model_control[29+i]=assist_primary_model_rmse[i].load();row.model_control[32+i]=assist_primary_base_rmse[i].load();}
    for(int i=0;i<3;++i)row.model_control[35+i]=assist_reference_rate[i].load();
    for(int i=0;i<3;++i){row.model_control[38+i]=assist_candidate_weight[i].load();row.model_control[41+i]=assist_model_source[i].load();row.model_control[44+i]=assist_deployed_models[i].load();}
    for(int i=0;i<3;++i){row.model_control[47+i]=assist_pursuit[i].load();row.model_control[50+i]=assist_effective_damping[i].load();row.model_control[53+i]=assist_input_limit[i].load();}
    row.model_control[56]=control_mode.load();row.model_control[57]=control_mode_blend.load();
    row.pitch=pose_pitch.load();row.yaw=pose_yaw.load();row.roll=pose_roll.load();
    row.wp=filtered_pitch_rate;row.wy=filtered_yaw_rate;row.wr=filtered_roll_rate;row.aimp=target_pitch.load();row.aimy=target_yaw.load();
    {
        std::unique_lock<std::mutex> lock(shadow_input_mutex,std::try_to_lock);
        if(lock.owns_lock()&&shadow_input.pawn==pawn && now>=shadow_input.t){
            row.up=shadow_input.p;row.uy=shadow_input.y;row.ur=shadow_input.r;row.manual=shadow_input.manual;
            row.input_age=double(now-shadow_input.t)/1000;
        }
    }
    shadow_enqueue(row);
}
void shadow_drain(){
    if(!shadow_armed.load()||shadow_file_failed)return;
    std::array<ShadowRecord,256> batch;size_t count=0;
    {
        std::unique_lock<std::mutex> lock(shadow_mutex,std::try_to_lock);
        if(!lock.owns_lock())return;
        count=std::min(batch.size(),shadow_size);
        for(size_t i=0;i<count;++i)batch[i]=shadow_queue[(shadow_head+i)%shadow_queue.size()];
        shadow_head=(shadow_head+count)%shadow_queue.size();shadow_size-=count;
    }
    if(count && !shadow_file){
        wchar_t folder[MAX_PATH]{},path[MAX_PATH]{};swprintf_s(folder,L"%s\\..\\Logs",module_folder);CreateDirectoryW(folder,nullptr);
        for(int n=1;n<10000;++n){
            swprintf_s(path,L"%s\\Shadow-%05d.csv",folder,n);
            if(GetFileAttributesW(path)!=INVALID_FILE_ATTRIBUTES)continue;
            shadow_file=_wfsopen(path,L"w",_SH_DENYWR);break;
        }
        if(!shadow_file){shadow_file_failed=true;log_line("SHADOW_ERROR cannot create CSV; normal flight unaffected");return;}
        fprintf(shadow_file,"kind,t_us,epoch,pawn,plane_id,pitch_deg,yaw_deg,roll_deg,vx_cm_s,vy_cm_s,vz_cm_s,x_cm,y_cm,z_cm,u_pitch,u_yaw,u_roll,throttle,brake,manual_mask,input_age_ms,frame_dt,filtered_pitch_rate,filtered_yaw_rate,filtered_roll_rate,aim_pitch,aim_yaw,sim_seconds,assist_on,assist_delta_pitch,assist_delta_roll,assist_profile,confidence_pitch,confidence_roll,gain_pitch,gain_roll,learn_tau_pitch,learn_tau_roll,learn_weight_pitch,learn_weight_roll,learn_promotions_pitch,learn_promotions_roll,confidence_yaw,gain_yaw,learn_tau_yaw,learn_weight_yaw,learn_promotions_yaw,learn_delay_pitch,learn_delay_roll,learn_delay_yaw,primary_weight_pitch,primary_weight_roll,primary_weight_yaw,primary_command_pitch,primary_command_roll,primary_command_yaw,primary_delta_pitch,primary_delta_roll,primary_delta_yaw,primary_stop_pitch,primary_stop_roll,primary_stop_yaw,primary_horizon_pitch,primary_horizon_roll,primary_horizon_yaw,primary_ratecap_pitch,primary_ratecap_roll,primary_ratecap_yaw,primary_checks_pitch,primary_checks_roll,primary_checks_yaw,primary_model_rmse_pitch,primary_model_rmse_roll,primary_model_rmse_yaw,primary_base_rmse_pitch,primary_base_rmse_roll,primary_base_rmse_yaw,reference_rate_pitch,reference_rate_roll,reference_rate_yaw,candidate_weight_pitch,candidate_weight_roll,candidate_weight_yaw,model_source_pitch,model_source_roll,model_source_yaw,model_deployments_pitch,model_deployments_roll,model_deployments_yaw,pursuit_pitch,pursuit_roll,pursuit_yaw,effective_damping_pitch,effective_damping_roll,effective_damping_yaw,plan_input_limit_pitch,plan_input_limit_roll,plan_input_limit_yaw,selected_control_mode,control_mode_blend\n");
    }
    for(size_t i=0;i<count && shadow_rows<shadow_session_row_limit;++i){const auto& r=batch[i];
        int wrote=fprintf(shadow_file,"%c,%llu,%llu,%llX,%d,%.8g,%.8g,%.8g,%.8g,%.8g,%.8g,%.8g,%.8g,%.8g,%.8g,%.8g,%.8g,%.8g,%.8g,%d,%.8g,%.8g,%.8g,%.8g,%.8g,%.8g,%.8g,%.8g,%.8g,%.8g,%.8g,%.8g,%.8g,%.8g,%.8g,%.8g,%.8g,%.8g,%.8g,%.8g,%.8g,%.8g",
            r.kind,r.t,r.epoch,static_cast<unsigned long long>(r.pawn),r.plane,r.pitch,r.yaw,r.roll,r.vx,r.vy,r.vz,r.x,r.y,r.z,r.up,r.uy,r.ur,r.throttle,r.brake,r.manual,r.input_age,r.dt,r.wp,r.wy,r.wr,r.aimp,r.aimy,r.sim_seconds,r.assist_on,r.assist_p,r.assist_r,r.profile,r.conf_p,r.conf_r,r.gain_p,r.gain_r,r.tau_p,r.tau_r,r.weight_p,r.weight_r,r.promotions_p,r.promotions_r);
        if(wrote>=0){for(double value:r.model_control){if(fprintf(shadow_file,",%.8g",value)<0){wrote=-1;break;}}if(wrote>=0&&fputc('\n',shadow_file)==EOF)wrote=-1;}

        if(wrote<0){shadow_file_failed=true;log_line("SHADOW_ERROR CSV write failed; flight unaffected");break;}++shadow_rows;
    }
    if(count && shadow_file)fflush(shadow_file);
    if(shadow_rows>=shadow_session_row_limit){shadow_armed=false;log_line("SHADOW_LIMIT session720000row cap reached; ordinary flight continues");}
    if(GetTickCount64()-shadow_report_tick>=10000){shadow_report_tick=GetTickCount64();log_line("SHADOW_CAPTURE rows=%llu dropped=%llu recorder_only=1",shadow_rows,shadow_dropped.load());}
}
