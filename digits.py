from ctypes import (
    c_uint8 as u8,
    c_uint16 as u16,
    c_uint32 as u32,
    c_uint64 as u64,

    c_int8 as i8,
    c_int16 as i16,
    c_int32 as i32,
    c_int64 as i64,

    c_uint32 as DIGIT,
    c_int64 as WORD,
)

# import sys

DIGIT_NAILS         = 2
DIGIT_TYPE_BITS     = 32
DIGIT_SHIFT         = DIGIT_TYPE_BITS - DIGIT_NAILS
WORD_SHIFT          = DIGIT_SHIFT * 2
DIGIT_BASE          = 2**DIGIT_SHIFT
DIGIT_BASE_DECIMAL  = 10**9
DIGIT_MASK          = DIGIT_BASE - 1
DIGIT_MAX           = DIGIT_MASK

__RADIX_TABLE = \
    b"0123456789" \
    b"ABCDEFGHIJKLMNOPQRSTUVWXYZ" \
    b"abcdefghijklmnopqrstuvwxyz+/"

__RADIX_TABLE_REVERSE = {chr(char): value for value, char in enumerate(__RADIX_TABLE)}
__RADIX_PREFIX_TABLE = {
    'b': 2,  'B': 2,
    'o': 8,  'O': 8,
    'd': 10, 'D': 10,
    'x': 16, 'X': 16,
}

__TYPE_MAX = {
    u8:   u8(-1).value,
    u16: u16(-1).value,
    u32: u32(-1).value,
    u64: u64(-1).value,

    # Assuming two's complement.
    i8:   u8(-1).value >> 1,
    i16: u16(-1).value >> 1,
    i32: u32(-1).value >> 1,
    i64: u64(-1).value >> 1,
}

__TYPE_MIN = {
    u8:  0,
    u16: 0,
    u32: 0,
    u64: 0,

    # Assuming two's complement.
    i8:  -__TYPE_MAX[i8]  - 1,
    i16: -__TYPE_MAX[i16] - 1,
    i32: -__TYPE_MAX[i32] - 1,
    i64: -__TYPE_MAX[i64] - 1,
}

def int_max(*args) -> int:
    if len(args) == 1 and args[0] in __TYPE_MAX:
        return __TYPE_MAX[args[0]]
    else:
        return max(*args)


def int_min(*args) -> int:
    if len(args) == 1 and args[0] in __TYPE_MIN:
        return __TYPE_MIN[args[0]]
    else:
        return min(*args)


def int_place_value(value: int, base = 10) -> int:
    if value == 0:
        return 0

    place = 1

    # +-----------------------+--------------------+------------------------+
    # | Value                 | Place Value (Base) | Place Value            |
    # |-----------------------+--------------------+------------------------|
    # | 1234 = 0b100_11010010 |      b100_00000000 |  1024 = 0b100_00000000 |
    # | base =           0b10 |                    |  base =           0b10 |
    # |-----------------------+--------------------+------------------------|
    # | 1234 =        0o2_322 |            0o1_000 |   512 =  0b10_00000000 |
    # | base =           0o10 |                    |  base =         0b1000 |
    # |-----------------------+--------------------+------------------------|
    # | 1234 =          0x4d2 |              0x100 |   256 =   0b1_00000000 |
    # | base =           0x10 |                    |  base =        0b10000 |
    # |-----------------------+--------------------+------------------------|
    # | 1234 = base-32:   16I |       base-32: 100 |  1024 = 0b100_00000000 |
    # | base = base-32:    10 |                    |  base = 0b000_00100000 |
    # |-----------------------+--------------------+------------------------|
    # | 1234 = base-64:    JI |       base-64:  10 |    64 =      0b1000000 |
    # | base = base-64:    10 |                    |  base =      0b1000000 |
    # +-----------------------+--------------------+------------------------+
    if base == 2:
        place <<= value.bit_length() - 1 # 2**floor(log2(value))
    else:
        while place * base <= value:
            place *= base
    return place


def __bitfield_get(digits: list[int], bit_index: int, bit_count: int) -> int:
    if bit_count == 1:
        # `i` where `digits[i]` holds the bit at `bit_index`.
        digit_index = bit_index // DIGIT_SHIFT
        mask  = DIGIT(1 << (bit_index % DIGIT_SHIFT)).value
        digit = digits[digit_index]
        # Truncate result to exactly 1 bit.
        return 1 if (digit & mask) else 0

    bit_field = 0
    while bit_count > 0:
        bit = __bitfield_get(digits, bit_index + bit_count - 1, bit_count=1)

        bit_field <<= 1
        bit_field  |= bit
        bit_count  -= 1

    return bit_field


