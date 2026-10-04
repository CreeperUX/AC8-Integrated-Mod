-- Bounded, opt-in state discovery. Reads reflected fields only; no setters/hooks.
local M={}
local requested=false
local capture,clock,started,next_sample,pawn_address
local watched={}
local last_heartbeat=-1
local last_view_target
local watch_limit_reported=false
local function expanded(label)
    return label=='ImpactCamera' or label=='CameraViewComponent' or label=='ImpactCameraParameter'
end
local function relevant(name)
    local s=name:lower()
    return s:find('look') or s:find('focus') or s:find('cinema') or s:find('attention')
        or s:find('view') or s:find('camera') or s:find('target') or s:find('active')
        or s:find('playing') or s:find('playback') or s:find('state') or s:find('current')
        or s:find('blend') or s:find('sequence') or s:find('override')
end
local function write(s) capture:write(s,'\n') end
local function stop(reason)
    if capture then write('STOP '..reason); capture:close(); capture=nil end
    watched={}; pawn_address=nil
end
function M.request() requested=true end
local function inspect(label,obj,seen)
    if not obj or not obj:IsValid() or seen[obj:GetAddress()] then return end
    seen[obj:GetAddress()]=true
    write('OBJECT '..label..' '..obj:GetFullName())
    local klass=obj:GetClass()
    local names={}
    while klass and klass:IsValid() do
        write('CLASS '..klass:GetFullName())
        klass:ForEachFunction(function(fn)
            if expanded(label) or relevant(fn:GetFName():ToString()) then
                write('FUNCTION '..fn:GetFullName())
                fn:ForEachProperty(function(p) write('  '..p:GetFullName()) end)
            end
        end)
        klass:ForEachProperty(function(p)
            local name=p:GetFName():ToString()
            if (not relevant(name) and label~='FocusTargetComponent' and not expanded(label)) or names[name] then return end
            names[name]=true
            local full=p:GetFullName()
            write('PROPERTY '..full)
            local kind=full:match('^(%w+) ')
            if #watched<384 and (kind=='BoolProperty' or kind=='EnumProperty' or
                kind=='ByteProperty' or kind=='IntProperty' or kind=='ObjectProperty' or
                kind=='WeakObjectProperty' or kind=='ObjectPtrProperty' or kind=='NameProperty' or
                kind=='FloatProperty' or kind=='DoubleProperty') then
                watched[#watched+1]={object=obj,name=name,label=label..'.'..name,kind=kind}
            elseif #watched>=384 and not watch_limit_reported then
                watch_limit_reported=true
                write('WATCH LIMIT reached: further field schemas listed but not sampled')
            end
        end)
        klass=klass:GetSuperStruct()
    end
end
local function value(field)
    if not field.object:IsValid() then return '<owner invalid>' end
    local v=field.object[field.name]
    if v==nil then return 'nil' end
    if field.kind=='WeakObjectProperty' then v=v:Get(); if not v then return '<no object>' end end
    if field.kind:find('Object') then
        if not v:IsValid() then return '<no object>' end
        return v:GetFullName()
    end
    if type(v)=='number' and (field.kind=='FloatProperty' or field.kind=='DoubleProperty') then return string.format('%.3f',v) end
    if type(v)=='boolean' or type(v)=='number' or type(v)=='string' then return tostring(v) end
    if field.kind=='NameProperty' then return v:ToString() end
    local ok,n=pcall(function() return v:get() end)
    if ok and (type(n)=='boolean' or type(n)=='number' or type(n)=='string') then return tostring(n) end
    return '<unsupported value>'
