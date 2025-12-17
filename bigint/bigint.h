#ifndef BIGINT_H
#define BIGINT_H

#include <mem/allocator.h>

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
typedef BIGINT_DIGIT_TYPE           BigInt_DIGIT;
typedef BIGINT_WORD_TYPE            BigInt_WORD;

typedef enum {
    BIGINT_POSITIVE,
    BIGINT_NEGATIVE,
} BigInt_Sign;

typedef struct BigInt BigInt;
struct BigInt {
    // Digits are stored in a little-endian fashion; the least significant
    // digit is stored at the 0th index. E.g. 1234 is stored as {4,3,2,1}.
    BigInt_DIGIT *data;

    // How many digits are validly indexable in `data`.
    size_t len;

    // How many digits are allocated for in `data`.
    size_t cap;

    BigInt_Sign sign;

    // Each BigInt remembers its allocator.
    Allocator allocator;
};

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
    BIGINT_LESS     = -1,
    BIGINT_EQUAL    =  0,
    BIGINT_GREATER  =  1,
} BigInt_Comparison;

void
bigint_init(BigInt *b, Allocator allocator);

BigInt_Error
bigint_init_int(BigInt *b, int i, Allocator allocator);

/** @brief Write the integer string `s` (bounded by length `n`) of given
 *  `base` into `dst`.
 *
 * @param base  If 0, we will attempt to determine it based on any prefixes.
 *              Otherwise, it will default to 10.
 */
BigInt_Error
bigint_init_base_lstring(BigInt *dst, const char *data, size_t len, int base, Allocator allocator);


/** @brief Writes the integer nul-terminated string `s`, of base `base`,
 *  into `dst`. */
BigInt_Error
bigint_init_base_string(BigInt *dst, const char *s, int base, Allocator allocator);


/** @brief Write integer string `data` (bounded by `len`), of unknown base,
 *  into `dst` to be initialized with the Allocator `allocator`.
 *
 * If the base cannot be determined, it will default to 10.
 */
BigInt_Error
bigint_init_lstring(BigInt *dst, const char *s, size_t n, Allocator allocator);


/** @brief Write integer in the nul-terminated string `string`, of unknown
 *  base into the BigInt `dst`.
 */
BigInt_Error
bigint_init_string(BigInt *dst, const char *s, Allocator allocator);

void
bigint_destroy(BigInt *b);

void
bigint_clear(BigInt *b);

BigInt_Error
bigint_copy(BigInt *dst, const BigInt *src);


/** @brief Write the integer string `data` (bounded by `len`) of base `base`
 * into the BigInt `dst`.
 *
 * @param dst Must be already initialized with an allocator.
 */
BigInt_Error
bigint_set_base_lstring(BigInt *dst, const char *data, size_t len, int base);


/** @brief Get the string length of the would be base-`base` representation. */
size_t
bigint_base_string_length(const BigInt *src, int base);


/** @brief Get the string length of the would-be base-10 representation. */
size_t
bigint_string_length(const BigInt *src);


/** @brief Write the base-N representation of `src` into a buffer from
 *  `allocator`.
 *
 * @param len
 *  Optional out-parameter to store the number of characters in the string.
 *
 * @return The buffer if successful, else `NULL` if the buffer could not fit
 *  the string representation of `b` in the given base and/or the buffer could
 *  not be resized.
 */
const char *
bigint_to_base_lstring(const BigInt *src, int base, size_t *len, Allocator allocator);


/** @brief Write the base-`base` representation of `src`. */
const char *
bigint_to_base_string(const BigInt *src, int base, Allocator allocator);


/** @brief Write the base-10 representation of `src`. */
const char *
bigint_to_lstring(const BigInt *src, size_t *len, Allocator allocator);


/** @brief Write the base-10 representation of `src`. */
const char *
bigint_to_string(const BigInt *src, Allocator allocator);


// === ARITHMETIC ========================================================== {{{


/** @brief `dst = a + b`
 *
 * @param dst
 *  Must be already initialized with an allocator.
 *  May alias either `a` and/or `b`.
 */
BigInt_Error
bigint_add(BigInt *dst, const BigInt *a, const BigInt *b);


/** @brief `dst = a - b`
 *
 * @param dst
 *  Must already be initialized with an allocator.
 *  May alias either `a` and/or `b`.
 */
