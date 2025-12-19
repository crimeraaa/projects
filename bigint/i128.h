/** @link https://github.com/michaeljclark/c128/blob/trunk/include/integer.h */
#ifndef BIGINT_I128_H
#define BIGINT_I128_H

#include <projects.h>

typedef struct u128le u128le;
struct u128le {
    u64 lo, hi;
};

typedef struct i128le i128le;
struct i128le {
    u64 lo;
    i64 hi;
};

typedef struct u128be u128be;
struct u128be {
    u64 hi, lo;
};

typedef struct i128be i128be;
struct i128be {
    i64 hi;
    u64 lo;
};

// Windows is almost always little endian, else check what GCC/Clang say.
#if defined(_MSC_VER) \
    || (defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__))

#define BIGINT_I128_ENDIAN_LITTLE   1
#define BIGINT_I128_ENDIAN_BIG      0

// u128le
#define BIGINT_U128_TYPE    u128le
#define U128_MAX            COMPOUND_LITERAL(u128le){U64_MAX,   U64_MAX}
#define U128_ONE            COMPOUND_LITERAL(u128le){1,         0}

// i128be
#define BIGINT_I128_TYPE    i128le
#define I128_MAX            COMPOUND_LITERAL(i128le){U64_MAX,   I64_MAX}
#define I128_MIN            COMPOUND_LITERAL(i128le){0,         I64_MIN}
#define I128_ONE            COMPOUND_LITERAL(i128le){1,         0}

#elif defined(__BYTE_ORDER__) && (__BYTE_ORDER__ == __ORDER_BIG_ENDIAN__)

#define BIGINT_I128_ENDIAN_LITTLE   0
#define BIGINT_I128_ENDIAN_BIG      1

// u128be
#define BIGINT_U128_TYPE    u128be
#define U128_MAX            COMPOUND_LITERAL(u128be){U64_MAX,   U64_MAX}
#define U128_ONE            COMPOUND_LITERAL(u128be){0,         1}

// i128be
#define BIGINT_I128_TYPE    i128be
#define I128_MAX            COMPOUND_LITERAL(i128be){U64_MAX,   U64_MAX}
#define I128_MIN            COMPOUND_LITERAL(i128be){0,         I64_MIN}
#define I128_ONE            COMPOUND_LITERAL(i128be){0,         1}

#else

#define BIGINT_I128_ENDIAN_LITTLE   0
#define BIGINT_I128_ENDIAN_BIG      0

#endif

// Sanity check.
#if BIGINT_I128_ENDIAN_LITTLE == BIGINT_I128_ENDIAN_BIG
#error Imposible endianness!
#endif

typedef BIGINT_U128_TYPE u128;
typedef BIGINT_I128_TYPE i128;

#define U128_ZERO   COMPOUND_LITERAL(u128){0, 0}
#define I128_ZERO   COMPOUND_LITERAL(i128){0, 0}

#define BIGINT_I128_IMPLEMENTATION

// === CONVERSION OPERATIONS =============================================== {{{

u128
u128_from_u64(u64 a);

u128
u128_from_i64(i64 a);

u128
u128_from_i128(i128 a);

i128
i128_from_i64(i64 a);

i128
i128_from_u64(u64 a);

i128
i128_from_u128(u128 a);

bool
i128_sign(i128 a);

u128
u128_from_string(const char *restrict s, size_t n, const char **restrict end_ptr, int base);

i128
i128_from_string(const char *restrict s, size_t n, const char **restrict end_ptr, int base);

// === }}} =====================================================================
// === BITWISE OPERATIONS ================================================== {{{

u128
u128_not(u128 a);

u128
u128_and(u128 a, u128 b);

u128
u128_or(u128 a, u128 b);

u128
u128_xor(u128 a, u128 b);

u128
u128_shift_left_logical(u128 a, unsigned int n);

u128
u128_shift_right_logical(u128 a, unsigned int n);


// === }}} =====================================================================
// === ARITHMETIC OPERATIONS =============================================== {{{


/** @note `|a|` may cause signed overflow when `a == min(i128)`. */
i128
i128_abs(i128 a);


/** @brief `|a|` which works also for `a == min(i128)`. */
u128
i128_abs_unsigned(i128 a);

u128
u128_neg(u128 a);

i128
i128_neg(i128 a);

u128
u128_add(u128 a, u128 b);

u128
u128_sub(u128 a, u128 b);

u128
u128_mul(u128 a, u128 b);

u128
u128_add_u64(u128 a, u64 b);

u128
u128_mul_u64(u128 a, u64 b);

i128
i128_add(i128 a, i128 b);

i128
i128_sub(i128 a, i128 b);

i128
i128_mul(i128 a, i128 b);


/** @brief `*dst = a + b`.
 *
 * @param [out] dst Always assigned no matter what.
 *
 * @return
 *  `true` if the addition resulted in unsigned overflow, else `false`.
 */
bool
u128_checked_add(u128 *dst, u128 a, u128 b);


/** @brief `*dst = a - b`.
 *
 * @param [out] dst Always assigned no matter what.
 *
 * @return
 *  `true` if the subtraction resulted in unsigned overflow, else `false`.
 */
bool
u128_checked_sub(u128 *dst, u128 a, u128 b);


/** @brief `*dst = a * b`.
 *
 * @param [out] dst Always assigned no matter what.
 *
 * @return
 *  `true` if the multiplication resulted in unsigned overflow, else `false`.
 */
bool
u128_checked_mul(u128 *dst, u128 a, u128 b);


/** @brief `*dst = a + b`.
 *
 * @param [out] dst Always assigned no matter what.
 *
 * @return
 *  `true` if the addition resulted in signed overflow, else `false`.
 */
bool
i128_checked_add(i128 *dst, i128 a, i128 b);


/** @brief `*dst = a - b`.
 *
 * @param [out] dst Always assigned no matter what.
 *
 * @return
 *  `true` if the addition resulted in signed overflow, else `false`.
 */
bool
i128_checked_sub(i128 *dst, i128 a, i128 b);

// === }}} =====================================================================
// === COMPARISON OPERATIONS =============================================== {{{

bool
u128_eq(u128 a, u128 b);

bool
u128_lt(u128 a, u128 b);

bool
u128_leq(u128 a, u128 b);

bool
u128_neq(u128 a, u128 b);

bool
u128_gt(u128 a, u128 b);

bool
u128_geq(u128 a, u128 b);

bool
i128_eq(i128 a, i128 b);

bool
i128_lt(i128 a, i128 b);

bool
i128_leq(i128 a, i128 b);

bool
i128_neq(i128 a, i128 b);

bool
i128_gt(i128 a, i128 b);

bool
i128_geq(i128 a, i128 b);

// === }}} =====================================================================


#endif /* BIGINT_I128_H */
