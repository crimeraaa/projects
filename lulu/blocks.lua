local print = print
local x = "file"

do
    local x = "block 1"
    do local x = "block 1.1"; do local x = "block 1.1.1"; print(x); end; print(x); end
    do local x = "block 1.2"; do local x = "block 1.2.1"; print(x); end; print(x); end
    do local x = "block 1.3"; do local x = "block 1.3.1"; print(x); end; print(x); end
    print(x)
end

do
    local x = "block 2"
    do local x = "block 2.1"; do local x = "block 2.1.1"; print(x); end; print(x) end
    do local x = "block 2.2"; do local x = "block 2.2.1"; print(x); end; print(x) end
    do local x = "block 2.3"; do local x = "block 2.3.1"; print(x); end; print(x) end
    print(x)
end
print(x)
