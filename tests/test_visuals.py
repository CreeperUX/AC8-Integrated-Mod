from pathlib import Path
import sys,json
from lupa import LuaRuntime
lua=LuaRuntime(unpack_returned_tuples=True)
root=Path(__file__).resolve().parents[1]/'package-template/payload/Game/Binaries/Win64/ue4ss/Mods/AC8SourceInit/Scripts'
rules=lua.execute((root/'msl_visual_rules.lua').read_text())
lua.globals().rules=rules
lua.execute(r'''
objects={};logs={};hooks={};jobs={};writes=0;counter=100;retained={}
function print(s)logs[#logs+1]=s end
function obj(p,name,data)
 data=data or{};counter=counter+1;local addr=counter
 data.IsValid=function()return not data.invalid end
 data.GetAddress=function()return addr end
 data.GetFullName=function()return name..' '..p end
 data.GetClass=function()return {IsValid=function()return true end,GetFName=function()return {ToString=function()return name end}end}end
 data.IsA=function(_,cls)return name=='StaticMeshComponent'and cls==staticClass end
 objects[p]=data;return data
end
function ctx(o)return {get=function()return o end}end
staticClass=obj('/Script/Engine.StaticMeshComponent','Class')
for k,p in pairs(rules.models)do
 local mat=obj(p..'_mat','Material')
 obj(p,'StaticMesh',{StaticMaterials={{MaterialInterface=mat}}})
end
foreignMesh=obj('foreign.mesh','StaticMesh',{StaticMaterials={{MaterialInterface=obj('foreign.mat','Material')}}})
package.preload.msl_visual_rules=function()return rules end
package.preload.source_retention=function()return {acquire=function()return {hold=function(o)retained[o:GetAddress()]=true end}end}end
function LoadAsset(p)assert(objects[p],p);return objects[p]end
function StaticFindObject(p)return objects[p]end
function RegisterLoadMapPreHook(fn)mapPre=fn end
function RegisterBeginPlayPostHook(fn)beginPlay=fn end
function RegisterHook(p,pre,post)hooks[p]={pre,post}end
function ExecuteInGameThreadAfterFrames(n,fn)jobs[#jobs+1]={frames=n,fn=fn}end
function LoopInGameThreadAfterFrames(n,fn)assert(n==60);mountedTick=fn end
function meshComponent(p,mesh)
 local c=obj(p,'StaticMeshComponent',{StaticMesh=mesh,AttachChildren={},collision=0})
 c.material=mesh.StaticMaterials[1].MaterialInterface
 c.GetCollisionEnabled=function(self)return self.collision end
 c.GetNumMaterials=function()return 1 end
 c.GetMaterial=function(self,i)assert(i==0);return self.material end
 c.SetStaticMesh=function(self,m)writes=writes+1;self.StaticMesh=m end
 c.SetMaterial=function(self,i,m)
  assert(i==0);if self.failMaterial then self.failMaterial=false;error('injected material error')end
  writes=writes+1;self.material=m
 end
 return c
end
controller=obj('controller','PlayerController')
player=obj('localPlayer','LocalPlayer',{PlayerController=controller})
instance=obj('gameInstance','GameInstance',{LocalPlayers={player}})
world=obj('world','World',{OwningGameInstance=instance})
function aircraft(name,p)
 local a=obj(p or name,name,{GetWorld=function()return world end})
 a.WeaponMeshBase=obj((p or name)..'.weapons','SceneComponent',{AttachChildren={}})
 a.PlayerWeaponActivator=obj((p or name)..'.activator','Activator',{CachedPlayerPlane=a,SpawnedWeapons={},ActiveWeapons={}})
 return a
end
function weapon(pawn,variant,p)
 local mesh=objects[variant=='BP_plwp_msl_a1_C'and rules.models.qaam or rules.models.original]
 local c=meshComponent(p..'.mesh',mesh)
 return obj(p,variant,{GetWorld=function()return world end,OwningGameObject=pawn,StaticMesh=c,
 Damage=80,LoadTime=1,LockOnTargetType=0,MaxHomingAngle=777})
end
''')
lua.globals().visual=lua.execute((root/'msl_visuals.lua').read_text())
lua.execute(r'''
visual.start();visual.prepare()
local cases=0
for name,rule in pairs(rules.planes)do
 local p=aircraft(name);controller.Pawn=p
 local w=weapon(p,rule.variant,'weapon.'..name)
 local before=w.StaticMesh.StaticMesh;local n=writes
 visual.weapon(w,'test')
 local desired=rule.model=='original'and before or objects[rules.models[rule.model]]
 assert(w.StaticMesh.StaticMesh==desired,name)
 if rule.model=='original'then assert(writes==n,'excluded modified: '..name)end
 assert(w.Damage==80 and w.LoadTime==1 and w.LockOnTargetType==0 and w.MaxHomingAngle==777)
 local after=writes;visual.weapon(w,'test');assert(writes==after,'not idempotent')
 cases=cases+1
end
assert(cases==36)
-- Real crash regression: an invalid UObject is truthy Lua userdata, not nil.
-- Type logging must not even attempt the native GetClass call on that wrapper.
local invalidClassReads=0
local badOwner={IsValid=function()return false end,GetClass=function()
 invalidClassReads=invalidClassReads+1;error('native null GetClass would crash before pcall')
end}
local unowned=weapon(nil,'BP_plwp_msl_a0_C','unowned.begin')
unowned.OwningGameObject=badOwner
local beforeInvalid=writes
hooks['/Game/Blueprints/Weapons/MSL/Player/Msl/BP_plwp_msl_a0.BP_plwp_msl_a0_C:ReceiveBeginPlay'][1](ctx(unowned))
visual.weapon(unowned,'repeat-invalid')
unowned.OwningGameObject=nil;visual.weapon(unowned,'nil-owner')
assert(invalidClassReads==0 and writes==beforeInvalid,'unsafe invalid-object method call')
jobs={}
local f=aircraft('BP_PlayerPlane_PP0025_f15e_C','test.f15');controller.Pawn=f
local w=weapon(f,'BP_plwp_msl_a0_C','pooled')
visual.weapon(w,'pool');assert(w.StaticMesh.StaticMesh==objects[rules.models.qaam])
local ty=aircraft('BP_PlayerPlane_PP0011_typn_C','test.ty');controller.Pawn=ty;w.OwningGameObject=ty
visual.weapon(w,'pool');assert(w.StaticMesh.StaticMesh==objects[rules.models.sasm])
local excluded=aircraft('BP_PlayerPlane_PP0023_f14d_C','test.f14');controller.Pawn=excluded;w.OwningGameObject=excluded
visual.weapon(w,'pool');assert(w.StaticMesh.StaticMesh==objects[rules.models.original])
controller.Pawn=f
local other=aircraft('BP_PlayerPlane_PP0025_f15e_C','other.player');local npc=weapon(other,'BP_plwp_msl_a0_C','npc');local n=writes
visual.weapon(npc,'foreign-owner');assert(writes==n)
local special=weapon(f,'BP_plwp_sasm_a0_C','special');visual.weapon(special,'special');assert(writes==n)
local unmapped=aircraft('BP_PlayerPlane_PP9999_unknown_C');controller.Pawn=unmapped
visual.weapon(weapon(unmapped,'BP_plwp_msl_a0_C','unknown'),'unknown');assert(writes==n)
controller.Pawn=f
local foreign=weapon(f,'BP_plwp_msl_a0_C','foreign');foreign.StaticMesh.StaticMesh=foreignMesh
visual.weapon(foreign,'foreign');assert(writes==n)
local collision=weapon(f,'BP_plwp_msl_a0_C','collision');collision.StaticMesh.collision=1
visual.weapon(collision,'collision');assert(writes==n)
local rollback=weapon(f,'BP_plwp_msl_a0_C','rollback');rollback.StaticMesh.failMaterial=true
local ok=pcall(function()visual.weapon(rollback,'rollback')end);assert(not ok)
assert(rollback.StaticMesh.StaticMesh==objects[rules.models.original]);assert(rollback.StaticMesh.material==objects[rules.models.original].StaticMaterials[1].MaterialInterface)
-- Bounded native equipment refresh covers pooled weapons and attached main meshes.
local pool=weapon(f,'BP_plwp_msl_a0_C','equipment')
local attached=meshComponent('mounted.msl',objects[rules.models.original])
local untouched=meshComponent('mounted.foreign',foreignMesh)
f.PlayerWeaponActivator.SpawnedWeapons={pool};f.WeaponMeshBase.AttachChildren={attached,untouched}
hooks['/Script/Live.LivePlayerWeaponActivator:MainWeaponPressed'][1](ctx(f.PlayerWeaponActivator))
assert(pool.StaticMesh.StaticMesh==objects[rules.models.qaam]);assert(attached.StaticMesh==objects[rules.models.qaam]);assert(untouched.StaticMesh==foreignMesh)
-- Fixed initialization jobs deduplicate; never follow a new world or a reused address/path.
-- Reproduce the reported failure: empty projectile arrays, only foreign/SWP
-- meshes under WeaponMeshBase, real MSL meshes solely in WeaponMeshCache.
controller.Pawn=ty
local cached=meshComponent('weapon-cache.msl',objects[rules.models.original])
local cacheSP=meshComponent('weapon-cache.sp',foreignMesh)
ty.PlayerWeaponActivator.WeaponManager=obj('manager','LiveWeaponManager',{
 WeaponMeshManager=obj('mesh-manager','LiveWeaponMeshManager',{WeaponMeshCache={cached,cacheSP}})})
ty.WeaponMeshBase.AttachChildren={untouched}
visual.refresh(ty,'cache-regression')
assert(cached.StaticMesh==objects[rules.models.sasm]);assert(cacheSP.StaticMesh==foreignMesh)
local steady=writes;visual.refresh(ty,'cache-regression');assert(writes==steady)
-- Verify the actual Blueprint override is also covered, not just native hooks.
local bp=weapon(ty,'BP_plwp_msl_a0_C','bp-hook')
hooks['/Game/Blueprints/Weapons/MSL/Player/Msl/BP_plwp_msl_a0.BP_plwp_msl_a0_C:ReceiveBeginPlay'][1](ctx(bp))
assert(bp.StaticMesh.StaticMesh==objects[rules.models.sasm]);jobs={}
controller.Pawn=f
beginPlay(ctx(f));beginPlay(ctx(f));assert(#jobs==4)
local old=jobs[1];mapPre();visual.prepare();n=writes;old.fn();assert(writes==n)
beginPlay(ctx(f));local fresh=jobs[#jobs]
objects[f:GetFullName():match('^[^ ]+ (.+)$')]=aircraft('BP_PlayerPlane_PP0025_f15e_C','test.f15')
fresh.fn();assert(writes==n)
''')
lua.execute(r'''
-- Checkpoint regression: same local pawn, same components, no BeginPlay event.
mapPre();visual.prepare();jobs={}
local p=aircraft('BP_PlayerPlane_PP0032_f18e_C','checkpoint.f18');controller.Pawn=p
local c=meshComponent('checkpoint.msl',objects[rules.models.original])
local sp=meshComponent('checkpoint.sp',foreignMesh)
local mm=obj('checkpoint.mm','LiveWeaponMeshManager',{WeaponMeshCache={c,sp,c}})
p.PlayerWeaponActivator.WeaponManager=obj('checkpoint.manager','LiveWeaponManager',{WeaponMeshManager=mm})
beginPlay(ctx(p));assert(#jobs==4)
local initial=jobs;jobs={};for _,job in ipairs(initial)do job.fn()end
assert(c.StaticMesh==objects[rules.models.qaam])
local steady=writes;for i=1,1000 do mountedTick()end;assert(writes==steady,'steady checks must never rewrite')
-- Model and materials restored by checkpoint; identities and cache size stable.
c.StaticMesh=objects[rules.models.original];c.material=c.StaticMesh.StaticMaterials[1].MaterialInterface
mountedTick();assert(c.StaticMesh==objects[rules.models.qaam]);assert(writes==steady+2)
steady=writes;mountedTick();assert(writes==steady)
-- A completed initialization schedule can rearm on the SAME path/address.
beginPlay(ctx(p));assert(#jobs==4);beginPlay(ctx(p));assert(#jobs==4)
local second=jobs;jobs={};for _,job in ipairs(second)do job.fn()end
-- Same manager/cache count but new component address.
local replacement=meshComponent('checkpoint.msl.rebuilt',objects[rules.models.original])
mm.WeaponMeshCache={replacement,sp,replacement};mountedTick()
assert(replacement.StaticMesh==objects[rules.models.qaam]and sp.StaticMesh==foreignMesh)
-- New cache manager while the aircraft instance is reused.
local c2=meshComponent('checkpoint.msl2',objects[rules.models.original])
p.PlayerWeaponActivator.WeaponManager.WeaponMeshManager=obj('checkpoint.mm2','LiveWeaponMeshManager',{WeaponMeshCache={c2}})
mountedTick();assert(c2.StaticMesh==objects[rules.models.qaam])
-- New pawn, same local controller. Find only its own cache; rules remain intact.
local ty2=aircraft('BP_PlayerPlane_PP0011_typn_C','checkpoint.typhoon');controller.Pawn=ty2
local tyMesh=meshComponent('checkpoint.ty.msl',objects[rules.models.original])
ty2.PlayerWeaponActivator.WeaponManager=obj('checkpoint.ty.manager','LiveWeaponManager',{
 WeaponMeshManager=obj('checkpoint.ty.mm','LiveWeaponMeshManager',{WeaponMeshCache={tyMesh}})})
mountedTick();assert(tyMesh.StaticMesh==objects[rules.models.sasm]);assert(c2.StaticMesh==objects[rules.models.qaam])
-- Retain the original collision and actor-lifetime guards during repair.
tyMesh.StaticMesh=objects[rules.models.original];tyMesh.collision=1;steady=writes
mountedTick();mountedTick();assert(writes==steady and tyMesh.StaticMesh==objects[rules.models.original])
mapPre();steady=writes;mountedTick();assert(writes==steady)
''')
source=(root/'msl_visuals.lua').read_text()
for forbidden in ('FindAllOf(', 'LoopAsync(', 'StaticConstructObject(', 'RegisterKeyBind(', '.MainWeaponClass=', '.Damage=', '.LoadTime=', '.LockOnTargetType='):
    assert forbidden not in source,forbidden
print('PASS 36 aircraft rules; original-group exclusions; a1 already QAAM; pool reuse QAAM -> SASM -> original; local owner, unknown/SP/foreign/collision guards; material readback/rollback; fixed stale-safe callbacks; mounted-only tree; numeric performance unchanged. Real-game event coverage is not proven by mocks.')
print('PASS checkpoint mesh overwrite, component replacement, manager replacement, new local pawn, same-address schedule rearm, 1000 unchanged guard ticks with zero writes; live checkpoint repair still needs verification.')

