/** @link https://github.com/michaeljclark/c128/blob/trunk/include/integer.h */
#ifndef BIGINT_I128_H
#define BIGINT_I128_H

#include <projects.h>

// Little-endian i128.
typedef struct i128le i128le;
struct i128le {
    uint64_t lo, hi;
};

// Big-endian i128.
typedef struct i128be i128be;
struct i128be {
    uint64_t hi, lo;
};

// Windows is almost always little endian, else check what GCC/Clang say.
#if defined(_MSC_VER) || __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__

#define BIGINT_I128_ENDIAN_LITTLE   1
#define BIGINT_I128_ENDIAN_BIG      0
#define BIGINT_I128_TYPE            i128le
#define I128_ONE                    COMPOUND_LITERAL(i128le){1, 0}

#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__

#define BIGINT_I128_ENDIAN_LITTLE   0
#define BIGINT_I128_ENDIAN_BIG      1
#define BIGINT_I128_TYPE            i128be
#define I128_ONE                    COMPOUND_LITERAL(i128be){0, 1}

#else

#define BIGINT_I128_ENDIAN_LITTLE   0
#define BIGINT_I128_ENDIAN_BIG      0

#endif

// Sanity check.
#if BIGINT_I128_ENDIAN_LITTLE == BIGINT_I128_ENDIAN_BIG
#error Imposible endianness!
#endif

typedef BIGINT_I128_TYPE i128;

// Valid for either endianness.
#define I128_ZERO   COMPOUND_LITERAL(i128){0, 0}


#define BIGINT_I128_IMPLEMENTATION

// CONVERSION OPERATIONS

/** @note Assumes two's complement. */
i128
i128_from_int(int a);

/** @note Assumes two's complement. */
i128
i128_from_i64(int64_t a);

i128
i128_from_u64(uint64_t a);


/** @brief Get the sign bit of `a`. As in two's complement, a sign bit of 0
 *  indicates a positive value while a sign bit of 1 is negative. */
bool
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


/** @brief `|a|` as an unsigned value. Signed representation may overflow. */
i128
i128_abs(i128 a);


/** @brief `-a` */
i128
i128_neg(i128 a);


/** @brief `a + b` without returning any information on overflows.
 *  Works properly for either signed or unsigned addition. */
i128
i128_add(i128 a, i128 b);


/** @brief `a - b` without returning any information on overflows.
 *  Works properly for either signed or unsigned subtraction. */
i128
i128_sub(i128 a, i128 b);


/** @brief `*dst = a + b`. Works properly for either signed or unsigned
 *  addition.
 *
 * @return whether `a + b` overflowed.
 */
bool
i128_checked_add(i128 *dst, i128 a, i128 b);


/** @brief `*dst = a - b`. Works properly for either signed or unsigned
 *  addition.
 *
 * @return whether `a - b` overflowed.
 */
bool
i128_checked_sub(i128 *dst, i128 a, i128 b);


// COMPARISON OPERATIONS

bool
i128_eq(i128 a, i128 b);

bool
i128_lt_unsigned(i128 a, i128 b);

bool
i128_leq_unsigned(i128 a, i128 b);

bool
i128_neq(i128 a, i128 b);

bool
i128_gt_unsigned(i128 a, i128 b);

bool
i128_geq_unsigned(i128 a, i128 b);

bool
i128_lt_signed(i128 a, i128 b);

bool
i128_leq_signed(i128 a, i128 b);

bool
i128_gt_signed(i128 a, i128 b);

bool
i128_geq_signed(i128 a, i128 b);


#ifdef BIGINT_I128_IMPLEMENTATION

// CONVERSION OPERATIONS

i128
i128_from_int(int a)
{
    i128 dst;
    dst.lo = cast(uint64_t)a;
    dst.hi = -cast(uint64_t)(a < 0);
    return dst;
}

i128
i128_from_i64(int64_t a)
{
    i128 dst;
    // For negative values, -(value < 0) will be -1, resulting in all high
    // bits being set. This is known as 'sign extension'.
    dst.lo = cast(uint64_t)a;
    dst.hi = -cast(uint64_t)(a < 0);
    return dst;
}

