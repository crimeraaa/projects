/** @link https://github.com/michaeljclark/c128/blob/trunk/include/integer.h */
#ifndef BIGINT_I128_H
#define BIGINT_I128_H

#include <projects.h>

typedef struct i128 i128;
struct i128 {
    uint64_t lo, hi;
};

#define BIGINT_I128_IMPLEMENTATION

// CONVERSION OPERATIONS

/** @note Assumes two's complement. */
i128
i128_from_i64(int64_t value);

i128
i128_from_u64(uint64_t value);

/** @brief Get the sign bit of `a`. */
int
i128_sign(i128 a);

// BITWISE OPERATIONS


/** @brief `~a` */
i128
i128_not(i128 a);

/** @brief `a & b` */
i128
i128_and(i128 a, i128 b);


/** @brief `a | b` */
i128
i128_or(i128 a, i128 b);


/** @brief `a ^ b` */
i128
i128_xor(i128 a, i128 b);


/** @brief `a << n` */
i128
i128_shift_left_logical(i128 a, unsigned int n);


/** @brief `a >> n` */
i128
i128_shift_right_logical(i128 a, unsigned int n);


// ARITHMETIC OPERATIONS

/** @brief `dst = a + b`
 *  @return overflow bit */
bool
i128_checked_add_unsigned(i128 *dst, i128 a, i128 b);


/** @brief `dst = a - b`
 *  @return overflow bit */
bool
i128_checked_sub_unsigned(i128 *dst, i128 a, i128 b);


/** @brief `a + b` without returning any information on overflows. */
i128
i128_add_unsigned(i128 a, i128 b);


/** @brief `a - b` without returning any information on overflows. */
i128
i128_sub_unsigned(i128 a, i128 b);

// COMPARISON OPERATIONS

bool
i128_eq(i128 a, i128 b);

bool
i128_lt_unsigned(i128 a, i128 b);

bool
i128_le_unsigned(i128 a, i128 b);

bool
i128_neq(i128 a, i128 b);

bool
i128_gt_unsigned(i128 a, i128 b);

bool
i128_ge_unsigned(i128 a, i128 b);

#ifdef BIGINT_I128_IMPLEMENTATION

// CONVERSION OPERATIONS

i128
i128_from_i64(int64_t value)
{
    i128 dst;
    if (value >= 0) {
        dst.lo = cast(uint64_t)value;
        dst.hi = 0;
    }
    // Concept check:
    //  int64_t(-1) lo = `0xffff_ffff_ffff_ffff`, hi = `0xffff_ffff_ffff_ffff`
    //  int64_t(-2) lo = `0xffff_ffff_ffff_fffe`, hi = `0xffff_ffff_ffff_ffff`
    else {
        dst.lo = cast(uint64_t)value;
        dst.hi = ~cast(uint64_t)0;
    }
    return dst;
}

i128
i128_from_u64(uint64_t value)
{
    i128 dst;
    dst.lo = value;
    dst.hi = 0;
    return dst;
}

int
i128_sign(i128 a)
{
    return cast(int)(a.hi >> (TYPE_BITS(a.hi) - 1));
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
i128_shift_left_logical(i128 a, unsigned int n)
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
        dst.lo = 0;
        dst.hi = (dst.lo << (n - TYPE_BITS(dst.lo)));
    }
    return dst;
}

i128
i128_shift_right_logical(i128 a, unsigned int n)
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
        dst.lo = (a.hi >> (n - TYPE_BITS(a.hi)));
        dst.hi = 0;
    }
    return dst;
}

// ARITHMETIC OPERATIONS

bool
i128_checked_add_unsigned(i128 *dst, i128 a, i128 b)
{
    bool overflow;
    // If overflow occured, then result is much smaller than either operand.
    dst->lo  = a.lo + b.lo;
    overflow = (dst->lo < a.lo && dst->lo < b.lo);
    dst->hi  = a.hi + b.hi + cast(uint64_t)overflow;
    overflow = (dst->hi < a.hi && dst->hi < b.hi);
    return overflow;
}

bool
i128_checked_sub_unsigned(i128 *dst, i128 a, i128 b)
{
    bool overflow;
    // If overflow occured, then result is much larger than either operand.
    dst->lo  = a.lo - b.lo;
    overflow = (dst->lo > a.lo && dst->lo > b.lo);
    dst->hi  = a.hi - b.hi - cast(uint64_t)overflow;
    overflow = (dst->hi > a.hi && dst->hi > b.hi);
    return dst;
}

i128
i128_add_unsigned(i128 a, i128 b)
{
    i128 dst;
    i128_checked_add_unsigned(&dst, a, b);
    return dst;
}

i128
i128_sub_unsigned(i128 a, i128 b)
{
    i128 dst;
    i128_checked_sub_unsigned(&dst, a, b);
    return dst;
}

i128
i128_mul_unsigned_u64_u64(uint64_t a, uint64_t b)
{
    i128 dst;
    const uint64_t mask32 = 0xfffffffful;
    uint64_t a0, a1, b0, b1, term0, term1, term2, term3, c0, c1, hi;

    // Extract 32-bit terms
    a0 =  a        & mask32;
    a1 = (a >> 32) & mask32;
    b0 =  b        & mask32;
    b1 = (b >> 32) & mask32;

    // Multiply the 32-bit terms
    //   a =             a1      a0
    // * b =           * b1      b0
    //     ===========================
    //     (                a0 * b0) * 2**0
    //   + (        a1 * b0        ) * 2**32
    //   + (        a0 * b1        ) * 2**32
    //   + (a1 * 1b                ) * 2**64
    term0 = a0 * b0;
    term1 = a1 * b0;
    term2 = a0 * b1;
    term3 = a1 * b1;

    // Get the would-be higher 64 bits of the 128-bit result `a * b`.
    c0 = term1 + (term0 >> 32);
    c1 = term2 + (c0 & mask32);
    hi = term3 + (c0 >> 32) + (c1 >> 32);

    dst.lo = a * b;
    dst.hi = hi;
    return dst;
}

i128
i128_mul_unsigned(i128 a, i128 b)
{
    i128 dst;
    uint64_t a0, a1, b0, b1;

    a0 = a.lo;
    a1 = a.hi;
    b0 = b.lo;
    b1 = b.hi;

    dst = i128_mul_unsigned_u64_u64(a0, b0);
    dst.hi += (a0 * b1) + (a1 * b0);
    return dst;
}

// COMPARISON OPERATIONS

bool
i128_eq(i128 a, i128 b)
{
    return (a.hi == b.hi) && (a.lo == b.lo);
}

bool
i128_lt_unsigned(i128 a, i128 b)
{
    // a = a1 a0
    // b = b1 b0
    return (a.hi < b.hi) || (a.hi == b.hi && a.lo < b.lo);
}

bool
i128_le_unsigned(i128 a, i128 b)
{
    return (a.hi <= b.hi) || (a.hi == b.hi && a.lo <= b.lo);
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
i128_ge_unsigned(i128 a, i128 b)
{
    // (a >= b) <=> (b <= a)
    return i128_le_unsigned(b, a);
}


#endif /* BIGINT_I128_IMPLEMENTATION */

#endif /* BIGINT_I128_H */
