#include "src/mouse_aim.cpp"
#include <cassert>
#include <cstdio>
int main(){
 aircraft=65536;assist_plane_type=6010;assist_speed_pawn=65536;assist_speed=1000;assist_brake=0;assist_environment_unsafe=false;
 game_paused=false;gaze_active=false;model_assist_enabled=true;control_mode=2;capture_mode_weight=1;
 auto step=[](unsigned mask,bool stale=false){
  auto now=GetTickCount64();assist_speed_tick=stale?now-1000:now;
  adaptive_owner=65536;adaptive_history.clear();for(auto t=now-1500;t<=now;t+=5)adaptive_history.add({t,.05f,-.1f,.02f});
  observe_assist_input(65536,.05f,-.1f,mask,.02f);float p=0,r=0,y=0;
  apply_model_control(1,30,.2f,1,3,.2f,1,3,.2f,.02f,p,r,y,90,{},flight::unit({1,.003f,.017f}),{0,.5f,.8660254f});
  return std::array<float,3>{p,r,y};
 };
 for(int i=0;i<30;++i){auto u=step(0);for(auto x:u)assert(std::isfinite(x)&&std::abs(x)<=1.00001f);}
 assert(capture_aircraft&&capture_aircraft->id==6010);
 assert(std::abs(capture_model_diagnostics[0].load()-shared_braking::gain(shared_braking::pitch,1000))<.001f);
 assert(std::abs(capture_model_diagnostics[15].load()-shared_braking::gain(shared_braking::roll,1000))<.001f);
 auto manual=step(7);assert(std::abs(manual[0]-.05f)<1e-6&&std::abs(manual[1]+.1f)<1e-6&&std::abs(manual[2]-.02f)<1e-6);
 assert(capture_model_diagnostics[14]==0&&capture_model_diagnostics[44]==0&&capture_model_diagnostics[47]==0);
 step(0,true);for(auto& d:capture_model_diagnostics)assert(d==0);
 // Reuse the actual accepted aircraft model only after new-flight validation.
 auto& bank=capture_aircraft->axes[0].banks[3];bank.accepted=true;bank.selected=capture_adaptation::parameters(shared_braking::pitch,1000);bank.blend=.9f;bank.fresh=20;
 full_model_new_flight=true;step(0);assert(bank.accepted&&bank.blend==0&&bank.fresh==0);
 assist_plane_type=14010;step(0);assert(capture_aircraft->id==14010&&capture_aircraft->axes[0].promotions()==0);
 auto& old=capture_fleet.get(6010,GetTickCount64());assert(old.axes[0].banks[3].accepted);
 // The causal rate endpoint is bounded, shares the observed rate and resets.
 capture_rate::Endpoint endpoint;auto m=shared_braking::pitch;
 assert(endpoint.step(2,.02f,m,400)==2);auto rate=endpoint.step(2.2f,.02f,m,400);assert(std::abs(rate-2.3f)<1e-5f);
 auto shock=endpoint.step(600,.02f,m,400);assert(shock>=600&&shock<650);
 assert(endpoint.step(NAN,.02f,m,400)==0&&!endpoint.seen);
 puts("PASS third-mode adaptive runtime: actual1000speed gain, finite commands, manual ownership, stale diagnostics, flight revalidation, multi-plane isolation, causal endpoint rate");
}
