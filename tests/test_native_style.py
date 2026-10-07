from pathlib import Path
from lupa import LuaRuntime
from PIL import Image
r=Path(__file__).resolve().parents[1];lua=LuaRuntime(unpack_returned_tuples=True)
lua.execute("""
allowed=1;projection=1;noseGood=1;freelook=0;crossOn=1;hmd=0;lines=0;reports=0;created=0;layouts=0;errors=0;loads=0
function print()end
local nextid=100
function obj(t)t=t or{};nextid=nextid+1;local id=nextid;t.IsValid=function(self)return not self.invalid end;t.GetAddress=function()return id end;t.GetFullName=function()return 'Object test.'..id end;return t end
imageClass=obj();panelClass=obj();local refs={};local gi=obj({ReferencedObjects=refs});local world=obj({OwningGameInstance=gi})
ringX=960;ringY=540;renderCalls=0;geometryHeight=0;local slate=obj({GetLocalSize=function()return {X=geometryHeight*16/9,Y=geometryHeight}end})
local rendering=obj({ImportFileAsTexture2D=function(self,hud,path)loads=loads+1;return obj({name=path:match('([%w%-]+)%.png$')})end})
function StaticFindObject(p)if p:find('CanvasPanel')then return panelClass elseif p:find('SlateBlueprintLibrary')then return obj({GetCDO=function()return slate end})elseif p:find('KismetRenderingLibrary')then return obj({GetCDO=function()return rendering end})else return imageClass end end
function LoadAsset()return obj()end
function StaticConstructObject(cls,parent)assert(cls==imageClass);created=created+1;return obj({SetBrushFromTexture=function(self,t)self.tex=t end,SetOpacity=function()end,SetVisibility=function(self,v)self.visibility=v end,SetRenderTransformAngle=function()end,SetRenderTranslation=function(self,v)renderCalls=renderCalls+1;self.translation={X=v.X,Y=v.Y}end,RemoveFromParent=function(self)self.removed=true end})end
parent=obj({children={},IsA=function(self,c)return c==panelClass end,GetPaintSpaceGeometry=function()return {}end,GetCachedGeometry=function()return {}end,IsVisible=function()return true end})
parent.GetChildrenCount=function(self)return #self.children end;parent.GetChildAt=function(self,i)return self.children[i+1]end
parent.AddChildToCanvas=function(self,image)self.children[#self.children+1]=image;image.Slot=obj({LayoutData={Anchors={Minimum={},Maximum={}},Alignment={},Offsets={}},SetAutoSize=function()end,SetZOrder=function()end,SetLayout=function()layouts=layouts+1 end});return image.Slot end
hud=obj({AlwaysVisibleCanvas=parent,GetWorld=function()return world end});controller=obj({GetHUD=function()return hud end});pawn=obj()
function package.loadlib(path,name)
 if name:find('canvas_mode')then return function()return 3 end end
 if name:find('canvas_gate')then return function()return allowed,0 end end
 if name:find('canvas_viewport')then return function()return 1920,1080,100 end end
 if name:find('canvas_ring')then return function()return projection,ringX,ringY,21,1.3,42 end end
 if name:find('canvas_nose')then return function()return noseGood,1200,540 end end
 if name:find('canvas_ui')then return function()return 0,hmd,1,1,.1,.1,1.75,1.75,62,60,0,0,0,.65,1,0,freelook,crossOn end end
 if name:find('canvas_look')then return function()return 1,700,400 end end
 if name:find('canvas_report')then return function(n)if n<0 then errors=errors+1 else reports=reports+1 end end end
 error(name)
end
""")
lua.globals().mod=lua.execute((r/'package-template/payload/Game/Binaries/Win64/ue4ss/Mods/AC8MouseAim/Scripts/native_hud.lua').read_text())
lua.execute("""
mod.start('test/');local read=function(v,k)return v[k]end
mod.update(controller,pawn,read);assert(errors==0 and reports==1 and created==5 and loads==3) -- reference cross, ring and three ticks, zero geometry allowed
-- The reference gun cross is drawn first, at the nose, independently of the ring.
local bore,ring=parent.children[1],parent.children[2]
assert(bore.tex.name=='boresight' and ring.tex.name=='ring' and bore.visibility==3 and ring.visibility==3)
local previous=layouts;mod.update(controller,pawn,read);assert(layouts==previous and loads==3)
assert(ring.PixelSnapping==1)
local boreX=bore.translation and bore.translation.X
-- Motion uses a render transform while slot layout stays fixed. No fractional
-- displacement is rounded, even when the viewport canvas is authored at 4K.
for i=1,100 do ringX=960+i*.1;mod.update(controller,pawn,read)end
assert(layouts==previous and errors==0 and renderCalls>100)
assert(math.abs(ring.translation.X-20)<.0001)
assert(ring.Slot.LayoutData.Anchors.Minimum.X==.5)
assert((bore.translation and bore.translation.X)==boreX) -- the cross stays at the nose while the ring moves
ringX=960;mod.update(controller,pawn,read)

hmd=1;mod.update(controller,pawn,read);assert(loads==4 and created==5 and ring.tex.name=='ring-hmd')
-- Keep parent ownership stable as other canvases become laid out, and prevent size
-- falling back to the 4K default after a temporary geometry gap.
assert(ring.Slot.LayoutData.Offsets.Right==120)
geometryHeight=1080;mod.update(controller,pawn,read)
assert(ring.Slot.LayoutData.Offsets.Right==60)
geometryHeight=0;mod.update(controller,pawn,read)
assert(ring.Slot.LayoutData.Offsets.Right==60)
local another=obj({IsA=function()return true end,IsVisible=function()return true end,GetPaintSpaceGeometry=function()return {}end,GetCachedGeometry=function()return {}end})
hud.NonGlowWidget=obj({WidgetTree=obj({RootWidget=another})});geometryHeight=2160
mod.update(controller,pawn,read);assert(errors==0 and created==5)
assert(ring.Slot.LayoutData.Offsets.Right==120)

allowed=2;mod.update(controller,pawn,read);for _,o in ipairs(parent.children)do assert(o.visibility==2)end
-- C free look with HMD: the bracketed ring marks the view point (target switching picks there); the mouse ring
-- returns to the plain style and still marks the flight target.
allowed=1;freelook=1;mod.update(controller,pawn,read);assert(errors==0 and created==6 and loads==4 and ring.tex.name=='ring')
local look=parent.children[6];assert(look.tex.name=='ring-hmd' and look.visibility==3 and bore.visibility==3)
freelook=0;mod.update(controller,pawn,read);assert(ring.tex.name=='ring-hmd' and look.visibility==2)
-- 2.4.1: the cross can be switched off (hud_boresight / Alt+F7); ring and ticks are unaffected, and it comes back.
crossOn=0;mod.update(controller,pawn,read);assert(errors==0 and bore.visibility==2 and ring.visibility==3)
crossOn=1;mod.update(controller,pawn,read);assert(bore.visibility==3 and ring.visibility==3)
-- Without a valid ring projection the ring and its ticks hide; the cross does not depend on the ring.
allowed=1;projection=-7;mod.update(controller,pawn,read);for _,o in ipairs(parent.children)do assert(o.visibility==(o==bore and 3 or 2))end
noseGood=0;mod.update(controller,pawn,read);for _,o in ipairs(parent.children)do assert(o.visibility==2)end;noseGood=1
projection=1;mod.update(controller,pawn,read);mod.hide(controller);for _,o in ipairs(parent.children)do assert(o.visibility==2)end
""")
a=r/'package-template/payload/Game/Binaries/Win64/ue4ss/Mods/AC8MouseAim/Scripts/ui-native'
assert {p.stem for p in a.glob('*.png')}=={'ring','ring-hmd','tick','boresight'}
for name in ['ring','ring-hmd','boresight']:
 im=Image.open(a/(name+'.png'));assert im.mode=='RGBA' and im.getpixel((64,64))[3]==0
assert len({(a/(n+'.png')).read_bytes() for n in ['ring','ring-hmd','boresight']})==3
print('PASS zero-geometry tracking, native ring/three ticks, reference gun cross independent of the ring and switchable, HMD ring at the view point in C free look, texture reuse/retention, F7/offscreen hiding and transparent assets')
