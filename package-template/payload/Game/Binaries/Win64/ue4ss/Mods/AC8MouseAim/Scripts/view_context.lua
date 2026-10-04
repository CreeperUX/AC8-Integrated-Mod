-- Scalar lifecycle state only; never retains engine object references.
local M={}
function M.new()
    local s={owner=nil,controller=nil,pending=true,stable=0,ready=false,probes={}}
    function s:reset(reason)
        self.pending=true;self.stable=0;self.ready=false;self.reason=reason or 'mission';self.probes={};self.last_clock=nil
    end
    function s:optional_bool(object,name,method)
        if self.probes[name]==false then return nil end
        local ok,value=pcall(function()
            if method then return object[name](object) end
            return object[name]
        end)
        if not ok or type(value)~='boolean' then self.probes[name]=false;return nil end
        self.probes[name]=true;return value
    end
    function s:update(owner,controller,eligible,dt,clock)
        if self.owner~=owner or self.controller~=controller then
            self.owner=owner;self.controller=controller;self:reset('player-acquired')
        end
        if type(clock)=='number' and clock==clock then
            if self.last_clock and clock<self.last_clock-.1 then self:reset('world-clock-reset')end
            self.last_clock=clock
        end
        if not eligible then
            if self.ready then self.reason='camera-control-return' end
            self.pending=true;self.stable=0;self.ready=false;return false,false
        end
        if self.pending then
            if type(dt)~='number' or dt~=dt or dt<=0 or dt>.1 then self.stable=0;return false,false end
            self.stable=self.stable+dt
            if self.stable<.2 then return false,false end
            self.pending=false;self.ready=true;return true,true
        end
        self.ready=true;return true,false
    end
    return s
end
return M
