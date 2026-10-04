-- Player MSL cosmetics only. No class swaps, CDO changes, performance writes,
-- global actor enumeration or persistent UObject handles. A bounded reference
-- check watches only the local mounted MSL cache; unchanged meshes incur no writes.
local rules=require('msl_visual_rules')
local retention=require('source_retention')
local M={}
local started,ready=false,false
local epoch,budget=0,120
local queuedCount,ownedCount=0,0
local blueprintHook=false
local defer
local mountedWatch,repairSerial=nil,0
local prepared,queued,owned,reported={},{},{},{}
local function read(fn)local ok,v=pcall(fn);if ok then return v end end
local function valid(o)return o and read(function()return o:IsValid()end)==true end
local function path(o)
 if not valid(o)then return nil end
 local n=read(function()return o:GetFullName()end)
 return type(n)=='string'and n:match('^[^ ]+ (.+)$')or nil
end
local function leaf(o)
 -- Invalid Unreal wrappers are still Lua userdata. GetClass on a null native
 -- object faults in UE4SS before Lua pcall can catch anything (UE4SS+2ace67).
 if not valid(o)then return nil end
 local c=read(function()return o:GetClass()end)
 return valid(c)and read(function()return c:GetFName():ToString()end)or nil
end
local function same(a,b)return valid(a)and valid(b)and a:GetAddress()==b:GetAddress()end
local function log(key,s)
 if budget<=0 or reported[key]then return end
 reported[key]=true;budget=budget-1;print('[AC8MSLVisual] epoch='..epoch..' '..s..'\n')
