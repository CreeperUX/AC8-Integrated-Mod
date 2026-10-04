local original=require('source_spec')
local mode=require('config').missile_mode
if mode=='full' then return original end
assert(mode=='guidance','Unsupported missile mode')
local patches={}
for _,patch in ipairs(original.patches)do
    if patch.field=='HomingForesightAmount' then patches[#patches+1]=patch end
end
assert(#patches==32,'Guidance specification changed; validate before installation')
return {profiles=original.profiles,tablePath=original.tablePath,patches=patches}
