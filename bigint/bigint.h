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
    void *(*fn)(void *ptr, size_t old_size, size_t new_size, void *context);
    void *context;
} Allocator;

typedef struct {
    // Digits are stored in a little-endian fashion; the least significant
    // digit is stored at the 0th index. E.g. 1234 is stored as {4,3,2,1}.
    BigInt_Digit *data;

    // Each BigInt remembers its allocator.
    const Allocator *allocator;

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

typedef enum {
    BIGINT_POSITIVE = false,
    BIGINT_NEGATIVE = true,
} BigInt_Sign;

typedef enum {
    BIGINT_LESS     = -1,
    BIGINT_EQUAL    =  0,
    BIGINT_GREATER  =  1,
} BigInt_Comparison;

void
bigint_init(BigInt *b, const Allocator *a);

BigInt_Error
bigint_init_int(BigInt *b, int i, const Allocator *a);

/** @brief Write the integer string `s` (bounded by length `n`) of given `base`
 * into `b`.
 *
 * @param base  If 0, we will attempt to determine it based on any prefixes.
 *              Otherwise, it will default to 10.
 */
BigInt_Error
bigint_init_base_lstring(BigInt *b, const char *s, size_t n, int base,
    const Allocator *a);


/** @brief Write the integer string `s` (bounded by `n`) of base `base` into
 * the BigInt `b`.
 *
 * @param b Must be already initialized with an allocator.
 */
BigInt_Error
bigint_set_base_lstring(BigInt *b, const char *s, size_t n, int base);


/** @brief Get the string length of the would be base-`base` representation. */
size_t
bigint_base_string_length(const BigInt *b, int base);


/** @brief Write the base-N representation of `b` into a buffer from `a`.
 *  Stores the string length in `len`.
 *
 * @return The buffer if successful, else `NULL` if the buffer could not fit
 *  the string representation of `b` in the given base and/or the buffer could
 *  not be resized.
 */
const char *
bigint_to_base_lstring(const BigInt *b, const Allocator *a, int base,
    size_t *len);


/** @brief Writes the integer nul-terminated string `s`, of base `b`, into
 *  `b` to be initialized with the Allocator `a`.
 *
 * @param b     `BigInt *`
 * @param s     `const char *`
 * @param base  `int`
 * @param a     `const Allocator *`
 */
#define bigint_init_base_string(b, s, base, a) \
    bigint_init_base_lstring(b, s, strlen(s), base, a)


/** @brief Write integer string `s` (bounded by `n`), of unknown base, into
 *  `b` to be initialized with the Allocator `a`.
 *
 * If the base cannot be determined, it will default to 10.
 *
 * @param b `BigInt *`
 * @param s `const char *`
 * @param a `const Allocator *`
 */
#define bigint_init_lstring(b, s, n, a) \
    bigint_init_base_lstring(b, s, n, /*base=*/0, a)


/** @brief Write integer in the nul-terminated string `s`, of unknown base into
 *  the BigInt `b`. */
#define bigint_init_string(b, s, a) \
    bigint_init_lstring(b, s, strlen(s), a)


/** @brief Get the string length of the would-be base-10 representation. */
#define bigint_string_length(b) \
    bigint_base_string_length(b, /*base=*/10)


/** @brief Write the base-N representation of `b` into a buffer allocated by `a`. */
#define bigint_to_base_string(b, a, base) \
    bigint_to_base_lstring(b, a, base, NULL)


/** @brief Write the base-10 representation of `b` into a buffer allocated by
 * `a`. Stores the string length in `len`.
 *
 * @param b     `BigInt *`
 * @param a     `const Allocator *`
 * @param len   `size_t *`
 */
#define bigint_to_lstring(b, a, len) \
    bigint_to_base_lstring(b, a, /*base=*/10, len)


/** @brief Write the base-10 representation of `b` into a buffer allocated by
 * `a`.
 *
 * @param b     `BigInt *`
 * @param a     `const Allocator *`
 */
#define bigint_to_string(b, a) \
    bigint_to_lstring(b, a, NULL)

void
bigint_destroy(BigInt *b);

void
bigint_clear(BigInt *b);


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

/** @brief `|a| < |b|` */
bool
bigint_lt_abs(const BigInt *a, const BigInt *b);


/** @brief `|a| <= |b|` */
bool
bigint_leq_abs(const BigInt *a, const BigInt *b);


/** @brief `|a| > |b|`  <=> `|b| <  |a|` */
#define bigint_gt_abs(a, b)     bigint_lt_abs(b, a)


/** @brief `|a| >= |b|` <=> `|b| <= |a|` */
#define bigint_geq_abs(a, b)    bigint_leq_abs(b, a)


/** @brief `|a| < |b|` */
bool
bigint_lt_digit_abs(const BigInt *a, BigInt_Digit b);

// === }}} =====================================================================


#endif // BIGINT_H
