#pragma once
#include <cstdint>
// A transient nonblocking read failure must not hide the previous valid frame.
template<class Frame> struct HudFrameCache {
    Frame value{};bool present=false;
    void observe(bool received,const Frame& frame){if(received){value=frame;present=frame.pawn!=0;}}
    void reset(){present=false;value={};}
    bool fresh(uintptr_t pawn,uint64_t now)const{return present && value.pawn==pawn && now>=value.tick && now-value.tick<250;}
    static int pacing(bool gameplay_visible,int target){return gameplay_visible?target:10;}
};
