#pragma once
#include "flight_math.h"
namespace flight {
struct FreeLook {
    bool held=false;
    V direction{1,0,0};
    void reset() { held=false; }
    V step(bool down, V& aim, const Basis& view, float dx, float dy,float fov=62,float reference_fov=62,V offset={}) {
        // Discard boundary-frame deltas so release cannot move the flight target.
        if (down!=held) {
            held=down;
            if (held) direction=view.f;
            dx=dy=0;
        }
        if (held) {
            // World-up yaw and bounded pitch give an orbit without pole flips.
            const float p=std::clamp(pitch(direction)-dy,-89.0f,89.0f);
            direction=basis(p,yaw(direction)+dx,0).f;
            return direction;
        }
        aim=move_world_target(aim,view,dx,dy,fov,reference_fov,offset);
        return aim;
    }
};
}
