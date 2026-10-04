#pragma once
#include <atomic>
#include <cstdint>
#include <algorithm>
struct MouseDelta {int32_t x=0,y=0;};
class PairedMouse {
    std::atomic<uint64_t> value{0};
    static int32_t clamp(int64_t v){return int32_t(std::clamp(v,int64_t(INT32_MIN),int64_t(INT32_MAX)));}
    static uint64_t pack(int32_t x,int32_t y){return uint64_t(uint32_t(x))|(uint64_t(uint32_t(y))<<32);}
    static MouseDelta unpack(uint64_t v){return {int32_t(uint32_t(v)),int32_t(uint32_t(v>>32))};}
public:
    void add(int32_t x,int32_t y){
        auto old=value.load(std::memory_order_relaxed);
        for(;;){auto d=unpack(old);auto next=pack(clamp(int64_t(d.x)+x),clamp(int64_t(d.y)+y));
            if(value.compare_exchange_weak(old,next,std::memory_order_relaxed))return;}
    }
    MouseDelta take(){return unpack(value.exchange(0,std::memory_order_relaxed));}
    void clear(){value.store(0,std::memory_order_relaxed);}
};
