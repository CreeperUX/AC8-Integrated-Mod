#include "src/mouse_aim.cpp"
#include <cassert>
#include <sstream>
#include <vector>

static std::vector<std::string> csv_fields(const std::string& line){
 std::vector<std::string> fields;std::istringstream in(line);std::string field;
 while(std::getline(in,field,',')){if(!field.empty()&&field.back()=='\n')field.pop_back();fields.push_back(field);}
 return fields;
}
static std::string read_csv_line(FILE* file){
 std::string line;for(int c;(c=fgetc(file))!=EOF;){line.push_back(char(c));if(c=='\n')break;}return line;
}
static void verify_capture_schema(uint64_t first_begin,uint64_t first_end,uint64_t second_begin,uint64_t second_end){
 assert(shadow_file);fflush(shadow_file);rewind(shadow_file);
 auto header=csv_fields(read_csv_line(shadow_file));
 const auto legacy171=csv_fields("kind,t_us,epoch,pawn,plane_id,pitch_deg,yaw_deg,roll_deg,vx_cm_s,vy_cm_s,vz_cm_s,x_cm,y_cm,z_cm,u_pitch,u_yaw,u_roll,throttle,brake,manual_mask,input_age_ms,frame_dt,filtered_pitch_rate,filtered_yaw_rate,filtered_roll_rate,aim_pitch,aim_yaw,sim_seconds,assist_on,assist_delta_pitch,assist_delta_roll,assist_profile,confidence_pitch,confidence_roll,gain_pitch,gain_roll,learn_tau_pitch,learn_tau_roll,learn_weight_pitch,learn_weight_roll,learn_promotions_pitch,learn_promotions_roll,confidence_yaw,gain_yaw,learn_tau_yaw,learn_weight_yaw,learn_promotions_yaw,learn_delay_pitch,learn_delay_roll,learn_delay_yaw,primary_weight_pitch,primary_weight_roll,primary_weight_yaw,primary_command_pitch,primary_command_roll,primary_command_yaw,primary_delta_pitch,primary_delta_roll,primary_delta_yaw,primary_stop_pitch,primary_stop_roll,primary_stop_yaw,primary_horizon_pitch,primary_horizon_roll,primary_horizon_yaw,primary_ratecap_pitch,primary_ratecap_roll,primary_ratecap_yaw,primary_checks_pitch,primary_checks_roll,primary_checks_yaw,primary_model_rmse_pitch,primary_model_rmse_roll,primary_model_rmse_yaw,primary_base_rmse_pitch,primary_base_rmse_roll,primary_base_rmse_yaw,reference_rate_pitch,reference_rate_roll,reference_rate_yaw,candidate_weight_pitch,candidate_weight_roll,candidate_weight_yaw,model_source_pitch,model_source_roll,model_source_yaw,model_deployments_pitch,model_deployments_roll,model_deployments_yaw,pursuit_pitch,pursuit_roll,pursuit_yaw,effective_damping_pitch,effective_damping_roll,effective_damping_yaw,plan_input_limit_pitch,plan_input_limit_roll,plan_input_limit_yaw,selected_control_mode,control_mode_blend,capture_weight,level_phase,actual_bank_deg,capture_roll_error_deg,capture_roll_ratecap_deg_s,roll_limit_cause,level_weight,relative_pointing_rate_deg_s,capture_joint_delta_l1,capture_joint_roll_delta,capture_trim_pitch_deg_s,capture_trim_yaw_deg_s,capture_target_bank_deg,capture_gain_pitch,capture_tau_pitch,capture_delay_pitch,capture_total_bias_pitch,capture_mix_pitch,capture_rate_rmse_pitch,capture_trust_pitch,capture_gain_bound_pitch,capture_tau_bound_pitch,capture_delay_bound_pitch,capture_deployments_pitch,capture_stop_residual_pitch,capture_finite_crossing_pitch,capture_inherited_crossing_pitch,capture_finite_source_checked_pitch,capture_gain_roll,capture_tau_roll,capture_delay_roll,capture_total_bias_roll,capture_mix_roll,capture_rate_rmse_roll,capture_trust_roll,capture_gain_bound_roll,capture_tau_bound_roll,capture_delay_bound_roll,capture_deployments_roll,capture_stop_residual_roll,capture_finite_crossing_roll,capture_inherited_crossing_roll,capture_finite_source_checked_roll,capture_gain_yaw,capture_tau_yaw,capture_delay_yaw,capture_total_bias_yaw,capture_mix_yaw,capture_rate_rmse_yaw,capture_trust_yaw,capture_gain_bound_yaw,capture_tau_bound_yaw,capture_delay_bound_yaw,capture_deployments_yaw,capture_stop_residual_yaw,capture_finite_crossing_yaw,capture_inherited_crossing_yaw,capture_finite_source_checked_yaw,capture_joint_crossing_before,capture_joint_crossing_after,capture_joint_checked,capture_endpoint_rate_pitch,capture_endpoint_qualified_pitch,capture_endpoint_noise_pitch,capture_endpoint_rate_roll,capture_endpoint_qualified_roll,capture_endpoint_noise_roll,capture_endpoint_rate_yaw,capture_endpoint_qualified_yaw,capture_endpoint_noise_yaw,capture_control_revision");assert(legacy171.size()==171);for(size_t i=0;i<legacy171.size();++i)assert(header[i]==legacy171[i]);
 assert(header[171]=="vector_gain_pitch"&&header[280]=="vector_plan_valid");
 // Frozen original 100 columns protect existing DictReader consumers and order.
 const auto old=csv_fields("kind,t_us,epoch,pawn,plane_id,pitch_deg,yaw_deg,roll_deg,vx_cm_s,vy_cm_s,vz_cm_s,x_cm,y_cm,z_cm,u_pitch,u_yaw,u_roll,throttle,brake,manual_mask,input_age_ms,frame_dt,filtered_pitch_rate,filtered_yaw_rate,filtered_roll_rate,aim_pitch,aim_yaw,sim_seconds,assist_on,assist_delta_pitch,assist_delta_roll,assist_profile,confidence_pitch,confidence_roll,gain_pitch,gain_roll,learn_tau_pitch,learn_tau_roll,learn_weight_pitch,learn_weight_roll,learn_promotions_pitch,learn_promotions_roll,confidence_yaw,gain_yaw,learn_tau_yaw,learn_weight_yaw,learn_promotions_yaw,learn_delay_pitch,learn_delay_roll,learn_delay_yaw,primary_weight_pitch,primary_weight_roll,primary_weight_yaw,primary_command_pitch,primary_command_roll,primary_command_yaw,primary_delta_pitch,primary_delta_roll,primary_delta_yaw,primary_stop_pitch,primary_stop_roll,primary_stop_yaw,primary_horizon_pitch,primary_horizon_roll,primary_horizon_yaw,primary_ratecap_pitch,primary_ratecap_roll,primary_ratecap_yaw,primary_checks_pitch,primary_checks_roll,primary_checks_yaw,primary_model_rmse_pitch,primary_model_rmse_roll,primary_model_rmse_yaw,primary_base_rmse_pitch,primary_base_rmse_roll,primary_base_rmse_yaw,reference_rate_pitch,reference_rate_roll,reference_rate_yaw,candidate_weight_pitch,candidate_weight_roll,candidate_weight_yaw,model_source_pitch,model_source_roll,model_source_yaw,model_deployments_pitch,model_deployments_roll,model_deployments_yaw,pursuit_pitch,pursuit_roll,pursuit_yaw,effective_damping_pitch,effective_damping_roll,effective_damping_yaw,plan_input_limit_pitch,plan_input_limit_roll,plan_input_limit_yaw,selected_control_mode,control_mode_blend");
 assert(old.size()==100&&header.size()==370);
 assert(header[297]=="vector_envelope_mix"&&header[302]=="vector_control_revision");
 assert(header[343]=="heritage_weight"&&header[357]=="heritage_model_mix_yaw"&&header[369]=="heritage_reason_yaw");
 assert(header[337]=="vector_terminal_stop_allowance_deg");
 assert(header[338]=="vector_roll_raw_error"&&header[342]=="vector_roll_audit_fraction");
 assert(header[323]=="vector_signed_model_weight"&&header[330]=="vector_signed_check_long"&&header[331]=="vector_signed_control_blend");
 assert(header[314]=="vector_learning_allowed"&&header[322]=="vector_environment_unsafe");
 assert(header[303]=="vector_baseline_bias_pitch"&&header[313]=="vector_local_bias_rejected_yaw");
 for(size_t i=0;i<old.size();++i)assert(header[i]==old[i]);
 const std::array<std::string,13> added{"capture_weight","level_phase","actual_bank_deg","capture_roll_error_deg","capture_roll_ratecap_deg_s","roll_limit_cause","level_weight","relative_pointing_rate_deg_s","capture_joint_delta_l1","capture_joint_roll_delta","capture_trim_pitch_deg_s","capture_trim_yaw_deg_s","capture_target_bank_deg"};
 for(size_t i=0;i<added.size();++i)assert(header[100+i]==added[i]);
 auto input=csv_fields(read_csv_line(shadow_file)),first=csv_fields(read_csv_line(shadow_file)),second=csv_fields(read_csv_line(shadow_file));
 assert(input.size()==header.size()&&first.size()==header.size()&&second.size()==header.size());
 assert(input[0]=="I"&&first[0]=="S"&&second[0]=="S");
 for(size_t i=100;i<header.size();++i)assert(std::stod(input[i])==0);
 auto value=[&](const std::vector<std::string>& row,const char* key){
  auto i=std::find(header.begin(),header.end(),key);assert(i!=header.end());return std::stod(row[size_t(i-header.begin())]);
 };
 assert(header[113]=="capture_gain_pitch"&&header[160]=="capture_joint_checked"&&header[170]=="capture_control_revision");
 assert(value(first,"capture_gain_pitch")==42&&value(first,"capture_joint_checked")==1);
 assert(value(first,"vector_baseline_bias_pitch")==3&&value(first,"vector_local_bias_weight_pitch")==.75&&value(first,"vector_joint_axes")==2&&value(first,"vector_pending_error_pitch")==.125&&value(first,"vector_local_bias_accepted_pitch")==4);
 assert(value(first,"vector_envelope_mix")==1&&value(first,"vector_pitch_pursuit_cap")==160&&value(first,"vector_pitch_stop_cap")==115&&value(first,"vector_pitch_reachable_high")==98&&value(first,"vector_requested_rate")==120&&value(first,"vector_control_revision")==4);
 assert(std::abs(value(first,"vector_terminal_stop_allowance_deg")-.05)<1e-6);
 assert(value(first,"heritage_weight")==1&&value(first,"heritage_revision")==9&&value(first,"heritage_boost_pitch")==.625&&value(first,"heritage_source_pitch")==2&&value(first,"heritage_reason_yaw")==8);
 // Diagnostic and attitude/input values join on the very same S timestamp/epoch.
 const auto t1=std::stoull(first[1]),t2=std::stoull(second[1]);
 assert(t1>=first_begin&&t1<=first_end&&t2>=second_begin&&t2<=second_end&&t2>=t1);
 assert(value(first,"epoch")==value(second,"epoch"));
 assert(value(first,"selected_control_mode")==2&&value(second,"selected_control_mode")==2);
 assert(std::abs(value(first,"u_roll")-.4)<1e-6&&value(first,"roll_deg")==-104);
 assert(value(first,"control_mode_blend")==.25&&value(first,"capture_weight")==.75);
 assert(value(second,"control_mode_blend")==0&&value(second,"capture_weight")==1);
 const std::array<double,13> expected{.75,2,104,-7,85,3,.875,1.5,.125,-.0625,.5,-.25,111};
 for(size_t i=0;i<expected.size();++i)assert(std::abs(std::stod(first[100+i])-expected[i])<1e-6);
 assert(value(second,"level_phase")==3&&value(second,"level_weight")==1&&value(second,"capture_target_bank_deg")==0);
 assert(value(first,"capture_target_bank_deg")==value(first,"actual_bank_deg")-value(first,"capture_roll_error_deg"));
 assert(value(second,"capture_roll_error_deg")==value(second,"actual_bank_deg"));
 // Return to append position so window/cap regressions below keep using this file.
 assert(fseek(shadow_file,0,SEEK_END)==0);
}
int main(){
 QueryPerformanceFrequency(&perf_frequency);self_module=GetModuleHandleW(nullptr);
 wcscpy_s(module_folder,L"test-runtime\\Scripts");
 // Keep recorder schema tests self-contained without creating a game CSV.
 shadow_file=std::tmpfile();assert(shadow_file);fputs(shadow_csv_header,shadow_file);
 shadow_start();double state[13]={65536,1,2,3,30000,0,0,.5,0,8010,.011,1,0};
 assist_profile=6;assist_learn_tau_pitch=.9f;assist_learn_weight_pitch=.3f;assist_learn_promotions_pitch=1;
 control_mode=2;control_mode_blend=.25f;pose_roll=-104;
 capture_mode_weight=.75f;capture_level_phase=2;capture_actual_bank=104;capture_roll_error=-7;
 capture_roll_ratecap=85;capture_roll_limit_cause=3;capture_level_weight=.875f;capture_relative_pointing_rate=1.5f;
 capture_joint_delta=.125f;capture_joint_roll_delta=-.0625f;capture_trim_pitch=.5f;capture_trim_yaw=-.25f;capture_target_bank=111;
 capture_model_diagnostics[0]=42;capture_model_diagnostics[47]=1;
 vector_coordination_diagnostics[34]=.05f;vector_coordination_diagnostics[0]=3;vector_coordination_diagnostics[2]=.75f;vector_coordination_diagnostics[4]=2;vector_coordination_diagnostics[5]=.125f;vector_coordination_diagnostics[7]=4;
 vector_performance_diagnostics[0]=1;vector_performance_diagnostics[1]=160;vector_performance_diagnostics[2]=115;vector_performance_diagnostics[3]=98;vector_performance_diagnostics[4]=120;vector_performance_diagnostics[5]=4;
 heritage_diagnostics[0]=1;heritage_diagnostics[2]=9;heritage_diagnostics[9]=.625f;heritage_diagnostics[21]=2;heritage_diagnostics[26]=8;
 shadow_capture_input(65536,.2f,-.3f,.4f,0);
 auto first_begin=shadow_now();shadow_capture_state(state);auto first_end=shadow_now();shadow_drain();assert(shadow_rows==2);
 auto epoch=shadow_epoch.load();auto first=shadow_first_capture.load();
 control_mode_blend=0;capture_mode_weight=1;capture_level_phase=3;capture_level_weight=1;capture_target_bank=0;capture_roll_error=104;
 auto second_begin=shadow_now();shadow_capture_state(state);auto second_end=shadow_now();assert(shadow_epoch==epoch&&shadow_first_capture==first);
 shadow_drain();verify_capture_schema(first_begin,first_end,second_begin,second_end);
 // Unchanged hooks must still emit I heartbeats after100ms.
 shadow_last_input_emit=shadow_now()-100001;auto before_heartbeat=shadow_size;shadow_capture_input(65536,.2f,-.3f,.4f,0);assert(shadow_size==before_heartbeat+1);shadow_drain();
 shadow_origin-=static_cast<uint64_t>(perf_frequency.QuadPart)*901;
 assert(!shadow_allowed());auto size=shadow_size;shadow_capture_state(state);assert(shadow_size==size);
 // Exact missing-Rafale regression: old JAS window expired, actor address reused.
 state[9]=14010;shadow_capture_state(state);assert(shadow_allowed()&&shadow_epoch==epoch+1);
 shadow_capture_input(65536,.1f,0,.2f,0);shadow_drain();assert(shadow_rows>=5);
 epoch=shadow_epoch;shadow_origin-=static_cast<uint64_t>(perf_frequency.QuadPart)*901;assert(!shadow_allowed());
 // Restart the same aircraft with the same address after Lua release.
 shadow_new_flight=true;shadow_capture_state(state);assert(shadow_allowed()&&shadow_epoch==epoch+1);
 epoch=shadow_epoch;state[0]=65544;shadow_capture_state(state);assert(shadow_epoch==epoch+1);
 while(shadow_size)shadow_drain();
 // Session-wide cap still bounds disk growth and never resets on another plane.
 shadow_rows=shadow_session_row_limit-1;ShadowRecord row;row.t=shadow_now();row.pawn=65544;
 shadow_enqueue(row);shadow_enqueue(row);shadow_drain();assert(shadow_rows==shadow_session_row_limit&&!shadow_armed);
 auto end_epoch=shadow_epoch.load();state[9]=11010;shadow_capture_state(state);assert(!shadow_armed&&shadow_epoch==end_epoch);
 assert(shadow_file);fclose(shadow_file);shadow_file=nullptr;
 puts("PASS original CSV schema preserved; sampled CAPTURE state/timestamp and actual mode weight; input-row defaults; expired JAS->Rafale window restart; same-address aircraft change; same-plane new flight; epoch isolation; hard global row cap");
}



