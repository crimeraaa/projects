local BigInt = require "bigint"

-- local a = BigInt("123_456_789_101_112_131_415")
local a = BigInt "0xfeedbeef"

print(a)
print(BigInt.tostring(a, 2))
print(BigInt.tostring(a, 8))
print(BigInt.tostring(a, 16))
