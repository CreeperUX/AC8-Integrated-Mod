-- Optional acceptance observer. No keys, polling, or gameplay writes.
local M={}
local epoch,budget=0,250
local pending,perClass={},{}
local started,blueprintHook=false,false
local function read(fn)local ok,v=pcall(fn);if ok then return v end end
local function valid(o)return o and read(function()return o:IsValid()end)==true end
local function name(o)return valid(o)and read(function()return o:GetFullName()end)or nil end
local function log(s)
 if budget<=0 then return end
 budget=budget-1;print('[AC8Acceptance] epoch='..epoch..' '..s..'\n')
end
local function count(v)
 local n=read(function()return #v end)
 return type(n)=='number'and n>=0 and n<=256 and n or nil
end
local function values(o,fields)
 if not valid(o)then return 'unavailable' end
 local result={}
 for _,field in ipairs(fields)do
  local v=read(function()return o[field]end)
  if type(v)=='number'or type(v)=='boolean'then result[#result+1]=field..'='..tostring(v)end
 end
 return table.concat(result,' ')
end
local fields={'HomingForesightAmount','MaxRotationAngle','MaxHomingAngle','SpeedMaximum',
 'Acceleration','LifeTime','Damage','LoadTime','MaxLoadedCount','LockonRange','LockonAngle',
 'MultipleLockOnMaxNum','bFireAllInOneShot','ProximityFuseRadiusSquared','bShouldDetonateOnMiss',
 'ExplosionRadius','AreaOfEffectDamage','DistanceFromTargetSquared'}
local function context(p)return read(function()return p:get()end)end
local function equipment(pawn,reason)
 if not valid(pawn)then return end
 local world=read(function()return pawn:GetWorld()end)
 if not valid(world)then return end
 local instance=read(function()return world.OwningGameInstance end)
 if not valid(instance)then return end
 local players=read(function()return instance.LocalPlayers end)
 local n=count(players);if not n or n<1 or n>4 then return end
 local player=read(function()return players[1]end)
 if not valid(player)then return end
 local controller=read(function()return player.PlayerController end)
 if not valid(controller)then return end
 local localPawn=read(function()return controller.Pawn end)
 if name(localPawn)~=name(pawn)then return end
 local act=read(function()return pawn.PlayerWeaponActivator end)
 if not valid(act)then return end
 local eq=read(function()return act.EquippedWeapons end)
 if not valid(eq)then return end
 local configs=read(function()return eq.Configs end)
 local total=count(configs);if not total or total>16 then return end
 log('EQUIPMENT '..reason..' pawn='..tostring(name(pawn))..' configs='..total)
 for i=1,total do
  local cfg=read(function()return configs[i]end)
  if cfg then
   local cdo=read(function()return cfg.WeaponClassDefaultObject end)
   local slots=count(read(function()return cfg.WeaponSlots end))
   local sockets=count(read(function()return cfg.WeaponSockets end))
   log('CONFIG index='..i..' source='..tostring(name(cdo))..' slots='..tostring(slots)..' sockets='..tostring(sockets)..' '..values(cdo,fields))
  end
 end
 local selection=read(function()return pawn.TargetSelectionComponent end)
 log('SELECTION '..values(selection,{'TargetDistanceLimit','TargetRangeLimitInMeter','bInWideMode'}))
end
local function observeWeapon(phase,p)
 local o=context(p);if not valid(o)then return end
 local cls=read(function()return o:GetClass()end);if not valid(cls)then return end
 local leaf=read(function()return cls:GetFName():ToString()end)
 if type(leaf)~='string'or not leaf:lower():match('^bp_plwp_')then return end
 -- At most six samples per class/phase/world; no unbounded runtime log growth.
 local key=leaf..':'..phase
 if(perClass[key]or 0)>=6 then return end
 perClass[key]=(perClass[key]or 0)+1
 local target=read(function()return o.TargetToHome end)
 log('WEAPON phase='..phase..' object='..tostring(name(o))..' target='..tostring(name(target))..' '..values(o,fields))
end
local function hook(path,phase)
 local ok,err=pcall(function()RegisterHook(path,function(p)observeWeapon(phase,p)end)end)
 log('HOOK '..phase..' registered='..tostring(ok)..(ok and ''or(' '..tostring(err))))
 return ok
end
function M.start()
 if started then return end;started=true
 RegisterLoadMapPreHook(function()epoch=epoch+1;budget=250;pending={};perClass={}end)
 RegisterBeginPlayPostHook(function(p)
  local pawn=context(p);local full=name(pawn)
  if type(full)~='string'or not full:find('/Game/Maps/Ingame/',1,true)then return end
  local cls=read(function()return pawn:GetClass()end);if not valid(cls)then return end
  local leaf=read(function()return cls:GetFName():ToString()end)
  if type(leaf)~='string'or not leaf:match('^BP_PlayerPlane_')then return end
  local path=full:match('^[^ ]+ (.+)$');if not path or pending[path]then return end
  pending[path]=true;local generation=epoch
  -- A single delayed callback, storing only the canonical path and generation.
  ExecuteInGameThreadAfterFrames(2,function()
   if generation~=epoch then return end
   local current=read(function()return StaticFindObject(path)end)
   equipment(current,'post-begin-play')
  end)
 end)
 hook('/Script/Live.LiveWeaponBase:OnImpact','impact')
 hook('/Script/Live.LiveWeaponBase:OnWeaponDestroyed','destroy')
end
function M.installBlueprintHook()
 if blueprintHook then return end
 -- The missile base Blueprint overrides this event. Hooking only the native
 -- declaration does not establish coverage of the Blueprint implementation.
 blueprintHook=hook('/Game/Blueprints/Weapons/MSL/BP_wp_msl_BaseClass.BP_wp_msl_BaseClass_C:OnTargetWithinProximity','proximity-blueprint')
end
return M