end
function M.update(pawn,controller,directory)
    if not requested and not capture then return end
    local ok,err=pcall(function()
        if requested then
            requested=false
            if capture then stop('manual'); print('[AC8MouseAim] Gaze capture stopped.\n'); return end
            clock=StaticFindObject('/Script/Engine.Default__GameplayStatics')
            local path=directory..'gaze-state-'..os.date('%Y%m%d-%H%M%S')..'.txt'
            capture=assert(io.open(path,'a'))
            started=clock:GetRealTimeSeconds(pawn); next_sample=started
            last_heartbeat=-1; last_view_target=nil; watch_limit_reported=false
            pawn_address=pawn:GetAddress()
            write('START '..os.date('%Y-%m-%d %H:%M:%S')..' READ-ONLY 0.2.27 ImpactCamera state probe')
            local seen={}
            -- Prioritize the cinematic path before the bounded watch list fills.
            for _,name in ipairs({'ImpactCamera','CameraViewComponent'}) do
                local checked,msg=pcall(function() inspect(name,pawn[name],seen) end)
                if not checked then write('INSPECT FAILED '..name..' '..tostring(msg)) end
            end
            local checked,msg=pcall(function() inspect('ImpactCameraParameter',pawn.ImpactCamera.CameraParameter,seen) end)
            if not checked then write('INSPECT FAILED ImpactCameraParameter '..tostring(msg)) end
            inspect('plane',pawn,seen)
            inspect('controller',controller,seen)
            inspect('manager',controller.PlayerCameraManager,seen)
            for _,name in ipairs({'CameraViewComponent','CameraView','ThirdPersonCamera',
                'ThirdPersonCameraInput','FirstPersonCameraInput','CockpitCameraInput','ImpactCamera','TargetSelectionComponent'}) do
                local got,obj=pcall(function() return pawn[name] end)
                if got then
                    local checked,msg=pcall(inspect,name,obj,seen)
                    if not checked then write('INSPECT FAILED '..name..' '..tostring(msg)) end
                end
            end
            local got,focus=pcall(function() return pawn.CameraViewComponent.CachedFocusTarget end)
            if got then
                local checked,msg=pcall(inspect,'FocusTargetComponent',focus,seen)
                if not checked then write('INSPECT FAILED FocusTargetComponent '..tostring(msg)) end
            end
            capture:flush()
            print('[AC8MouseAim] Gaze capture started for up to 180 seconds; F6 stops.\n')
        end
        if not capture then return end
        if pawn:GetAddress()~=pawn_address then stop('aircraft changed'); return end
        local now=clock:GetRealTimeSeconds(pawn)
        if now-started>=180 then stop('180 second limit'); print('[AC8MouseAim] Gaze capture completed.\n'); return end
        if now<next_sample then return end
        next_sample=now+0.1
        local changed=false
        -- Known read-only getter already used by the camera implementation;
        -- other discovered functions are listed only, never invoked.
        local view_ok,view_value=pcall(function()
            local view=controller:GetViewTarget()
            return view and view:IsValid() and view:GetFullName() or '<no view target>'
        end)
        if not view_ok then view_value='<view target read failed>' end
        if view_value~=last_view_target then
            write(string.format('%.3f ACTUAL_VIEW_TARGET = %s',now-started,view_value))
            last_view_target=view_value; changed=true
        end
        if math.floor(now-started)~=last_heartbeat then
            last_heartbeat=math.floor(now-started)
            write(string.format('HEARTBEAT %.3f watched=%d',now-started,#watched))
            changed=true
        end
        for _,field in ipairs(watched) do
            if not field.failed then
                local read,result=pcall(value,field)
                if not read then field.failed=true; result='<read failed: '..tostring(result)..'>' end
                if result~=field.last then
                    write(string.format('%.3f %s = %s',now-started,field.label,result))
                    field.last=result; changed=true
                end
            end
        end
        if changed then capture:flush() end -- buffered flush, not FlushFileBuffers
    end)
    if not ok then
        pcall(stop,'error '..tostring(err))
        capture=nil; requested=false
        print('[AC8MouseAim] Gaze capture disabled after error: '..tostring(err)..'\n')
    end
end
function M.end_mission() if capture then stop('mission ended') end end
return M
