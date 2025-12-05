from ctypes import (
    c_uint32 as DIGIT,
    c_int64 as WORD,
    sizeof,
)

DIGIT_NAILS         = 2
DIGIT_TYPE_BITS     = sizeof(DIGIT) * 8
DIGIT_BITS          = DIGIT_TYPE_BITS - DIGIT_NAILS
WORD_BITS           = DIGIT_BITS * 2
DIGIT_BASE          = int(2**DIGIT_BITS)
DIGIT_BASE_DECIMAL  = 10**9
DIGIT_MASK          = DIGIT_BASE - 1
DIGIT_MAX           = DIGIT_MASK

RADIX_TABLE = \
    b"0123456789" \
    b"ABCDEFGHIJKLMNOPQRSTUVWXYZ" \
    b"abcdefghijklmnopqrstuvwxyz+/"

RADIX_TABLE_REVERSE = {chr(char): value for value, char in enumerate(RADIX_TABLE)}
RADIX_PREFIX_TABLE = {
    'b': 2,  'B': 2,
    'o': 8,  'O': 8,
    'd': 10, 'D': 10,
    'x': 16, 'X': 16,
}

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
        digit_index = bit_index // DIGIT_BITS
        mask  = DIGIT(1 << (bit_index % DIGIT_BITS)).value
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

    assert(1 <= bit_count and bit_count <= WORD_BITS)
    return __bitfield_get(digits, bit_index, bit_count)


def int_count_digits(value: int, base = 10) -> int:
    count = 0
    if value < 0:
        value = abs(value)

    while value > 0:
        count += 1
        value //= base

    return count


def int_encode_base(value: int, base = 10, *, min_groups = 0, group_size = 0) -> str:
    assert(2 <= base and base <= 64)
    assert(group_size == 0 or 2 <= group_size and group_size <= 64)
    if value == 0 and min_groups == 0:
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

        sb += RADIX_TABLE[digit].to_bytes()
        value //= base
        skip_counter -= 1

        if value == 0:
            break

        if skip_counter == 0 and skip_size > 0:
            skip_counter = skip_size
            min_groups -= 1
            sb += b'_'

    # Add leading zeroes for binary, octal and hexadecimal.
    if base != 10 and skip_size > 0:
        while True:
            while skip_counter > 0:
                skip_counter -= 1
                sb += b'0'
            min_groups -= 1
            if min_groups <= 0:
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
            tmp = RADIX_PREFIX_TABLE[value[1]]
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
        result += RADIX_TABLE_REVERSE[c]

    return -result if sign else result


def sub(a: int, b: int) -> tuple[int, int]:
    diff  = DIGIT(a - b).value
    carry = diff >> (DIGIT_TYPE_BITS - 1)
    diff &= DIGIT_MASK
    return diff, carry


def int_split_digits(value: int, base = DIGIT_BASE) -> list[int]:
    if value == 0:
        return [0]

    digits = []
    value  = abs(value)
    while value > 0:
        lsd = value % base
        value //= base
        digits.append(lsd)
    return digits


def int_combine_digits(digits: list[int], base = DIGIT_BASE) -> int:
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

a = 1_234_567_890
digits  = int_split_digits(a)
a_bin = int_encode_base(a, base=2,  group_size=DIGIT_BITS)
a_dec = int_encode_base(a, base=10, group_size=10)

# Not the same as converting each `BASE` digit to octal because the octal
# place values do not line up with `DIGIT_BITS`. Same for hex.
a_oct = int_encode_base(a, base=8,  group_size=11)
a_hex = int_encode_base(a, base=16, group_size=8)

print(a, bin(a), oct(a), hex(a), sep='\n')
# print("Pretty:", a_dec, a_bin, a_oct, a_hex, sep="\n\t")
