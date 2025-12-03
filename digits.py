from ctypes import (
    c_uint32 as DIGIT,
    c_int64 as WORD,
    sizeof,
)

DIGIT_TYPE_BITS     = sizeof(DIGIT) * 8
DIGIT_NAILS         = 2
DIGIT_BITS          = DIGIT_TYPE_BITS - DIGIT_NAILS
DIGIT_BASE          = 2**DIGIT_BITS
DIGIT_BASE_DECIMAL  = 10**9
DIGIT_MASK          = DIGIT_BASE - 1
DIGIT_MAX           = DIGIT_MASK

def count_digits(value: int, base: int = 10) -> int:
    count = 0
    if value < 0:
        value = abs(value)
    
    while value > 0:
        count += 1
        value /= base

    return count


def digit_to_ascii(digit: int) -> int:
    # binary, octal, decimal
    if 0 <= digit and digit < 10:
        return digit + ord('0')
    # dozenal (base-12), hexadecimal, base-32, base-36
    elif 10 <= digit and digit < 36:
        return digit + ord('a') - 10
    raise ValueError(f"Invalid digit {digit}")


def encode_base(value: int, base: int = 10) -> str:
    assert(2 <= base and base <= 36)
    if value == 0:
        return '0'

    sb = bytearray()
    if value < 0:
        sb += b'-'
        value = abs(value)
    
    match base:
        case 2:
            sb += b"0b"
            skip_size = 8
        case 8:
            sb += b"0o"
            skip_size = 3
        case 10:
            skip_size = 3
        case 16:
            sb += b"0x"
            skip_size = 4
        case _:
            skip_size = 0

    # Write LSD to MSD.
    skip_counter = skip_size
    offset = len(sb)
    while True:
        sb += digit_to_ascii(value % base).to_bytes()
        value //= base
        skip_counter -= 1
        
        if value == 0:
            break

        if skip_counter == 0 and skip_size > 0:
            skip_counter = skip_size
            sb += b'_'

    # Add leading zeroes for binary, octal and hexadecimal.
    if base != 10 and skip_size > 0:
        while skip_counter > 0:
            skip_counter -= 1
            sb += b'0'
            
    # sb currently reads LSD to MSD. Swap them.
    left, right = offset, len(sb) - 1
    while left < right:
        sb[left], sb[right] = sb[right], sb[left]
        left  += 1
        right -= 1

    return sb.decode()


def decode_base(value: str, base: int = 0) -> int:
    assert(base == 0 or 2 <= base and base <= 36)

    value = value.strip()
    sign = 0
    
    # Get sign
    while len(value) > 0:
        c = value[0]
        # Unary minus?
        if c == '-':
            sign = 0 if sign else 1
        # Not unary plus nor space?
        elif not c == '+' and not c.isspace():
            break
        
        value = value[1:]

    # Get base
    if len(value) > 2 and value[0] == '0':
        tmp = 0
        match value[1]:
            case 'b': tmp = 2
            case 'o': tmp = 8
            case 'd': tmp = 10
            case 'x': tmp = 16

        # Skip ^0[bBdDoOxX] otherwise just skip ^0.
        value = value[2 if tmp else 1:]

        if not base:
            base = tmp or 10
        elif tmp and base != tmp:
            raise ValueError(f"Got string base {tmp} but have input base {base}")

    # Write MSD to LSD.
    result = 0
    for c in value:
        # Don't attempt to enforce correctness here.
        if c == '_' or c == ',' or c.isspace():
            continue
        
        result *= base
        result += int(c, base)

    return -result if sign else result
    

def sub(a: int, b: int) -> tuple[int, int]:
    diff  = DIGIT(a - b).value
    carry = diff >> (DIGIT_TYPE_BITS - 1)
    diff &= DIGIT_MASK
    return diff, carry


def split_digits(value: int, base: int) -> list[int]:
    if value == 0:
        return [0]

    digits = []
    value  = abs(value)
    while value > 0:
        lsd = value % base
        value //= base
        digits.append(lsd)
    return digits

a = 1234567891011121314151617181920


def combine_digits(digits: list[int], base: int) -> int:
    value = 0
    
    # Assume little-endian, so convert to big-endian
    for digit in reversed(digits):
        value *= base
        value += digit
    
    return value


def get_base_fast(base: int) -> int:
    base_fast = base
    while base_fast * base < DIGIT_BASE:
        base_fast *= base
    return base_fast
