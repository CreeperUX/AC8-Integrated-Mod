#pragma once
#include <cmath>
#include <algorithm>
#include <array>
namespace hud_geometry {
struct Point {float x=0,y=0;};
constexpr float ring_radius=21.f;
struct Tick {Point a,b;float opacity=0;};
struct Link {std::array<Tick,3> ticks{};};
// Three transverse ticks on the nose-facing side of the target ring.
// Spatial indication only: no timed animation or filtered mouse position.
inline Link connector(Point nose,Point target,float scale,bool always){
    Link out;float dx=nose.x-target.x,dy=nose.y-target.y,distance=std::hypot(dx,dy);
    if(!std::isfinite(distance)||!std::isfinite(scale)||scale<=0||distance<=.001f)return out;
    float ux=dx/distance,uy=dy/distance;
    float spacing=std::clamp(distance*.03f,10.f*scale,18.f*scale);
    for(int i=0;i<3;++i){
        float offset=27.f*scale+i*spacing;
        float available=distance-offset-16.f*scale;
        if(available<=0)continue;
        float fade=always?1.f:std::clamp(available/(14.f*scale),0.f,1.f);
        Point center{target.x+ux*offset,target.y+uy*offset};float half=3.f*scale;
        out.ticks[i]={{center.x-uy*half,center.y+ux*half},{center.x+uy*half,center.y-ux*half},fade*(1.f-.16f*i)};
    }
    return out;
}
}
