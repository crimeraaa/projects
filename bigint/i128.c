// i128
#include "i128.h"
#include <utils/strings.h>
#include <math/checked.c>

// === CONVERSION OPERATIONS =============================================== {{{

static inline u64
internal_i64_to_twos_complement(i64 a)
{
    // e.g. `cast(u64)-1 == max(u64)` is guaranteed by the C standard.
    u64 dst = cast(u64)a;

    // If negative, force it into Two's complement representation as
    // given by the formula: `~abs(a) + 1`.
    if (a < 0) {
        dst = ~(0 - dst) + 1;
    }
    return dst;
}

u128
u128_from_i64(i64 a)
{
    u128 dst;
    // For negative values, do sign extension.
    dst.lo = internal_i64_to_twos_complement(a);
    dst.hi = (a < 0) ? U64_MAX : 0;
    return dst;
}

u128
u128_from_u64(u64 a)
{
    u128 dst;
    dst.lo = a;
    dst.hi = 0;
    return dst;
}

u128
u128_from_i128(i128 a)
{
    u128 dst;
    dst.lo = a.lo;
    dst.hi = a.hi;
    return dst;
}


i128
i128_from_i64(i64 a)
{
    i128 dst;
    // For negative values, do sign extension.
    dst.lo = internal_i64_to_twos_complement(a);
    dst.hi = (a < 0) ? U64_MAX : 0;
    return dst;
}

i128
i128_from_u64(u64 a)
{
    i128 dst;
    dst.lo = a;
    dst.hi = 0;
    return dst;
}

i128
i128_from_u128(u128 a)
{
    i128 dst;
    dst.lo = a.lo;
    dst.hi = a.hi;
    return dst;
}

static inline bool
internal_u64_sign(u64 a)
{
    return cast(bool)(a >> (TYPE_BITS(a) - 1));
}

static inline bool
internal_u128_sign(u128 a)
{
    return internal_u64_sign(a.hi);
}

bool
i128_sign(i128 a)
{
    return internal_u64_sign(a.hi);
}

u128
u128_from_string(const char *restrict s, size_t n, const char **restrict end_ptr, int base)
{
    u128 dst = U128_ZERO;
    size_t i = 0;
    bool sign = false;

    // Check leading characters.
    for (; i < n; i += 1) {
        char ch;

        ch = s[i];
        if (ch == '+' || char_is_space(ch)) {
            continue;
        } else if (ch == '-') {
            sign = !sign;
            continue;
        } else {
            break;
        }
    }

    // Read base prefix, if we might have one.
    if (i < n && s[i] == '0') {
        int string_base = 0;
        i += 1;
        if (i < n) {
            switch (s[i]) {
            case 'b': case 'B': string_base = 2;  i += 1; break;
            case 'o': case 'O': string_base = 8;  i += 1; break;
            case 'd': case 'D': string_base = 10; i += 1; break;
            case 'x': case 'X': string_base = 16; i += 1; break;
            }
        }

        // Didn't know the base beforehand, so we have it now.
        if (base == 0 && string_base != 0) {
            base = string_base;
        }
        // Inconsistent base received?
        else if (base != string_base) {
            goto finish;
        }
    }
    // No base prefix but caller doesn't know the base either.
    else if (base == 0) {
        base = 10;
    }

    for (; i < n; i += 1) {
        u64 digit = 0;
        char ch;

        ch = s[i];
        if (ch == '_' || ch == ',' || char_is_space(ch)) {
            continue;
        }

        if (char_is_digit(ch)) {
            digit = cast(u64)(ch - '0');
        } else if (char_is_upper(ch)) {
            digit = cast(u64)(ch - 'A' + 10);
        } else if (char_is_lower(ch)) {
            digit = cast(u64)(ch - 'a' + 10);
        } else {
            break;
        }

        // Not a valid digit in this base?
        if (digit >= cast(u64)base) {
            break;
        }

        // dst *= base
        // dst += digit
        dst = u128_mul_u64(dst, cast(u64)base);
        dst = u128_add_u64(dst, digit);
    }

finish:
    if (end_ptr) {
        *end_ptr = &s[i];
    }
    return sign ? u128_neg(dst) : dst;
}

