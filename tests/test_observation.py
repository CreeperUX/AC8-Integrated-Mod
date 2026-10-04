from pathlib import Path
from lupa import LuaRuntime

scripts=Path(__file__).resolve().parents[1]/'package-template/payload/Game/Binaries/Win64/ue4ss/Mods/AC8MouseAim/Scripts'
lua=LuaRuntime()
lua.globals().observation=lua.execute((scripts/'observation.lua').read_text())
lua.execute('''
assert(observation.identity(25010,'F15E')==25010)
assert(observation.identity(nil,'BP_PlayerPlane_PP0013_a06e_ms15_C')==2147483013)
assert(observation.identity(-1,'BP_PlayerPlane_PP0013_a06e_ms15_C')==2147483013)
assert(observation.identity(13010,'BP_PlayerPlane_PP0013_a06e_ms15_C')~=13010)
assert(observation.identity(2147483647,'Other')==2147483647)
assert(observation.identity(2147483648,'Other')==nil)
assert(observation.identity(1.5,'Other')==nil)
assert(observation.identity(0/0,'Other')==nil)
assert(observation.identity(nil,'Other')==nil)
''')
# Execute the real Lua observation block with independent recorder failures.
source=(scripts/'main.lua').read_text()
block=source[source.index('            -- Essential metadata'):source.index('            local desired_camera')]
lua.execute('''
on=1; address=65536; shadow_retry=0; shadow_next=0; shadow_sim_seconds=1;
observation_notice_time=0; dt=.01; position={X=0,Y=0,Z=0}; now=100
os.time=function()return now end
print=function()end
unwrap_number=function(v)return v end
rotation_component=function(v,k)return v[k]end
control_calls=0; recorder_calls=0
pawn=setmetatable({GetFullName=function()return 'BP_PlayerPlane_PP0013_a06e_ms15_C /Mission/Pawn'end,
 GetVelocity=function()return {X=30000,Y=0,Z=0}end,InputBrake=0,InputThrottle=.5},
 {__index=function(t,k)if k=='PlaneTypeID'then error('unavailable')end end})
pause_gameplay={GetRealTimeSeconds=function()return now end}
control_native=function(p,x,y,z,brake,id,env)
 assert(p==65536 and x==30000 and id==2147483013);control_calls=control_calls+1;return 1
end
shadow_native=function(...)recorder_calls=recorder_calls+1;error('recorder failure')end
''')
for _ in range(5):lua.execute(block)
lua.execute('assert(control_calls==5 and recorder_calls==1); now=111')
lua.execute(block)
lua.execute('assert(control_calls==6 and recorder_calls==2)')
print('PASS real Lua observation block: mission fallback, unavailable game ID, per-frame metadata, recorder isolation and bounded retry.')
