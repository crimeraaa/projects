local BigInt = require "bigint"

a = BigInt(" -1_234_567_890")
b = BigInt("-101_112_131_415")
c = a * b

print(a, "*", b, "=", c)
