#include "src/hud_interpolation.h"
#include <cassert>
#include <cstdio>
int main(){HudInterpolation h;float x=0,y=0;h.step(1,1,0,true,x,y,true);
x=10;h.step(1,2,16,true,x,y,true);assert(x==0);
x=10;h.step(1,2,24,true,x,y,true);assert(x==5);
x=10;h.step(1,2,32,true,x,y,true);assert(x==10);
x=20;h.step(1,3,32,true,x,y,true);assert(x==10);
x=20;h.step(1,3,40,true,x,y,true);assert(x==15);
x=40;h.step(1,4,48,true,x,y,false);assert(x==40&&!h.valid);
x=80;h.step(2,5,60,true,x,y,true);assert(x==80);
x=500;h.step(2,6,76,true,x,y,true);assert(x==500);
x=510;h.step(2,7,200,true,x,y,true);assert(x==510);
h.step(2,7,201,false,x,y,true);assert(!h.valid);
puts("PASS display-only 60-to-120 intermediate samples, endpoints, mode/identity/stale/jump resets; no prediction");}