i128
i128_from_string(const char *restrict s, size_t n, const char **restrict end_ptr, int base)
{
    i128 dst;
    u128 tmp;

    tmp = u128_from_string(s, n, end_ptr, base);
    dst = i128_from_u128(tmp);
    return dst;
}

// === }}} =====================================================================
// === BITWISE OPERATIONS ================================================== {{{

u128
u128_not(u128 a)
{
    u128 dst;
    dst.lo = ~a.lo;
    dst.hi = ~a.hi;
    return dst;
}

u128
u128_and(u128 a, u128 b)
{
    u128 dst;
    dst.lo = a.lo & b.lo;
    dst.hi = a.hi & b.hi;
    return dst;
}

u128
u128_or(u128 a, u128 b)
{
    u128 dst;
    dst.lo = a.lo | b.lo;
    dst.hi = a.hi | b.hi;
    return dst;
}

u128
u128_xor(u128 a, u128 b)
{
    u128 dst;
    dst.lo = a.lo ^ b.lo;
    dst.hi = a.hi ^ b.hi;
    return dst;
}

i128
i128_not(i128 a)
{
    i128 dst;
    dst.lo = ~a.lo;
    dst.hi = ~a.hi;
    return dst;
}

i128
i128_and(i128 a, i128 b)
{
    i128 dst;
    dst.lo = a.lo & b.lo;
    dst.hi = a.hi & b.hi;
    return dst;
}

i128
i128_or(i128 a, i128 b)
{
    i128 dst;
    dst.lo = a.lo | b.lo;
    dst.hi = a.hi | b.hi;
    return dst;
}

i128
i128_xor(i128 a, i128 b)
{
    i128 dst;
    dst.lo = a.lo ^ b.lo;
    dst.hi = a.hi ^ b.hi;
    return dst;
}

u128
u128_shift_left(u128 a, uint n)
{
    u128 dst;
    // Resulting logical left-shift may result in nonzero `lo` and `hi`?
    if (n < TYPE_BITS(dst.lo)) {
        dst.lo = (a.lo << n);
        dst.hi = (a.hi << n) | (a.lo >> (TYPE_BITS(a.lo) - n));
    }
    // Resulting logical left-shift completely clears out `lo`?
    else {
        n -= TYPE_BITS(dst.lo);
        dst.lo = 0;
        dst.hi = a.lo << n;
    }
    return dst;
}

u128
u128_shift_right(u128 a, uint n)
{
    u128 dst;
    // Resulting logical right-shift may result in both nonzero `lo` and `hi`?
    if (n < TYPE_BITS(dst.lo)) {
        dst.lo = (a.lo >> n) | (a.hi << (TYPE_BITS(a.hi) - n));
        dst.hi = (a.hi >> n);
    }
    // Resulting logical right-shift completely clears out `hi`?
    else {
        n -= TYPE_BITS(a.hi);
        dst.lo = a.hi >> n;
        dst.hi = 0;
    }
    return dst;
}

static i128
internal_i128_shift_logical(i128 a, u128 (*shift_fn)(u128 a, uint n), uint n)
{
    u128 tmp;
    i128 dst;
    tmp = shift_fn(u128_from_i128(a), n);
    dst = i128_from_u128(tmp);
    return dst;
}

i128
i128_shift_left(i128 a, uint n)
{
    return internal_i128_shift_logical(a, u128_shift_left, n);
}

i128
i128_shift_right_logical(i128 a, uint n)
{
    return internal_i128_shift_logical(a, u128_shift_right, n);
}

