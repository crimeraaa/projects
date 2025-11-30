def write_binary(digit: int, base: int) -> str:
    s = ""
    while True:
        lsd = digit % base
        c = chr(lsd + (ord('0') if lsd < 10 else ord('a') - 10))
        s += c
        digit //= base
        if digit == 0:
            break
    return s[::-1]
