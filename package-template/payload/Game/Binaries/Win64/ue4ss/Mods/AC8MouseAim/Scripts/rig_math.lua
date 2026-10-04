local M={}
local function dot(a,b)return a.X*b.X+a.Y*b.Y+a.Z*b.Z end
local function cross(a,b)return {X=a.Y*b.Z-a.Z*b.Y,Y=a.Z*b.X-a.X*b.Z,Z=a.X*b.Y-a.Y*b.X}end
local function unit(a)local n=math.sqrt(dot(a,a));assert(n>1e-9,'Degenerate camera basis');return {X=a.X/n,Y=a.Y/n,Z=a.Z/n}end
local function rotate(v,axis,angle)
 local c,s=math.cos(angle),math.sin(angle);local w=cross(axis,v);local d=dot(axis,v)*(1-c)
 return {X=v.X*c+w.X*s+axis.X*d,Y=v.Y*c+w.Y*s+axis.Y*d,Z=v.Z*c+w.Z*s+axis.Z*d}
end
function M.step(forward,up,target,dt,follow_rate,level_rate)
 forward=unit(forward);target=unit(target)
 up=unit(cross(forward,unit(cross(up,forward))))
 local axis=cross(forward,target);local sine=math.sqrt(dot(axis,axis));local cosine=math.max(-1,math.min(1,dot(forward,target)))
 if sine>1e-9 or cosine<0 then
  axis=sine>1e-9 and unit(axis) or up
  local angle=math.atan(sine,cosine)*(1-math.exp(-(follow_rate or 8)*dt))
  forward=unit(rotate(forward,axis,angle));up=unit(rotate(up,axis,angle))
 end
 -- Parallel transport first, then independently return toward the horizon.
 -- Smoothly fade horizon correction near vertical to avoid pole flips.
 local fade=math.max(0,math.min(1,(0.95-math.abs(forward.Z))/0.10));fade=fade*fade*(3-2*fade)
 if fade>0 and (level_rate or 3)>0 then
  local desired=unit({X=-forward.X*forward.Z,Y=-forward.Y*forward.Z,Z=1-forward.Z*forward.Z})
  local error=math.atan(dot(forward,cross(up,desired)),math.max(-1,math.min(1,dot(up,desired))))
  up=rotate(up,forward,error*(1-math.exp(-(level_rate or 3)*fade*dt)))
 end
 up=unit(cross(forward,unit(cross(up,forward))))
 return forward,up
end
return M