i128
i128_shift_right_arithmetic(i128 a, uint n)
{
    i128 dst;

    // Resulting arithmetic right-shift may result in both nonzero `lo` and `hi`?
    if (n < TYPE_BITS(dst.lo)) {
        u64 sx_hi = 0;
        uint bits_lo;

        bits_lo = TYPE_BITS(a.hi) - n;
        if (i128_sign(a)) {
            // Concept check:
            //     (0b1001_1101 >> 2)
            //  ==  0bxx10_0111
            //
            //  bits = 8
            //  n = 2
            //  bits_lo = bits - n
            //          = 6
            //
            //  sx_hi   = ~((0b0000_0001 << bits_right)  - 1)
            //          = ~((0b0000_0001 << 6)           - 1)
            //          = ~( 0b0100_0000                 - 1)
            //          = ~  0b0011_1111
            //          =    0b1100_0000
            sx_hi = ~((cast(u64)1 << bits_lo) - 1);
        }
        dst.lo = (a.lo >> n) | (a.hi << bits_lo);
        dst.hi = (a.hi >> n) | sx_hi;
    }
    // Resulting arithmetic right-shift completely clears out `hi`?
    else {
        // Concept check:
        //    (0b1001_1101_0000_0011 >> n)
        // ==  0bxxxx_xxxx_xx10_0111
        //
        //  bits = 8, n = 10
        //  bits_right = n - bits
        //  = 10 - 8
        //  = 2
        //
        //  sx_left =
        u64 sx_hi = 0, sx_lo = 0;
        uint bits_lo;

        bits_lo = n - TYPE_BITS(a.hi);
        if (i128_sign(a)) {
            sx_hi = U64_MAX;
            sx_lo = ~((cast(u64)1 << bits_lo) - 1);
        }

        dst.lo = (a.hi >> bits_lo) | sx_lo;
        dst.hi = sx_hi;
    }
    return dst;
}

// === }}} =====================================================================
// === ARITHMETIC OPERATIONS =============================================== {{{

i128
i128_abs(i128 a)
{
    return i128_sign(a) ? i128_neg(a) : a;
}

u128
i128_abs_unsigned(i128 a)
{
    u128 dst;

    dst = u128_from_i128(a);
    if (i128_sign(a)) {
        dst = u128_neg(dst);
    }
    return dst;
}

u128
u128_neg(u128 a)
{
    // https://en.wikipedia.org/wiki/Two%27s_complement
    u128 dst;
    dst = u128_not(a);
    dst = u128_add(dst, U128_ONE);
    return dst;
}

i128
i128_neg(i128 a)
{
    u128 tmp;
    i128 dst;

    tmp = u128_neg(u128_from_i128(a));
    dst = i128_from_u128(tmp);
    return dst;
}

u128
u128_add(u128 a, u128 b)
{
    u128 dst;
    u128_checked_add(&dst, a, b);
    return dst;
}

u128
u128_sub(u128 a, u128 b)
{
    u128 dst;
    u128_checked_sub(&dst, a, b);
    return dst;
}


