-- local x = "-3.14e+6"
local x = true
local n = tonumber(x)
local t = type(x)
if n then
    print("number", n)
-- elseif t == "string" then
--     if _G[x] then
--         local id = "_G[\"" .. x .. "\"]"
--         print(id, _G[x])
--     else
--         print("string", x)
--     end
else
    print(t, x)
end

-- print("radda radda")