def int_bitfield_get(value: int|list[int], bit_index: int, bit_count = 1) -> int:
    if isinstance(value, list):
        # Empty list is equivalent to 0.
        if len(value) == 0:
            return 0

        # Ensure list items are all integers.
        assert(isinstance(value[0], int))

        digits = value
    else:
        digits = int_split_digits(value, DIGIT_BASE)

    assert(1 <= bit_count and bit_count <= WORD_SHIFT)
    return __bitfield_get(digits, bit_index, bit_count)


def int_count_digits(value: int, base = 10) -> int:
    count = 0
    if value < 0:
        value = abs(value)

    while value > 0:
        count += 1
        value //= base

    return count


def int_bin(value: int, group_size = 64, group_min = 2) -> str:
    value = __int_to_twos_complement(value, (group_min or 1) * group_size)
    return int_encode_base(value, 2, group_size, group_min)


# Not correct for signed values
def int_oct(value: int, group_size = 16, group_min = 2) -> str:
    bit_shift = 3
    bit_count = (group_min or 1) * bit_shift * group_size
    value     = __int_to_twos_complement(value, bit_count)
    return int_encode_base(value, 8, group_size, group_min)


def int_hex(value: int, group_size = 8, group_min = 4) -> str:
    bit_shift = 4
    bit_count = (group_min or 1) * bit_shift * group_size
    value     = __int_to_twos_complement(value, bit_count)
    return int_encode_base(value, 16, group_size, group_min)


# Convert to an unsigned integer representing two's complement bits.
def __int_to_twos_complement(value: int, bit_count: int):
    if value < 0:
        bit_mask = (1 << bit_count) - 1
        value   &= bit_mask
    return value


def int_encode_base(value: int, base = 10, group_size = 0, group_min = 0) -> str:
    assert(2 <= base and base <= 64)
    assert(group_size == 0 or 2 <= group_size and group_size <= 64)
    if value == 0 and group_min == 0:
        return '0'

    sb = bytearray()
    if value < 0:
        sb += b'-'
        value = abs(value)

    skip_size = 0
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

    # Write LSD to MSD.
    if group_size:
        skip_size = group_size

    skip_counter = skip_size
    offset = len(sb)
    while True:
        digit = value % base
        # Convert to uppercase if case-insensitive (hexadecimal, base-32).
        if base <= 36 and digit >= 36: # digit >= RADIX_TABLE_REVERSE['a']
            digit -= 32 # tolower(digit) = digit - ('a' - 'A')

        sb += __RADIX_TABLE[digit].to_bytes()
        value //= base
        skip_counter -= 1

        if value == 0:
            break

        if skip_counter == 0 and skip_size > 0:
            skip_counter = skip_size
            group_min -= 1
            sb += b'_'

    # Add leading zeroes for binary, octal and hexadecimal.
    if base != 10 and skip_size > 0:
        while True:
            while skip_counter > 0:
                skip_counter -= 1
                sb += b'0'
            group_min -= 1
            if group_min <= 0:
                break
            skip_counter = skip_size
            sb += b'_'

    # sb currently reads LSD to MSD. Swap them.
    left, right = offset, len(sb) - 1
    while left < right:
        sb[left], sb[right] = sb[right], sb[left]
        left  += 1
        right -= 1

    return sb.decode()


def int_decode_base(value: str, base = 0) -> int:
    assert(base == 0 or 2 <= base and base <= 64)

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
        try:
            tmp = __RADIX_PREFIX_TABLE[value[1]]
        except KeyError:
            tmp = 0

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
        result += __RADIX_TABLE_REVERSE[c]

    return -result if sign else result


__INT_DIGIT_STRINGS = {
    1: "one",   2: "two",   3: "three", 4:  "four", 5: "five", 6: "six",
    7: "seven", 8: "eight", 9: "nine",  10: "ten", 11: "eleven", 12: "twelve",
    13: "thirteen", 14: "fourteen", 15: "fifteen", 16: "sixteen", 17: "seventeen",
    18: "eighteen", 19: "nineteen", 20: "twenty", 30: "thirty", 40: "fourty",
    50: "fifty", 60: "sixty", 70: "seventy", 80: "eighty", 90: "ninety"
}


# http://sunshine.chpc.utah.edu/Labs/ScientificNotation/ManSciNot1/table.html
# https://en.wikipedia.org/wiki/Names_of_large_numbers
__INT_PLACE_VALUE_STRINGS = {
    10**3: "thousand",
    10**6: "million",
    10**9: "billion",
    10**12: "trillion",
    10**15: "quadrillion",
    10**18: "quintillion",
    10**21: "sextillion",
    10**24: "septillion",
    10**27: "octillion",
    10**30: "nonillion",
    10**33: "decillion",
    10**36: "undecillion",
    10**39: "duodecllion",
    10**42: "tredecillion",
    10**45: "quattrodecillion",
    10**48: "quindecillion",
    10**51: "sexdecillion",
    10**54: "septendecillion",
    10**57: "octodecillion",
    10**60: "novemdecillion",
    10**64: "vigintillion",
}

