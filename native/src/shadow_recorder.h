#pragma once
#include <share.h>
inline constexpr char shadow_csv_header[] = "kind,t_us,epoch,pawn,plane_id,pitch_deg,yaw_deg,roll_deg,vx_cm_s,vy_cm_s,vz_cm_s,x_cm,y_cm,z_cm,u_pitch,u_yaw,u_roll,throttle,brake,manual_mask,input_age_ms,frame_dt,filtered_pitch_rate,filtered_yaw_rate,filtered_roll_rate,aim_pitch,aim_yaw,sim_seconds,assist_on,assist_delta_pitch,assist_delta_roll,assist_profile,confidence_pitch,confidence_roll,gain_pitch,gain_roll,learn_tau_pitch,learn_tau_roll,learn_weight_pitch,learn_weight_roll,learn_promotions_pitch,learn_promotions_roll,confidence_yaw,gain_yaw,learn_tau_yaw,learn_weight_yaw,learn_promotions_yaw,learn_delay_pitch,learn_delay_roll,learn_delay_yaw,primary_weight_pitch,primary_weight_roll,primary_weight_yaw,primary_command_pitch,primary_command_roll,primary_command_yaw,primary_delta_pitch,primary_delta_roll,primary_delta_yaw,primary_stop_pitch,primary_stop_roll,primary_stop_yaw,primary_horizon_pitch,primary_horizon_roll,primary_horizon_yaw,primary_ratecap_pitch,primary_ratecap_roll,primary_ratecap_yaw,primary_checks_pitch,primary_checks_roll,primary_checks_yaw,primary_model_rmse_pitch,primary_model_rmse_roll,primary_model_rmse_yaw,primary_base_rmse_pitch,primary_base_rmse_roll,primary_base_rmse_yaw,reference_rate_pitch,reference_rate_roll,reference_rate_yaw,candidate_weight_pitch,candidate_weight_roll,candidate_weight_yaw,model_source_pitch,model_source_roll,model_source_yaw,model_deployments_pitch,model_deployments_roll,model_deployments_yaw,pursuit_pitch,pursuit_roll,pursuit_yaw,effective_damping_pitch,effective_damping_roll,effective_damping_yaw,plan_input_limit_pitch,plan_input_limit_roll,plan_input_limit_yaw,selected_control_mode,control_mode_blend,capture_weight,level_phase,actual_bank_deg,capture_roll_error_deg,capture_roll_ratecap_deg_s,roll_limit_cause,level_weight,relative_pointing_rate_deg_s,capture_joint_delta_l1,capture_joint_roll_delta,capture_trim_pitch_deg_s,capture_trim_yaw_deg_s,capture_target_bank_deg,capture_gain_pitch,capture_tau_pitch,capture_delay_pitch,capture_total_bias_pitch,capture_mix_pitch,capture_rate_rmse_pitch,capture_trust_pitch,capture_gain_bound_pitch,capture_tau_bound_pitch,capture_delay_bound_pitch,capture_deployments_pitch,capture_stop_residual_pitch,capture_finite_crossing_pitch,capture_inherited_crossing_pitch,capture_finite_source_checked_pitch,capture_gain_roll,capture_tau_roll,capture_delay_roll,capture_total_bias_roll,capture_mix_roll,capture_rate_rmse_roll,capture_trust_roll,capture_gain_bound_roll,capture_tau_bound_roll,capture_delay_bound_roll,capture_deployments_roll,capture_stop_residual_roll,capture_finite_crossing_roll,capture_inherited_crossing_roll,capture_finite_source_checked_roll,capture_gain_yaw,capture_tau_yaw,capture_delay_yaw,capture_total_bias_yaw,capture_mix_yaw,capture_rate_rmse_yaw,capture_trust_yaw,capture_gain_bound_yaw,capture_tau_bound_yaw,capture_delay_bound_yaw,capture_deployments_yaw,capture_stop_residual_yaw,capture_finite_crossing_yaw,capture_inherited_crossing_yaw,capture_finite_source_checked_yaw,capture_joint_crossing_before,capture_joint_crossing_after,capture_joint_checked,capture_endpoint_rate_pitch,capture_endpoint_qualified_pitch,capture_endpoint_noise_pitch,capture_endpoint_rate_roll,capture_endpoint_qualified_roll,capture_endpoint_noise_roll,capture_endpoint_rate_yaw,capture_endpoint_qualified_yaw,capture_endpoint_noise_yaw,capture_control_revision,vector_gain_pitch,vector_tau_pitch,vector_delay_pitch,vector_total_bias_pitch,vector_model_mix_pitch,vector_prediction_rmse_pitch,vector_trust_pitch,vector_fit_samples_pitch,vector_short_validation_pitch,vector_long_validation_pitch,vector_learning_stage_pitch,vector_candidate_source_pitch,vector_proposals_pitch,vector_accepted_pitch,vector_rejected_pitch,vector_expired_pitch,vector_ambiguous_skips_pitch,vector_innovation_skips_pitch,vector_attempt_age_ms_pitch,vector_bias_mix_pitch,vector_bias_fit_pitch,vector_bias_validation_pitch,vector_bias_accepted_pitch,vector_bias_rejected_pitch,vector_endpoint_rate_pitch,vector_endpoint_qualified_pitch,vector_endpoint_noise_pitch,vector_desired_rate_pitch,vector_stop_distance_pitch,vector_safety_priority_pitch,vector_issued_command_pitch,vector_slew_pitch,vector_gain_roll,vector_tau_roll,vector_delay_roll,vector_total_bias_roll,vector_model_mix_roll,vector_prediction_rmse_roll,vector_trust_roll,vector_fit_samples_roll,vector_short_validation_roll,vector_long_validation_roll,vector_learning_stage_roll,vector_candidate_source_roll,vector_proposals_roll,vector_accepted_roll,vector_rejected_roll,vector_expired_roll,vector_ambiguous_skips_roll,vector_innovation_skips_roll,vector_attempt_age_ms_roll,vector_bias_mix_roll,vector_bias_fit_roll,vector_bias_validation_roll,vector_bias_accepted_roll,vector_bias_rejected_roll,vector_endpoint_rate_roll,vector_endpoint_qualified_roll,vector_endpoint_noise_roll,vector_desired_rate_roll,vector_stop_distance_roll,vector_safety_priority_roll,vector_issued_command_roll,vector_slew_roll,vector_gain_yaw,vector_tau_yaw,vector_delay_yaw,vector_total_bias_yaw,vector_model_mix_yaw,vector_prediction_rmse_yaw,vector_trust_yaw,vector_fit_samples_yaw,vector_short_validation_yaw,vector_long_validation_yaw,vector_learning_stage_yaw,vector_candidate_source_yaw,vector_proposals_yaw,vector_accepted_yaw,vector_rejected_yaw,vector_expired_yaw,vector_ambiguous_skips_yaw,vector_innovation_skips_yaw,vector_attempt_age_ms_yaw,vector_bias_mix_yaw,vector_bias_fit_yaw,vector_bias_validation_yaw,vector_bias_accepted_yaw,vector_bias_rejected_yaw,vector_endpoint_rate_yaw,vector_endpoint_qualified_yaw,vector_endpoint_noise_yaw,vector_desired_rate_yaw,vector_stop_distance_yaw,vector_safety_priority_yaw,vector_issued_command_yaw,vector_slew_yaw,vector_weight,vector_phase,vector_angle,vector_along_rate,vector_transverse_rate,vector_path_error,vector_common_rate,vector_joint_delta,vector_predicted_path_before,vector_predicted_path_after,vector_roll_error,vector_roll_rate_cap,vector_target_stop_guard,vector_plan_valid,vector_terminal_mix,vector_roll_terminal_mix,vector_maneuver_mode,vector_maneuver_roll_goal,vector_pull_completion_s,vector_push_completion_s,vector_pull_cost,vector_push_cost,vector_pull_alignment,vector_pull_preparing,vector_positive_pitch_gain,vector_negative_pitch_gain,vector_positive_fit_samples,vector_negative_fit_samples,vector_push_weight,vector_maneuver_revision,vector_envelope_mix,vector_pitch_pursuit_cap,vector_pitch_stop_cap,vector_pitch_reachable_high,vector_requested_rate,vector_control_revision,vector_baseline_bias_pitch,vector_baseline_bias_yaw,vector_local_bias_weight_pitch,vector_local_bias_weight_yaw,vector_joint_axes,vector_pending_error_pitch,vector_pending_error_yaw,vector_local_bias_accepted_pitch,vector_local_bias_accepted_yaw,vector_local_bias_rejected_pitch,vector_local_bias_rejected_yaw,vector_learning_allowed,vector_local_feedback_allowed,vector_local_bias_status_pitch,vector_local_bias_status_yaw,vector_terminal_retained,vector_level_rate_scale,vector_terminal_audit_fraction_pitch,vector_terminal_audit_fraction_yaw,vector_environment_unsafe,vector_signed_model_weight,vector_signed_model_rmse,vector_signed_accepted,vector_signed_rejected,vector_signed_fit_short,vector_signed_fit_long,vector_signed_check_short,vector_signed_check_long,vector_signed_control_blend,vector_terminal_prediction_weight_pitch,vector_terminal_prediction_weight_yaw,vector_response_weight_pitch,vector_response_weight_yaw,vector_terminal_calibration_weight,vector_terminal_stop_allowance_deg,vector_roll_raw_error,vector_roll_handoff,vector_roll_regularization,vector_roll_pending_error,vector_roll_audit_fraction,heritage_weight,heritage_plan_valid,heritage_revision,heritage_boost_guard,heritage_roll_guard,heritage_cross_guard,heritage_angle,heritage_along_rate,heritage_transverse_rate,heritage_boost_pitch,heritage_boost_yaw,heritage_roll_peace,heritage_model_mix_pitch,heritage_model_mix_roll,heritage_model_mix_yaw,heritage_joint_choice,heritage_joint_checked,heritage_joint_cost_improvement,heritage_path_before,heritage_path_after,heritage_transport_deg,heritage_source_pitch,heritage_source_roll,heritage_source_yaw,heritage_reason_pitch,heritage_reason_roll,heritage_reason_yaw\n";
// Observation only. Bounded queue; all disk access is on existing logger thread.
struct ShadowRecord {
    char kind='S';uint64_t t=0,epoch=0;uintptr_t pawn=0;int plane=-1,manual=0;
    double pitch=0,yaw=0,roll=0,vx=0,vy=0,vz=0,x=0,y=0,z=0;
    double up=0,uy=0,ur=0,throttle=-999,brake=-999,input_age=-1,dt=0;
    double wp=0,wy=0,wr=0,aimp=0,aimy=0,sim_seconds=-1,assist_on=0,assist_p=0,assist_r=0,profile=0,conf_p=0,conf_r=0,gain_p=1,gain_r=1;
    double tau_p=1,tau_r=1,weight_p=0,weight_r=0,promotions_p=0,promotions_r=0;
    std::array<double,58> model_control{};
    // Sampled with each S state (~30 Hz in observed flights), not each render.
    // I rows keep zero defaults; columns append to preserve the original schema.
    // actual_bank_deg is the geometric level error (-UE roll), while roll_deg
    // retains UE roll. capture_weight is the third policy's actual blend; the
    // original control_mode_blend continues to describe CLASSIC/AGILE blending.
    // Target bank uses that same geometric sign; its zero explicitly means
    // horizontal. Joint delta is the existing L1 sum, plus signed roll delta.
    std::array<double,13> capture_control{};
    std::array<double,58> capture_models{};
    std::array<double,112> vector_control{};
    std::array<double,14> vector_decision{};
    std::array<double,6> vector_performance{};
    std::array<double,40> vector_coordination{};
    std::array<double,27> heritage{};
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
ShadowInput shadow_input;uint64_t shadow_last_input_emit=0;
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
             std::abs(shadow_input.y-y)>0.0001f || std::abs(shadow_input.r-r)>0.0001f || input.t-shadow_last_input_emit>=100000;
        shadow_input=input;if(emit)shadow_last_input_emit=input.t;
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
    row.capture_control={double(capture_mode_weight.load()),double(capture_level_phase.load()),
        double(capture_actual_bank.load()),double(capture_roll_error.load()),double(capture_roll_ratecap.load()),
        double(capture_roll_limit_cause.load()),double(capture_level_weight.load()),double(capture_relative_pointing_rate.load()),
        double(capture_joint_delta.load()),double(capture_joint_roll_delta.load()),double(capture_trim_pitch.load()),double(capture_trim_yaw.load()),
        double(capture_target_bank.load())};
    for(size_t i=0;i<row.capture_models.size();++i)row.capture_models[i]=capture_model_diagnostics[i].load();
    for(size_t i=0;i<row.vector_control.size();++i)row.vector_control[i]=vector_diagnostics[i].load();
    for(size_t i=0;i<row.vector_decision.size();++i)row.vector_decision[i]=vector_decision_diagnostics[i].load();
    for(size_t i=0;i<row.vector_performance.size();++i)row.vector_performance[i]=vector_performance_diagnostics[i].load();
    for(size_t i=0;i<row.vector_coordination.size();++i)row.vector_coordination[i]=vector_coordination_diagnostics[i].load();
    for(size_t i=0;i<row.heritage.size();++i)row.heritage[i]=heritage_diagnostics[i].load();
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
        fputs(shadow_csv_header,shadow_file);
    }
    for(size_t i=0;i<count && shadow_rows<shadow_session_row_limit;++i){const auto& r=batch[i];
        int wrote=fprintf(shadow_file,"%c,%llu,%llu,%llX,%d,%.8g,%.8g,%.8g,%.8g,%.8g,%.8g,%.8g,%.8g,%.8g,%.8g,%.8g,%.8g,%.8g,%.8g,%d,%.8g,%.8g,%.8g,%.8g,%.8g,%.8g,%.8g,%.8g,%.8g,%.8g,%.8g,%.8g,%.8g,%.8g,%.8g,%.8g,%.8g,%.8g,%.8g,%.8g,%.8g,%.8g",
            r.kind,r.t,r.epoch,static_cast<unsigned long long>(r.pawn),r.plane,r.pitch,r.yaw,r.roll,r.vx,r.vy,r.vz,r.x,r.y,r.z,r.up,r.uy,r.ur,r.throttle,r.brake,r.manual,r.input_age,r.dt,r.wp,r.wy,r.wr,r.aimp,r.aimy,r.sim_seconds,r.assist_on,r.assist_p,r.assist_r,r.profile,r.conf_p,r.conf_r,r.gain_p,r.gain_r,r.tau_p,r.tau_r,r.weight_p,r.weight_r,r.promotions_p,r.promotions_r);
        if(wrote>=0){for(double value:r.model_control){if(fprintf(shadow_file,",%.8g",value)<0){wrote=-1;break;}}}
        if(wrote>=0){for(double value:r.capture_control){if(fprintf(shadow_file,",%.8g",value)<0){wrote=-1;break;}}}
        if(wrote>=0){for(double value:r.capture_models){if(fprintf(shadow_file,",%.8g",value)<0){wrote=-1;break;}}}
        if(wrote>=0){for(double value:r.vector_control){if(fprintf(shadow_file,",%.8g",value)<0){wrote=-1;break;}}}
        if(wrote>=0){for(double value:r.vector_decision){if(fprintf(shadow_file,",%.8g",value)<0){wrote=-1;break;}}}
        if(wrote>=0){for(double value:r.vector_performance){if(fprintf(shadow_file,",%.8g",value)<0){wrote=-1;break;}}}
        if(wrote>=0){for(double value:r.vector_coordination){if(fprintf(shadow_file,",%.8g",value)<0){wrote=-1;break;}}}
        if(wrote>=0){for(double value:r.heritage){if(fprintf(shadow_file,",%.8g",value)<0){wrote=-1;break;}}if(wrote>=0&&fputc('\n',shadow_file)==EOF)wrote=-1;}

        if(wrote<0){shadow_file_failed=true;log_line("SHADOW_ERROR CSV write failed; flight unaffected");break;}++shadow_rows;
    }
    if(count && shadow_file)fflush(shadow_file);
    if(shadow_rows>=shadow_session_row_limit){shadow_armed=false;log_line("SHADOW_LIMIT session720000row cap reached; ordinary flight continues");}
    if(GetTickCount64()-shadow_report_tick>=10000){shadow_report_tick=GetTickCount64();log_line("SHADOW_CAPTURE rows=%llu dropped=%llu recorder_only=1",shadow_rows,shadow_dropped.load());}
}

