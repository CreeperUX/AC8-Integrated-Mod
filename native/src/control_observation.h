#pragma once
// Essential control metadata has no dependency on recorder position/time fields.
// Status: 1 accepted, -2 owner, -3 velocity, -4 identity, -5 environment.
int accept_control_observation(const double (&v)[7]) {
    if(!live_pointer_number(v[0]) || static_cast<uintptr_t>(v[0])!=aircraft.load())return -2;
    for(int i=1;i<=3;++i)if(!std::isfinite(v[i])||std::abs(v[i])>1e12)return -3;
    // Lua numbers exactly represent every signed 32-bit integer. Do not impose
    // the old arbitrary 100000 ceiling on mission-specific identity values.
    if(!std::isfinite(v[5])||v[5]<0||v[5]>2147483647.0||v[5]!=std::floor(v[5]))return -4;
    if(!std::isfinite(v[4])||(v[6]!=0&&v[6]!=1))return -5;
    assist_speed.store(float(std::sqrt(v[1]*v[1]+v[2]*v[2]+v[3]*v[3])/100));
    assist_brake.store(float(v[4]));
    assist_environment_unsafe.store(v[6]!=0||v[4]<0||v[4]>1);
    assist_plane_type.store(int(v[5]));
    assist_speed_pawn.store(static_cast<uintptr_t>(v[0]));
    assist_speed_tick.store(GetTickCount64());
    return 1;
}
