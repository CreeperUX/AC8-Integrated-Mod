#pragma once
#include <algorithm>
#include <cmath>

// Third policy roll guidance. Angles are degrees and rates are degrees/second.
// This selects a roll goal; the shared input controller still enforces input
// bounds, delay prediction, braking and slew. It does not alter the old modes.
namespace capture_roll {
inline float smooth(float x) {
    x=std::clamp(x,0.f,1.f);
    return x*x*(3.f-2.f*x);
}
struct Input {
    float dt=.02f;
    float angle=0;                  // Total pointing error.
    float up=0,right=0;             // Target coordinates in the aircraft basis.
    float turn_error=0;             // Full roll-and-pull pursuit roll error.
    float level_error=0;            // atan2(body_right.z, body_up.z), degrees.
    float pitch_rate=0,yaw_rate=0;  // Aircraft rates minus target references.
    float roll_rate=0;
    float target_rate=0;            // Magnitude of actual world target motion.
    float target_jump=0;            // World target displacement this update.
    float pole_clearance=1;         // hypot(body_right.z, body_up.z); 0 at pole.
    float pursuit_rate=140;
    bool active=true;               // False at manual, pause and policy boundaries.
};
struct Output {
    bool valid=false,leveling=false;
    float roll_error=0,rate_limit=0,level_weight=0;
    float stable_time=0,cross_rate=0;
};
struct Guidance {
    bool leveling=false;
    float stable_time=0,moving_time=0;
    int level_direction=0;

    void reset() { *this=Guidance{}; }

    Output update(const Input& in) {
        Output out;
        if(!in.active||!std::isfinite(in.dt)||in.dt<=0||in.dt>.1f||
           !std::isfinite(in.angle)||in.angle<0||in.angle>180.f||
           !std::isfinite(in.up)||!std::isfinite(in.right)||
           !std::isfinite(in.turn_error)||!std::isfinite(in.level_error)||
           !std::isfinite(in.pitch_rate)||!std::isfinite(in.yaw_rate)||
           !std::isfinite(in.roll_rate)||!std::isfinite(in.target_rate)||
           !std::isfinite(in.target_jump)||!std::isfinite(in.pole_clearance)||
           !std::isfinite(in.pursuit_rate)||in.pursuit_rate<=0) {
            reset();
            return out;
        }

        float relative_rate=std::hypot(in.pitch_rate,in.yaw_rate);
        float length=std::hypot(in.up,in.right);
        out.cross_rate=length>1e-5f?
            std::abs((in.pitch_rate*in.right-in.yaw_rate*in.up)/length):relative_rate;

        // Capture uses pointing stability relative to the target. Absolute rate
        // would reject a steadily moving target which the aircraft already follows.
        bool stable=in.angle<=2.5f&&relative_rate<=3.5f;
        stable_time=stable?std::min(.2f,stable_time+in.dt):0;
        bool fast_new_target=in.angle>3.5f&&std::abs(in.target_rate)>10.f;
        moving_time=fast_new_target?std::min(.2f,moving_time+in.dt):0;
        bool release=in.angle>=7.f||std::abs(in.target_jump)>=2.5f||moving_time+1e-6f>=.12f;
        if(release) {
            leveling=false;
            stable_time=0;
            level_direction=0;
        } else if(!leveling&&stable_time+1e-6f>=.1f) {
            leveling=true;
        }

        out.valid=true;
        out.leveling=leveling;
        out.level_weight=leveling?1.f:0.f;
        out.stable_time=stable_time;
        float pole=smooth((std::clamp(in.pole_clearance,0.f,1.f)-.05f)/.15f);
        if(!leveling) {
            level_direction=0;
            // Do not turn the pursuit roll goal into bank hold below 8 degrees.
            // The target plane remains available until stable capture occurs.
            out.roll_error=std::clamp(in.turn_error,-180.f,180.f);
            out.rate_limit=std::clamp(in.pursuit_rate,70.f,200.f);
            return out;
        }

        // An explicit horizontal attitude objective survives ordinary residual
        // pointing error and rate noise. Risk reduces speed, never this angle.
        float level_error=std::remainder(in.level_error,360.f);
        float magnitude=std::abs(level_error);
        if(magnitude<150.f)level_direction=0;
        if(magnitude>=170.f&&!level_direction) {
            // Near inverted, atan2 can alternate between almost +180 and -180.
            // Pick one recovery direction; existing roll momentum breaks the
            // geometric tie without asking the plant to reverse abruptly.
            float choice=std::abs(in.roll_rate)>=15.f?in.roll_rate:level_error;
            level_direction=choice<0?-1:1;
        }
        if(level_direction&&magnitude>=150.f&&level_error*level_direction<0)
            level_error+=360.f*level_direction;
        out.roll_error=level_error;
        float angle_risk=smooth((in.angle-2.5f)/4.5f);
        float motion_risk=smooth((relative_rate-3.5f)/9.5f);
        float cross_risk=smooth((out.cross_rate-2.f)/8.f);
        float rate=90.f-22.f*angle_risk-20.f*motion_risk-13.f*cross_risk;
        // A transverse pointing residual rotates with bank. Limit that induced
        // angular motion continuously; retain useful leveling authority.
        constexpr float radians=.017453292519943295f;
        float coupling=std::sin(std::min(in.angle,90.f)*radians);
        if(coupling>.001f)rate=std::min(rate,3.5f/coupling);
        out.rate_limit=std::clamp(rate,35.f,100.f);
        out.rate_limit=35.f+(out.rate_limit-35.f)*pole;
        // At the exact vertical pole the projected horizontal frame is undefined.
        // Brake existing roll rather than chase a discontinuous atan2 sign.
        if(in.pole_clearance<=.05f) {
            out.roll_error=0;
            level_direction=0;
        }
        return out;
    }
};
}
