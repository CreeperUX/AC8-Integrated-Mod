#include "src/mouse_aim.cpp"
#include <cassert>
#include <cstdio>
int main(){
 aircraft=65536;hud_epoch=10;
 HudFrame f;f.pawn=65536;f.tick=GetTickCount64();f.epoch=10;f.fov=62;f.ty=5;f.y=3;
 publish_hud_frame(f);float x,y,r,nx,ny;
 assert(canvas_position_status(65536,1920,1080,x,y,r)==1);float initial=x;
 assert(canvas_nose_position(65536,1920,1080,nx,ny));float initial_nose=nx;
 // Asynchronous target changes must not move the circle against an unchanged camera snapshot.
 target_pitch=20;target_yaw=-60;
 assert(canvas_position_status(65536,1920,1080,x,y,r)==1&&x==initial);
 // A publisher between the two Lua calls cannot split ring and connector across frames.
 f.cy=8;publish_hud_frame(f);
 assert(canvas_nose_position(65536,1920,1080,nx,ny)&&nx==initial_nose);
 assert(canvas_position_status(65536,1920,1080,x,y,r)==1&&x!=initial);
 assert(canvas_nose_position(65536,1920,1080,nx,ny)&&nx!=initial_nose);
 // Camera and goal advance together at constant angular separation: screen point stays still.
 f.cy=10;f.ty=15;publish_hud_frame(f);assert(canvas_position_status(65536,1920,1080,x,y,r)==1);assert(std::abs(x-initial)<.001);
 hud_epoch=11;assert(canvas_position_status(65536,1920,1080,x,y,r)==-4);assert(!canvas_nose_position(65536,1920,1080,nx,ny));
 f.epoch=11;f.tick=GetTickCount64()-300;publish_hud_frame(f);assert(canvas_position_status(65536,1920,1080,x,y,r)==-4);
 f.tick=GetTickCount64();publish_hud_frame(f);assert(canvas_position_status(65537,1920,1080,x,y,r)==-3);
 puts("PASS asynchronous goal isolation, ring/nose transaction, constant relative angle, epoch/stale/pawn rejection (not game validation)");
}
