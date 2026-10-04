local source=require('source_init')
local config=require('config')
local visuals
if config.msl_visuals then
 local ok,module=pcall(require,'msl_visuals')
 if ok then
  local started,err=pcall(module.start)
  if started then visuals=module else print('[AC8MSLVisual] START_FAILED '..tostring(err)..'\n')end
 else print('[AC8MSLVisual] LOAD_FAILED '..tostring(module)..'\n')end
end
local observer
if config.acceptance_recording then
 local ok,module=pcall(require,'acceptance_observer')
 if ok then
  local started,err=pcall(module.start)
  if started then observer=module else print('[AC8Acceptance] OBSERVER_FAILED '..tostring(err)..'\n')end
 else print('[AC8Acceptance] OBSERVER_FAILED '..tostring(module)..'\n')end
end
local ready,applying,disabled=false,false,false
local function log(s)print('[AC8SourceInit] '..s..'\n')end
local function applyAt(reason)
 if applying or disabled then return end
 applying=true
 local result=source.apply(false)
 applying=false
 if not result.ok then
  disabled=true
  log('SOURCE_FAILED reason='..reason..' '..tostring(result.error)..' rollbackErrors='..tostring(#(result.rollbackErrors or{})))
  return
 end
 log('SOURCE_APPLIED mode='..(config.missile_mode or 'full')..' reason='..reason..' verified='..result.verified..' written='..result.written)
 if visuals then
  local ok,err=pcall(visuals.prepare)
  if not ok then print('[AC8MSLVisual] PREPARE_FAILED '..tostring(err)..'\n')end
 end
 if observer then
  local ok,err=pcall(observer.installBlueprintHook)
  if not ok then log('OBSERVER_HOOK_FAILED '..tostring(err))end
 end
end
RegisterInitGameStatePreHook(function(context)
 if not ready then return end
 local ok,name=pcall(function()
  local mode=context:get()
  if mode and mode:IsValid()then return mode:GetFullName()end
 end)
 if ok and type(name)=='string'and name:find('/Game/Maps/Ingame/',1,true)then
  applyAt('mission-initialize')
 end
end)
-- Startup's first InitGameState can precede UE4SS game-thread detection.
-- This bootstrap is one-shot; later mission initialization uses the native event.
ExecuteInGameThreadAfterFrames(2,function()
 ready=true
 applyAt('engine-ready')
end)
log('CANDIDATE loaded; source-field validation is not proof of flight/lock/fuse/salvo behavior.')
