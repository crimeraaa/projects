#ifndef BIGINT_H
#define BIGINT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// [BIGINT] CONFIGURATION

// The desired integer base. For simplicity, we use some multiple of 10.
#define BIGINT_DIGIT_BASE           1000000000

// The number of base-10 digits in `base - 1`.
#define BIGINT_DIGIT_BASE10_LENGTH  9

// The primary digit type. Must be able to hold the range `[0, base)`.
#define BIGINT_DIGIT_TYPE           uint32_t

// The secondary digit type. Must be able to hold the range `[0, base*base)`.
#define BIGINT_DIGIT_RESULT         uint64_t

// Convenience typedefs.
typedef BIGINT_DIGIT_TYPE           BigInt_Digit;
typedef BIGINT_DIGIT_RESULT         BigInt_Result;

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

const char *
bigint_to_lstring(const BigInt *b, char *buf, size_t cap, size_t *len);

#define bigint_to_string(b, buf, cap)   bigint_to_lstring(b, buf, cap, NULL)

void
bigint_destroy(BigInt *b);


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

BigInt_Error
bigint_add_digit(BigInt *out, const BigInt *a, BigInt_Digit b);


/** @brief `out = a - b`
 *
 * @note(2025-11-01) Assumptions:
 *  1.) `out` is already initialized if it needed to be.
 *  2.) `out` may alias either of the input parameters `a` or `b`.
 */
BigInt_Error
bigint_sub(BigInt *out, const BigInt *a, const BigInt *b);

BigInt_Error
bigint_sub_digit(BigInt *out, const BigInt *a, BigInt_Digit b);


/** @brief `out = a * b`
 *
 * @note(2025-11-01)
 *  `out` may NOT alias either of the input parameters `a` or `b`.
 */
BigInt_Error
bigint_mul(BigInt *restrict out, const BigInt *a, const BigInt *b);


/** @brief `out = a * b`
 *
 * @note(2025-11-01) Assumptions
 *  1.) `out` may alias `a` because we only iterate over the digit sequence
 *      once in this case.
 */
BigInt_Error
bigint_mul_digit(BigInt *out, const BigInt *a, BigInt_Digit b);


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

bool
bigint_is_zero(const BigInt *b);

bool
bigint_eq(const BigInt *a, const BigInt *b);

bool
bigint_lt(const BigInt *a, const BigInt *b);

bool
bigint_leq(const BigInt *a, const BigInt *b);

#define bigint_neq(a, b)    (!bigint_eq(a, b))
#define bigint_gt(a, b)     bigint_lt(b, a)
#define bigint_geq(a, b)    bigint_leq(b, a)

#endif // BIGINT_H