/** @link catid on stackoverflow: https://stackoverflow.com/a/51587262 */
static u128
internal_u64_mul_u64(u64 a, u64 b)
{
    u128 dst;
    u64 a0, a1, b0, b1, p00, p10, p01, p11, mid;
    const u64 mask = UINT32_MAX;

    // 64x64 multiplication results in 128-bit results. We simulate it by
    // chopping it into multiple 32x32 multiplications with 64-bit results.
    a0 =  a        & mask; // a[00:32]
    a1 = (a >> 32) & mask; // a[32:64]
    b0 =  b        & mask; // b[00:32]
    b1 = (b >> 32) & mask; // b[32:64]

    // 32x32 products sans place values.
    //
    // +----------------+---------------+---------------+---------------+
    // |   dst[128.................64]  |  dst[64...................0]  |
    // |   hi[64...00]  |  hi[32...00]  |  lo[64...32]  |  lo[32...00]  |
    // +----------------+---------------+---------------+---------------+
    // |                |               |           a1  |           a0  |
    // | *              |               |           b1  |           b0  |
    // +----------------+---------------+---------------+---------------+
    // | =              |               |               |  p00 = a0*b0  |
    // | +              |               |  p10 = a1*b0  |               |
    // | +              |               |  p01 = a0*b1  |               |
    // | +              |  p11 = a1*b1  |               |               |
    // +----------------+---------------+---------------+---------------+
    // | =              |               |  p00[64..32]  |  p00[32..00]  |
    // | +              |  p10[64..32]  |  p10[32..00]  |               |
    // | +              |  p01[64..32]  |  p01[32..00]  |               |
    // | + p11[64..32]  |  p11[32..00]  |               |               |
    // +----------------+---------------+---------------+---------------+
    p00 = a0 * b0;
    p10 = a1 * b0;
    p01 = a0 * b1;
    p11 = a1 * b1;

    // Calculate part of 64x32 96-bit product `a * b0` that is to be
    // part of dst[64...00].
    //  = (a1 * b0) * (2**32)**1
    //  + (a0 * b0) * (2**32)**0
    //
    // The 64x32 product goes into dst[96..00].
    // It is split between dst.lo[64..00] and dst.hi[32..00].
    //
    //  p10[64..00] - Overlaps both intermediates, never overflows.
    //  p01[32..00] - Overlaps the dst[64..00].
    //  p00[64..32] - Only the upper half overlaps the 96-bit product.
    //
    // Concept check: (using `digits.py`)
    // ```py
    // bits = 32
    // mask = int_max(u32) # 0b11111111111111111111111111111111
    // a0 = mask
    // a1 = mask
    // b0 = mask
    // b1 = mask
    //
    // p00 = a0 * b0
    // p10 = a1 * b0
    // p01 = a0 * b1
    // mid = p10 + (p01 & mask) + (p00 >> bits)
    //
    // print(int_bin(p10, min_groups=2, group_size=32))
    // # '0b11111111111111111111111111111110_00000000000000000000000000000001'
    //
    // print(int_bin(p01 & mask, min_groups=2, group_size=32))
    // # '0b00000000000000000000000000000000_00000000000000000000000000000001'
    //
    // print(int_bin(p00 >> bits, min_groups=2, group_size=32))
    // # '0b00000000000000000000000000000000_11111111111111111111111111111110'
    //
    // print(int_bin(mid, min_groups=2, group_size=32))
    // # '0b11111111111111111111111111111111_00000000000000000000000000000000'
    // ```
    mid = p10 + (p01 & mask) + (p00 >> 32);

    // Lower 64-bits of the 64x64 128-bit product is in dst[64..00].
    //
    //  mid[32..00] - 96-bit product a*b0 that goes in dst[64..32].
    //  p00[32..00] - p00[32:64] was already added to `mids`.
    dst.lo = (mid << 32) | (p00 & mask);

    // Upper 64 bits of the 64x64 128-bit product is in dst[128..64].
    // We know that 64x64 multiplication cannot possibly overflow
    // the 128-bits hence we do not check for it.
    //
    //  p11[64..00] - Goes into dst[128..64].
    //  mid[64..32] - 96-bit product a*b0 that goes in dst[96..64].
    //  p01[64..32] - The portion that goes in dst[96..64].
    dst.hi = p11 + (mid >> 32) + (p01 >> 32);
    return dst;
}

u128
u128_mul(u128 a, u128 b)
{
    // +--------------------------------+-------------------------------+
    // |   overflow[256...........128]  |  dst[128................000]  |
    // |   o[256..192]  | o[192...128]  |  hi[64...00]  |  lo[64...00]  |
    // +----------------+---------------+---------------+---------------+
    // |                |               |  a[128..................000]  |
    // | *              |               |  b[128..................000]  |
    // +----------------+---------------+-------------------------------+
    // |                |               |           a1  |           a0  |
    // | *              |               |           b1  |           b0  |
    // +----------------+---------------+-------------------------------+
    // | =              |               |               |  p00 = a0*b0  |
    // | +              |               |  p10 = a1*b0  |               |
    // | +              |               |  p01 = a0*b1  |               |
    // | +              |  p11 = a1*b1  |               |               |
    // +----------------+---------------+---------------+---------------+
    // | =              |               | p00[128..64]  |  p00[64..00]  |
    // | +              | p10[128..64]  | p10[64...00]  |               |
    // | +              | p01[128..64]  | p01[64...00]  |               |
    // | + p11[128..64] | p11[64...00]  |               |               |
    // +----------------+---------------+---------------+---------------+
    u128 dst;
    u128_checked_mul(&dst, a, b);
    // u64 a0, a1, b0, b1, p01, p10;

    // a0 = a.lo;
    // a1 = a.hi;
    // b0 = b.lo;
    // b1 = b.hi;

    // dst     = u128_from_u64_mul_u64(a0, b0);
    // p10     = a1 * b0;
    // p01     = a0 * b1;
    // dst.hi += p10 + p01;
    return dst;
}

