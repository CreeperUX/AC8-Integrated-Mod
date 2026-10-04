#include "src/mouse_aim.cpp"
#include <cassert>
int main(){
 QueryPerformanceFrequency(&perf_frequency);self_module=GetModuleHandleW(nullptr);
 wcscpy_s(module_folder,L"test-runtime\\Scripts");
 shadow_start();double state[13]={65536,1,2,3,30000,0,0,.5,0,8010,.011,1,0};
 assist_profile=6;assist_learn_tau_pitch=.9f;assist_learn_weight_pitch=.3f;assist_learn_promotions_pitch=1;
 shadow_capture_input(65536,.2f,-.3f,.4f,0);shadow_capture_state(state);shadow_drain();assert(shadow_rows==2);
 auto epoch=shadow_epoch.load();auto first=shadow_first_capture.load();
 shadow_capture_state(state);assert(shadow_epoch==epoch&&shadow_first_capture==first);
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
 puts("PASS expired JAS->Rafale window restart; same-address aircraft change; same-plane new flight; no reset each sample; epoch isolation; hard global row cap");
}

