local int = require "bigint"

-- local a = int("123_456_789_101_112_131_415")
-- local a = int "0xfeedbeef"
local a = int(arg and arg[1] or "1_234_567_890")

print(a)
print(a:tostring(2))
print(a:tostring(8))
print(a:tostring(16))

print(a - 999999999)