i128
i128_mul(i128 a, i128 b)
{
    i128 dst;
    u128 tmp;
    tmp = u128_mul(u128_from_i128(a), u128_from_i128(b));
    dst = i128_from_u128(tmp);
    return dst;
}

u128
u128_add_u64(u128 a, u64 b)
{
    u128 dst;
    bool carry;

    // No `b.hi`, so we can save a few instructions.
    carry  = a.lo > U64_MAX - b;
    dst.lo = a.lo + b;
    dst.hi = a.hi + cast(u64)carry;
    return dst;
}

u128
u128_mul_u64(u128 a, u64 b)
{
    u128 dst;
    u64 a0, a1, b0, p10;

    a0 = a.lo;
    b0 = b;
    a1 = a.hi;

    // No `b1`, meaning no `a0 * b1`, so we can save a few instructions.
    dst     = internal_u64_mul_u64(a0, b0);
    p10     = a1 * b0;
    dst.hi += p10;
    return dst;
}

i128
i128_add(i128 a, i128 b)
{
    i128 dst;
    u128 tmp;

    // Two's complement addition is the same as unsigned addition.
    tmp = u128_add(u128_from_i128(a), u128_from_i128(b));
    dst = i128_from_u128(tmp);
    return dst;
}

i128
i128_sub(i128 a, i128 b)
{
    i128 dst;
    u128 tmp;

    // Two's complement subtraction is the same as unsigned subtraction.
    tmp = u128_sub(u128_from_i128(a), u128_from_i128(b));
    dst = i128_from_u128(tmp);
    return dst;
}

// === CHECKED ARITHMETIC OPERATIONS ======================================= {{{

bool
u128_checked_add(u128 *dst, u128 a, u128 b)
{
    bool carry_in, carry_out;

    // Overflow check (128-bit unsigned addition):
    //
    //      a + b > max(u128)
    //
    // Can be divided into:
    //
    //      carry_in  := a.lo + b.lo > max(u64)
    //      carry_out := a.hi + b.hi + carry_in > max(u64)
    //
    carry_in  = u64_checked_add(&dst->lo, a.lo, b.lo);
    carry_out = u64_checked_add_carry(&dst->hi, a.hi, b.hi, cast(u64)carry_in);
    return carry_out;
}

bool
u128_checked_sub(u128 *dst, u128 a, u128 b)
{
    bool carry_in, carry_out;

    // Overflow check (128-bit unsigned subtraction):
    //
    //      a - b < 0
    //
    // Can be divided into:
    //
    //      carry_in  := a.lo - b.lo < 0
    //      carry_out := a.hi - b.hi - carry_in < 0
    //
    carry_in  = u64_checked_sub(&dst->lo, a.lo, b.lo);
    carry_out = u64_checked_sub_carry(&dst->hi, a.hi, b.hi, cast(u64)carry_in);
    return carry_out;
}

bool
u128_checked_sub_u64(u128 *dst, u128 a, u64 b)
{
    bool carry_in, carry_out;
    // carry_in  := a.lo - b < 0
    //            = a.lo < b
    // carry_out := a.hi - carry_in < 0
    //            = a.hi < carry_in
    carry_in  = u64_checked_sub(&dst->lo, a.lo, b);
    carry_out = u64_checked_sub(&dst->hi, a.hi, cast(u64)carry_in);
    return carry_out;
}

