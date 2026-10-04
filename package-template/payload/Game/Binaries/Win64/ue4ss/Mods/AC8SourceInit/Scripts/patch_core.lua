-- Candidate core only. Not installed. Caller must run at a verified game-thread
-- initialization boundary, with the exact player DataTable for the pinned build.
-- All persistent state is scalar; row/UObject references stay inside apply().
local M={}
local function equal(a,b)
 if type(a)~=type(b)then return false end
 if type(a)=='number'then return math.abs(a-b)<=math.max(1e-5,math.max(math.abs(a),math.abs(b))*1e-7)end
 return a==b
end
function M.apply(dataTable,spec)
 local ok,result=pcall(function()
  assert(dataTable and dataTable:IsValid(),'DataTable unavailable')
  local rows,operations={},{}
  -- Preflight every row/field before the first write. Unknown baselines abort.
  for _,item in ipairs(spec)do
   local row=rows[item.row]
   if not row then row=dataTable:FindRow(item.row);assert(row,'Missing row '..item.row);rows[item.row]=row end
   assert(type(item.before)=='number'or type(item.before)=='boolean','Unsupported scalar specification')
   assert(type(item.after)==type(item.before),'Type-changing specification')
   local current=row[item.field]
   assert(equal(current,item.before)or equal(current,item.after),'Unexpected value '..item.row..'.'..item.field)
   if not equal(current,item.after)then
    operations[#operations+1]={row=item.row,field=item.field,before=current,after=item.after}
   end
  end
  local attempted={}
  local wrote,writeError=pcall(function()
   for _,op in ipairs(operations)do
    -- Include the setter being attempted: it may mutate before reporting error.
    attempted[#attempted+1]=op
    rows[op.row][op.field]=op.after
    assert(equal(rows[op.row][op.field],op.after),'Readback mismatch '..op.row..'.'..op.field)
   end
  end)
  if not wrote then
   local failures={}
   for i=#attempted,1,-1 do
    local op=attempted[i]
    local restored,err=pcall(function()
     local current=rows[op.row][op.field]
     assert(equal(current,op.before)or equal(current,op.after),'Unexpected concurrent value')
     if not equal(current,op.before)then rows[op.row][op.field]=op.before end
     assert(equal(rows[op.row][op.field],op.before),'Rollback readback failed')
    end)
    if not restored then failures[#failures+1]=op.row..'.'..op.field..': '..tostring(err)end
   end
   return {ok=false,error=tostring(writeError),rollbackErrors=failures,attempted=#attempted}
  end
  return {ok=true,written=#operations,verified=#spec,changes=operations}
 end)
 if not ok then return {ok=false,error=tostring(result),attempted=0,rollbackErrors={}}end
 return result
end
return M
