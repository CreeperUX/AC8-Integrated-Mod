-- Only compute orientation; native post-update supplies current-frame position.
local M={}
local directory=assert(debug.getinfo(1,'S').source:sub(2):match('^(.*[/\\])'))
local mathrig=dofile(directory..'rig_math.lua')
local owner,forward,up
local settings
function M.configure()
    local v=dofile(directory.."camera-settings.lua")
    assert(type(v)=="table", "Camera settings must return a table")
    assert(type(v.follow_rate)=="number" and v.follow_rate>=2 and v.follow_rate<=20,"follow_rate must be 2..20")
    assert(type(v.level_rate)=="number" and v.level_rate>=0 and v.level_rate<=10,"level_rate must be 0..10")
    settings=v
end
M.configure()
local function direction(p,y)
    p=math.rad(p); y=math.rad(y)
    return {X=math.cos(p)*math.cos(y),Y=math.cos(p)*math.sin(y),Z=math.sin(p)}
end
local function cross(a,b) return {X=a.Y*b.Z-a.Z*b.Y,Y=a.Z*b.X-a.X*b.Z,Z=a.X*b.Y-a.Y*b.X} end
local function dot(a,b) return a.X*b.X+a.Y*b.Y+a.Z*b.Z end
function M.restore() owner=nil; forward=nil; up=nil end
function M.seed(rotation,component)
    local p=assert(component(rotation,'Pitch'))
    local y=assert(component(rotation,'Yaw'))
    local r=math.rad(assert(component(rotation,'Roll')))
    forward=direction(p,y)
    local right={X=-math.sin(math.rad(y)),Y=math.cos(math.rad(y)),Z=0}
    local neutral_up=cross(forward,right)
    up={X=neutral_up.X*math.cos(r)+right.X*math.sin(r),
        Y=neutral_up.Y*math.cos(r)+right.Y*math.sin(r),Z=neutral_up.Z*math.cos(r)+right.Z*math.sin(r)}
end
function M.update(pawn,controller,rotation,component,on,p,y,dt,view_owned)
    local address=pawn:GetAddress()
    if owner~=address then M.restore(); owner=address end
    -- Same-frame values returned by native bridge; no disk snapshots, polling
    -- or stale file replay. Native pose/camera timeout guards remain in place.
    if on~=1 or type(p)~='number' or type(y)~='number' then forward=nil; return nil end
    if view_owned==nil then
        local target=controller:GetViewTarget()
        view_owned=target and target:IsValid() and target:GetAddress()==address
    end
    if not view_owned then forward=nil; return nil end
    if not forward then
        forward=direction(assert(component(rotation,'Pitch')),assert(component(rotation,'Yaw')))
        up={X=0,Y=0,Z=1}
    end
    forward,up=mathrig.step(forward,up,direction(assert(tonumber(p)),assert(tonumber(y))),math.max(0,math.min(0.1,dt)),settings.follow_rate,settings.level_rate)
    local pitch=math.deg(math.asin(math.max(-1,math.min(1,forward.Z))))
    local yaw=math.deg(math.atan(forward.Y,forward.X))
    local right=cross(up,forward)
    local neutral_right={X=-math.sin(math.rad(yaw)),Y=math.cos(math.rad(yaw)),Z=0}
    local neutral_up=cross(forward,neutral_right)
    local roll=math.deg(math.atan(-dot(right,neutral_up),dot(right,neutral_right)))
    return {pitch=pitch,yaw=yaw,roll=roll}
end
return M
