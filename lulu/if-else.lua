local x = "-3.14e+6"
local n = tonumber(x)
local s = tostring(x)
if n then
    print("number", n)
elseif s then
    print("string", s)
elseif _G[x] then
    -- print("global")
    local id = "_G[\"" .. x .. "\"]"
    print(id, _G[x])
else
    print(x, "nope")
end

-- print("radda radda")
