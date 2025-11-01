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
#define BIGINT_DIGIT_DIGIT          uint32_t

// The secondary digit type. Must be able to hold the range `[0, base*base)`.
#define BIGINT_DIGIT_RESULT         uint64_t

// Convenience typedefs.
typedef BIGINT_DIGIT_DIGIT          BigInt_Digit;
typedef BIGINT_DIGIT_RESULT         BigInt_Result;

typedef struct {
    // Digits are stored in a little-endian fashion; the least significant
    // digit is stored at the 0th index. E.g. 1234 is stored as {4,3,2,1}.
    BigInt_Digit *data;

    // How many digits are validly indexable in `data`.
    int len;

    // How many digits are allocated for in `data`.
    int cap;

    bool sign;
} BigInt;

void
bigint_init(BigInt *b);

void
bigint_init_int(BigInt *b, int i);

void
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
void
bigint_add(BigInt *out, const BigInt *a, const BigInt *b);

void
bigint_add_digit(BigInt *out, const BigInt *a, BigInt_Digit digit);


/** @brief `out = a - b`
 *
 * @note(2025-11-01) Assumptions:
 *  1.) `out` is already initialized if it needed to be.
 *  2.) `out` may alias either of the input parameters `a` or `b`.
 */
void
bigint_sub(BigInt *out, const BigInt *a, const BigInt *b);


/** @brief `out = a * b`
 *
 * @note(2025-11-01)
 *  `out` may NOT alias either of the input parameters `a` or `b`.
 */
void
bigint_mul(BigInt *restrict out, const BigInt *a, const BigInt *b);


/** @brief `out = a * digit`
 *
 * @note(2025-11-01) Assumptions
 *  1.) `out` may alias `a` because we only iterate over the digit sequence
 *      once in this case.
 */
void
bigint_mul_digit(BigInt *out, const BigInt *a, BigInt_Digit digit);


/** @brief = `out = a / b`
 *
 * @note(2025-11-01)
 *  `out` may NOT alias either of the input parameters `a` or `b`.
 */
void
bigint_div(BigInt *restrict out, const BigInt *a, const BigInt *b);


/** @brief = `out = a % b` */
void
bigint_mod(BigInt *restrict out, const BigInt *a, const BigInt *b);

#endif // BIGINT_H
