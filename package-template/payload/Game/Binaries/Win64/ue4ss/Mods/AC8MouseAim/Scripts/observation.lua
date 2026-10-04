local M={}
function M.identity(raw_id,class_name)
    -- Mission 15's scripted A-6E gets a dedicated learner. This is a local
    -- model key, NOT a claimed game PlaneTypeID or the ordinary A-6E model.
    if class_name=='BP_PlayerPlane_PP0013_a06e_ms15_C' then return 2147483013 end
    if type(raw_id)=='number' and raw_id>=0 and raw_id<=2147483647 and raw_id==math.floor(raw_id) then return raw_id end
    return nil
end
return M