def int_to_words(a: int) -> str:
    if a < 0:
        a = abs(a)
        words = ["negative"]
        start_i = 1
    else:
        words: list[str] = []
        start_i = 0

    power = 0
    digits = int_split_digits(a, base=10)
    n = len(digits)
    # Iterate from LSD to MSD.
    for i in range(0, len(digits), 3):
        place = 10**power
        ones = digits[i]
        tens = digits[i + 1] if i + 1 < n else 0
        hundreds = digits[i + 2] if i + 2 < n else 0

        # print(f"power={power}: place={place}, ones={ones}, tens={tens}, hundreds={hundreds}")

        s = ""
        # [1,9] '-hundred' ( ' and ' )?
        if hundreds:
            s += f"{__INT_DIGIT_STRINGS[hundreds]}-hundred"
            if tens or ones:
                s += ' '

        if tens:
            # [10,19]
            if tens == 1:
                s += __INT_DIGIT_STRINGS[tens*10 + ones]
            # [2,9]*10 '-' ( <ones> )?
            else:
                s += __INT_DIGIT_STRINGS[tens*10]
                if ones:
                    s += f"-{__INT_DIGIT_STRINGS[ones]}"
        elif ones:
            s += __INT_DIGIT_STRINGS[ones]

        # Nonezero magnitude? i.e. hundreds*100 + tens*10 + ones != 0
        if (hundreds or tens or ones) and (place in __INT_PLACE_VALUE_STRINGS):
            s += f" {__INT_PLACE_VALUE_STRINGS[place]}"
            # Opinion: trailing comma only needed for two or more groups
            # of base-10 triples.
            # TODO(2025-12-20): Get 'one million *and* one'
            if i != 0 and len(words) > 0:
                s += ','

        # prepend
        if s:
            words.insert(start_i, s)
        power += 3

    return ' '.join(words)


def int_digit_add(a: int, b: int) -> tuple[int, int]:
    sum = DIGIT(a + b).value
    carry = sum >> DIGIT_SHIFT
    sum &= DIGIT_MASK
    return sum, carry


def int_digit_sub(a: int, b: int) -> tuple[int, int]:
    diff  = DIGIT(a - b).value
    carry = diff >> (DIGIT_TYPE_BITS - 1)
    diff &= DIGIT_MASK
    return diff, carry


def int_digit_mul(a: int, b: int) -> tuple[int, int]:
    prod  = WORD(a * b).value
    carry = prod >> DIGIT_SHIFT
    prod &= DIGIT_MASK
    return prod, carry


def int_split_digits(value: int, base = DIGIT_BASE) -> tuple[int]:
    if value == 0:
        return [0]

    # little endian ordering.
    digits = []
    value  = abs(value)
    while value > 0:
        lsd = value % base
        value //= base
        digits.append(lsd)
    return tuple(digits)


def int_combine_digits(digits: list[int] | tuple[int], base = DIGIT_BASE) -> int:
    value = 0

    # Assume little-endian, so convert to big-endian
    for digit in reversed(digits):
        value *= base
        value += digit

    return value


def int_get_base_fast(base: int) -> int:
    base_fast = base
    while base_fast * base < DIGIT_BASE:
        base_fast *= base
    return base_fast


# a = 123_456_789_101_112_131_415 #1617181920

# a = len(sys.argv) > 1 and int(sys.argv[1]) or 1_234_567_890
# digits  = int_split_digits(a)
# a_bin = int_encode_base(a, base=2,  group_size=DIGIT_SHIFT)
# a_dec = int_encode_base(a, base=10, group_size=10)

# # Not the same as converting each `BASE` digit to octal because the octal
# # place values do not line up with `DIGIT_BITS`. Same for hex.
# a_oct = int_encode_base(a, base=8,  group_size=11)
# a_hex = int_encode_base(a, base=16, group_size=8)

# print(a, bin(a), oct(a), hex(a), sep='\n')
# # print("Pretty:", a_dec, a_bin, a_oct, a_hex, sep="\n\t")


# Demonstrate a * b0
bits = 32
base = 2**bits
mask = int_max(u32)
# a = int_max(u64)
# b = a

a = 1_234_567_891_011_121_314_151_617_181_920
b =   212_223_242_526_272_829_303_132_333_435

# a0, a1 = int_split_digits(a, base)
# b0, b1 = int_split_digits(b, base)

# p00 = a0 * b0
# p10 = a1 * b0
# # p01 = a0 * b1

# # The following are equivalent.
# mid1 =   a * b0
# mid2 = (a1 * b0) * base**1 + (a0 * b0) * base**0

def int_print(value: int):
    print(f"bin({bin(value)})")
    print(f"oct({oct(value)})")
    print(f"hex({hex(value)})")
