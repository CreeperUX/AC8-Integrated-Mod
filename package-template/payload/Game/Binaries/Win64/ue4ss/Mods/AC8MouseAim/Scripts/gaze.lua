-- No key binding guesses or global object searches. Focus: 0.2.24 capture;
-- ImpactCamera activation: 0.2.27 capture at 5.801s / 12.489s.
local M={}
local owner,focus,impact,gameplay
local last_active=false
local last_time=nil
local release_at=0
local automatic_until,manual_until=0,0
local failed=false
local manual_gaze=false
local hold_threshold=0.35 -- Mod debounce; not claimed to be AC's internal threshold.
function M.reset() last_time=nil;owner=nil;focus=nil;impact=nil;last_active=false;release_at=0;automatic_until=0;manual_until=0;failed=false;manual_gaze=false end
function M.update(pawn,frame_time,manual_pressed)
    if owner~=pawn:GetAddress() then
        owner=pawn:GetAddress(); focus=nil; impact=nil; last_active=false; release_at=0; automatic_until=0; manual_until=0; failed=false; manual_gaze=false
    end
    if type(frame_time)=='number' then
        if last_time and frame_time<last_time-.1 then release_at=0;automatic_until=0;manual_until=0;manual_gaze=false end
        last_time=frame_time
    end
    if manual_pressed and type(frame_time)=='number' then manual_until=frame_time+0.25 end
    if failed then return false,false,manual_pressed==true or (type(frame_time)=='number' and frame_time<manual_until) end
    local ok,result,automatic,manual=pcall(function()
        if not focus or not focus:IsValid() then focus=pawn.CameraViewComponent.CachedFocusTarget end
        if not impact or not impact:IsValid() then impact=pawn.ImpactCamera end
        local cinematic=impact and impact:IsValid() and impact.bIsActive==true
        if not frame_time and (not gameplay or not gameplay:IsValid()) then gameplay=StaticFindObject('/Script/Engine.Default__GameplayStatics') end
        local valid_focus=focus and focus:IsValid()
        local event=valid_focus and focus.ProcessingEventFocusTarget:Get()
        -- CandidateEventFocusTarget and FocusTarget alone are NOT activation signals.
        local pressed=valid_focus and focus.bFocusInputPrevPressed==true
        local held=valid_focus and (tonumber(focus.FocusInputHoldDuration) or 0) or 0
        if not pressed then manual_gaze=false
        elseif held>=hold_threshold then manual_gaze=true end
        local forced=valid_focus and focus.bForceInput==true
        local event_active=event and event:IsValid()
        local now=frame_time or gameplay:GetRealTimeSeconds(pawn)
        if manual_gaze or manual_pressed then manual_until=now+0.25 end
        if event_active or cinematic or (forced and not manual_gaze and not manual_pressed and now>=manual_until) then automatic_until=now+0.25 end
        if manual_gaze or forced or event_active or cinematic then release_at=now+0.25 end
        local active=manual_gaze or forced or event_active or cinematic or now<release_at
        if active~=last_active then
            print(string.format('[AC8MouseAim] Camera focus %s (longHold=%s held=%.3f forced=%s event=%s cinematic=%s)\n',
                active and 'active' or 'clear',tostring(manual_gaze),held,tostring(forced),tostring(event_active==true),tostring(cinematic==true)))
            last_active=active
        end
        return active,now<automatic_until,manual_gaze or manual_pressed==true or now<manual_until
    end)
    if not ok then
        failed=true
        print('[AC8MouseAim] Gaze detection unavailable: '..tostring(result)..'; F8 remains available.\n')
        return false,false,manual_pressed==true or (type(frame_time)=='number' and frame_time<manual_until)
    end
    return result==true,automatic==true,manual==true
end
return M