end
local function count(a,limit)
 local n=read(function()return #a end)
 if type(n)=='number'and n>=0 and n<=limit then return n end
end
local function localPawn(o)
 local w=read(function()return o:GetWorld()end);if not valid(w)then return end
 local gi=read(function()return w.OwningGameInstance end);if not valid(gi)then return end
 local ps=read(function()return gi.LocalPlayers end);if count(ps,4)~=1 then return end
 local pc=read(function()return ps[1].PlayerController end);if not valid(pc)then return end
 local p=read(function()return pc.Pawn end);if valid(p)then return p,pc end
end
function M.prepare()
 ready=false
 local holder=retention.acquire();local nextPaths={}
 for key,assetPath in pairs(rules.models)do
  local mesh=LoadAsset(assetPath)
  assert(valid(mesh)and leaf(mesh)=='StaticMesh','Visual asset unavailable: '..key)
  holder.hold(mesh);nextPaths[key]=assert(path(mesh))
 end
 prepared=nextPaths;ready=true
 if not blueprintHook then
  local hook='/Game/Blueprints/Weapons/MSL/Player/Msl/BP_plwp_msl_a0.BP_plwp_msl_a0_C:ReceiveBeginPlay'
  local ok,err=pcall(function()RegisterHook(hook,function(p)
   local o=read(function()return p:get()end)
   if valid(o)then
    local applied,why=pcall(M.weapon,o,'blueprint-begin-play')
    if not applied then log('blueprint-error','ERROR blueprint-begin-play '..tostring(why))end
    defer(o,'weapon')
   end
  end)end)
  blueprintHook=ok
  log('blueprint-hook','HOOK MSL ReceiveBeginPlay registered='..tostring(ok)..(ok and ''or(' '..tostring(err))))
 end
 log('ready','READY models=original,qaam,sasm; class/performance fields unchanged')
end
local function policy(pawn,weaponLeaf)
 local rule=rules.planes[leaf(pawn)]
 if not rule or rule.variant~=weaponLeaf then return end
 if weaponLeaf~='BP_plwp_msl_a0_C'and weaponLeaf~='BP_plwp_msl_a1_C'then return end
 return rule.model
end
local function swap(component,model,reason,pawnName)
 if not valid(component)then return false end
 local cls=StaticFindObject('/Script/Engine.StaticMeshComponent')
 if not valid(cls)or not component:IsA(cls)then return false end
 local cp=path(component);if not cp then return false end
 local address=component:GetAddress()
 local current=component.StaticMesh;local currentPath=path(current)
 if not currentPath then return false end
 local prior=owned[cp]
 if prior and prior.address~=address then prior=nil;owned[cp]=nil;ownedCount=math.max(0,ownedCount-1)end
 -- Excluded aircraft are untouched, except restoration of our own reused mesh.
 if model=='original'and not prior then return false end
 if currentPath~=prepared.original and currentPath~=prepared.qaam and not(prior and currentPath==prepared.sasm)then
  log('foreign:'..currentPath,'SKIP foreign mesh='..currentPath);return false
 end
 local desiredPath=model=='original'and prior.original or prepared[model]
 if currentPath==desiredPath then
  log('verified:'..pawnName..':'..reason..':'..model,'VERIFIED plane='..pawnName..' route='..reason..' model='..model..' mesh='..desiredPath)
  return false
 end
 if not prior and ownedCount>=4096 then log('limit','SKIP visual ownership limit');return false end
 local mesh=StaticFindObject(desiredPath);if not valid(mesh)then return false end
 -- Only visual components. A collidable mesh could alter collision geometry.
 if component:GetCollisionEnabled()~=0 then
  log('collision:'..cp,'SKIP collision-enabled component='..cp);return false
 end
 local mats=mesh.StaticMaterials;local n=count(mats,8)
 local oldCount=component:GetNumMaterials()
 if not n or n<1 or type(oldCount)~='number'or oldCount<0 or oldCount>n then
  log('materials:'..cp,'SKIP unsupported material layout component='..cp);return false
 end
 local nextMats,oldMats={},{}
 for i=1,n do nextMats[i]=mats[i].MaterialInterface;assert(valid(nextMats[i]),'Target material unavailable')end
 for i=1,oldCount do oldMats[i]=component:GetMaterial(i-1);assert(valid(oldMats[i]),'Original material unavailable')end
 local ok,err=pcall(function()
  component:SetStaticMesh(mesh)
  assert(same(component.StaticMesh,mesh),'Mesh readback mismatch')
  for i=1,n do
   component:SetMaterial(i-1,nextMats[i])
   assert(same(component:GetMaterial(i-1),nextMats[i]),'Material readback mismatch')
  end
  assert(component:GetCollisionEnabled()==0,'Collision mode changed')
 end)
 if not ok then
  local back,backErr=pcall(function()
   component:SetStaticMesh(current)
   for i=1,oldCount do component:SetMaterial(i-1,oldMats[i])end
   assert(same(component.StaticMesh,current),'Restore readback mismatch')
  end)
  error('Mesh update failed: '..tostring(err)..'; rollback='..tostring(back)..' '..tostring(backErr))
 end
 if model=='original'then owned[cp]=nil;ownedCount=math.max(0,ownedCount-1)
 else
  if not prior then ownedCount=ownedCount+1 end
  owned[cp]={address=address,original=prior and prior.original or currentPath}
 end
 log('apply:'..pawnName..':'..reason..':'..model,'APPLIED plane='..pawnName..' route='..reason..' model='..model..' mesh='..desiredPath)
 return true
end
function M.weapon(o,reason)
 if not ready or not valid(o)then return end
 local wl=leaf(o)
 if wl~='BP_plwp_msl_a0_C'and wl~='BP_plwp_msl_a1_C'then return end
 log('weapon-seen:'..reason,'WEAPON_SEEN class='..wl..' event='..reason)
 local owner=o.OwningGameObject
 if not valid(owner)or not same(owner,localPawn(o))then
  log('owner:'..reason,'SKIP owner-not-local class='..wl..' ownerClass='..tostring(leaf(owner))..' event='..reason)
  return
 end
 local model=policy(owner,wl);if not model then return end
 swap(o.StaticMesh,model,reason,leaf(owner))
 -- Some layouts use a separate visual subcomponent for loaded missiles.
 local hidden=read(function()return o.HiddenMissileMesh end)
 if valid(hidden)and not same(hidden,o.StaticMesh)then swap(hidden,model,reason..'-hidden',leaf(owner))end
end
function M.refresh(pawn,reason)
 if not ready or not valid(pawn)then return end
 local actual,controller=localPawn(pawn)
 if not same(pawn,actual)then return end
 local rule=rules.planes[leaf(pawn)];if not rule then return end
 log('rule:'..leaf(pawn),'RULE plane='..leaf(pawn)..' variant='..rule.variant..' model='..rule.model)
 if not policy(pawn,rule.variant)then return end
 local act=pawn.PlayerWeaponActivator;if not valid(act)then return end
 for _,field in ipairs({'SpawnedWeapons','ActiveWeapons'})do
  local arr=read(function()return act[field]end);local n=count(arr,512)
  log('array:'..leaf(pawn)..':'..reason..':'..field,'EQUIPMENT plane='..leaf(pawn)..' route='..reason..' '..field..'='..tostring(n))
  if n then for i=1,n do M.weapon(arr[i],reason..'-'..field)end end
 end
 -- LiveWeaponMeshManager owns loaded/displayed missile meshes separately from
 -- the aircraft's SWP/pylon attachment tree. The cache element type is MeshComponent.
 local manager=read(function()return act.WeaponManager end)
 local meshManager=valid(manager)and read(function()return manager.WeaponMeshManager end)
 local cache=valid(meshManager)and read(function()return meshManager.WeaponMeshCache end)
 local cacheCount=count(cache,512)
 log('cache:'..leaf(pawn)..':'..reason..':'..tostring(cacheCount),'MESH_CACHE plane='..leaf(pawn)..' route='..reason..' count='..tostring(cacheCount))
 if cacheCount then
  local changes=0
  for i=1,cacheCount do
   if swap(cache[i],rule.model,reason..'-weapon-mesh-cache',leaf(pawn))then changes=changes+1 end
  end
  log('cache-result:'..leaf(pawn)..':'..reason..':'..tostring(cacheCount)..':'..changes,'CACHE_RESULT count='..cacheCount..' changed='..changes)
 end
 -- Keep paths/addresses only. Do not retain UObject wrappers across a checkpoint.
 -- Index identity catches rebuilt caches even when the pawn itself is reused.
 if rule.model~='original'and cacheCount and valid(meshManager)then
  local nextWatch={controllerPath=path(controller),controllerAddress=controller:GetAddress(),pawnAddress=pawn:GetAddress(),
   managerAddress=meshManager:GetAddress(),cacheCount=cacheCount,slots={}}
  for i=1,cacheCount do
   local c=cache[i]
   if valid(c)then
    local current=read(function()return c.StaticMesh end)
    if path(current)==prepared[rule.model]and read(function()return c:GetCollisionEnabled()end)==0 then
     if #nextWatch.slots<16 then nextWatch.slots[#nextWatch.slots+1]={index=i,address=c:GetAddress(),meshAddress=current:GetAddress()}end
    end
   end
  end
  if nextWatch.controllerPath then mountedWatch=nextWatch end
 elseif rule.model=='original'then mountedWatch=nil end
 -- Attached weapon meshes only; do not walk airframe/cockpit or pylon geometry.
 local root=read(function()return pawn.WeaponMeshBase end)
 if not valid(root)then return end
 local queue={root};local seen={};local index=1
 while index<=#queue and index<=128 do
  local node=queue[index];index=index+1
  if valid(node)and not seen[node:GetAddress()]then
   seen[node:GetAddress()]=true
   swap(node,rule.model,reason..'-attached',leaf(pawn))
   local children=read(function()return node.AttachChildren end);local n=count(children,128)
   if n then for i=1,n do if #queue<128 then queue[#queue+1]=children[i]end end end
  end
 end
end
function M.checkMounted()
 local watch=mountedWatch
 if not ready or not watch then return end
 local pc=StaticFindObject(watch.controllerPath)
 if not valid(pc)or pc:GetAddress()~=watch.controllerAddress then mountedWatch=nil;return end
 local pawn=read(function()return pc.Pawn end)
 if not valid(pawn)then return end
 local why
 if pawn:GetAddress()~=watch.pawnAddress then why='pawn-changed'
 else
  local act=read(function()return pawn.PlayerWeaponActivator end)
  local manager=valid(act)and read(function()return act.WeaponManager end)
  local mm=valid(manager)and read(function()return manager.WeaponMeshManager end)
  if not valid(mm)then return end
  local cache=read(function()return mm.WeaponMeshCache end);local n=count(cache,512)
  if not n then return end
  if mm:GetAddress()~=watch.managerAddress or n~=watch.cacheCount then why='cache-rebuilt'
  else
   for _,slot in ipairs(watch.slots)do
    local c=cache[slot.index]
    if not valid(c)or c:GetAddress()~=slot.address then why='component-replaced';break end
    local mesh=read(function()return c.StaticMesh end)
    if not valid(mesh)or mesh:GetAddress()~=slot.meshAddress then why='mesh-overwritten';break end
   end
  end
 end
 if why then
  mountedWatch=nil;repairSerial=repairSerial+1
  log('repair:'..repairSerial,'MOUNTED_REPAIR reason='..why..' serial='..repairSerial)
  M.refresh(pawn,'mounted-repair-'..repairSerial)
  if why=='pawn-changed'then defer(pawn,'plane')end
 end
end
local function safe(key,fn)
 local ok,err=pcall(fn);if not ok then log('error:'..key,'ERROR '..key..' '..tostring(err))end
end
local function context(p)return read(function()return p:get()end)end
defer=function(o,kind)
 local p=path(o);if not p then return end
 local addr=o:GetAddress();local key=p..'@'..addr
 if queued[key]then return end
 -- A fixed number of initialization callbacks; never reschedules itself.
 if queuedCount>=4096 then return end
 queued[key]=true;queuedCount=queuedCount+1;local generation=epoch
 local schedule=kind=='plane'and{2,30,120,300}or{2,10}
 for _,frames in ipairs(schedule)do
  ExecuteInGameThreadAfterFrames(frames,function()
   if generation~=epoch then return end
   if frames==schedule[#schedule]and queued[key]then queued[key]=nil;queuedCount=math.max(0,queuedCount-1)end
   local current=StaticFindObject(p)
   if not valid(current)or current:GetAddress()~=addr then return end
   safe('deferred-'..kind,function()
    if kind=='plane'then M.refresh(current,'equipment-ready')else M.weapon(current,'spawn-ready')end
   end)
  end)
 end
end
function M.start()
 if started then return end;started=true
 RegisterLoadMapPreHook(function()
  epoch=epoch+1;budget=120;ready=false;prepared={};queued={};owned={};reported={};queuedCount=0;ownedCount=0
  mountedWatch=nil;repairSerial=0
 end)
 RegisterBeginPlayPostHook(function(p)
  local o=context(p);if not valid(o)then return end
  local name=leaf(o)
  if rules.planes[name]then defer(o,'plane')
  elseif name=='BP_plwp_msl_a0_C'or name=='BP_plwp_msl_a1_C'then
   safe('begin-weapon',function()M.weapon(o,'begin-play')end);defer(o,'weapon')
  end
 end)
 for _,event in ipairs({'OnInitialized','OnCreateWeapon_BP'})do
  local hook='/Script/Live.LiveWeaponBase:'..event
  local ok,err=pcall(function()RegisterHook(hook,function()end,function(p)
   safe(event,function()M.weapon(context(p),event)end)
  end)end)
  log('hook:'..event,'HOOK '..event..' registered='..tostring(ok)..(ok and ''or(' '..tostring(err))))
 end
 local ok,err=pcall(function()
  local function fire(p)
   local a=context(p);if valid(a)then safe('fire',function()M.refresh(a.CachedPlayerPlane,'fire')end)end
  end
  RegisterHook('/Script/Live.LivePlayerWeaponActivator:MainWeaponPressed',fire,fire)
 end)
 log('hook:fire','HOOK MainWeaponPressed registered='..tostring(ok)..(ok and ''or(' '..tostring(err))))
 -- The custom game-thread loop is already provided by this integration. At
 -- 60-frame intervals inspect at most16 known slot identities, not the world.
 if type(LoopInGameThreadAfterFrames)=='function'then
  LoopInGameThreadAfterFrames(60,function()safe('mounted-watch',M.checkMounted)end)
  log('mounted-watch','WATCH local mounted slots interval=60frames max_slots=16; writes only after drift')
 else log('mounted-watch-missing','WATCH unavailable; event-based refresh only')end
end
return M
