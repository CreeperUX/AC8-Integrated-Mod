-- Screen-space ranking uses the same 500 m aim sphere and camera basis as HUD.
-- Only engine TargetCandidates are eligible; native SelectTarget keeps weapon
-- eligibility/lock timing. No actor or lock-state properties are written here.
local M={}
local rad=math.pi/180
local function finite(n)return type(n)=='number' and n==n and math.abs(n)<1e12 end
local function dot(a,b)return a[1]*b[1]+a[2]*b[2]+a[3]*b[3]end
local function sub(a,b)return {a[1]-b[1],a[2]-b[2],a[3]-b[3]}end
local function basis(p,y,r)
    p,y,r=p*rad,y*rad,r*rad
    local cp,sp,cy,sy,cr,sr=math.cos(p),math.sin(p),math.cos(y),math.sin(y),math.cos(r),math.sin(r)
    return {cp*cy,cp*sy,sp},{-sy*cr+sp*cy*sr,cy*cr+sp*sy*sr,-cp*sr},
        {-sp*cy*cr-sy*sr,-sp*sy*cr+cy*sr,cp*cr}
end
function M.choose(candidates,selected,origin,camera,rotation,aim,fov,aspect)
    for _,v in ipairs({origin,camera,rotation,aim})do for _,n in ipairs(v)do if not finite(n)then return nil end end end
    if not finite(fov)or fov<15 or fov>150 or not finite(aspect)or aspect<=0 then return nil end
    local forward,right,up=basis(table.unpack(rotation))
    local limit=math.tan(fov*rad*.5)
    local function project(pos)
        local v=sub(pos,camera);local z=dot(v,forward)
        if z<=.01 then return end
        local x,y=dot(v,right)/z,dot(v,up)/z
        if math.abs(x)>limit or math.abs(y)>limit/aspect then return end
        return x,y
    end
    local direction=basis(aim[1],aim[2],0)
    local ax,ay=project({origin[1]+direction[1]*50000,origin[2]+direction[2]*50000,origin[3]+direction[3]*50000})
    if not ax then return nil end
    local ranked,seen={},{}
    -- A radius of 12% of screen width, independent of resolution.
    local radius=limit*.24
    for _,c in ipairs(candidates)do
        if c.id and c.position and not seen[c.id] then
            seen[c.id]=true
            local valid=true;for _,n in ipairs(c.position)do if not finite(n)then valid=false end end
            if valid then
                local x,y=project(c.position)
                if x then
                    local d=(x-ax)^2+(y-ay)^2
                    if d<=radius^2 then ranked[#ranked+1]={id=c.id,d=d} end
                end
            end
        end
    end
    table.sort(ranked,function(a,b)if math.abs(a.d-b.d)<1e-9 then return a.id<b.id end return a.d<b.d end)
    if #ranked==0 then return nil end
    -- Prioritize the closest OTHER target, including after moving the mouse.
    for _,c in ipairs(ranked)do if c.id~=selected then return c.id end end
    return ranked[1].id -- a lone current target stays selected
end
function M.new(submit)
    local self={next_sample=0,pawn=0}
    function self:clear()submit(0,0,0,0);self.next_sample=0;self.pawn=0 end
    function self:update(pawn,position,camera,rotation,aim,fov,aspect,now,read)
        if self.pawn~=pawn:GetAddress() then self:clear();self.pawn=pawn:GetAddress() end
        if now>=self.next_sample-.05 and now<self.next_sample then return end
        self.next_sample=now+.033 -- <=30 Hz; native snapshots expire after 100 ms.
        submit(0,0,0,0)
        -- Reflected LivePlayerPlane property; use the current pawn's owned component.
        local component=pawn.TargetSelectionComponent
        if not component or not component:IsValid()then error('selection component unavailable')end
        local selected=component:GetSelectedTarget()
        local selected_id=selected and selected:IsValid()and selected:GetAddress()or 0
        local function xyz(v)return {assert(read(v,'X')),assert(read(v,'Y')),assert(read(v,'Z'))}end
        local list=component.TargetCandidates
        if #list>4096 then error('candidate count exceeds guard')end
        local candidates={}
        for i=1,#list do
            local target=list[i]
            if target and target:IsValid()then
                candidates[#candidates+1]={id=target:GetAddress(),position=xyz(target:K2_GetActorLocation())}
            end
        end
        local chosen=M.choose(candidates,selected_id,xyz(position),xyz(camera),rotation,aim,fov,aspect)
        submit(pawn:GetAddress(),component:GetAddress(),selected_id,chosen or 0)
    end
    return self
end
return M
