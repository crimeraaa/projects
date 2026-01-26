local x = "-3.14e+6"
if true then
    local n = tonumber(x)
    if n then
        print("number", n)
    else
        print("string", x)
    end
elseif _G[x] then
    local id = "_G[\"" .. x .. "\"]"
    print(id, _G[x])
else
    print(x, "nope")
end

-- print("radda radda")
