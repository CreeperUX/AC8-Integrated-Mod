from pathlib import Path
from lupa import LuaRuntime
from PIL import Image
r=Path(__file__).resolve().parents[1];lua=LuaRuntime(unpack_returned_tuples=True)
lua.execute("""
allowed=1;projection=1;hmd=0;lines=0;reports=0;created=0;layouts=0;errors=0;loads=0
function print()end
local nextid=100
function obj(t)t=t or{};nextid=nextid+1;local id=nextid;t.IsValid=function(self)return not self.invalid end;t.GetAddress=function()return id end;t.GetFullName=function()return 'Object test.'..id end;return t end
imageClass=obj();panelClass=obj();local refs={};local gi=obj({ReferencedObjects=refs});local world=obj({OwningGameInstance=gi})
geometryHeight=0;local slate=obj({GetLocalSize=function()return {X=geometryHeight*16/9,Y=geometryHeight}end})
local rendering=obj({ImportFileAsTexture2D=function(self,hud,path)loads=loads+1;return obj()end})
function StaticFindObject(p)if p:find('CanvasPanel')then return panelClass elseif p:find('SlateBlueprintLibrary')then return obj({GetCDO=function()return slate end})elseif p:find('KismetRenderingLibrary')then return obj({GetCDO=function()return rendering end})else return imageClass end end
function LoadAsset()return obj()end
function StaticConstructObject(cls,parent)assert(cls==imageClass);created=created+1;return obj({SetBrushFromTexture=function(self,t)self.tex=t end,SetOpacity=function()end,SetVisibility=function(self,v)self.visibility=v end,SetRenderTransformAngle=function()end,RemoveFromParent=function(self)self.removed=true end})end
parent=obj({children={},IsA=function(self,c)return c==panelClass end,GetPaintSpaceGeometry=function()return {}end,GetCachedGeometry=function()return {}end,IsVisible=function()return true end})
parent.GetChildrenCount=function(self)return #self.children end;parent.GetChildAt=function(self,i)return self.children[i+1]end
parent.AddChildToCanvas=function(self,image)self.children[#self.children+1]=image;image.Slot=obj({LayoutData={Anchors={Minimum={},Maximum={}},Alignment={},Offsets={}},SetAutoSize=function()end,SetZOrder=function()end,SetLayout=function()layouts=layouts+1 end});return image.Slot end
hud=obj({AlwaysVisibleCanvas=parent,GetWorld=function()return world end});controller=obj({GetHUD=function()return hud end});pawn=obj()
function package.loadlib(path,name)
 if name:find('canvas_mode')then return function()return 3 end end
 if name:find('canvas_gate')then return function()return allowed,0 end end
 if name:find('canvas_viewport')then return function()return 1920,1080,100 end end
 if name:find('canvas_ring')then return function()return projection,960,540,21,1.3,42 end end
 if name:find('canvas_nose')then return function()return 1,1200,540 end end
 if name:find('canvas_ui')then return function()return 0,hmd,1,1,.1,.1,1.75,1.75,62,60,0,0,0,.65,1,0 end end
 if name:find('canvas_report')then return function(n)if n<0 then errors=errors+1 else reports=reports+1 end end end
 error(name)
end
""")
lua.globals().mod=lua.execute((r/'package-template/payload/Game/Binaries/Win64/ue4ss/Mods/AC8MouseAim/Scripts/native_hud.lua').read_text())
lua.execute("""
mod.start('test/');local read=function(v,k)return v[k]end
mod.update(controller,pawn,read);assert(errors==0 and reports==1 and created==4 and loads==2) -- ring and three ticks, zero geometry allowed
local previous=layouts;mod.update(controller,pawn,read);assert(layouts==previous and loads==2)
hmd=1;mod.update(controller,pawn,read);assert(loads==3 and created==4)
-- Keep parent ownership stable as other canvases become laid out, and prevent size
-- falling back to the 4K default after a temporary geometry gap.
assert(parent.children[1].Slot.LayoutData.Offsets.Right==120)
geometryHeight=1080;mod.update(controller,pawn,read)
assert(parent.children[1].Slot.LayoutData.Offsets.Right==60)
geometryHeight=0;mod.update(controller,pawn,read)
assert(parent.children[1].Slot.LayoutData.Offsets.Right==60)
local another=obj({IsA=function()return true end,IsVisible=function()return true end,GetPaintSpaceGeometry=function()return {}end,GetCachedGeometry=function()return {}end})
hud.NonGlowWidget=obj({WidgetTree=obj({RootWidget=another})});geometryHeight=2160
mod.update(controller,pawn,read);assert(errors==0 and created==4)
assert(parent.children[1].Slot.LayoutData.Offsets.Right==120)

allowed=2;mod.update(controller,pawn,read);for _,o in ipairs(parent.children)do assert(o.visibility==2)end
allowed=1;projection=-7;mod.update(controller,pawn,read);for _,o in ipairs(parent.children)do assert(o.visibility==2)end
projection=1;mod.update(controller,pawn,read);mod.hide(controller);for _,o in ipairs(parent.children)do assert(o.visibility==2)end
""")
a=r/'package-template/payload/Game/Binaries/Win64/ue4ss/Mods/AC8MouseAim/Scripts/ui-native'
assert {p.stem for p in a.glob('*.png')}=={'ring','ring-hmd','tick'}
for name in ['ring','ring-hmd']:
 im=Image.open(a/(name+'.png'));assert im.mode=='RGBA' and im.getpixel((64,64))[3]==0
assert (a/'ring.png').read_bytes()!=(a/'ring-hmd.png').read_bytes()
print('PASS zero-geometry tracking, native ring/three ticks, texture reuse/retention, HMD state, F7/offscreen hiding and transparent assets')
