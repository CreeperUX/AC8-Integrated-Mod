#pragma once
#include <algorithm>
#include <cstdint>
namespace control_timing {
inline float delta(uint64_t now,uint64_t previous,uint64_t frequency,bool continuing){
 if(!continuing||!previous||now<=previous||!frequency)return 1.f/60;
 return std::clamp(float(double(now-previous)/double(frequency)),.001f,.1f);
}
}