bool
u128_checked_mul(u128 *dst, u128 a, u128 b)
{
    u64 tmp, a0, a1, b0, b1, p10, p01;
    bool carry = false;

    a0 = a.lo;
    a1 = a.hi;
    b0 = b.lo;
    b1 = b.hi;

    *dst = internal_u64_mul_u64(a0, b0);

    // Overflow check for upper 64 bits:
    //
    //      dst.hi + p10 + p01 > max(u64)
    //      dst.hi > max(u64) - p10 - p01
    //
    // Problem: p10 and p01 could themselves overflow! (See above).
    // Note that p11 is not included because it always overflows.
    p10 = a1 * b0;
    p01 = a0 * b1;
    dst->hi += p10 + p01;

    // a1 * b0 > max(u64)
    // a0 * b1 > max(u64)
    // a1 * b1 > 0 always overflows the 128-bit product.
    carry = u64_checked_mul(&tmp, a1, b0);
    if (!carry && b1 > 0) {
        carry = u64_checked_mul(&tmp, a0, b1);
        carry = !carry && a1 != 0;
    }

    // Lastly, if we know the terms themselves didn't overflow,
    // then try to check the result of subtraction.
    if (!carry) {
        u64 diff;

        // Might overflow `p10 - p01` or `U64_MAX - p10`.
        carry = u64_checked_sub_carry(&diff, U64_MAX, p10, p01);
        carry = !carry && dst->hi > diff;
    }
    return carry;
}

bool
i128_checked_add(i128 *dst, i128 a, i128 b)
{
    bool carry;
    // Overflow check: (128-bit signed addition)
    //  a + b > max(i128)
    //  a > max(i128) - b
    carry = i128_gt(a, i128_sub(I128_MAX, b));
    *dst  = i128_add(a, b);
    return carry;
}

bool
i128_checked_sub(i128 *dst, i128 a, i128 b)
{
    bool carry;

    // Overflow check (128-bit signed subtraction):
    //
    //      a - b < 0
    //      a < b
    carry = i128_lt(a, b);
    *dst  = i128_sub(a, b);
    return carry;
}

// === }}} =====================================================================
// === }}} =====================================================================
// === COMPARISON OPERATIONS =============================================== {{{

bool
u128_eq(u128 a, u128 b)
{
    // Concept check: `(a ^ b) == 0` iff `a == b`
    u64 xor_hi, xor_lo;

    xor_hi = a.hi ^ b.hi;
    xor_lo = a.lo ^ b.lo;
    return (xor_hi | xor_lo) == 0;
}

#define FLAG_ZERO       0x1
#define FLAG_SIGN       0x2

// If unsigned arithmetic resulted in a value too large.
#define FLAG_CARRY      0x4

// If signed arithmetic resulted in a value too small or too large and had
// to be truncated.
#define FLAG_OVERFLOW   0x8

// Check the flags of `a - b`, similar to the x86 `cmp` instruction.
static uint
internal_u128_cmp(u128 a, u128 b)
{
    u128 diff;
    uint flags = 0;
    bool a_sign, b_sign, diff_sign, carry;

    a_sign  = internal_u128_sign(a);
    b_sign  = internal_u128_sign(b);
    carry   = u128_checked_sub(&diff, a, b);
    if (u128_eq(diff, U128_ZERO)) {
        flags |= FLAG_ZERO;
    }

    // Sign bit is toggled?
    diff_sign = internal_u128_sign(diff);
    if (diff_sign) {
        flags |= FLAG_SIGN;
    }

    if (carry) {
        flags |= FLAG_CARRY;
    }

    // Signed overflow occured?
    if (a_sign != b_sign && diff_sign != a_sign) {
        flags |= FLAG_OVERFLOW;
    }

    return flags;
}

static uint
internal_u128_cmp_u64(u128 a, u64 b)
{
    u128 diff;
    uint flags = 0;
    bool a_sign, b_sign, diff_sign, carry_out;

    a_sign    = internal_u128_sign(a);
    b_sign    = internal_u64_sign(b);
    carry_out = u128_checked_sub_u64(&diff, a, b);
    if (u128_eq(diff, U128_ZERO)) {
        flags |= FLAG_ZERO;
    }

    // Sign bit is toggled?
    diff_sign = internal_u128_sign(diff);
    if (diff_sign) {
        flags |= FLAG_SIGN;
    }

    if (carry_out) {
        flags |= FLAG_CARRY;
    }

    // Signed overflow occured?
    if (a_sign != b_sign && diff_sign != a_sign) {
        flags |= FLAG_OVERFLOW;
    }
    return flags;
}

