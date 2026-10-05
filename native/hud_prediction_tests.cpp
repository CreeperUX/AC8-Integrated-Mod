#include "src/hud_prediction.h"
#include <cassert>
#include <cstdio>
#include <cstring>
using namespace hud_prediction;
Sample sample(unsigned i,float yaw=0){return {1,2,3,i+1,i*16.667,5,yaw,25,62};}
int main(){
 for(float p:{-80.f,0.f,80.f})for(float y:{-179.f,0.f,179.f})for(float r:{-120.f,0.f,170.f}){
  auto a=flight::basis(p,y,r),b=basis(orientation(a));assert(flight::dot(a.f,b.f)>.99999&&flight::dot(a.r,b.r)>.99999);
 }
 Predictor pred;for(unsigned i=0;i<10;++i)pred.observe(sample(i,i*.1f));
 auto target=flight::basis(5,1,0).f,nose=target;auto result=pred.evaluate(9*16.667+8,target,nose,{},1920,1080,true);
 assert(result.applied&&result.horizon_ms<=8&&result.shift_px<=1080*.0035f+.001);
 auto truth=flight::basis(5,.9f+.1f*8/16.667,25);assert(flight::dot(truth.f,result.view.f)>.999999);
 auto disabled=pred.evaluate(9*16.667+8,target,nose,{},1920,1080,false);assert(!disabled.applied);
 assert(!pred.evaluate(1000,target,nose,{},1920,1080,true).applied);
 auto changed=sample(10,1);changed.fov=40;pred.observe(changed);assert(!pred.evaluate(changed.milliseconds+8,target,nose,{},1920,1080,true).applied);
 for(unsigned i=0;i<12;++i)pred.observe(sample(i,178+i*.3f));
 assert(pred.evaluate(11*16.667+8,target,nose,{},1920,1080,true).reason==4); // offscreen: refuse
 // Continuous short path across the Euler wrap, with a target in view.
 pred.reset();for(unsigned i=0;i<12;++i){float y=179+i*.2f;if(y>180)y-=360;pred.observe(sample(i,y));}
 auto forward=flight::basis(5,-178.8f,0).f;auto wrap=pred.evaluate(11*16.667+8,forward,forward,{},1920,1080,true);assert(wrap.applied);
 // Sudden reversal and epochs must invalidate warmup.
 pred.observe(sample(12,-179.0f));assert(!pred.evaluate(12*16.667+8,forward,forward,{},1920,1080,true).applied);
 auto epoch=sample(13,-178.9f);epoch.epoch=8;pred.observe(epoch);assert(!pred.evaluate(epoch.milliseconds+8,forward,forward,{},1920,1080,true).applied);
 // Large rates are bounded in pixels, even at narrow FOV.
 pred.reset();for(unsigned i=0;i<12;++i){auto s=sample(i,i*1.f);s.fov=20;pred.observe(s);}
 forward=flight::basis(5,11,0).f;auto fast=pred.evaluate(11*16.667+8,forward,forward,{},3840,2160,true);assert(fast.applied&&fast.shift_px<=2160*.0035f+.001&&fast.horizon_ms<8);
 // Held-out high-rate truth: past-only predictions outperform zero-order hold for constant motion.
 double raw_error=0,pred_error=0;pred.reset();
 for(unsigned i=0;i<60;++i){pred.observe(sample(i,i*.1f));if(i<8)continue;
  auto actual=flight::basis(5,i*.1f+.1f*.5f,25);auto base=flight::basis(5,i*.1f,25);
  auto a=pred.evaluate(i*16.667+8,actual.f,actual.f,{},1920,1080,true);assert(a.applied);
  raw_error+=1-flight::dot(actual.f,base.f);pred_error+=std::max(0.f,1-flight::dot(actual.f,a.view.f));
 }
 assert(pred_error<raw_error*.1);
 puts("PASS quaternion roundtrip/wrap, warmup, bounded past-only forecast, FOV/reversal/epoch/stale/offscreen guards and held-out constant-motion truth; not game validation");
}
