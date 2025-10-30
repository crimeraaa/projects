#ifndef BIGINT_H
#define BIGINT_H

#include <stdbool.h>
#include <stddef.h>

// [BIGINT] CONFIGURATION

// The desired integer base. For simplicity, we use some multiple of 10.
#define BIGINT_DIGIT_BASE           100

// The number of base-10 digits in `base - 1`.
#define BIGINT_DIGIT_BASE10_LENGTH  2

// The primary digit type. Must be able to hold the range `[0, base)`.
#define BIGINT_DIGIT_TYPE           unsigned char

// The secondary digit type. Must be able to hold the range `[0, base*base)`.
#define BIGINT_DIGIT_RESULT         unsigned char

// Convenience typedefs.
typedef BIGINT_DIGIT_TYPE           Big_Int_Digit;
typedef BIGINT_DIGIT_RESULT         Big_Int_Result;

typedef struct {
    // Digits are stored in a little-endian fashion; the least significant
    // digit is stored at the 0th index. E.g. 1234 is stored as {4,3,2,1}.
    Big_Int_Digit *data;

    // How many digits are validly indexable in `data`.
    int len;

    // How many digits are allocated for in `data`.
    int cap;

    bool sign;
} Big_Int;

void
big_int_init(Big_Int *b);

void
big_int_init_int(Big_Int *b, int i);

const char *
big_int_to_string(const Big_Int *b, char *buf, size_t cap);

void
big_int_destroy(Big_Int *b);


/**
 * @note(2025-10-31)
 *  For simplicity's sake we assume none of the pointers alias, especially
 *  with `out`. If they do, then that makes allocating more difficult.
 */
void
big_int_add(const Big_Int *restrict a, const Big_Int *restrict b,
    Big_Int *restrict out);

void
big_int_sub(const Big_Int *restrict a, const Big_Int *restrict b,
    Big_Int *restrict out);

void
big_int_mul(const Big_Int *restrict a, const Big_Int *restrict b,
    Big_Int *restrict out);

void
big_int_div(const Big_Int *restrict a, const Big_Int *restrict b,
    Big_Int *restrict out);

void
big_int_mod(const Big_Int *restrict a, const Big_Int *restrict b,
    Big_Int *restrict out);

#endif // BIGINT_H