i128
i128_from_u64(uint64_t a)
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
i128_checked_add(i128 *dst, i128 a, i128 b)
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
    overflow = a.hi > (UINT64_MAX - b.hi - cast(uint64_t)carry_lo);

    dst->lo  = a.lo + b.lo;
    dst->hi  = a.hi + b.hi + cast(uint64_t)carry_lo;
    return overflow;
}

i128
i128_add(i128 a, i128 b)
{
    i128 dst;
    i128_checked_add(&dst, a, b);
    return dst;
}

bool
i128_checked_sub(i128 *dst, i128 a, i128 b)
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
    overflow = a.hi < (b.hi + cast(uint64_t)carry_lo);

    dst->lo = a.lo - b.lo;
    dst->hi = a.hi - b.hi - cast(uint64_t)carry_lo;
    return overflow;
}


i128
i128_sub(i128 a, i128 b)
{
    i128 dst;
    i128_checked_sub(&dst, a, b);
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
    // Concept check: `a ^ b == 0` where `a == b`
    uint64_t xor_hi, xor_lo;

    xor_hi = a.hi ^ b.hi;
    xor_lo = a.lo ^ b.lo;
    return (xor_hi | xor_lo) == 0;
}

bool
i128_lt_unsigned(i128 a, i128 b)
{
    // Simulate `a - b < 0`.
    bool less, carry;

    // Overflow check:
    //  carry = a.lo - b.lo < 0
    //  carry = a.lo        < b.lo
    //
    //  a.hi - b.hi - carry < 0
    //  a.hi                < b.hi + carry
    carry = a.lo < b.lo;
    less  = a.hi < b.hi + cast(uint64_t)carry;
    return less;
}

bool
i128_leq_unsigned(i128 a, i128 b)
{
    // Simulate `a - b <= 0`.
    bool less_eq, carry;

    // Overflow checks:
    //  carry = a.lo - b.lo < 0
    //  carry = a.lo        < b.lo
    //
    //  a.hi - b.hi - carry <= 0
    //  a.hi                <= b.hi + carry
    carry   = a.lo <  b.lo;
    less_eq = a.hi <= b.hi + cast(uint64_t)carry;
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

/*```c
// Inputs:
//  rdi: u64 = a.lo
//  rsi: i64 = a.hi
//  rdx: u64 = b.lo
//  rcx: i64 = b.hi
//
// Outputs:
//  al: bool
bool
__int128_lt(__int128_t a, __int128_t b)
{
    // cmp  rdi, rdx    ; carry      = (a.lo - b.lo < 0)
    // sbb  rsi, rcx    ; a.hi      -= b.hi + carry
    //                  ; sign       = (a.lo < 0)
    //                  ; overflow   = ???
    // setl al          ; <retval>   = (sign != overflow)
    return a < b;
}
```*/
bool
i128_lt_signed(i128 a, i128 b)
{
    i128 dst;
    bool overflow, sign;

    overflow = i128_checked_sub(&dst, a, b);
    sign     = i128_sign(dst);
    return sign || overflow;
}

/*```c
// Inputs:
//  rdi: u64 = a.lo
//  rsi: i64 = a.hi
//  rdx: u64 = b.lo
//  rcx: i64 = b.hi
//
// Outputs:
//  al: bool
bool
__int128_leq(__int128_t a, __int128_t b)
{
    // cmp  rdx, rdi    ; carry = b.lo - a.lo < 0
    // sbb  rcx, rsi    ; b.hi -= a.hi + carry
    //                  ; carry = ???
    //                  ; sign  = (b.hi < 0)
    //                  ; overflow = ???
    // setge al         ; <retval> = (sign == overflow)
    return a <= b;
}
```*/

bool
i128_leq_signed(i128 a, i128 b)
{
    i128 dst;
    bool overflow, sign;

    overflow = i128_checked_sub(&dst, a, b);
    sign     = i128_sign(dst);
    return sign || overflow || i128_eq(dst, I128_ZERO);
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


#endif /* BIGINT_I128_IMPLEMENTATION */
#endif /* BIGINT_I128_H */
