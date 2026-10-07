#include "src/capture_adaptation.h"
#include <cassert>
#include <cstdio>
#include <memory>
using namespace capture_adaptation;

static void add(adaptive_braking::History& h,uint64_t t,int axis,float u){
 adaptive_braking::Input input{t,0,0,0};if(axis==0)input.pitch=u;else if(axis==1)input.roll=u;else input.yaw=u;h.add(input);
}
static float command(uint64_t tick){
 if(tick<1500)return 0;
 // Mixed sign holds create pursuit, braking, release and reversal evidence.
 constexpr std::array<float,13> inputs{.7f,.7f,-.6f,0,.85f,-.85f,.3f,0,-.4f,.65f,0,-.75f,.2f};
 return inputs[((tick-1500)/396)%inputs.size()];
}
struct Plant {
 shared_braking::Model model;float speed,rate=0;uint64_t tick=1000;
 // Independent analytic ZOH generator at 1 ms. It reads the deterministic
 // input schedule, never the identifier's path/basis/prediction functions.
 float advance(uint64_t until){
  const float decay=std::exp(-.001f/model.tau),k=shared_braking::gain(model,speed);
  for(;tick<until;++tick){const auto delay=uint64_t(std::lround(model.delay*1000));
   const float u=tick>=delay?command(tick-delay):0;
   rate=decay*rate+(1-decay)*(k*u+model.bias);}
  return rate;
 }
};
static void populate(Plant& plant,Axis& axis,shared_braking::Model prior,int channel,
 adaptive_braking::History& history,uint64_t end,bool eligible=true){
 for(uint64_t t=plant.tick;t<=end;t+=33){
  // Records at exact input transitions in addition to measurement timestamps.
  const auto previous=plant.tick;
  for(uint64_t tt=previous;tt<=t;++tt)if(tt==1000||tt==1500||(tt>=1500&&(tt-1500)%396==0))add(history,tt,channel,command(tt));
  const float actual=plant.advance(t);add(history,t,channel,command(t));
  axis.observe(prior,channel,t,actual,plant.speed,history,eligible);axis.effective(prior,plant.speed,.033f);
 }
}
static void three_plants(){
 const shared_braking::Model prior{1.f,.18f,50.f,0,0};
 const std::array<shared_braking::Model,3> truths{{{.6f,.10f,80.f,0,2.f},{1.3f,.24f,30.f,0,-1.f},{1.75f,.06f,68.f,0,.5f}}};
 for(size_t i=0;i<truths.size();++i){auto axis=std::make_unique<Axis>();adaptive_braking::History h;Plant plant{truths[i],i==2?1000.f:400.f};
  populate(plant,*axis,prior,0,h,90000);
  const auto m=axis->effective(prior,plant.speed,.033f);const auto& bank=axis->banks[band(plant.speed)];
  std::printf("plant %zu: promotions %u blend %.3f K %.3f tau %.3f delay %.3f bias %.3f\n",i,axis->promotions(),bank.blend,m.g0,m.tau,m.delay,m.bias);
  assert(axis->promotions()>0&&bank.accepted&&bank.blend>.9f);
  assert(std::abs(m.g0-truths[i].g0)<2.f&&std::abs(m.bias-truths[i].bias)<.15f);
  assert(std::abs(m.tau-truths[i].tau)<.1f&&std::abs(m.delay-truths[i].delay)<.035f);
  assert(m.g1==0); // The third predictor may safely freeze gain at actual speed.
 }
}
static void validation_and_reversal(){
 const shared_braking::Model prior{1.f,.18f,50.f,0,0},truth{.6f,.10f,80.f,0,2.f};
 auto axis=std::make_unique<Axis>();adaptive_braking::History h;Plant plant{truth,400};
 uint64_t first_proposal=0;
 for(uint64_t t=1000;t<30000;t+=33){
  const auto previous=plant.tick;for(uint64_t tt=previous;tt<=t;++tt)if(tt==1000||tt==1500||(tt>=1500&&(tt-1500)%396==0))add(h,tt,0,command(tt));
  auto actual=plant.advance(t);add(h,t,0,command(t));axis->observe(prior,0,t,actual,400,h,true);axis->effective(prior,400,.033f);
  auto& b=axis->banks[1];if(!first_proposal&&b.validating)first_proposal=t;
  if(first_proposal&&t-first_proposal<600)assert(!b.accepted);
  if(b.validating)assert(b.validation_epoch>=first_proposal&&b.last_promotion<b.validation_epoch);
 }
 assert(first_proposal&&axis->banks[1].accepted);
 auto m=axis->effective(prior,400,.033f);float learned_error=0,baseline_error=0;
 // Held-out reversals use a different sequence from the training generator.
 const std::array<float,8> reversal{.8f,-.8f,0,-.7f,.9f,0,.4f,-.4f};
 float actual=0,anchor_rate=0;adaptive_braking::History blind;uint64_t anchor_tick=1400;
 const float d=std::exp(-.001f/truth.tau);size_t count=0;
 for(uint64_t t=1001;t<16000;++t){const auto from=t-1,index=(from-1000)/700;
  const float u=reversal[index%reversal.size()];if((from-1000)%700==0||from%33==0)add(blind,from,0,u);
  const auto delayed=from>=100?from-100:0;const float issued=delayed>=1000?reversal[((delayed-1000)/700)%reversal.size()]:0;
  actual=d*actual+(1-d)*(truth.g0*issued+truth.bias);
  if(t==1400)anchor_rate=actual;
  if(t>1400&&t-anchor_tick==330){
   const float lp=prediction(m,0,anchor_tick,t,anchor_rate,blind),bp=prediction(frozen(prior,400),0,anchor_tick,t,anchor_rate,blind);
   if(std::isfinite(lp)&&std::isfinite(bp)){learned_error+=std::pow(lp-actual,2);baseline_error+=std::pow(bp-actual,2);++count;}
   anchor_rate=actual;anchor_tick=t;
  }
 }
 std::printf("blind reversal: learned %.5f baseline %.5f n %zu\n",learned_error,baseline_error,count);
 assert(count>20&&learned_error<.08f*baseline_error);
 const auto n=axis->banks[1].observations;auto risk_before=axis->uncertainty(400,10,.5f).rate_rms;
 // Unqualified data can enlarge observed risk, but cannot be fitted/promoted.
 const auto promotions=axis->promotions();plant.model.g0=150;populate(plant,*axis,prior,0,h,42000,false);
 assert(axis->promotions()==promotions&&axis->banks[1].observations>n);
 assert(axis->uncertainty(400,10,.5f).rate_rms>risk_before);
}
static void fleet_and_speed(){
 const shared_braking::Model prior{1.f,.18f,50.f,5.f,0},truth{.6f,.1f,80.f,8.f,1.f};
 auto fleet=std::make_unique<Fleet>();auto& first=fleet->get(17,1000);adaptive_braking::History h;
 Plant plant{truth,1000};populate(plant,first.axes[0],prior,0,h,75000);
 auto learned=first.axes[0].effective(prior,1000,.033f);assert(first.axes[0].promotions()>0);
 auto& other=fleet->get(24,76000);auto cold=other.axes[0].effective(prior,1000,.033f);
 assert(cold.g1==0&&std::abs(cold.g0-shared_braking::gain(prior,1000))<1e-5f&&other.axes[0].promotions()==0);
 assert(learned.g0>cold.g0*1.3f);
 auto& axis=first.axes[0];auto before=axis.current(prior,850-.01f),after=axis.current(prior,850+.01f);
 assert(std::abs(before.g0-after.g0)<.03f&&std::abs(before.bias-after.bias)<.003f);
 auto high900=axis.current(prior,900),high1000=axis.current(prior,1000);
 assert(high900.g0>shared_braking::gain(prior,900)&&high1000.g0>high900.g0&&high900.g1==0&&high1000.g1==0);
 const auto promotions=axis.promotions();fleet->begin_flight();
 assert(axis.promotions()==promotions&&axis.banks[3].accepted&&axis.model_weight==0&&axis.anchors[0].tick==0);
 // The retained model needs fresh monitoring before it is applied next flight.
 assert(axis.current(prior,1000).g0==shared_braking::gain(prior,1000));
}
static void degeneracy_and_corruption(){
 const shared_braking::Model prior{1.f,.18f,50.f,0,0};auto axis=std::make_unique<Axis>();adaptive_braking::History h;
 for(uint64_t t=1000;t<30000;t+=33){add(h,t,0,.3f);axis->observe(prior,0,t,15.f,400,h,true);axis->effective(prior,400,.033f);}
 assert(!axis->promotions());
 axis->observe(prior,0,31000,NAN,400,h,true);assert(axis->anchors[0].tick==0);
 add(h,32000,0,NAN);axis->observe(prior,0,32000,0,400,h,true);
 for(uint64_t t=32033;t<35000;t+=33){add(h,t,0,.8f);axis->observe(prior,0,t,1e5f,400,h,true);}
 assert(!axis->promotions());
 assert(!path(h,32000,32300,0,0).valid);
 // Duplicate/backward observations discard anchors and fit chronology.
 axis->observe(prior,0,36000,0,400,h,true);axis->observe(prior,0,35000,0,400,h,true);
 assert(!axis->banks[1].validating&&axis->banks[1].fit_count==0);
}
static void incumbent_survives_bad_candidate(){
 const shared_braking::Model prior{1.f,.18f,50.f,0,0},good{.6f,.10f,80.f,0,2.f},bad{1.3f,.24f,35.f,0,-1.f};
 Bank b;b.accepted=true;b.selected=parameters(good,400);const auto original=b.selected;
 adaptive_braking::History h;Plant plant{bad,400};
 for(uint64_t t=1000;t<=1500;++t)if(t%33==0||t==1000||t==1500)add(h,t,0,command(t));
 float previous=plant.advance(1500);uint64_t start=1500;
 // A coherent but wrong training block proposes a different plant. Restore
 // the real plant for held-out validation; the incumbent must survive.
 for(unsigned i=0;i<28;++i){uint64_t end=start+264;
  for(auto t=start+1;t<=end;++t)if(t%33==0||(t>=1500&&(t-1500)%396==0))add(h,t,0,command(t));
  auto actual=plant.advance(end);b.collect(prior,0,start,end,previous,actual,400,h,4.f,1,false);previous=actual;start=end;
 }
 assert(b.validating&&!b.promotions&&b.proposal.k<50);
 plant.model=good;
 for(unsigned i=0;i<20;++i){uint64_t end=start+264;
  for(auto t=start+1;t<=end;++t)if(t%33==0||(t>=1500&&(t-1500)%396==0))add(h,t,0,command(t));
  auto actual=plant.advance(end);b.collect(prior,0,start,end,previous,actual,400,h,4.f,1,false);
  // Independent 0.6 s horizons seeded from the correct plant's anchor.
  if(b.validating&&i>=3){uint64_t far=end+600;float anchor=actual;
   for(auto t=end+1;t<=far;++t)if(t%33==0||(t>=1500&&(t-1500)%396==0))add(h,t,0,command(t));
   auto at_far=plant.advance(far);b.collect(prior,0,end,far,anchor,at_far,400,h,4.f,1,true);
   actual=at_far;end=far;
  }
  previous=actual;start=end;
 }
 assert(b.accepted&&b.rejections>0&&b.promotions==0&&b.selected.k==original.k&&b.selected.bias==original.bias);
}
static void high_rate_command_history(){
 adaptive_braking::History h;float actual=0,anchor=0;const float tau=.6f,k=70,d=std::exp(-.001f/tau);
 const shared_braking::Model m{tau,.08f,k,0,1.f};
 for(uint64_t t=1000;t<2300;++t){
  auto u=[](uint64_t tick){return tick<1000?0.f:.7f*std::sin(float((tick/8)*8)*.009f);};
  if(t%8==0)add(h,t,0,u(t));
  if(t==1200)anchor=actual;
  if(t==2200){auto forecast=prediction(m,0,1200,2200,anchor,h);assert(std::isfinite(forecast)&&std::abs(forecast-actual)<.01f);break;}
  actual=d*actual+(1-d)*(k*u(t-80)+1.f);
 }
}
static void gap_and_stale_statistics(){
 const shared_braking::Model prior{1.f,.18f,50.f,0,0},truth{.6f,.10f,80.f,0,2.f};
 auto axis=std::make_unique<Axis>();adaptive_braking::History h;Plant plant{truth,400};populate(plant,*axis,prior,0,h,6200);
 auto& bank=axis->banks[1];assert(bank.fit_count>0&&bank.fit_count<28);const auto count=bank.fit_count;
 auto tick=plant.tick+500;add(h,tick,0,0);axis->observe(prior,0,tick,plant.rate,400,h,true);
 assert(bank.fit_count==count&&axis->anchors[0].tick==tick);
 // A hard backwards clock clears the attempt and revalidates accepted models.
 bank.accepted=true;bank.selected=parameters(truth,400);bank.blend=.8f;bank.fresh=20;
 axis->observe(prior,0,tick-100,plant.rate,400,h,true);
 assert(bank.accepted&&bank.fit_count==0&&bank.blend==0&&bank.fresh==0);
 // Lifetime and material prior-shape changes clear only the ongoing attempt.
 bank.fit_count=12;bank.seed=parameters(prior,400);bank.fit_epoch=tick;
 axis->observe(prior,0,tick+attempt_lifetime_ms+1,plant.rate,400,h,true);
 assert(bank.fit_count==0&&bank.expired_fits==1&&bank.accepted);
 bank.fit_count=12;bank.seed=parameters(prior,400);bank.fit_epoch=tick+attempt_lifetime_ms+1;
 auto changed=prior;changed.tau=1.5f;axis->observe(changed,0,tick+attempt_lifetime_ms+34,plant.rate,400,h,true);
 assert(bank.fit_count==0&&bank.expired_fits==2&&bank.accepted);
}
static void served_total_trim_uncertainty(){
 const shared_braking::Model prior{1.f,.18f,50.f,0,0},truth{1.f,.18f,50.f,0,3.f};
 auto adjusted=std::make_unique<Axis>(),unadjusted=std::make_unique<Axis>();adaptive_braking::History h;Plant plant{truth,400};
 for(uint64_t t=1000;t<15000;t+=33){const auto previous=plant.tick;
  for(uint64_t tt=previous;tt<=t;++tt)if(tt==1000||tt==1500||(tt>=1500&&(tt-1500)%396==0))add(h,tt,0,command(tt));
  const float actual=plant.advance(t);add(h,t,0,command(t));
  for(auto* a:{adjusted.get(),unadjusted.get()}){a->observe(prior,0,t,actual,400,h,false);a->effective(prior,400,.033f);}
  adjusted->served(t,truth);
 }
 assert(adjusted->promotions()==0&&unadjusted->promotions()==0);
 assert(adjusted->uncertainty(400,0,.5f).rate_rms<.4f);
 assert(unadjusted->uncertainty(400,0,.5f).rate_rms>1.f);
 for(const auto& b:adjusted->banks)assert(b.fit_count==0); // No total-trim fit leakage.
}
int main(){std::setvbuf(stdout,nullptr,_IONBF,0);three_plants();validation_and_reversal();fleet_and_speed();degeneracy_and_corruption();incumbent_survives_bad_candidate();high_rate_command_history();gap_and_stale_statistics();served_total_trim_uncertainty();std::puts("CAPTURE adaptive tests PASS");}
