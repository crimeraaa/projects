/** @link https://github.com/michaeljclark/c128/blob/trunk/include/integer.h */
#ifndef BIGINT_I128_H
#define BIGINT_I128_H

#include <projects.h>

// Little-endian i128.
typedef struct i128le i128le;
struct i128le {
    u64 lo, hi;
};

// Big-endian i128.
typedef struct i128be i128be;
struct i128be {
    u64 hi, lo;
};

// Windows is almost always little endian, else check what GCC/Clang say.
#if defined(_MSC_VER) \
    || (defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__))

#define BIGINT_I128_ENDIAN_LITTLE   1
#define BIGINT_I128_ENDIAN_BIG      0
#define BIGINT_I128_TYPE            i128le
#define I128_ONE                    COMPOUND_LITERAL(i128le){1, 0}

#elif defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)

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
i128_from_i64(i64 a);

i128
i128_from_u64(u64 a);


/** @brief Get the sign bit of `a`. As in two's complement, a sign bit of 0
 *  indicates a positive value while a sign bit of 1 is negative. */
bool
i128_sign(i128 a);

i128
i128_from_lstring(const char *restrict s, size_t n, const char **restrict end_ptr, int base);

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
i128_logical_left_shift(i128 a, unsigned int n);


/** @brief `a >> n` */
i128
i128_logical_right_shift(i128 a, unsigned int n);


// ARITHMETIC OPERATIONS


/** @brief `|a|` as an unsigned value. Signed representation may overflow. */
i128
i128_abs(i128 a);


/** @brief `-a` */
i128
i128_neg(i128 a);


/** @brief `*dst = a + b`.
 *
 * @return whether `a + b` as an unsigned addition overflowed.
 */
bool
i128_checked_add_unsigned(i128 *dst, i128 a, i128 b);


/** @brief `*dst = a + b`.
 *
 * @return whether `a + b` as a signed addition overflowed.
 */
bool
i128_checked_add_signed(i128 *dst, i128 a, i128 b);


/** @brief `a + b` without returning any information on overflows.
 *  Works properly for either signed or unsigned addition. */
i128
i128_add(i128 a, i128 b);


/** @brief `*dst = a - b`.
 *
 * @return whether `a - b` as an unsigned subtraction overflowed.
 */
bool
i128_checked_sub_unsigned(i128 *dst, i128 a, i128 b);


/** @brief `*dst = a - b`.
 *
 * @return whether `a - b` as a signed subtraction overflowed.
 */
bool
i128_checked_sub_signed(i128 *dst, i128 a, i128 b);


/** @brief `a - b` without returning any information on overflows.
 *  Works properly for either signed or unsigned subtraction. */
i128
i128_sub(i128 a, i128 b);


// /** @brief `*dst = a * b`.
//  *
//  * @return whether `a * b` as an unsigned multiplication overflowed.
//  */
// bool
// i128_checked_mul_unsigned(i128 *dst, i128 a, i128 b);


/** @brief `a * b` without returning any information on overflows.
 *  Works properly for either signed or unsigned multiplication. */
i128
i128_mul(i128 a, i128 b);

i128
i128_mul_u64(i128 a, u64 b);

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


#endif /* BIGINT_I128_H */
