#include "src/hybrid_hud_policy.h"
#include <cassert>
#include <cstdio>
int main(){using namespace hybrid_hud;
 assert(!needs_overlay(true,false,false));assert(needs_overlay(true,true,false));assert(needs_overlay(true,false,true));assert(needs_overlay(false,false,false));
 assert(pacing(false,false,true,false,120)==10);assert(pacing(true,false,true,false,120)==20);assert(pacing(true,false,true,true,120)==60);assert(pacing(true,false,false,false,120)==120);
 puts("PASS hybrid overlay absent at rest, on-demand panel/notification pacing and legacy compatibility");}
