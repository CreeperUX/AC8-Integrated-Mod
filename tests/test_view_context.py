from pathlib import Path
from lupa import LuaRuntime
scripts=Path(__file__).resolve().parents[1]/'package-template/payload/Game/Binaries/Win64/ue4ss/Mods/AC8MouseAim/Scripts'
lua=LuaRuntime()
lua.globals().view_context=lua.execute((scripts/'view_context.lua').read_text()).new()
main=(scripts/'main.lua').read_text()
block=main[main.index('            local target=controller:GetViewTarget()'):main.index('            local on,target_pitch,target_yaw=frame_native')]
lua.execute('''
function print()end
address=65536;dt=.05;gazing=false;paused=false;automatic_view=false;manual_view=false;previous_camera_mode=1;camera_value=1
local other={GetAddress=function()return 65544 end,IsValid=function()return true end}
pawn={GetAddress=function()return address end,IsValid=function()return true end,bEnableWingInput=true,bEnableMovmentInput=true}
controller={GetAddress=function()return 70000 end,GetViewTarget=function(self)return self.target end,target=pawn,bCinematicMode=false,IsMoveInputIgnored=function()return false end}
other_target=other
centers=0;suspended=0;restores=0
aim_camera={restore=function()restores=restores+1 end}
context_native=function(blocked,center) suspended=blocked;centers=centers+center;return 1,camera_value end
''')
for _ in range(20):lua.execute(block)
lua.execute('assert(centers==1 and suspended==0)')
lua.execute('controller.target=other_target;manual_view=true')
for _ in range(20):lua.execute(block)
lua.execute('controller.target=pawn;manual_view=false')
for _ in range(20):lua.execute(block)
lua.execute('assert(centers==1 and suspended==0)')
# Native target focus may change view ownership: preserve the existing aim on return.
lua.execute('gazing=true;controller.target=other_target;before_focus_centers=centers')
for _ in range(20):lua.execute(block)
lua.execute('assert(suspended==0);gazing=false;controller.target=pawn')
for _ in range(20):lua.execute(block)
lua.execute('assert(centers==before_focus_centers)')
# Scripted view with a reused pawn must suspend and re-center exactly once.
lua.execute('automatic_view=true')
lua.execute(block)
lua.execute('assert(suspended==1);automatic_view=false')
for _ in range(20):lua.execute(block)
lua.execute('assert(centers==2 and suspended==0);view_context:reset("mission-initialize")')
for _ in range(20):lua.execute(block)
lua.execute('assert(centers==3);pawn.bEnableWingInput=false')
lua.execute(block)
lua.execute('assert(suspended==1);pawn.bEnableWingInput=true')
for _ in range(20):lua.execute(block)
lua.execute('assert(centers==4);before_restores=restores;camera_value=0')
lua.execute(block)
lua.execute('assert(centers==4 and restores==before_restores);camera_value=1')
lua.execute(block)
lua.execute('assert(centers==4 and restores==before_restores)')
lua.execute('frame_time=100')
lua.execute(block)
lua.execute('frame_time=0')
lua.execute(block)
lua.execute('assert(suspended==1)')
for _ in range(20):lua.execute(block)
lua.execute('assert(centers==5 and suspended==0)')
# Unknown optional fields are probed once and cannot permanently block control.
lua.execute('''
local count=0
local object=setmetatable({},{__index=function()count=count+1;error('unsupported property')end})
for i=1,50 do assert(view_context:optional_bool(object,'unknown',false)==nil)end
assert(count==1)
''')
# Execute the production manual-vs-scripted handoff expression.
classification=next(line.strip() for line in main.splitlines() if line.strip().startswith('gazing=automatic_view'))
lua.execute('gazing=true;automatic_view=false;manual_view=true')
lua.execute(classification);lua.execute('assert(gazing==false)')
lua.execute('gazing=true;automatic_view=true;manual_view=true')
lua.execute(classification);lua.execute('assert(gazing==true)')
gaze=LuaRuntime()
gaze.execute('function print()end')
gaze.globals().gaze=gaze.execute((scripts/'gaze.lua').read_text())
gaze.execute('''
local event=nil
local focus={IsValid=function()return true end,bFocusInputPrevPressed=true,FocusInputHoldDuration=.4,bForceInput=false,ProcessingEventFocusTarget={Get=function()return event end}}
local pawn={GetAddress=function()return 65536 end,CameraViewComponent={CachedFocusTarget=focus},ImpactCamera={IsValid=function()return true end,bIsActive=false}}
local active,auto,manual=gaze.update(pawn,1);assert(active and not auto and not manual)
focus.bFocusInputPrevPressed=false
active,auto,manual=gaze.update(pawn,1.1);assert(active and not auto and not manual)
active,auto,manual=gaze.update(pawn,1.3);assert(not active and not auto and not manual)
event={IsValid=function()return true end}
active,auto,manual=gaze.update(pawn,2);assert(active and auto and not manual)
event=nil;active,auto,manual=gaze.update(pawn,2.1);assert(active and auto)
active,auto,manual=gaze.update(pawn,2.3);assert(not active and not auto)
-- C plus native focus signals must keep the mod orbit, including release grace.
gaze.reset();focus.bFocusInputPrevPressed=true;focus.FocusInputHoldDuration=.8
active,auto,manual=gaze.update(pawn,2.5,true);assert(active and not auto and manual)
focus.bFocusInputPrevPressed=false
active,auto,manual=gaze.update(pawn,2.6,false);assert(manual and not auto)
active,auto,manual=gaze.update(pawn,2.9,false);assert(not manual and not auto)
-- A fresh native-focus hold without C must yield, with no automatic recenter.
focus.bFocusInputPrevPressed=true
active,auto,manual=gaze.update(pawn,3,false);assert(active and not auto and not manual)
gaze.reset()
local broken={GetAddress=function()return 88888 end}
active,auto,manual=gaze.update(broken,3,true);assert(not auto and manual)
active,auto,manual=gaze.update(broken,3.1,false);assert(not auto and manual)
active,auto,manual=gaze.update(broken,3.3,false);assert(not auto and not manual)
''')
print('PASS real view-context block: startup, scripted-camera return, reused pawn, input gating, manual C retention, view switching, optional-field probe bound and gaze classification.')
