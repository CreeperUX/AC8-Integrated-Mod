-- GameInstance owns these references so source classes survive level GC.
-- Never constructs a synthetic holder, sets raw root flags, or clears others' refs.
local M={}
local function valid(o)return o~=nil and o:IsValid()end
function M.acquire()
 local engine=FindFirstOf('GameEngine') -- once per source initialization, not a loop
 assert(valid(engine),'Engine unavailable for source ownership')
 local viewport=engine.GameViewport;assert(valid(viewport),'Viewport unavailable')
 local world=viewport.World;assert(valid(world),'World unavailable')
 local owner=world.OwningGameInstance;assert(valid(owner),'GameInstance unavailable')
 local refs=owner.ReferencedObjects
 local length=#refs;assert(length>=0 and length<=2048,'Unexpected reference array size')
 local addresses={}
 for i=1,length do
  local object=refs[i]
  if valid(object)then addresses[object:GetAddress()]=true end
 end
 local added=0
 return {
  engine=engine,
  hold=function(object)
   assert(valid(owner)and valid(object),'Invalid source owner/object')
   local address=object:GetAddress()
   if addresses[address]then return end
   local at=#refs+1;assert(at<=2048,'Source reference limit reached')
   refs[at]=object
   local stored=refs[at]
   assert(#refs==at and valid(stored)and stored:GetAddress()==address,'Engine reference readback failed')
   addresses[address]=true;added=added+1
  end,
  added=function()return added end,
 }
end
return M
