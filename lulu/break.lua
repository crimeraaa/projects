local x = 1
while x < 3 do
    print("looping")
    if x % 2 == 0 then
        print("break!")
        break
    else
        print("continue")
    end
    x = x + 1
end

return x
