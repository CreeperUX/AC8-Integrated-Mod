from pathlib import Path
from lupa import LuaRuntime
root=Path(__file__).resolve().parents[1]
lua=LuaRuntime()
lua.globals().helmet=lua.execute((root/'package-template/payload/Game/Binaries/Win64/ue4ss/Mods/AC8MouseAim/Scripts/helmet.lua').read_text())
lua.execute('''
local function pick(list,selected,aim,camera,rotation)
 return helmet.choose(list,selected or 0,{0,0,0},camera or {0,0,0},rotation or {0,0,0},aim or {0,0},90,16/9)
end
local list={{id=1,position={10000,0,0}},{id=2,position={10000,1000,0}},{id=3,position={10000,2000,0}}}
assert(pick(list)==1)
assert(pick(list,1)==2 and pick(list,2)==1 and pick(list,3)==1)
assert(pick({list[1]},1)==1)
-- Mouse is 45 degrees from the nose: choose target at cursor, not nose.
assert(pick({list[1],{id=4,position={10000,10000,0}}},0,{0,45},{0,0,0},{0,30,0})==4)
assert(pick({{id=1,position={-1000,0,0}}})==nil) -- behind camera
assert(pick({{id=1,position={1000,500,0}}})==nil) -- outside pick circle
assert(pick({{id=1,position={1000,0,2000}}})==nil) -- outside screen
assert(pick({{id=1,position={0/0,0,0}}})==nil)
assert(pick({},0,{0,180})==nil) -- cursor offscreen
assert(pick({{id=2,position={10000,0,0}},list[1]})==1) -- deterministic tie
assert(pick({list[1],list[1],list[2]},1)==2) -- duplicate candidates
-- Camera offset projection agrees with HUD's finite-distance aim point.
assert(pick({{id=5,position={50000,0,0}}},0,{0,0},{-3600,0,600})==5)
-- Rolled view changes axes, not target identity at the cursor.
assert(pick({list[1]},0,{0,0},{0,0,0},{0,0,90})==1)
local submissions={}
local sampler=helmet.new(function(...)submissions[#submissions+1]={...}end)
local function actor(id,x,y,z)return {IsValid=function()return true end,GetAddress=function()return id end,K2_GetActorLocation=function()return {X=x,Y=y,Z=z}end}end
local a,b=actor(1,10000,0,0),actor(2,10000,1000,0)
local component={IsValid=function()return true end,GetAddress=function()return 32 end,GetSelectedTarget=function()return a end,TargetCandidates={a,b}}
function StaticFindObject()error('HMD must not search global objects')end
local pawn={GetAddress=function()return 16 end,TargetSelectionComponent=component}
local function sample(t)sampler:update(pawn,{X=0,Y=0,Z=0},{X=0,Y=0,Z=0},{0,0,0},{0,0},90,16/9,t,function(v,k)return v[k]end)end
sample(1);assert(submissions[#submissions][4]==2)
local count=#submissions;sample(1.01);assert(#submissions==count)
component.TargetCandidates={};sample(1.04);assert(submissions[#submissions][4]==0)
sampler:clear();assert(submissions[#submissions][1]==0)
sample(.1);assert(submissions[#submissions][4]==0) -- mission clock reset
''')
print('PASS HMD screen ranking, off-axis aim, cycling, camera offset/roll, invalid/offscreen targets and sampling lifecycle')
