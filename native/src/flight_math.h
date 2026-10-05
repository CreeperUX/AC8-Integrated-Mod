#pragma once
#include <cmath>
#include <algorithm>
// MouseFlight-inspired local-space guidance; Unreal axes: X forward, Y right, Z up.
namespace flight {
constexpr float rad = 0.017453292519943295f;
struct V {
    float x{}, y{}, z{};
    V operator+(V b) const { return {x+b.x,y+b.y,z+b.z}; }
    V operator-(V b) const { return {x-b.x,y-b.y,z-b.z}; }
    V operator*(float k) const { return {x*k,y*k,z*k}; }
};
inline float dot(V a,V b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
inline V cross(V a,V b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
inline V unit(V a) { float n=std::sqrt(dot(a,a)); return n>1e-6f ? a*(1/n):V{1,0,0}; }
struct Basis { V f,r,u; };
inline Basis basis(float pitch,float yaw,float roll) {
    float p=pitch*rad,y=yaw*rad,r=roll*rad;
    V f{std::cos(p)*std::cos(y),std::cos(p)*std::sin(y),std::sin(p)};
    V right{-std::sin(y),std::cos(y),0};
    V up=cross(f,right);
    return {f,right*std::cos(r)-up*std::sin(r),up*std::cos(r)+right*std::sin(r)};
}
inline V rotate(V v,V axis,float angle) {
    float c=std::cos(angle),s=std::sin(angle);
    return unit(v*c+cross(axis,v)*s+axis*(dot(axis,v)*(1-c)));
}
// Reproject only when mouse input exists. Camera motion alone NEVER changes aim.
inline V move_world_target(V aim,const Basis& view,float dx,float dy,float fov,float reference_fov,V offset={}) {
    if(dx==0 && dy==0)return aim;
    const float scale=std::tan(std::clamp(fov,15.0f,150.0f)*0.5f*rad)/std::tan(std::clamp(reference_fov,30.0f,150.0f)*0.5f*rad);
    V point=aim*50000.0f-offset;float depth=dot(point,view.f);
    if(depth>500.0f){
        float x=dot(point,view.r)/depth+dx*rad*scale;
        float y=dot(point,view.u)/depth-dy*rad*scale;
        V ray=unit(view.f+view.r*x+view.u*y);
        float along=dot(offset,ray);
        float disc=along*along+50000.0f*50000.0f-dot(offset,offset);
        if(disc>0)return unit(offset+ray*(-along+std::sqrt(disc)));
    }
    // Offscreen/behind-camera fallback preserves spherical world aiming.
    V axis=view.r*dy+view.u*dx;float angle=std::sqrt(dot(axis,axis));
    return angle>0?rotate(aim,unit(axis),angle*rad*scale):aim;
}
inline float pitch(V v) { return std::asin(std::clamp(v.z,-1.0f,1.0f))/rad; }
inline float yaw(V v) { return std::atan2(v.y,v.x)/rad; }
inline float dead(float x,float d) { return std::copysign(std::max(0.0f,std::abs(x)-d),x); }
inline float tracking_weight(float angle) {
    float t=std::clamp((angle-0.75f)/4.25f,0.0f,1.0f);
    return t*t*(3-2*t);
}
// Conservative stopping envelope: distance = speed*delay + speed^2/(2*deceleration).
struct LevelBlend {
    bool leveling=false;
    float weight=1;
    void reset() { leveling=false; weight=1; }
    float step(float angle,float dt) {
        if(angle<=3) leveling=true;
        else if(angle>=6) leveling=false;
        const float wanted=leveling?0:tracking_weight(angle);
        const float delta=std::clamp(wanted-weight,-4*dt,4*dt);
        weight=std::clamp(weight+delta,0.0f,1.0f);
        return weight;
    }
};
// Parameters describe an estimated input response, not changes to the flight model.
inline float arrival_rate(float error,float max_rate,float deceleration,float gain,float zone) {
    float distance=std::max(0.0f,std::abs(error)-zone);
    float delay_speed=deceleration*0.15f;
    float stoppable=std::sqrt(delay_speed*delay_speed+2*deceleration*distance)-delay_speed;
    return std::copysign(std::min({max_rate,gain*distance,stoppable}),error);
}
inline float arrival_command(float error,float actual,float max_rate,float deceleration,
                             float gain,float zone,float full_rate,float limit,float braking=1.0f) {
    const float closing=std::max(0.0f,actual*std::copysign(1.0f,error));
    const float anticipation=std::clamp(braking-1.0f,0.0f,0.35f)*0.30f;
    const float predicted=std::copysign(std::max(0.0f,std::abs(error)-closing*anticipation),error);
    float wanted=arrival_rate(predicted,max_rate,deceleration,gain,zone);
    float command=wanted/full_rate+(wanted-actual)/full_rate;
    // When closing faster than the stopping envelope permits, actively counter-steer.
    if(std::abs(error)<=zone || actual*std::copysign(1.0f,error)>std::abs(wanted)+3.0f)
        command=braking*(wanted-actual)/(full_rate*0.65f);
    return std::clamp(command,-limit,limit);
}
// One crossing hold per substantial approach. Small steady tracking keeps the
// established controller; no repeated high-gain braking around tiny errors.
struct ArrivalState {
    bool armed=false,holding=false,seen=false;float previous_error=0,age=0,direction=0,crossing_rate=1;
    void reset(){*this=ArrivalState{};}
    float command(float error,float rate,float max_rate,float deceleration,float gain,float zone,float full_rate,float limit,float braking,float dt){
        float base=arrival_command(error,rate,max_rate,deceleration,gain,zone,full_rate,limit,braking);
        if(!std::isfinite(error)||!std::isfinite(rate)||!std::isfinite(dt)||dt<=0||dt>.1){reset();return base;}
        if(!holding&&std::abs(error)>8.f&&error*rate>0)armed=true;
        if(armed&&seen&&previous_error*error<0&&previous_error*rate>0&&std::abs(rate)>5.f){
            holding=true;armed=false;age=0;direction=std::copysign(1.f,rate);crossing_rate=std::abs(rate);
        }
        previous_error=error;seen=true;
        if(holding){
            if(direction*rate<=0||direction*error>zone||age>=.35f){holding=false;return base;}
            float closing=std::max(0.f,rate*std::copysign(1.f,error));
            float predicted=std::copysign(std::max(0.f,std::abs(error)-closing*std::clamp(braking-1.f,0.f,.35f)*.30f),error);
            float wanted=arrival_rate(predicted,max_rate,deceleration,gain,zone);
            float strong=std::clamp(braking*(wanted-rate)/(full_rate*.65f),-limit,limit);
            float fade=std::clamp((age-.20f)/.15f,0.f,1.f);fade=fade*fade*(3-2*fade);
            float rate_weight=std::clamp((direction*rate/crossing_rate-.5f)/.5f,0.f,1.f);
            rate_weight=rate_weight*rate_weight*(3-2*rate_weight);
            age+=dt;return std::clamp(base+(strong-base)*(1-fade)*rate_weight,-limit,limit);
        }
        return base;
    }
};
}
