#include "i128.h"

#include <utils/strings.h>

// CONVERSION OPERATIONS

i128
i128_from_i64(i64 a)
{
    i128 dst;
    // For negative values, -(value < 0) will be -1, resulting in all high
    // bits being set. This is known as 'sign extension'.
    dst.lo = cast(u64)a;
    dst.hi = -cast(u64)(a < 0);
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

bool
i128_sign(i128 a)
{
    return cast(bool)(a.hi >> (TYPE_BITS(a.hi) - 1));
}

i128
i128_from_lstring(const char *restrict s, size_t n, const char **restrict end_ptr, int base)
{
    i128 dst, base_i128;
    size_t i = 0;
    bool sign = false;

    dst = I128_ZERO;

    // Check leading characters.
    for (; i < n; i += 1) {
        char ch;

        ch = s[i];
        if (ch == '+' || is_space(ch)) {
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

    base_i128 = i128_from_u64(cast(u64)base);

    for (; i < n; i += 1) {
        i128 digit_i128;
        u64 digit = 0;
        char ch;

        ch = s[i];
        if (ch == '_' || ch == ',' || is_space(ch)) {
            continue;
        }

        if (is_digit(ch)) {
            digit = cast(u64)(ch - '0');
        } else if (is_upper(ch)) {
            digit = cast(u64)(ch - 'A' + 10);
        } else if (is_lower(ch)) {
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
        digit_i128 = i128_from_u64(digit);
        dst        = i128_mul(dst, base_i128);
        dst        = i128_add(dst, digit_i128);
    }

finish:
    if (end_ptr) {
        *end_ptr = &s[i];
    }
    return sign ? i128_neg(dst) : dst;
}

// BITWISE OPERATIONS

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

i128
i128_logical_left_shift(i128 a, unsigned int n)
{
    i128 dst;
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

i128
i128_logical_right_shift(i128 a, unsigned int n)
{
    i128 dst;
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

// ARITHMETIC OPERATIONS

i128
i128_abs(i128 value)
{
    return i128_sign(value) ? i128_neg(value) : value;
}

i128
i128_neg(i128 value)
{
    // https://en.wikipedia.org/wiki/Two%27s_complement
    i128 dst;
    dst = i128_not(value);
    dst = i128_add(dst, I128_ONE);
    return dst;
}

bool
i128_checked_add_unsigned(i128 *dst, i128 a, i128 b)
{
    bool carry_lo, overflow;

    // Overflow check for the lower 32 bits:
    //      a.lo + b.lo > UINT64_MAX
    //      a.lo        > UINT64_MAX - b.lo
    //
    // Overflow check for the higher 64 bits:
    //      a.hi + b.hi + carry_lo > UINT64_MAX
    //      a.hi                   > UINT64_MAX - b.hi - carry_lo
    carry_lo = a.lo > (UINT64_MAX - b.lo);
    overflow = a.hi > (UINT64_MAX - b.hi - cast(u64)carry_lo);

    dst->lo  = a.lo + b.lo;
    dst->hi  = a.hi + b.hi + cast(u64)carry_lo;
    return overflow;
}

bool
i128_checked_add_signed(i128 *dst, i128 a, i128 b)
{
    bool carry_lo, overflow;

    // Same idea as unsigned, but treat the higher 64 bits as signed
    // when doing comparisons.
    carry_lo = a.lo > (UINT64_MAX - b.lo);
    overflow = cast(i64)a.hi > (INT64_MAX - cast(i64)b.hi - cast(i64)carry_lo);

    dst->lo  = a.lo + b.lo;
    dst->hi  = a.hi + b.hi + cast(u64)carry_lo;
    return overflow;
}

i128
i128_add(i128 a, i128 b)
{
    i128 dst;
    i128_checked_add_unsigned(&dst, a, b);
    return dst;
}

bool
i128_checked_sub_unsigned(i128 *dst, i128 a, i128 b)
{
    bool carry_lo, overflow;

    // Overflow check for the lower 32 bits:
    //      a.lo - b.lo < 0
    //      a.lo        < b.lo
    //
    // Overflow check for the upper 32 bits:
    //      a.hi - b.hi - carry_lo < 0
    //      a.hi                   < b.hi + carry_lo
    carry_lo = a.lo < b.lo;
    overflow = a.hi < (b.hi + cast(u64)carry_lo);

    dst->lo = a.lo - b.lo;
    dst->hi = a.hi - b.hi - cast(u64)carry_lo;
    return overflow;
}

bool
i128_checked_sub_signed(i128 *dst, i128 a, i128 b)
{
    bool carry_lo, overflow;

    // Same idea as unsigned, but treat the higher 64 bits as signed
    // when doing comparisons.
    carry_lo = a.lo < b.lo;
    overflow = cast(i64)a.hi < (cast(i64)b.hi + cast(i64)carry_lo);

    dst->lo = a.lo - b.lo;
    dst->hi = a.hi - b.hi - cast(u64)carry_lo;
    return overflow;
}

i128
i128_sub(i128 a, i128 b)
{
    i128 dst;
    i128_checked_sub_unsigned(&dst, a, b);
    return dst;
}


/** @link catid on stackoverflow: https://stackoverflow.com/a/51587262 */
static i128
u64_mul_u64(u64 a, u64 b)
{
    i128 dst;
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

// bool
// i128_checked_mul_unsigned(i128 *dst, i128 a, i128 b)
// {
//     // dst.hi + (a.lo * b.hi) + (a.hi * b.lo) > UINT64_MAX
//     // dst.hi > UINT64_MAX - (a.lo * b.hi) - (a.hi * b.lo)
//     *dst     = internal_u64_mul_u64(a.lo, b.lo);
//     dst->hi += (a.lo * b.hi) + (a.hi * b.lo);
//     return false;
// }

i128
i128_mul(i128 a, i128 b)
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
    i128 dst;
    u64 a0, a1, b0, b1, p01, p10;

    a0 = a.lo;
    a1 = a.hi;
    b0 = b.lo;
    b1 = b.hi;

    // Overflow check for upper 64 bits:
    //  dst.hi + p10 + p01 > UINT64_MAX
    //  dst.hi > UINT64_MAX - p10 - p01
    //
    // Problem: p10 or p01 could themselves overflow! (See above).
    dst     = u64_mul_u64(a0, b0);
    p10     = a1 * b0;
    p01     = a0 * b1;
    dst.hi += p10 + p01;
    return dst;
}

i128
i128_mul_u64(i128 a, u64 b)
{
    i128 dst;
    u64 a0, a1, b0, p10;

    a0 = a.lo;
    b0 = b;
    a1 = a.hi;

    // Overflow check for upper 64 bits:
    //  dst.hi + p10 > UINT64_MAX
    //  dst.hi > UINT64_MAX - p10
    //
    // Problem: p10 (a.k.a. a1*b0) itself could overflow!
    dst     = u64_mul_u64(a0, b0);
    p10     = a1 * b0;
    dst.hi += p10;
    return dst;
}

// COMPARISON OPERATIONS

bool
i128_eq(i128 a, i128 b)
{
    // Concept check: `(a ^ b) == 0` iff `a == b`
    u64 xor_hi, xor_lo;

    xor_hi = a.hi ^ b.hi;
    xor_lo = a.lo ^ b.lo;
    return (xor_hi | xor_lo) == 0;
}

bool
i128_lt_unsigned(i128 a, i128 b)
{
    // Simulate `a - b < 0` without actually doing the full subtraction.
    bool less, carry;

    // Overflow checks:
    //  carry = a.lo - b.lo < 0
    //  carry = a.lo        < b.lo
    //
    //  a.hi - b.hi - carry < 0
    //  a.hi                < b.hi + carry
    carry = a.lo < b.lo;
    less  = a.hi < b.hi + cast(u64)carry;
    return less;
}

bool
i128_leq_unsigned(i128 a, i128 b)
{
    // Simulate `a - b <= 0` without actually doing the full subtraction.
    bool less_eq, carry;

    // Overflow checks:
    //  carry = a.lo - b.lo < 0
    //  carry = a.lo        < b.lo
    //
    //  a.hi - b.hi - carry <= 0
    //  a.hi                <= b.hi + carry
    carry   = a.lo <  b.lo;
    less_eq = a.hi <= b.hi + cast(u64)carry;
    return less_eq;
}

bool
i128_neq(i128 a, i128 b)
{
    // (a != b) == !(a == b)
    return !i128_eq(a, b);
}

bool
i128_gt_unsigned(i128 a, i128 b)
{
    // (a > b) <=> (b < a)
    return i128_lt_unsigned(b, a);
}

bool
i128_geq_unsigned(i128 a, i128 b)
{
    // (a >= b) <=> (b <= a)
    return i128_leq_unsigned(b, a);
}

bool
i128_lt_signed(i128 a, i128 b)
{
    // Simulate `a - b < 0` without actually doing the full subtraction.
    bool less, carry;

    // Same idea as unsigned comparison except we treat the upper 64 bits
    // as signed.
    carry = a.lo < b.lo;
    less  = cast(i64)a.hi < cast(i64)b.hi + cast(i64)carry;
    return less;
}

bool
i128_leq_signed(i128 a, i128 b)
{
    // Simulate `a - b <= 0` without actually doing the full subtraction.
    bool less_eq, carry;

    // Same idea as unsigned comparison except we treat the upper 64 bits
    // as signed.
    carry   = a.lo < b.lo;
    less_eq = cast(i64)a.hi <= cast(i64)b.hi + cast(i64)carry;
    return less_eq;
}

bool
i128_gt_signed(i128 a, i128 b)
{
    return i128_lt_signed(b, a);
}

bool
i128_geq_signed(i128 a, i128 b)
{
    return i128_leq_signed(b, a);
}
