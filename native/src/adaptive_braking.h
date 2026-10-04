#pragma once
#include "shared_braking.h"
#include <array>
#include <cstdint>
namespace adaptive_braking {
inline float smooth(float x){x=std::clamp(x,0.f,1.f);return x*x*(3-2*x);}
inline float speed_weight(float v){return smooth((v-130)/30)*(1-smooth((v-700)/60));}
struct FastRate {
    float value=0;bool initialized=false;
    void reset(){value=0;initialized=false;}
    float step(float raw,float fallback,float dt){
        if(!std::isfinite(raw)||std::abs(raw)>600||dt<=0||dt>.1){value=fallback;initialized=true;return value;}
        if(!initialized){value=fallback;initialized=true;}
        value+=(raw-value)*(1-std::exp(-30*dt));return value;
    }
};
struct Input {uint64_t tick=0;float pitch=0,roll=0,yaw=0;};
struct History {
    std::array<Input,1024> values{};size_t begin=0,size=0;
    void clear(){begin=size=0;}
    void add(Input v){
        if(size){auto& last=values[(begin+size-1)%values.size()];if(v.tick<last.tick){clear();}else if(v.tick==last.tick){last=v;return;}}
        if(size==values.size()){begin=(begin+1)%values.size();--size;}
        values[(begin+size)%values.size()]=v;++size;
    }
    bool covers(uint64_t t)const{return size && values[begin].tick<=t;}
    float at(uint64_t t,bool roll)const{
        for(size_t i=size;i>0;--i){auto v=values[(begin+i-1)%values.size()];if(v.tick<=t)return roll?v.roll:v.pitch;}
        return 0;
    }
    float at_axis(uint64_t t,int axis)const{
        for(size_t i=size;i>0;--i){auto v=values[(begin+i-1)%values.size()];if(v.tick<=t)return axis==2?v.yaw:axis==1?v.roll:v.pitch;}
        return 0;
    }
};
struct Axis {
    float confidence=.35f,gain_scale=1,delta=0;
    uint64_t anchor_tick=0;float anchor_rate=0,anchor_speed=0;
    void reset(){*this=Axis{};}
    void observe(shared_braking::Model model,bool roll,uint64_t now,float rate,float speed,const History& h){
        if(!std::isfinite(rate)||!std::isfinite(speed)){reset();return;}
        if(!anchor_tick){anchor_tick=now;anchor_rate=rate;anchor_speed=speed;return;}
        float span=float(now-anchor_tick)/1000;
        if(span<.2f)return;
        uint64_t delay=uint64_t(model.delay*1000);
        if(span>.35f || anchor_tick<delay || !h.covers(anchor_tick-delay)){
            anchor_tick=now;anchor_rate=rate;anchor_speed=speed;confidence=std::min(confidence,.35f);return;
        }
        float decay=std::exp(-span/model.tau),response=0,min_u=2,max_u=-2,sum_u=0;
        constexpr int steps=20;float dt=span/steps;
        for(int i=0;i<steps;++i){
            uint64_t t=anchor_tick+uint64_t((i+.5f)*dt*1000)-delay;
            float u=h.at(t,roll);float d=std::exp(-dt/model.tau);
            response=d*response+(1-d)*u;min_u=std::min(min_u,u);max_u=std::max(max_u,u);sum_u+=u;
        }
        float g=shared_braking::gain(model,std::clamp(anchor_speed,130.f,660.f));
        float prediction=decay*anchor_rate+g*gain_scale*response+model.bias*(1-decay);
        float scale=std::max(roll?15.f:5.f,.15f*g);
        float desired=std::clamp(1-std::abs(rate-prediction)/scale,0.f,1.f);
        float blend=1-std::exp(-span/(desired<confidence?.4f:2.f));
        confidence=std::clamp(confidence+(desired-confidence)*blend,0.f,1.f);
        // Learn gain only from sustained commands with useful excitation.
        float mean=sum_u/steps;
        if(confidence>.55f && max_u-min_u<.08f && std::abs(mean)>.25f && mean*rate>0 && std::abs(g*response)>2){
            float estimate=(rate-decay*anchor_rate-model.bias*(1-decay))/(g*response);
            if(std::isfinite(estimate))gain_scale+=std::clamp((std::clamp(estimate,.8f,1.2f)-gain_scale)*.03f,-.01f,.01f);
            gain_scale=std::clamp(gain_scale,.8f,1.2f);
        }
        anchor_tick=now;anchor_rate=rate;anchor_speed=speed;
    }
    float correction(shared_braking::Model model,float error,float rate,float previous_input,float baseline,float speed,
                     float horizon,float range,float zone,float strength,float cap,float slew,float dt){
        if(!std::isfinite(error)||!std::isfinite(rate)||!std::isfinite(previous_input)||!std::isfinite(baseline)||
           !std::isfinite(speed)||!std::isfinite(strength)||!std::isfinite(confidence)||!std::isfinite(gain_scale)||!std::isfinite(dt)||dt<=0||dt>.1){delta=0;return 0;}
        float wanted=0,maximum=cap*std::clamp(strength/.2f,0.f,1.75f);
        if(std::abs(error)<range && std::abs(rate)>=1 && (error*rate>0 || std::abs(error)<=zone)){
            float sign=std::copysign(1.f,rate);model.g0*=gain_scale;model.g1*=gain_scale;
            float prediction=sign*shared_braking::travel(model,rate,previous_input,baseline,std::clamp(speed,130.f,660.f),horizon);
            float remaining=std::max(0.f,sign*error-zone);
            float excess=std::max(0.f,prediction-remaining);
            float fraction=std::clamp(excess/std::max(1.f,std::abs(prediction)+remaining),0.f,1.f);
            float near_weight=1-std::pow(std::abs(error)/range,2.f);
            wanted=-sign*maximum*near_weight*fraction*speed_weight(speed)*confidence;
        }
        if(delta*rate>0||std::abs(rate)<1)delta=0;
        delta+=std::clamp(wanted-delta,-slew*dt,slew*dt);
        delta=std::clamp(delta,-maximum,maximum);return delta;
    }
};
}

