local x = "assert"
if tonumber(x) then
    local id = "tonumber(" .. tostring(x) .. ")"
    print(id, tonumber(x))
elseif _G[x] then
    local id = "_G[\"" .. x .. "\"]"
    print(id, _G[x])
-- else
--     print(x, "nope")
end

print("radda radda")
