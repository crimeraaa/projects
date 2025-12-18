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

    dst = I128_ZERO;
    if (n > 2 && s[i] == '0') {
        int string_base = 0;

        i++;
        switch (s[i]) {
        case 'b': case 'B': string_base = 2;  i++; break;
        case 'o': case 'O': string_base = 8;  i++; break;
        case 'd': case 'D': string_base = 10; i++; break;
        case 'x': case 'X': string_base = 16; i++; break;
        }

        // Didn't know the base beforehand, so we have it now.
        if (base == 0 && string_base != 0) {
            base = string_base;
        }
        // Inconsistent base received?
        else if (base != string_base) {
            goto finish;
        }
    } else if (base == 0) {
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
    return dst;
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
    // Nothing to do?
    if (n == 0) {
        dst.lo = a.lo;
        dst.hi = a.hi;
    }
    // Resulting logical left-shift may result in nonzero `lo` and `hi`?
    else if (n < TYPE_BITS(dst.lo)) {
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
    // Nothing to do?
    if (n == 0) {
        dst.lo = a.lo;
        dst.hi = a.hi;
    }
    // Resulting logical right-shift may result in both nonzero `lo` and `hi`?
    else if (n < TYPE_BITS(dst.lo)) {
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
    i128_checked_add_signed(&dst, a, b);
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
    i128_checked_sub_signed(&dst, a, b);
    return dst;
}


/** @link catid on stackoverflow: https://stackoverflow.com/a/51587262 */
static i128
internal_u64_mul_u64(u64 a, u64 b)
{
    i128 dst;
    u64 a0, a1, b0, b1, a0b0, a1b0, a0b1, a1b1, mids;
    const u64 mask = UINT32_MAX;

    // 64-bit by 64-bit multiplication results in 128-bit results.
    // We simulate it by chopping it into multiple 32-bit by 32-bit
    // multiplications with 64-bit results.
    a0 =  a        & mask;
    a1 = (a >> 32) & mask;
    b0 =  b        & mask;
    b1 = (b >> 32) & mask;

    //  let BASE = 2**32
    //                       (a1   * BASE**1) + (a0   * BASE**0)
    //  x                    (b1   * BASE**1) + (b0   * BASE**0)
    //=========================================================================
    //                                          (a0b0 * BASE**0)  | LOW  00..32
    //------------------------------------------------------------+------------
    //  +                    (a1b0 * BASE**1)                     | MID  32..64
    //  +                    (a0b1 * BASE**1)                     | MID  32..64
    //------------------------------------------------------------+------------
    //  + (a1b1 * BASE**2)                                        | HIGH 64..96
    //------------------------------------------------------------+------------
    a0b0 = a0 * b0;
    a1b0 = a1 * b0;
    a0b1 = a0 * b1;
    a1b1 = a1 * b1;

    // Prod[32:96] as a 64-bit product with two 32-bit values.
    // This is because `a0b1` it may straddle both 64-bit halves.
    // (i.e. it is split between Prod[32:64] and Prod[64:96])
    //
    //  `a1b0`: [:]     - Likely overlaps both 64-bit halves.
    //  `a0b1`: [:32]   - Bits [32:64] overflow to Prod[64:96].
    //  `a0b0`: [32:64] - Bits [:32] do not overflow to here.
    mids = a1b0 + (a0b1 & mask) + (a0b0 >> 32);

    // Lower 64-bits of the 128-bit product. (Prod[0:64])
    //
    //  `mids`: [:32]   - The portion that goes in Prod[32:64]
    //  `a0b0`: [:32]   - Bits [32:64] overflowed to `mids`.
    dst.lo = (mids << 32) | (a0b0 & mask);

    // Upper 64 bits of the 128-bit product. (Prod[64:128])
    //
    //  `a1b1`: [:]     - Goes into Prod[64:128].
    //  `mids`: [32:64] - The portion that goes in Prod[64:96].
    //  `a0b1`: [32:64] - The portion that goes in Prod[64:96].
    dst.hi = a1b1 + (mids >> 32) + (a0b1 >> 32);
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
    i128 dst;
    dst     = internal_u64_mul_u64(a.lo, b.lo);
    dst.hi += (a.lo * b.hi) + (a.hi * b.lo);
    return dst;
}

i128
i128_mul_u64(i128 a, u64 b)
{
    i128 dst;
    dst     = internal_u64_mul_u64(a.lo, b);
    dst.hi += a.hi * b;
    return dst;
}

// COMPARISON OPERATIONS

bool
i128_eq(i128 a, i128 b)
{
    // Concept check: `(a ^ b) == 0` where `a == b`
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