bool
u128_lt(u128 a, u128 b)
{
    uint flags;
    flags = internal_u128_cmp(a, b);
    return (flags & FLAG_CARRY) != 0;
}

bool
u128_leq(u128 a, u128 b)
{
    uint flags;
    flags = internal_u128_cmp(a, b);
    return (flags & (FLAG_ZERO | FLAG_CARRY)) != 0;
}

bool
u128_neq(u128 a, u128 b)
{
    // (a != b) == !(a == b)
    return !u128_eq(a, b);
}

bool
u128_gt(u128 a, u128 b)
{
    // (a > b) <=> (b < a)
    return u128_lt(b, a);
}

bool
u128_geq(u128 a, u128 b)
{
    // (a >= b) <=> (b <= a)
    return u128_leq(b, a);
}

bool
i128_eq(i128 a, i128 b)
{
    return u128_eq(u128_from_i128(a), u128_from_i128(b));
}

bool
i128_lt(i128 a, i128 b)
{
    uint flags;
    bool sign, overflow;
    flags    = internal_u128_cmp(u128_from_i128(a), u128_from_i128(b));
    sign     = (flags & FLAG_SIGN) != 0;
    overflow = (flags & FLAG_OVERFLOW) != 0;
    return sign != overflow;
}

bool
i128_leq(i128 a, i128 b)
{
    uint flags;
    bool zero, sign, overflow;
    flags    = internal_u128_cmp(u128_from_i128(a), u128_from_i128(b));
    zero     = (flags & FLAG_ZERO) != 0;
    sign     = (flags & FLAG_SIGN) != 0;
    overflow = (flags & FLAG_OVERFLOW) != 0;
    return zero || (sign != overflow);
}

bool
i128_neq(i128 a, i128 b)
{
    //  (a != b)
    // !(a == b)
    return !i128_eq(a, b);
}

bool
i128_gt(i128 a, i128 b)
{
    // a > b
    // b < a
    // !(a <= b)
    return i128_lt(b, a);
}

bool
i128_geq(i128 a, i128 b)
{
    // a >= b
    // b <= a
    // !(a < b)
    return i128_leq(b, a);
}

bool
i128_eq_u64(i128 a, u64 b)
{
    return !i128_sign(a) && a.hi == 0 && a.lo == b;
}

bool
i128_lt_u64(i128 a, u64 b)
{
    uint flags;
    bool sign, overflow;
    flags    = internal_u128_cmp_u64(u128_from_i128(a), b);
    sign     = (flags & FLAG_SIGN) != 0;
    overflow = (flags & FLAG_OVERFLOW) != 0;
    return sign != overflow;
}

bool
i128_leq_u64(i128 a, u64 b)
{
    uint flags;
    bool zero, sign, overflow;
    flags    = internal_u128_cmp_u64(u128_from_i128(a), b);
    zero     = (flags & FLAG_ZERO) != 0;
    sign     = (flags & FLAG_SIGN) != 0;
    overflow = (flags & FLAG_OVERFLOW) != 0;
    return zero || (sign != overflow);
}

bool
i128_neq_u64(i128 a, u64 b)
{
    return !i128_eq_u64(a, b);
}

bool
i128_gt_u64(i128 a, u64 b)
{
    // a > b == !(a <= b)
    return !i128_leq_u64(a, b);
}

bool
i128_geq_u64(i128 a, u64 b)
{
    // a >= b == !(a < b)
    return !i128_lt_u64(a, b);
}

#undef FLAG_ZERO
#undef FLAG_SIGN
#undef FLAG_CARRY
#undef FLAG_OVERFLOW

// === }}} =====================================================================
