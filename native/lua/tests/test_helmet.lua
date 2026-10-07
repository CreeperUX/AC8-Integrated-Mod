-- helmet.lua selection with the C free-look view direction (run with texlua / Lua 5.3+).
local dir=assert(arg[1],'Scripts directory')
local helmet=dofile(dir..'helmet.lua')
local function pick(list,selected,aim,camera,rotation)
 return helmet.choose(list,selected or 0,{0,0,0},camera or {0,0,0},rotation or {0,0,0},aim or {0,0},90,16/9)
end
-- existing behaviour (ported from the repository's test_helmet.py)
local list={{id=1,position={10000,0,0}},{id=2,position={10000,1000,0}},{id=3,position={10000,2000,0}}}
assert(pick(list)==1)
assert(pick(list,1)==2 and pick(list,2)==1 and pick(list,3)==1)
assert(pick({{id=1,position={-1000,0,0}}})==nil)
assert(pick({},0,{0,180})==nil)
assert(pick({{id=5,position={50000,0,0}}},0,{0,0},{-3600,0,600})==5)
-- C free look: camera orbits to look right (yaw 90) from beside/behind the aircraft. A target on the right is
-- picked with the view direction (what the DLL now supplies while free look is held) ...
local right={{id=7,position={0,20000,0}},{id=8,position={20000,0,0}}}
local cam={-3000*0,-3600,600}   -- camera displaced along the orbit, looking along +Y
assert(pick(right,0,{0,90},cam,{0,90,0})==7,'view-direction pick')
-- ... while the flight target (straight ahead, yaw 0) is off-screen for this view: nothing could be picked before.
assert(pick(right,0,{0,0},cam,{0,90,0})==nil,'flight target off-screen in free look')
-- looking up and right
assert(pick({{id=9,position={0,14142,14142}}},0,{45,90},{0,-3000,-500},{45,90,0})==9)
print('PASS helmet: view-direction selection in C free look; flight-target selection unchanged')
