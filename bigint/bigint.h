#ifndef BIGINT_H
#define BIGINT_H

#include <stdbool.h>    // bool
#include <stddef.h>     // size_t
#include <stdint.h>     // u?int[\d]+_t, intmax_t

// === CONFIGUATION ======================================================== {{{

// The desired integer base. For simplicity, we use some multiple of 10.
#define BIGINT_DIGIT_BASE           1000000000

// The number of base-2 digits in `base - 1`.
#define BIGINT_DIGIT_BASE2_LENGTH   30

// The number of base-8 digits in `base - 1`.
#define BIGINT_DIGIT_BASE8_LENGTH   10

// The number of base-10 digits in `base - 1`.
#define BIGINT_DIGIT_BASE10_LENGTH  9

// The number of base-16 digits in `base - 1`.
#define BIGINT_DIGIT_BASE16_LENGTH  8

// The primary digit type used for addition and some multiplication.
// Must be able to hold the range `[0, base)`.
#define BIGINT_DIGIT_TYPE           uint32_t

// The secondary digit type used for subtraction and multiplication.
// Must be able to hold the range `[-(base*2), base*base)`.
#define BIGINT_WORD_TYPE            int64_t

// === }}} =====================================================================

#define BIGINT_DIGIT_MAX            (BIGINT_DIGIT_BASE - 1)

// Convenience typedefs.
typedef BIGINT_DIGIT_TYPE           BigInt_Digit;
typedef BIGINT_WORD_TYPE            BigInt_Word;

typedef struct {
    // Digits are stored in a little-endian fashion; the least significant
    // digit is stored at the 0th index. E.g. 1234 is stored as {4,3,2,1}.
    BigInt_Digit *data;

    // How many digits are validly indexable in `data`.
    int len;

    // How many digits are allocated for in `data`.
    int cap;

    // `true` indicates negative (e.g. sign bit toggled) while `false`
    // indicates positive.
    bool sign;
} BigInt;

typedef enum {
    BIGINT_OK,

    // When parsing a string, we received an invalid integer base prefix.
    BIGINT_ERROR_BASE,

    // When parsing a string, we found an invalid character of a certain base.
    BIGINT_ERROR_DIGIT,

    // We failed to (re)allocate something.
    BIGINT_ERROR_MEMORY,
} BigInt_Error;

void
bigint_init(BigInt *b);

BigInt_Error
bigint_init_int(BigInt *b, int i);

BigInt_Error
bigint_init_string(BigInt *b, const char *s);

size_t
bigint_string_length(const BigInt *b, int base);

/** @brief Write the base-10 representation of `b` into `buf[:cap]`. */
const char *
bigint_to_lstring(const BigInt *b, char *buf, size_t cap, size_t *len);

#define bigint_to_string(b, buf, cap)   bigint_to_lstring(b, buf, cap, NULL)

void
bigint_destroy(BigInt *b);


// === ARITHMETIC ========================================================== {{{


/**
 * @brief
 *  `out = a + b`
 *
 * @note(2025-11-01) Assumptions:
 *  1.) `out` is already initialized if it needed to be.
 *  2.) `out` may alias either of the input parameters `a` or `b`.
 */
BigInt_Error
bigint_add(BigInt *out, const BigInt *a, const BigInt *b);


/** @brief `out = a - b`
 *
 * @note(2025-11-01) Assumptions:
 *  1.) `out` is already initialized if it needed to be.
 *  2.) `out` may alias either of the input parameters `a` or `b`.
 */
BigInt_Error
bigint_sub(BigInt *out, const BigInt *a, const BigInt *b);

/** @brief `out = a * b`
 *
 * @note(2025-11-01)
 *  `out` may NOT alias either of the input parameters `a` or `b`.
 */
BigInt_Error
bigint_mul(BigInt *restrict out, const BigInt *a, const BigInt *b);


/** @brief = `out = a / b`
 *
 * @note(2025-11-01)
 *  `out` may NOT alias either of the input parameters `a` or `b`.
 */
BigInt_Error
bigint_div(BigInt *restrict out, const BigInt *a, const BigInt *b);


/** @brief = `out = a % b` */
BigInt_Error
bigint_mod(BigInt *restrict out, const BigInt *a, const BigInt *b);

BigInt_Error
bigint_add_digit(BigInt *out, const BigInt *a, BigInt_Digit b);

BigInt_Error
bigint_sub_digit(BigInt *out, const BigInt *a, BigInt_Digit b);


/** @brief `out = a * b`
 *
 * @note(2025-11-01) Assumptions
 *  1.) `out` may alias `a` because we only iterate over the digit sequence
 *      once in this case.
 */
BigInt_Error
bigint_mul_digit(BigInt *out, const BigInt *a, BigInt_Digit b);


/** @brief `out = -a` */
BigInt_Error
bigint_neg(BigInt *out, const BigInt *a);


// === }}} =====================================================================


// === COMPARISON ========================================================== {{{


/** @brief Queries `b == 0`. Assumes that `-0` is impossible. */
bool
bigint_is_zero(const BigInt *b);


/** @brief Queries `b < 0`. Assumes that `-0` is impossible. */
bool
bigint_is_neg(const BigInt *b);


/** @brief Queries `b >= 0`. */
#define bigint_is_pos(b)    (!bigint_is_neg(b))


/** @brief Queries `a == b`, considering signedness. */
bool
bigint_eq(const BigInt *a, const BigInt *b);


/** @brief Queries `a < b`, considering signedness. */
bool
bigint_lt(const BigInt *a, const BigInt *b);


/** @brief Queries `a <= b`, considering signedness. */
bool
bigint_leq(const BigInt *a, const BigInt *b);

// (x != y) == !(x == y)
#define bigint_neq(a, b)    (!bigint_eq(a, b))

// (x > y) == (y < x)
#define bigint_gt(a, b)     bigint_lt(b, a)

// (x >= y) == (y <= x)
#define bigint_geq(a, b)    bigint_leq(b, a)

bool
bigint_eq_digit(const BigInt *a, BigInt_Digit b);

bool
bigint_lt_digit(const BigInt *a, BigInt_Digit b);

bool
bigint_leq_digit(const BigInt *a, BigInt_Digit b);

// (x != y) == !(x == y)
#define bigint_neq_digit(a, b)  (!bigint_eq_digit(a, b))

// (x > y) == !(x <= y)
#define bigint_gt_digit(a, b)   (!bigint_leq_digit(a, b))

// (x >= y) == !(x < y)
#define bigint_geq_digit(a, b)  (!bigint_lt_digit(a, b))


// === }}} =====================================================================


#endif // BIGINT_H
