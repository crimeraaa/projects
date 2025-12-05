local BigInt = require "bigint"

-- local a = BigInt("123_456_789_101_112_131_415")
-- local a = BigInt "0xfeedbeef"
local a = BigInt "1_234_567_890"

print(a)
print(a:tostring(2))
print(a:tostring(8))
print(a:tostring(16))
