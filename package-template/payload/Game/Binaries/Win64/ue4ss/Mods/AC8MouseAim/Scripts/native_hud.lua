local M={};local mode,gate,query,report,viewport,ui,nose,look;local directory
local imageClass,panelClass,slate,rendering;local state,failed;local textures={};local retained={};local retainedOwner
local lastStatus;local budget=80;local logged={}
local motion={layouts=0,translations=0,since=nil}
local stock='/Game/UI/Texture/System/_Cmn/T_UI_SystemIcon_Cmn_Skip_Ring.T_UI_SystemIcon_Cmn_Skip_Ring'
local function valid(o)return o and o:IsValid()end
local function log(s)print('[AC8NativeUI] '..s..'\n')end
local function once(k,s)if budget>0 and not logged[k]then logged[k]=true;budget=budget-1;log(s)end end
local function status(s)if s~=lastStatus then lastStatus=s;once(s,'STATE '..s)end end
local function parents(hud)
 local out,seen={},{}
 local function add(k,p)if valid(p)and not seen[p:GetAddress()]then seen[p:GetAddress()]=true;out[#out+1]={key=k,node=p}end end
 for _,key in ipairs({'NonGlowWidget','HudWidget'})do local ok,p=pcall(function()local w=hud[key];if valid(w)and valid(w.WidgetTree)then return w.WidgetTree.RootWidget end end);if ok then add(key..'.RootWidget',p)end end
 for _,key in ipairs({'AlwaysVisibleCanvas','NoGlowDynamicCanvas','DynamicCanvas'})do local ok,p=pcall(function()return hud[key]end);if ok then add(key,p)end end
 return out
end
local function initialize()
 if valid(imageClass)and valid(panelClass)and valid(slate)then return end
 imageClass=StaticFindObject('/Script/UMG.Image');panelClass=StaticFindObject('/Script/UMG.CanvasPanel')
 local c=StaticFindObject('/Script/UMG.SlateBlueprintLibrary');slate=valid(c)and c:GetCDO()
 c=StaticFindObject('/Script/Engine.KismetRenderingLibrary');rendering=valid(c)and c:GetCDO()
 assert(valid(imageClass)and valid(panelClass)and valid(slate),'Native UI classes unavailable')
end
local function hold(texture,hud)
 local world=hud:GetWorld();assert(valid(world),'UI world unavailable');local owner=world.OwningGameInstance;assert(valid(owner),'UI owner unavailable')
 if retainedOwner~=owner:GetAddress()then retainedOwner=owner:GetAddress();retained={}end
 local addr=texture:GetAddress();if retained[addr]then return end
 local refs=owner.ReferencedObjects;local n=#refs;assert(n<2048,'UI retention limit');refs[n+1]=texture
 assert(valid(refs[n+1])and refs[n+1]:GetAddress()==addr,'UI texture retention failed');retained[addr]=true
end
local function texture(key,hud)
 if valid(textures[key])then return textures[key]end
 local ok,value=pcall(function()if valid(rendering)then return rendering:ImportFileAsTexture2D(hud,directory..'ui-native/'..key..'.png')end end)
 if not ok or not valid(value)then
  once('texture:'..key,'STYLE_FALLBACK '..key..' '..tostring(value))
  if key~='ring'and key~='ring-hmd'then return nil end
  value=LoadAsset(stock);if not valid(value)then return nil end
 end
 hold(value,hud);textures[key]=value;return value
end
local function locate(parent,n)
 local o=parent:GetChildAt(n.index)
 if valid(o)and o:GetAddress()==n.address and o:GetFullName()==n.path then return o end
 local count=parent:GetChildrenCount();assert(count<4096,'UI child count exceeds bound')
 for i=0,count-1 do o=parent:GetChildAt(i);if valid(o)and o:GetAddress()==n.address and o:GetFullName()==n.path then n.index=i;return o end end
end
local function node(parent,key)
 local n=state.nodes[key];local o=n and locate(parent,n)
 if not valid(o)then
  o=StaticConstructObject(imageClass,parent);assert(valid(o),'Native image construction failed')
  -- Set before Slate construction; moving indicators should preserve fractions.
  local pixelOk=pcall(function()o.PixelSnapping=1;assert(o.PixelSnapping==1)end)
  once('pixel-snap','PIXEL_SNAPPING '..(pixelOk and 'Disabled' or 'unavailable; inherited'))
  local slot=parent:AddChildToCanvas(o);assert(valid(slot),'UI attachment failed')
  n={address=o:GetAddress(),path=o:GetFullName(),index=parent:GetChildrenCount()-1,slot=slot:GetAddress(),visible=2};state.nodes[key]=n
  o:SetVisibility(2);slot:SetAutoSize(false);slot:SetZOrder(key=='panel'and 11000 or key:find('toast')and 13000 or key=='ring'and 10010 or 12000)
 end
 n.touched=true;return o,n
end
local function draw(parent,hud,key,tex,ax,ay,x,y,w,h,alignx,aligny,opacity,angle)
 local resource=texture(tex,hud);if not valid(resource)then return end
 local o,n=node(parent,key)
 if n.texture~=resource:GetAddress()then o:SetBrushFromTexture(resource,false);n.texture=resource:GetAddress()end
 local tx=(ax-.5)*state.width+x;local ty=(ay-.5)*state.heightForDraw+y
 if n.renderMotion~=false and (n.tx~=tx or n.ty~=ty)then
  local ok=pcall(function()o:SetRenderTranslation({X=tx,Y=ty})end)
  if ok then n.tx=tx;n.ty=ty;n.renderMotion=true;motion.translations=motion.translations+1
  else n.renderMotion=false;once('render-motion-fallback','RENDER_TRANSLATION unavailable; using original layout movement')end
 end
 if n.renderMotion then ax=.5;ay=.5;x=0;y=0 end
 local signature=table.concat({ax,ay,x,y,w,h,alignx or .5,aligny or .5},':')
 if signature~=n.layout then
  local slot=o.Slot;assert(valid(slot)and slot:GetAddress()==n.slot,'UI slot changed');local v=slot.LayoutData
  v.Anchors.Minimum.X=ax;v.Anchors.Maximum.X=ax;v.Anchors.Minimum.Y=ay;v.Anchors.Maximum.Y=ay
  v.Alignment.X=alignx or .5;v.Alignment.Y=aligny or .5;v.Offsets.Left=x;v.Offsets.Top=y;v.Offsets.Right=w;v.Offsets.Bottom=h
  slot:SetLayout(v);n.layout=signature;motion.layouts=motion.layouts+1
 end
 opacity=opacity or 1;angle=angle or 0
 if n.opacity~=opacity then o:SetOpacity(opacity);n.opacity=opacity end
 if n.angle~=angle then o:SetRenderTransformAngle(angle);n.angle=angle end
 if n.visible~=3 then o:SetVisibility(3);n.visible=3 end
end
local function sweep(parent)
 for _,n in pairs(state.nodes)do if not n.touched and n.visible~=2 then local o=locate(parent,n);if valid(o)then o:SetVisibility(2);n.visible=2 end end end
end
local function dimensions(p,read)
 for _,method in ipairs({'GetPaintSpaceGeometry','GetCachedGeometry'})do
  local ok,w,h=pcall(function()local a,b=slate:GetLocalSize(p[method](p));if type(a)=='number'and type(b)=='number'then return a,b end;if type(a)=='table'and a.ReturnValue then a=a.ReturnValue end;return read(a,'X'),read(a,'Y')end)
  if ok and w and h and w>=320 and h>=200 then return w,h,method end
 end
end
function M.start(dir)
 directory=dir;local function api(n)return assert(package.loadlib(dir..'ac8_mouse_aim_010.dll','ac8_mouseaim_canvas_'..n))end
 mode=api('mode');gate=api('gate');query=api('ring');report=api('report');viewport=api('viewport');ui=api('ui');nose=api('nose')
 local okLook,lookApi=pcall(api,'look');look=okLook and lookApi or nil
 log('Native reticle: render-translation ring120; reference gun cross at the nose (hud_boresight, Alt+F7); HMD bracket ring (view point in C free look); frame-synchronous goal and connector; panel/notifications use on-demand GPU overlay')
end
function M.hide(controller)
 if not state or not valid(controller)then return end
 pcall(function()local hud=controller:GetHUD();if not valid(hud)then return end
  for _,entry in ipairs(parents(hud))do if entry.node:GetAddress()==state.parent then for _,n in pairs(state.nodes)do n.touched=false end;sweep(entry.node);return end end
 end)
end
function M.update(controller,pawn,read)
 if not mode or (mode()~=2 and mode()~=3) or failed then return end
 local cleanup
 local ok,err=pcall(function()
  local hud=controller:GetHUD();if not valid(hud)then return end;initialize()
  local vw,vh,now=viewport();if not vw or vw<320 or not vh or vh<200 then return end
  local parent,pw,ph,fallback,field
  local candidates=parents(hud)
  -- Keep a still-owned parent stable even when another canvas gains/loses geometry.
  if state then for _,entry in ipairs(candidates)do local p=entry.node
   if p:GetAddress()==state.parent and p:IsA(panelClass)then parent=p;field=entry.key;pw,ph=dimensions(p,read);break end
  end end
  if not parent then for _,entry in ipairs(candidates)do local p=entry.node;if p:IsA(panelClass)then
   if not fallback then fallback=p;field=entry.key end
   local w,h,kind=dimensions(p,read)
   if w and math.abs(w/h-vw/vh)<.05*vw/vh and p:IsVisible()then parent=p;pw=w;ph=h;field=entry.key;break end
  end end end
  parent=parent or fallback;if not parent then status('no-canvas');return end;cleanup=parent
  if state and state.parent~=parent:GetAddress()then
   for _,entry in ipairs(parents(hud))do if entry.node:GetAddress()==state.parent then for _,n in pairs(state.nodes)do local o=locate(entry.node,n);if valid(o)then o:RemoveFromParent()end end end end
   state=nil
  end
  if not state then state={parent=parent:GetAddress(),nodes={}};log('ATTACHED '..field)end
  for _,n in pairs(state.nodes)do n.touched=false end
  local allowed,reason=gate();if allowed~=1 then sweep(parent);status('gate '..tostring(reason));return end
  -- Game HUD is authored at4K. Normalized anchors do not require cached geometry.
  if ph then state.height=ph end
  local lh=state.height or 2160;local unit=lh/vh;state.width=pw or vw*unit;state.heightForDraw=lh;local values={ui()}
  if #values<16 then sweep(parent);status('UI bridge unavailable');return end
  local panel,hmd,control,camera,sens,startSens,zoom,startZoom,fov,hz,toast,toastValue,toastAlpha,opacity,connector,always=table.unpack(values)
  local freelook=values[17]==1
  -- Reference gun cross switch (hud_boresight / Alt+F7, 2.4.1); a DLL without the 18th value keeps it shown.
  local boresight=values[18]~=0
  local on,x,y,radius,thickness,time=query(pawn:GetAddress(),vw,vh)
  local good,nx,ny=nose(pawn:GetAddress(),vw,vh)
  -- 120 design units: 50% larger; retain valid size through temporary geometry gaps.
  local ringSize=120*lh/2160
  local function onscreen(px,py,margin)return type(px)=='number'and type(py)=='number'and px>=margin and px<=vw-margin and py>=margin and py<=vh-margin end
  -- Reference gun cross: where the nose and guns actually point, on the same 500 m sphere as the ring (the cross
  -- sits in the ring once the nose has arrived). Drawn on its own, so it stays visible in C free look and while
  -- the ring is off-screen.
  if boresight and good==1 and onscreen(nx,ny,4)then draw(parent,hud,'bore','boresight',nx/vw,ny/vh,0,0,ringSize,ringSize,.5,.5,math.max(.85,opacity))end
  -- HMD: the bracketed ring marks where the target-switch key picks; in C free look that is the view direction.
  if hmd==1 and freelook and look then
   local lk,lx,ly=look(pawn:GetAddress(),vw,vh)
   if lk==1 and onscreen(lx,ly,8)then draw(parent,hud,'look','ring-hmd',lx/vw,ly/vh,0,0,ringSize,ringSize,.5,.5,math.max(.85,opacity))end
  end
  if on==1 then
   draw(parent,hud,'ring',(hmd==1 and not freelook)and 'ring-hmd'or'ring',x/vw,y/vh,0,0,ringSize,ringSize,.5,.5,math.max(.85,opacity))
   if connector==1 and good==1 then
    local dx,dy=nx-x,ny-y;local distance=math.sqrt(dx*dx+dy*dy)
    if distance>.001 then
     local ux,uy=dx/distance,dy/distance;local rs=ringSize/unit*.5*.8125;local scale=math.max(.75,vh/1080);local spacing=math.max(10*scale,math.min(18*scale,distance*.03))
     for i=0,2 do local offset=rs+8*scale+i*spacing;local available=distance-offset-16*scale
      if available>0 then local alpha=(always==1 and 1 or math.min(1,available/(14*scale)))*(1-.16*i)
       draw(parent,hud,'tick'..i,'tick',(x+ux*offset)/vw,(y+uy*offset)/vh,0,0,9*scale*unit,3.5*scale*unit,.5,.5,alpha*.85,math.deg(math.atan(uy,ux))+90)
      end
     end
    end
   end
   report(24,time);status('tracking styled ring')
   if not motion.since then motion.since=now end
   if now-motion.since>=10000 then log('MOTION render_translation_calls='..motion.translations..' layout_calls='..motion.layouts..' (not display FPS)');motion.since=now;motion.layouts=0;motion.translations=0 end
  else status('projection '..tostring(on))end
  sweep(parent)
 end)
 if not ok then
  pcall(function()if state and valid(cleanup)and cleanup:GetAddress()==state.parent then for _,n in pairs(state.nodes)do local o=locate(cleanup,n);if valid(o)then o:RemoveFromParent()end end end end)
  failed=true;report(-1,0);log('DISABLED '..tostring(err)..'; flight unaffected')
 end
end
return M
