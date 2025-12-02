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


def decode_base(i: int, base: int) -> str:
    assert(0 <= base and base <= 36)

    if i == 0:
        return '0'

    s = ""
    if i < 0:
        i = abs(i)
        s += '-'
    
    if base == 2:
        s += "0b"
    elif base == 8:
        s += "0o"
    elif base == 16:
        s += "0x"
    
    # Extract each digit of the given base, from LSD to MSD.
    digits: list[str] = []
    while i > 0:
        # Convert integer to appropriate character representation
        value = i % base
        if value < 10:
            ascii = str(value)
        else:
            ascii = chr(value - 10 + ord('a'))

        digits.append(ascii)
        i //= base
    digits.reverse()

    return s + "".join(digits)
