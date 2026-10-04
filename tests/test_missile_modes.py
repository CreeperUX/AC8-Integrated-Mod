from pathlib import Path
from lupa import LuaRuntime

scripts=Path(__file__).resolve().parents[1]/'package-template/payload/Game/Binaries/Win64/ue4ss/Mods/AC8SourceInit/Scripts'
for mode in ('guidance','full'):
    lua=LuaRuntime()
    for name in ('source_spec','selected_spec','config','patch_core','source_init'):
        lua.execute("local name,source=...;package.preload[name]=assert(load(source,'@'..name))",name,(scripts/(name+'.lua')).read_text())
    lua.globals().test_mode=mode
    lua.execute(r'''
package.preload.installation_mode=function()return test_mode end
local original=require('source_spec')
local config=require('config')
local spec=require('selected_spec')
assert(config.msl_visuals==(test_mode=='full'))
assert(#spec.patches==(test_mode=='guidance' and 32 or 375))
local values,rows,classes={}, {}, {}
local writes={}
for _,p in ipairs(original.patches)do
    if not values[p.row]then
        values[p.row]={};local key=p.row
        rows[key]=setmetatable({IsValid=function()return true end},{__index=values[key],__newindex=function(_,field,v)
            writes[#writes+1]={row=key,field=field};values[key][field]=v
        end})
    end
    values[p.row][p.field]=p.before
end
for _,p in ipairs(original.profiles)do
    local key,path=p.row,p.path
    assert(path:find('/PLAYER/',1,true))
    classes[path]={IsValid=function()return true end,IsAnyClass=function()return true end,
        GetFullName=function()return 'BlueprintGeneratedClass '..path end,GetCDO=function()return rows[key]end}
end
local tableObject={IsValid=function()return true end,
 GetRowStruct=function()return {IsValid=function()return true end,GetFullName=function()return 'ScriptStruct /Script/Live.LiveWeaponSettingsDataTable'end}end,
 FindRow=function(_,name)return rows['table:'..name]end}
assert(original.tablePath:find('/Player/',1,true))
function LoadAsset(path)if path==original.tablePath then return tableObject end;return assert(classes[path],path)end
function StaticFindObject(path)return assert(classes[path],path)end
package.preload.source_retention=function()return {acquire=function()return {hold=function()end}end}end
local source=require('source_init')
local result=source.apply(false);assert(result.ok and result.verified==#spec.patches)
for _,p in ipairs(original.patches)do
 local expected=(test_mode=='full' or p.field=='HomingForesightAmount')and p.after or p.before
 assert(values[p.row][p.field]==expected,p.row..'.'..p.field)
end
if test_mode=='guidance'then for _,w in ipairs(writes)do assert(w.field=='HomingForesightAmount')end end
assert(source.apply(false).written==0)
assert(source.apply(true).ok)
for _,p in ipairs(original.patches)do assert(values[p.row][p.field]==p.before)end
-- Execute the actual initialization entry point. Guidance must not even load
-- the visual replacement module, including later mission/checkpoint events.
visual_loads=0
package.preload.msl_visuals=function()visual_loads=visual_loads+1;return {start=function()end,prepare=function()end}end
package.preload.acceptance_observer=function()return {start=function()end,installBlueprintHook=function()end}end
function RegisterInitGameStatePreHook(fn)mission_hook=fn end
function ExecuteInGameThreadAfterFrames(n,fn)assert(n==2);bootstrap=fn end
function print()end
''')
    lua.execute((scripts/'main.lua').read_text())
    lua.execute(r'''
bootstrap()
mission_hook({get=function()return {IsValid=function()return true end,GetFullName=function()return 'Mode /Game/Maps/Ingame/Test'end}end})
assert(visual_loads==(test_mode=='full'and 1 or 0))
''')
    print(f'PASS {mode}: actual source resolver/apply/restore, field isolation, repeated initialization, visual-module loading gate.')
lua=LuaRuntime()
lua.execute("package.preload.config=assert(load(...))",(scripts/'config.lua').read_text())
lua.execute("package.preload.installation_mode=function()return 'invalid' end; assert(not pcall(require,'config'))")
print('PASS invalid runtime mode rejected.')
