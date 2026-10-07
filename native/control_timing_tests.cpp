#include "src/control_timing.h"
#include <cassert>
#include <cmath>
#include <cstdio>
int main(){using control_timing::delta;uint64_t previous=100000;double jitter=0;
 for(unsigned i=1;i<=120;++i){uint64_t next=100000+i*100000;float dt=delta(next,previous,6000000,true);jitter+=std::abs(dt-1.f/60);previous=next;}
 assert(jitter<.000001);assert(delta(previous,previous,6000000,true)==1.f/60);assert(delta(1,previous,6000000,true)==1.f/60);
 assert(delta(999999999,previous,6000000,false)==1.f/60);assert(delta(2000,1000,1000,true)==.1f);assert(delta(1001,1000,1000000,true)==.001f);
 puts("PASS constant60Hz without integer-ms modulation, discontinuity reset and bounded timing");}
