-- Offline harness for Scripts/native_hud.lua with mock UE objects and mock DLL exports (run with texlua / Lua 5.3+).
-- usage: texlua test_native_hud.lua <Scripts dir with trailing slash>
local dir=assert(arg[1],'Scripts directory')
local addr=1000
local function newaddr()addr=addr+16;return addr end
local function obj(t)t=t or {};local a=newaddr();t.IsValid=function()return true end;t.GetAddress=function()return a end;return t end
local imageClass,panelClass=obj({name='Image'}),obj({name='CanvasPanel'})
local importFail={}
local rendering=obj({ImportFileAsTexture2D=function(self,hud,path)
  local key=path:match('ui%-native/(.-)%.png$');if importFail[key]then return nil end;return obj({tex=key})end})
local slate=obj({GetLocalSize=function(self,g)return 3840,2160 end})
local classes={['/Script/UMG.Image']=imageClass,['/Script/UMG.CanvasPanel']=panelClass,
 ['/Script/UMG.SlateBlueprintLibrary']=obj({GetCDO=function()return slate end}),['/Script/Engine.KismetRenderingLibrary']=obj({GetCDO=function()return rendering end})}
function StaticFindObject(n)return classes[n]end
function LoadAsset(p)return obj({tex='stock'})end
local children={}
local panel=obj({IsA=function(self,c)return c==panelClass end,IsVisible=function()return true end,
 GetPaintSpaceGeometry=function()return {}end,GetCachedGeometry=function()return {}end,
 GetChildAt=function(self,i)return children[i+1]end,GetChildrenCount=function()return #children end,
 AddChildToCanvas=function(self,o)children[#children+1]=o;local slot=obj({SetAutoSize=function()end,SetZOrder=function()end,
   LayoutData={Anchors={Minimum={X=0,Y=0},Maximum={X=0,Y=0}},Alignment={X=0,Y=0},Offsets={Left=0,Top=0,Right=0,Bottom=0}},SetLayout=function(s,v)s.LayoutData=v end});o.Slot=slot;return slot end})
local fullname=0
function StaticConstructObject(cls,parent)
  assert(cls==imageClass);fullname=fullname+1;local name='Image_'..fullname
  return obj({GetFullName=function()return name end,vis=2,SetVisibility=function(s,v)s.vis=v end,SetBrushFromTexture=function(s,t)s.tex=t.tex end,
   SetRenderTranslation=function(s,v)s.tx=v.X;s.ty=v.Y end,SetOpacity=function()end,SetRenderTransformAngle=function()end,RemoveFromParent=function()end})
end
local owner=obj({ReferencedObjects={}})
local hud=obj({GetWorld=function()return obj({OwningGameInstance=owner})end,AlwaysVisibleCanvas=panel})
local controller=obj({GetHUD=function()return hud end})
local pawn=obj()
-- mock DLL exports, reconfigured per scenario
local S={}
local exports={
 mode=function()return 3 end,gate=function()return 1,0 end,report=function()end,viewport=function()return 1920,1080,1000 end,
 ui=function()local v={0,S.hmd,3,0,.1,.1,1.75,1.75,62,0,0,0,0,.65,1,0};if S.v17~=false then v[17]=S.freelook end;return table.unpack(v)end,
 ring=function()if S.ring then return 1,S.ring[1],S.ring[2],30,1.3,0 end;return S.ringStatus or -7 end,
 nose=function()if S.nose then return 1,S.nose[1],S.nose[2]end end,
 look=function()if S.look then return 1,S.look[1],S.look[2]end end}
local realLoadlib=package.loadlib
package.loadlib=function(path,fn)local k=fn:match('ac8_mouseaim_canvas_(.+)$');if S.noLook and k=='look'then return nil end;return exports[k]end
local function fresh()
  package.loaded['native_hud']=nil;children={};local M=dofile(dir..'native_hud.lua');M.start(dir);return M
end
local function visible()
  local v={};for _,o in ipairs(children)do if o.vis==3 then v[#v+1]=o.tex..'@'..math.floor((o.tx or 0)+.5)..','..math.floor((o.ty or 0)+.5)end end
  table.sort(v);return v
end
local function has(v,tex)for _,s in ipairs(v)do if s:find('^'..tex:gsub('%-','%%-')..'@')then return true end end;return false end
local function count(v,tex)local n=0;for _,s in ipairs(v)do if s:find('^'..tex:gsub('%-','%%-')..'@')then n=n+1 end end;return n end
local function run(M,cfg)for k in pairs(S)do S[k]=nil end;for k,v in pairs(cfg)do S[k]=v end;M.update(controller,pawn,function(t,k)return t[k]end);return visible()end
local M=fresh()
-- 1. normal flight: plain ring + reference cross + connector ticks
local v=run(M,{hmd=0,freelook=0,ring={960,540},nose={1000,600}})
assert(has(v,'ring')and not has(v,'ring-hmd')and has(v,'boresight')and count(v,'tick')>=1,table.concat(v,' '))
-- 2. HMD on: bracketed HMD ring at the cursor, cross unchanged
v=run(M,{hmd=1,freelook=0,ring={960,540},nose={960,540}})
assert(has(v,'ring-hmd')and count(v,'ring-hmd')==1 and has(v,'boresight'),table.concat(v,' '))
-- 3. HMD + C free look: cursor ring off-screen; bracket ring at the view point; cross still drawn
v=run(M,{hmd=1,freelook=1,ringStatus=-7,nose={700,500},look={960,560}})
assert(has(v,'ring-hmd')and has(v,'boresight')and not has(v,'ring'),table.concat(v,' '))
-- 3b. HMD + free look with the cursor ring visible: cursor ring is the plain ring, bracket ring at the view point
v=run(M,{hmd=1,freelook=1,ring={300,300},nose={960,540},look={960,560}})
assert(count(v,'ring-hmd')==1 and count(v,'ring')==1 and has(v,'boresight'),table.concat(v,' '))
-- 4. free look without HMD: no view marker
v=run(M,{hmd=0,freelook=1,ringStatus=-5,nose={960,540},look={960,560}})
assert(not has(v,'ring-hmd')and has(v,'boresight'),table.concat(v,' '))
-- 5. nose off-screen and ring behind the camera: nothing drawn, no error
v=run(M,{hmd=0,freelook=0,ringStatus=-5,nose={-50,540}})
assert(#v==0,table.concat(v,' '))
-- 6. nose unavailable (stale frame)
v=run(M,{hmd=0,freelook=0,ring={960,540}})
assert(has(v,'ring')and not has(v,'boresight'),table.concat(v,' '))
-- 7. older DLL: no look export and only 16 ui values -> no view marker, ring and cross still work
M=fresh();S.noLook=true
v=run(M,{hmd=1,freelook=1,v17=false,ring={960,540},nose={960,540},look={960,560},noLook=true})
assert(has(v,'ring-hmd')and has(v,'boresight'),table.concat(v,' '))
-- 8. cross texture import failure: ring still drawn, HUD not disabled
importFail['boresight']=true;M=fresh()
v=run(M,{hmd=0,freelook=0,ring={960,540},nose={1000,600}})
assert(has(v,'ring')and not has(v,'boresight'),table.concat(v,' '))
v=run(M,{hmd=0,freelook=0,ring={960,540},nose={1000,600}})
assert(has(v,'ring'),'HUD must keep running after a missing texture')
print('PASS native_hud: reference cross independent of the ring, HMD bracket ring at the cursor / view point in C free look, old-DLL and missing-texture fallbacks')
