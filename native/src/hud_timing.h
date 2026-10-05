#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <algorithm>
struct HudTiming {
 std::array<std::atomic<uint64_t>,129> bins{};std::atomic<uint64_t> last{0};
 void sample(double ms){if(ms<0)return;bins[std::min(size_t(ms*4),size_t(128))].fetch_add(1,std::memory_order_relaxed);}
 void event(uint64_t tick,uint64_t frequency){auto before=last.exchange(tick);if(before&&tick>=before&&tick-before<frequency/4)sample(double(tick-before)*1000/frequency);}
 struct Values {uint64_t n=0;double p50=0,p95=0,p99=0;};
 Values take(){std::array<uint64_t,129> b{};Values v;for(size_t i=0;i<b.size();++i){b[i]=bins[i].exchange(0);v.n+=b[i];}if(!v.n)return v;
  auto percentile=[&](double p){uint64_t acc=0;for(size_t i=0;i<b.size();++i){acc+=b[i];if(double(acc)>=v.n*p)return i*.25;}return 32.;};
  v.p50=percentile(.5);v.p95=percentile(.95);v.p99=percentile(.99);return v;
 }
};