BigInt_Error
bigint_sub(BigInt *dst, const BigInt *a, const BigInt *b);


/** @brief `dst = a * b`
 *
 * @param dst
 *  Must already be initialized with an allocator.
 *  May alias either `a` and/or `b`.
 */
BigInt_Error
bigint_mul(BigInt *dst, const BigInt *a, const BigInt *b);


/** @brief `dst = a / b`
 *
 * @param dst
 *  Must already be initialized with an allocator.
 *  May alias either `a` nor `b`.
 */
BigInt_Error
bigint_div_bigint(BigInt *dst, const BigInt *a, const BigInt *b);


/** @brief `dst = a % b` */
BigInt_Error
bigint_mod_bigint(BigInt *dst, const BigInt *a, const BigInt *b);

BigInt_Error
bigint_add_digit(BigInt *dst, const BigInt *a, BigInt_DIGIT b);

BigInt_Error
bigint_sub_digit(BigInt *dst, const BigInt *a, BigInt_DIGIT b);


/** @brief `dst = a * b`
 *
 * @param dst
 *  Must already be initialized with an allocator.
 *  May alias `a` because we only iterate over the digit sequence once.
 */
BigInt_Error
bigint_mul_digit(BigInt *dst, const BigInt *a, BigInt_DIGIT b);


/** @brief `dst = -a` */
BigInt_Error
bigint_neg(BigInt *dst, const BigInt *src);


// === }}} =====================================================================


// === COMPARISON ========================================================== {{{


BigInt_Comparison
bigint_compare(const BigInt *a, const BigInt *b);

BigInt_Comparison
bigint_compare_abs(const BigInt *a, const BigInt *b);

BigInt_Comparison
bigint_compare_digit(const BigInt *a, BigInt_DIGIT b);

BigInt_Comparison
bigint_compare_digit_abs(const BigInt *a, BigInt_DIGIT b);

/** @brief Queries `b == 0`. Assumes that `-0` is impossible. */
bool
bigint_is_zero(const BigInt *b);


/** @brief Queries `b < 0`. Assumes that `-0` is impossible. */
bool
bigint_is_neg(const BigInt *b);


/** @brief Queries `b >= 0`. */
bool
bigint_is_pos(const BigInt *b);


bool
bigint_eq(const BigInt *a, const BigInt *b);

bool
bigint_lt(const BigInt *a, const BigInt *b);

bool
bigint_leq(const BigInt *a, const BigInt *b);

bool
bigint_neq(const BigInt *a, const BigInt *b);

bool
bigint_gt(const BigInt *a, const BigInt *b);

bool
bigint_geq(const BigInt *a, const BigInt *b);

bool
bigint_eq_digit(const BigInt *a, BigInt_DIGIT b);

bool
bigint_lt_digit(const BigInt *a, BigInt_DIGIT b);

bool
bigint_leq_digit(const BigInt *a, BigInt_DIGIT b);

bool
bigint_neq_digit(const BigInt *a, BigInt_DIGIT b);

bool
bigint_gt_digit(const BigInt *a, BigInt_DIGIT b);

bool
bigint_geq_digit(const BigInt *a, BigInt_DIGIT b);

bool
bigint_eq_abs(const BigInt *a, const BigInt *b);

bool
bigint_lt_abs(const BigInt *a, const BigInt *b);

bool
bigint_leq_abs(const BigInt *a, const BigInt *b);

bool
bigint_neq_abs(const BigInt *a, const BigInt *b);

bool
bigint_gt_abs(const BigInt *a, const BigInt *b);

bool
bigint_geq_abs(const BigInt *a, const BigInt *b);

bool
bigint_eq_digit_abs(const BigInt *a, BigInt_DIGIT b);

bool
bigint_lt_digit_abs(const BigInt *a, BigInt_DIGIT b);

bool
bigint_leq_digit_abs(const BigInt *a, BigInt_DIGIT b);

bool
bigint_neq_digit_abs(const BigInt *a, BigInt_DIGIT b);

bool
bigint_gt_digit_abs(const BigInt *a, BigInt_DIGIT b);

bool
bigint_geq_digit_abs(const BigInt *a, BigInt_DIGIT b);

// === }}} =====================================================================


#endif // BIGINT_H
