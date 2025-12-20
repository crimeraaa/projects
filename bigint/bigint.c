#include <stdio.h>  // fprintf
#include <string.h> // strlen

#include "bigint.h"
#include <utils/strings.h>


void
bigint_init(BigInt *b, Allocator allocator)
{
    b->data      = NULL;
    b->allocator = allocator;
    b->len       = 0;
    b->cap       = 0;
    b->sign      = BIGINT_POSITIVE;
}

static BigInt_Error
internal_bigint_init_len_cap(BigInt *b, size_t len, size_t cap, Allocator allocator)
{
    b->data = array_make(BigInt_DIGIT, cap, allocator);
    if (b->data == NULL) {
        return BIGINT_ERROR_MEMORY;
    }
    b->allocator = allocator;
    b->len       = len;
    b->cap       = cap;
    b->sign      = BIGINT_POSITIVE;
    return BIGINT_OK;
}

static BigInt_Error
internal_bigint_init_len(BigInt *b, size_t len, Allocator allocator)
{
    return internal_bigint_init_len_cap(b, len, len, allocator);
}


/** @brief Count the number of base-`base` digits in `value`.
 *  Assumes that the original value was positive to begin with. */
static int
internal_count_digits(uintmax_t value, int base)
{
    // The below loop will never start.
    if (value == 0) {
        return 1;
    }

    // e.g. 9   (1 iteration)
    //      10  (2 iterations)
    //      0   (0 iterations)
    int count = 0;
    while (value > 0) {
        value /= cast(uintmax_t)base;
        count += 1;
    }
    return count;
}

/** @brief Workhorse function. Do not expose as `intmax_t` is a nightmare for APIs.
 * See: https://thephd.dev/intmax_t-hell-c++-c */
static BigInt_Error
internal_bigint_init_any_int(BigInt *b, intmax_t value, Allocator allocator)
{
    uintmax_t value_abs;
    BigInt_Error err;
    size_t digit_count;

    // Value checking.
    value_abs = (value >= 0) ? cast(uintmax_t)value : -cast(uintmax_t)value;
    b->sign   = (value >= 0) ? BIGINT_POSITIVE : BIGINT_NEGATIVE;
    if (b->sign == BIGINT_NEGATIVE) {
        // Concept check: -1234 % 10 == 6; We do not want this!
        value = -value;
    }

    digit_count = cast(size_t)internal_count_digits(value_abs, BIGINT_DIGIT_BASE);
    err  = internal_bigint_init_len_cap(b, digit_count, digit_count, allocator);
    if (err) return err;

    for (size_t i = 0; i < digit_count; i++) {
        // Get the (current) least significant digit.
        // Concept check: 1234 % 10 == 4
        b->data[i] = cast(BigInt_DIGIT)(value % BIGINT_DIGIT_BASE);

        // Pop the (current) least significant digit.
        // Concept check: 1234 // 10 == 123
        value /= BIGINT_DIGIT_BASE;
    }
    return err;
}

BigInt_Error
bigint_init_int(BigInt *b, int value, Allocator allocator)
{
    return internal_bigint_init_any_int(b, cast(intmax_t)value, allocator);
}

static BigInt_Sign
internal_string_get_sign(String *s)
{
    BigInt_Sign sign = BIGINT_POSITIVE;
    for (; s->len > 1; s->data += 1, s->len -= 1) {
        char ch = s->data[0];
        if (char_is_space(ch) || ch == '+') {
            // Unary plus does nothing, e.g. +2, -+2
            continue;
        } else if (ch == '-') {
            // Unary minus flips the sign, e.g. -2, +-2, --2, -+-2, ---2
            sign = (sign == BIGINT_NEGATIVE) ? BIGINT_POSITIVE : BIGINT_NEGATIVE;
        } else {
            break;
        }
    }
    return sign;
}

static int
internal_string_get_base(String *s)
{
    int base = 10;
    if (s->len > 2 && s->data[0] == '0') {
        char prefix = s->data[1];
        switch (prefix) {
        case 'b': case 'B': base = 2;  break;
        case 'd': case 'D': base = 10; break;
        case 'o': case 'O': base = 8;  break;
        case 'X': case 'x': base = 16; break;
        default:
            return -1;
        }
        // Skip "0[bBdDoOxX]" in the string.
        s->data += 2;
        s->len  -= 2;
    }
    return base;
}

static void
internal_string_trim(String *s)
{
    // Skip leading whitespace
    while (char_is_space(s->data[0])) {
        s->data += 1;
        s->len  -= 1;
    }
    // Skip trailing whitespace
    while (s->len > 0 && char_is_space(s->data[s->len - 1])) {
        s->len -= 1;
    }
}

static BigInt_DIGIT
internal_char_to_digit(char ch, int base)
{
    BigInt_DIGIT i = BIGINT_DIGIT_MAX;
    // Convert character to potential integer representation up to base-36
    if (char_is_digit(ch)) {
        i = cast(BigInt_DIGIT)(ch - '0');
    } else if (char_is_lower(ch)) {
        i = cast(BigInt_DIGIT)(ch - 'a' + 10);
    } else if (char_is_upper(ch)) {
        i = cast(BigInt_DIGIT)(ch - 'A' + 10);
    }

    // Ensure above conversion is valid
    if (i < cast(BigInt_DIGIT)base) {
        return i;
    }
    return BIGINT_DIGIT_MAX;
}

BigInt_Error
bigint_init_base_lstring(BigInt *dst, const char *s, size_t n, int base, Allocator allocator)
{
    bigint_init(dst, allocator);
    return bigint_set_base_lstring(dst, s, n, base);
}

BigInt_Error
bigint_init_base_string(BigInt *dst, const char *s, int base, Allocator allocator)
{
    size_t n = strlen(s);
    return bigint_init_base_lstring(dst, s, n, base, allocator);
}

BigInt_Error
bigint_init_lstring(BigInt *dst, const char *s, size_t n, Allocator allocator)
{
    int base = 0;
    return bigint_init_base_lstring(dst, s, n, base, allocator);
}

BigInt_Error
bigint_init_string(BigInt *dst, const char *s, Allocator allocator)
{
    size_t n = strlen(s);
    int base = 0;
    return bigint_init_base_lstring(dst, s, n, base, allocator);
}

void
bigint_destroy(BigInt *b)
{
    array_delete(b->data, b->cap, b->allocator);
}

void
bigint_clear(BigInt *b)
{
    b->sign = BIGINT_POSITIVE;
    b->len  = 0;
}

static bool
internal_bigint_resize(BigInt *b, size_t n)
{
    if (n > b->cap) {
        BigInt_DIGIT *ptr;

        // Don't free `b->data` because the outermost caller still owns it.
        ptr = array_resize(BigInt_DIGIT, b->data, b->cap, n, b->allocator);
        if (ptr == NULL) {
            return false;
        }
        b->data = ptr;
        b->cap  = n;
    }
    // Resizing always changes the user-facing length.
    b->len = n;
    return true;
}

BigInt_Error
bigint_copy(BigInt *dst, const BigInt *src)
{
    // Nothing to do?
    if (dst == src) {
        return BIGINT_OK;
    }

    dst->allocator = src->allocator;
    dst->sign      = src->sign;

    size_t len = src->len;
    if (!internal_bigint_resize(dst, len)) {
        return BIGINT_ERROR_MEMORY;
    }
    for (size_t i = 0; i < len; i += 1) {
        dst->data[i] = src->data[i];
    }
    return BIGINT_OK;
}

static void
internal_bigint_swap_ptr(const BigInt **a, const BigInt **b)
{
    const BigInt *tmp = *a;
    *a = *b;
    *b = tmp;
}

static void
internal_bigint_swap(BigInt *a, BigInt *b)
{
    BigInt tmp = *a;
    *a = *b;
    *b = tmp;
}

/** @brief Removes leading zeroes. If `|b| == 0`, then it is set positive.
 * Otherwise the sign is not touched. */
static BigInt_Error
internal_bigint_clamp(BigInt *b)
{
    // Have leading zeroes (zeroes in the place of MSD)?
    while (b->len > 0 && b->data[b->len - 1] == 0) {
        b->len -= 1;
    }

    if (bigint_is_zero(b)) {
        b->sign = BIGINT_POSITIVE;
    }
    return BIGINT_OK;
}

BigInt_Error
bigint_set_base_lstring(BigInt *dst, const char *data, size_t len, int base)
{
    String m = {data, len};
    internal_string_trim(&m);

    BigInt_Error err = BIGINT_OK;

    // Check for unary minus or unary plus. Don't set the sign yet;
    // Because the intermediate calculations will undo it anyway.
    // Delaying this also accounts for inputs like "-0".
    BigInt_Sign sign = internal_string_get_sign(&m);

    // No explicit base given, try to check for a prefix.
    if (base == 0) {
        base = internal_string_get_base(&m);
    }

    // Base 1 is silly. Base-0 is impossible. Base > 36 is unsupported due to
    // case sensitivity.
    if (base < 2 || base > 36) {
        err = BIGINT_ERROR_BASE;
        goto fail;
    }

    const char *start = m.data;
    const char *end   = m.data + m.len;
    for (const char *it = start; it < end; it++) {
        BigInt_DIGIT digit;
        char ch = *it;
        if (ch == '_' || ch == ',') {
            continue;
        }

        digit = internal_char_to_digit(ch, base);
        if (digit != BIGINT_DIGIT_MAX) {
            bigint_mul_digit(dst, dst, cast(BigInt_DIGIT)base);
            bigint_add_digit(dst, dst, digit);
        } else {
            err = BIGINT_ERROR_DIGIT;
fail:
            bigint_destroy(dst);
            return err;
        }
    }

    // Finalize the sign after all the intermediate calculations.
    // We assume all the above calculations already clamped us.
    dst->sign = bigint_is_zero(dst) ? BIGINT_POSITIVE : sign;
    return BIGINT_OK;

}

/** @brief Gets the place-value of the MSD of `d`, e.g. 1234 returns 1000. */
static BigInt_DIGIT
internal_digit_place_value(BigInt_DIGIT d, int base)
{
    // Use Result type in case of Digit overflow.
    BigInt_WORD place = 1;

    // Check the next place value; may be the one we are looking for.
    // E.g. 9 returns 1 but 10 returns 10.
    while (place * cast(BigInt_WORD)base <= cast(BigInt_WORD)d) {
        place *= cast(BigInt_WORD)base;
    }
    return cast(BigInt_DIGIT)place;
}

/** @brief Get the maximum number of base-`base` digits that would fit in a
 * base-`DIGIT_BASE` number. */
static int
internal_digit_length_in_base(int base)
{
    // Check the precalculated ones
    switch (base) {
    case 2:  return BIGINT_DIGIT_BASE2_LENGTH;
    case 8:  return BIGINT_DIGIT_BASE8_LENGTH;
    case 10: return BIGINT_DIGIT_BASE10_LENGTH;
    case 16: return BIGINT_DIGIT_BASE16_LENGTH;
    }
    // Always works, but marginally slower- not O(1) time.
    return internal_count_digits(BIGINT_DIGIT_MAX, base);
}

size_t
bigint_base_string_length(const BigInt *b, int base)
{
    if (bigint_is_zero(b)) {
        return 1;
    }

    size_t n_chars = 0;
    // Account for '-' char
    if (bigint_is_neg(b)) {
        n_chars += 1;
    }

    // Binary, octal and hexadecimal have widely-used base prefixes
    switch (base) {
    case 2:
    case 8:
    case 16: n_chars += 2; break;
    }

    size_t msd_index = b->len - 1;

    // MSD has variable width with no leading zeroes.
    n_chars += cast(size_t)internal_count_digits(b->data[msd_index], base);

    // Beyond MSD, all remaining digits have fixed width.
    n_chars += (b->len - 1) * cast(size_t)internal_digit_length_in_base(base);
    return n_chars;
}

size_t
bigint_string_length(const BigInt *src)
{
    int base = 10;
    return bigint_base_string_length(src, base);
}


/** @brief Writes all significant digits from MSD to LSD. */
static bool
internal_string_append_digit(String_Builder *sb, BigInt_DIGIT digit, int base)
{
    BigInt_DIGIT pv = internal_digit_place_value(digit, base);
    // Have digits to process and won't divide by zero?
    while (pv > 0) {
        // Get the leftmost digit (MSD), e.g. 1 in 1234.
        BigInt_DIGIT msd = digit / pv;
        if (!string_write_char(sb, cast(char)msd + '0')) {
            return false;
        }

        // "Trim" the MSD's magnitude, e.g. remove 1000 from 1234.
        digit -= msd * pv;
        pv /= cast(BigInt_DIGIT)base;
    }
    return true;
}

static bool
internal_string_append_base_prefix(String_Builder *sb, int base)
{
    switch (base) {
    case 2:
        if (!string_write_char(sb, '0')) return false;
        if (!string_write_char(sb, 'b')) return false;
        break;
    case 8:
        if (!string_write_char(sb, '0')) return false;
        if (!string_write_char(sb, 'o')) return false;
        break;
    case 16:
        if (!string_write_char(sb, '0')) return false;
        if (!string_write_char(sb, 'x')) return false;
        break;
    }
    return true;
}

const char *
bigint_to_base_lstring(const BigInt *src, int base, size_t *len, Allocator allocator)
{
    String_Builder sb;
    string_builder_init(&sb, allocator);

    // No digits to work with?
    if (bigint_is_zero(src)) {
        if (!string_write_char(&sb, '0')) {
            goto fail;
        }
        goto nul_terminate;
    }

    if (bigint_is_neg(src)) {
        if (!string_write_char(&sb, '-')) {
            goto fail;
        }
    }

    if (!internal_string_append_base_prefix(&sb, base)) {
        goto fail;
    }

    // Write the MSD. It will never have leading zeroes.
    size_t msd_index = src->len - 1;
    if (!internal_string_append_digit(&sb, src->data[msd_index], base)) {
        goto fail;
    }

    // Write everything past the MSD. They may have leading zeroes.
    for (size_t i = msd_index; i > 0; i -= 1) {
        BigInt_DIGIT digit = src->data[i - 1];

        // For each digit, write its base-10 representation from most-to-least
        // significant using the current maximum place value (`10**n`).
        BigInt_WORD tmp = cast(BigInt_WORD)digit;
        // E.g. in base-100, we want to pad '1' with 1 zero to get 01 in 1801.
        while (tmp * cast(BigInt_WORD)base < BIGINT_DIGIT_BASE) {
            if (!string_write_char(&sb, '0')) {
                goto fail;
            }
            tmp *= cast(BigInt_WORD)base;
        }
        if (!internal_string_append_digit(&sb, digit, base)) {
            goto fail;
        }
    }

nul_terminate:
    return string_to_cstring(&sb, len);

fail:
    return NULL;
}

const char *
bigint_to_base_string(const BigInt *src, int base, Allocator allocator)
{
    size_t *len = NULL;
    return bigint_to_base_lstring(src, base, len, allocator);
}

const char *
bigint_to_lstring(const BigInt *src, size_t *len, Allocator allocator)
{
    int base = 10;
    return bigint_to_base_lstring(src, base, len, allocator);
}

const char *
bigint_to_string(const BigInt *src, Allocator allocator)
{
    size_t *len = NULL;
    int base = 10;
    return bigint_to_base_lstring(src, base, len, allocator);
}

// === ARITHMETIC ========================================================== {{{


/** @brief `dst = |a| + |b|` without considering their signedness. */
static BigInt_Error
internal_bigint_add_unsigned(BigInt *dst, const BigInt *a, const BigInt *b)
{
    size_t max_used, min_used, i = 0;
    BigInt_DIGIT carry = 0;

    // Swap so we can assume a >= b (loosely)
    if (a->len < b->len) {
        internal_bigint_swap_ptr(&a, &b);
    }

    max_used = a->len;
    min_used = b->len;
    if (!internal_bigint_resize(dst, max_used + 1)) {
        return BIGINT_ERROR_MEMORY;
    }

    // Add up the digit places common to both numbers.
    for (; i < min_used; i += 1) {
        BigInt_DIGIT sum;

        sum = a->data[i] + b->data[i] + carry;
        if (sum > BIGINT_DIGIT_MAX) {
            sum -= BIGINT_DIGIT_BASE;
            // Notice how whenever we carry in addition, it is only 1 at most.
            // E.g. (base-10) 9 + 9 = 18 (8 carry 1).
            // This is of course not true in multiplication.
            carry = 1;
        } else {
            carry = 0;
        }
        dst->data[i] = sum;
    }

    // Copy over unadded digit places, propagating the carry.
    for (; i < max_used; i += 1) {
        BigInt_DIGIT sum;

        sum = a->data[i] + carry;
        if (sum > BIGINT_DIGIT_MAX) {
            sum -= BIGINT_DIGIT_BASE;
            carry = 1;
        } else {
            carry = 0;
        }
        dst->data[i] = sum;
    }

    // Carry may be 0 at this point. Clamp will get rid of it.
    dst->data[max_used] = carry;
    return internal_bigint_clamp(dst);
}


/** @brief `dst = |a| - |b|` where `|a| >= |b|`. */
static BigInt_Error
internal_bigint_sub_unsigned(BigInt *dst, const BigInt *a, const BigInt *b)
{
    size_t max_used, min_used, i = 0;
    BigInt_WORD borrow = 0;

    max_used = a->len;
    min_used = b->len;
    if (!internal_bigint_resize(dst, max_used + 1)) {
        return BIGINT_ERROR_MEMORY;
    }

    // Subtract the digit places common to both `a` and `b`, propagating the
    // carry as a negative offset.
    for (; i < min_used; i += 1) {
        BigInt_WORD diff = cast(BigInt_WORD)a->data[i]
                    - cast(BigInt_WORD)b->data[i]
                    - borrow;
        // Need to borrow?
        if (diff < 0) {
            // Recall when borrowing we only ever "cross out" 1 at a time.
            // This will be subtracted in subsequent iterations.
            borrow = 1;

            // This is the actual "borrowing". Recall how when we "cross out"
            // a digit to the left, the target digit has its base added to it.
            diff += BIGINT_DIGIT_BASE;
        } else {
            borrow = 0;
        }
        dst->data[i] = cast(BigInt_DIGIT)diff;
    }

    // Copy over the digit places in `a` that are not in `b`.
    for (; i < max_used; i += 1) {
        BigInt_WORD diff = cast(BigInt_WORD)a->data[i] - borrow;
        if (diff < 0) {
            borrow = 1;
            diff += BIGINT_DIGIT_BASE;
        } else {
            borrow = 0;
        }
        dst->data[i] = cast(BigInt_DIGIT)diff;
    }

    dst->data[max_used] = cast(BigInt_DIGIT)borrow;
    return internal_bigint_clamp(dst);
}


BigInt_Error
bigint_add(BigInt *dst, const BigInt *a, const BigInt *b)
{
    dst->sign = a->sign;

    // 1.) One of the operands is negative and the other is positive?
    if (a->sign != b->sign) {
        // 1.1.)   a  + (-b)  < 0 ; Concept check:   3 + (-12) = -9
        // 1.2.) (-a) +   b  >= 0 ; Concept check: (-3) +  12  =  9
        //  where |a| < |b|
        if (bigint_lt_abs(a, b)) {
            dst->sign = b->sign;
            internal_bigint_swap_ptr(&a, &b);
        }

        // 1.3.)  a  + (-b) >= 0 ; Concept check:   12  + (-3) =  9
        // 1.4) (-a) +   b  <  0 ; Concept check: (-12) +   3  = -9
        //  where |a| >= |b|
        return internal_bigint_sub_unsigned(dst, a, b);
    }

    // 2.) Both operands are both negative or positive?
    // 2.1.) (-a) + (-b) == -a - b == -(a + b)
    // 2.2.) a + b >= 0
    return internal_bigint_add_unsigned(dst, a, b);
}

BigInt_Error
bigint_sub(BigInt *dst, const BigInt *a, const BigInt *b)
{
    dst->sign = a->sign;

    // 1.) One of the operands is negative and the other is positive?
    if (a->sign != b->sign) {
        // Use the sign of `a` no matter what.
        //
        // 1.1.) (-a) - b < 0
        //  where a <  0
        //    and b >= 0
        //
        // Concept check:   (-2) - 3  == -(2 + 3) == -5
        //                  (-3) - 2  == -(3 + 2) == -5
        //
        // 1.2.) a - (-b) >= 0
        //  where a >= 0
        //    and b <  0
        //
        // Concept check:   2 - (-3) ==  2 + 3 == 5
        //                  3 - (-2) ==  3 + 2 == 5
        return internal_bigint_add_unsigned(dst, a, b);
    }

    // 2.) Ensure |a| >= |b| so that we can do unsigned subtraction.
    // Concept check:   2  -   3  == -(3 - 2)  == -1
    //                (-2) - (-3) ==  (-2) + 3 ==  3 - 2 == 1
    if (bigint_lt_abs(a, b)) {
        if (dst->sign == BIGINT_POSITIVE) {
            dst->sign = BIGINT_NEGATIVE;
        } else {
            dst->sign = BIGINT_POSITIVE;
        }
        internal_bigint_swap_ptr(&a, &b);
    }

    // 3.) a - b >= 0
    //  where a >= b
    //    and a >= 0
    //    and b >= 0
    //
    // Concept check:   3  -   2  == 1
    //                (-3) - (-2) == (-3) + 2 == -1
    return internal_bigint_sub_unsigned(dst, a, b);
}

BigInt_Error
bigint_mul(BigInt *dst, const BigInt *a, const BigInt *b)
{
    // Use a temporary to avoid aliasing issues as we iterate both `a` and `b`
    // multiple times.
    BigInt tmp;
    size_t max_used, min_used;
    BigInt_Error err;

    if (a->len < b->len) {
        internal_bigint_swap_ptr(&a, &b);
    }

    max_used = a->len;
    min_used = b->len;

    err = internal_bigint_init_len(&tmp, max_used + min_used, dst->allocator);
    if (err) return err;

    // 1.1.) +a * +b >= 0
    // 1.2.) +a * -b <  0
    // 1.3.) -a * -b <  0
    // 1.4.) -a * -b >= 0
    tmp.sign = (a->sign == b->sign) ? BIGINT_POSITIVE : BIGINT_NEGATIVE;

    // long multiplication
    for (size_t b_i = 0; b_i < min_used; b_i += 1) {
        BigInt_WORD mult, carry = 0;

        mult = cast(BigInt_WORD)b->data[b_i];
        for (size_t a_i = 0; a_i < max_used; a_i += 1) {
            BigInt_WORD prod = mult * cast(BigInt_WORD)a->data[a_i] + carry;
            carry = prod / BIGINT_DIGIT_BASE;
            tmp.data[b_i + a_i] += cast(BigInt_DIGIT)(prod % BIGINT_DIGIT_BASE);
        }
        tmp.data[b_i + max_used] += carry;
    }
    internal_bigint_swap(&tmp, dst);
    bigint_destroy(&tmp);
    return internal_bigint_clamp(dst);
}

BigInt_Error
bigint_div_bigint(BigInt *dst, const BigInt *a, const BigInt *b)
{
    // 1.1.) +a // +b >= 0
    // 1.2.) -a // -b >= 0
    // 1.3.) +a // -b <  0
    // 1.4.) -a // +b <  0
    dst->sign = (a->sign == b->sign) ? BIGINT_POSITIVE : BIGINT_NEGATIVE;
    stub();
    return BIGINT_OK;
}

BigInt_Error
bigint_mod_bigint(BigInt *dst, const BigInt *a, const BigInt *b)
{
    unused(dst);
    unused(a);
    unused(b);
    stub();
    return BIGINT_OK;
}


/** @brief `dst = |a| + |b|` */
static BigInt_Error
internal_bigint_add_digit_unsigned(BigInt *dst, const BigInt *a, BigInt_DIGIT b)
{
    size_t used = a->len;
    if (!internal_bigint_resize(dst, used + 1)) {
        return BIGINT_ERROR_MEMORY;
    }

    BigInt_DIGIT carry = b;
    for (size_t i = 0; i < used; i++) {
        BigInt_DIGIT sum = a->data[i] + carry;
        if (sum > BIGINT_DIGIT_MAX) {
            sum -= BIGINT_DIGIT_BASE;
            carry = 1;
        } else {
            carry = 0;
        }

        dst->data[i] = sum;
        if (carry == 0) {
            break;
        }
    }
    dst->data[used] = carry;
    return internal_bigint_clamp(dst);
}


/** @brief `dst = |a| - |b|` where `|a| >= |b|`. */
static BigInt_Error
internal_bigint_sub_digit_unsigned(BigInt *dst, const BigInt *a, BigInt_DIGIT b)
{
    size_t used = a->len;
    if (!internal_bigint_resize(dst, used + 1)) {
        return BIGINT_ERROR_MEMORY;
    }

    BigInt_DIGIT borrow = b;
    for (size_t i = 0; i < used; i += 1) {
        BigInt_WORD diff = cast(BigInt_WORD)a->data[i] - cast(BigInt_WORD)borrow;
        if (diff < 0) {
            borrow = 1;
            diff += BIGINT_DIGIT_BASE;
        } else {
            borrow = 0;
        }
        dst->data[i] = cast(BigInt_DIGIT)diff;
        if (borrow == 0) {
            break;
        }
    }
    dst->data[used] = borrow;
    return internal_bigint_clamp(dst);
}

BigInt_Error
bigint_add_digit(BigInt *dst, const BigInt *a, BigInt_DIGIT b)
{
    dst->sign = BIGINT_POSITIVE;

    // 1.) -a + b == |b| - |a| == -(|a| - |b|)
    //  where a < b
    //    and a < 0
    //    and b >= 0
    if (bigint_is_neg(a)) {
        // 1.1.) -a + b <= 0
        //  where |a| >= |b|
        //
        // Concept check: -3 + 2 = -1
        //                -2 + 2 =  0
        if (bigint_geq_digit_abs(a, b)) {
            BigInt_Error err = internal_bigint_sub_digit_unsigned(dst, a, b);
            if (err == BIGINT_OK) {
                bigint_neg(dst, dst);
            }
            return err;
        }

        // 1.2.) -a + b > 0
        //  where |a| < |b|
        //
        // Concept check: -2 + 3 = 1
        return internal_bigint_sub_digit_unsigned(dst, a, b);
    }

    // 2.) a + b
    // where a >= b
    //   and a >= 0
    //   and b >= 0
    return internal_bigint_add_digit_unsigned(dst, a, b);
}

BigInt_Error
bigint_sub_digit(BigInt *dst, const BigInt *a, BigInt_DIGIT b)
{
    dst->sign = a->sign;

    // Subtracting from a negative is the same as negating an addition.
    //
    // 1.) -a - b == -(a + b)
    //  where a <  b
    //    and a <  0
    //    and b >= 0
    if (bigint_is_neg(a)) {
        // 1.1.) -a - b <= 0
        // Concept check:   (-3) - 2 == -5
        //                  (-2) - 2 == -4
        return internal_bigint_add_digit_unsigned(dst, a, b);
    }

    // Simple because we are only working with one digit.
    //
    // 2.) a - b < 0 == -(-a + b) == -(b - a)
    //  where a < b
    //    and a >= 0
    //    and b >= 0
    if (bigint_lt_digit_abs(a, b)) {
        internal_bigint_resize(dst, a->len + 1);
        BigInt_WORD diff = b - a->data[0];
        dst->data[0] = cast(BigInt_DIGIT)diff;
        bigint_neg(dst, dst);
        return internal_bigint_clamp(dst);
    }

    // 3.) a - b >= 0
    //  where a >= b
    //    and a >= 0
    //    and b >= 0
    return internal_bigint_sub_digit_unsigned(dst, a, b);
}


/** @brief `dst = |a| * |b|` */
static BigInt_Error
internal_bigint_mul_digit_unsigned(BigInt *dst, const BigInt *a, BigInt_DIGIT b)
{
    // Save in case `dst` aliases `a`
    size_t used = a->len;
    BigInt_WORD carry = 0;

    // Add 1 because multiplication results in at most 1 extra digit.
    // Concept check (base-10): 9*9 = 81
    if (!internal_bigint_resize(dst, used + 1)) {
        return BIGINT_ERROR_MEMORY;
    }

    for (size_t i = 0; i < used; i++) {
        BigInt_WORD prod = cast(BigInt_WORD)a->data[i] * cast(BigInt_WORD)b;
        // Adjust for any previous carries.
        prod += carry;

        // New carry is the overflow from this product.
        // Unlike addition and subtraction, this may be > 1.
        carry = prod / BIGINT_DIGIT_BASE;

        // Actual result digit is the portion that fits in the given base.
        dst->data[i] = cast(BigInt_DIGIT)(prod % BIGINT_DIGIT_BASE);
    }
    dst->data[used] = cast(BigInt_DIGIT)carry;
    return internal_bigint_clamp(dst);
}

BigInt_Error
bigint_mul_digit(BigInt *dst, const BigInt *a, BigInt_DIGIT b)
{
    // 1.) a * b >= 0
    //  where a >= 0
    //    and b >= 0
    //
    // 2.) a * b < 0
    //  where a <  0
    //    and b >= 0
    //
    // `b` cannot be < 0 since it is a digit.
    dst->sign = a->sign;
    return internal_bigint_mul_digit_unsigned(dst, a, b);
}

// === }}} =====================================================================

// === COMPARISON ========================================================== {{{


// Never fails when `dst == a`.
BigInt_Error
bigint_neg(BigInt *dst, const BigInt *src)
{
    BigInt_Sign sign = BIGINT_NEGATIVE;
    // 1.) `dst = -0`
    // 2.) `dst = -(-a)`
    if (bigint_is_zero(src) || bigint_is_neg(src)) {
        sign = BIGINT_POSITIVE;
    }
    BigInt_Error err = bigint_copy(dst, src);
    dst->sign = sign;
    return err;
}

bool
bigint_is_zero(const BigInt *b)
{
    // We assume that most operations clamp eventually, thus 0 is never an MSD.
    return b->len == 0;
}

bool
bigint_is_neg(const BigInt *b)
{
    return b->sign == BIGINT_NEGATIVE;
}

bool
bigint_is_pos(const BigInt *b)
{
    return b->sign == BIGINT_POSITIVE;
}

BigInt_Comparison
bigint_compare(const BigInt *a, const BigInt *b)
{
    bool a_is_neg;

    if (a == b) {
        return BIGINT_EQUAL;
    }

    // 1.) One is positive while the other is negative?
    a_is_neg = bigint_is_neg(a);
    if (a->sign != b->sign) {
        // 1.1.) -a < +b
        // 1.2.) +a > -b
        return a_is_neg ? BIGINT_LESS : BIGINT_GREATER;
    }

    // 2.) Same signs but differing lengths.
    if (a->len != b->len) {
        // 2.1.) -a < -b where #a > #b
        //  else -a > -b where #a < #b
        if (a_is_neg) {
            return (a->len > b->len) ? BIGINT_LESS : BIGINT_GREATER;
        }
        // 2.2.) a < b where #a < #b
        else {
            return (a->len < b->len) ? BIGINT_LESS : BIGINT_GREATER;
        }
    }

    // 3.) Same signs and same lengths are "roughly" equal. Compare MSD to LSD.
    for (size_t iter = a->len; iter > 0; iter -= 1) {
        size_t i = iter - 1;
        // 2.1.) |a[i]| < |b[i]|
        //
        // Concept check:   2  <   3
        //                |-2| < |-3|
        if (a->data[i] < b->data[i]) {
            return (a_is_neg) ? BIGINT_GREATER : BIGINT_LESS;
        }
        // 2.1.) |a[i]| > |b[i]|
        //
        // Concept check:   5  >   1
        //                |-3| > |-2|
        else if (a->data[i] > b->data[i]) {
            return (a_is_neg) ? BIGINT_LESS : BIGINT_GREATER;
        }
    }
    return BIGINT_EQUAL;
}

bool
bigint_eq(const BigInt *a, const BigInt *b)
{
    return bigint_compare(a, b) == BIGINT_EQUAL;
}

bool
bigint_lt(const BigInt *a, const BigInt *b)
{
    return bigint_compare(a, b) == BIGINT_LESS;
}

bool
bigint_leq(const BigInt *a, const BigInt *b)
{
    return bigint_compare(a, b) <= BIGINT_EQUAL;
}

bool
bigint_neq(const BigInt *a, const BigInt *b)
{
    return bigint_compare(a, b) != BIGINT_EQUAL;
}

bool
bigint_gt(const BigInt *a, const BigInt *b)
{
    return bigint_compare(a, b) == BIGINT_GREATER;
}

bool
bigint_geq(const BigInt *a, const BigInt *b)
{
    return bigint_compare(a, b) >= BIGINT_EQUAL;
}


BigInt_Comparison
bigint_compare_abs(const BigInt *a, const BigInt *b)
{
    if (a == b) {
        return BIGINT_EQUAL;
    }

    // 1.) Can already tell the ordering based on digit sequence length?
    if (a->len != b->len) {
        return (a->len > b->len) ? BIGINT_GREATER : BIGINT_LESS;
    }

    // 2.) Same lengths. Do not consider signedness. Compare MSD to LSD.
    for (size_t it = a->len; it > 0; it -= 1) {
        size_t i = it - 1;
        if (a->data[i] < b->data[i]) {
            return BIGINT_LESS;
        }
        else if (a->data[i] > b->data[i]) {
            return BIGINT_GREATER;
        }
    }
    return BIGINT_EQUAL;
}

bool
bigint_eq_abs(const BigInt *a, const BigInt *b)
{
    return bigint_compare_abs(a, b) == BIGINT_EQUAL;
}

bool
bigint_lt_abs(const BigInt *a, const BigInt *b)
{
    return bigint_compare_abs(a, b) == BIGINT_LESS;
}

bool
bigint_leq_abs(const BigInt *a, const BigInt *b)
{
    return bigint_compare_abs(a, b) <= BIGINT_EQUAL;
}

bool
bigint_neq_abs(const BigInt *a, const BigInt *b)
{
    return bigint_compare_abs(a, b) != BIGINT_EQUAL;
}

bool
bigint_gt_abs(const BigInt *a, const BigInt *b)
{
    return bigint_compare_abs(a, b) == BIGINT_GREATER;
}

bool
bigint_geq_abs(const BigInt *a, const BigInt *b)
{
    return bigint_compare_abs(a, b) >= BIGINT_EQUAL;
}

BigInt_Comparison
bigint_compare_digit(const BigInt *a, BigInt_DIGIT b)
{
    // 1.) -a < b
    //  where  a  < 0
    //    and |a| > 0
    //    and  b >= 0
    if (bigint_is_neg(a)) {
        return BIGINT_LESS;
    }
    return bigint_compare_digit_abs(a, b);
}

bool
bigint_eq_digit(const BigInt *a, BigInt_DIGIT b)
{
    return bigint_compare_digit(a, b) == BIGINT_EQUAL;
}

bool
bigint_lt_digit(const BigInt *a, BigInt_DIGIT b)
{
    return bigint_compare_digit(a, b) == BIGINT_LESS;
}

bool
bigint_leq_digit(const BigInt *a, BigInt_DIGIT b)
{
    return bigint_compare_digit(a, b) <= BIGINT_EQUAL;
}

bool
bigint_neq_digit(const BigInt *a, BigInt_DIGIT b)
{
    return bigint_compare_digit(a, b) != BIGINT_EQUAL;
}

bool
bigint_gt_digit(const BigInt *a, BigInt_DIGIT b)
{
    return bigint_compare_digit(a, b) == BIGINT_GREATER;
}

bool
bigint_geq_digit(const BigInt *a, BigInt_DIGIT b)
{
    return bigint_compare_digit(a, b) >= BIGINT_EQUAL;
}


BigInt_Comparison
bigint_compare_digit_abs(const BigInt *a, BigInt_DIGIT b)
{
    // 2.) a <= b
    //  where a == 0
    //    and b >= 0
    if (bigint_is_zero(a)) {
        // 2.1.) 0 == 0
        // 2.2.) 0  < b
        return (b == 0) ? BIGINT_EQUAL : BIGINT_LESS;
    }

    // 3.) a > b
    //  where #a >  1
    //    and #b == 1
    if (a->len > 1) {
        return BIGINT_GREATER;
    }

    if (a->data[0] > b) {
        return BIGINT_GREATER;
    } else if (a->data[0] < b) {
        return BIGINT_LESS;
    }
    return BIGINT_EQUAL;
}

bool
bigint_eq_digit_abs(const BigInt *a, BigInt_DIGIT b)
{
    return bigint_compare_digit_abs(a, b) == BIGINT_EQUAL;
}

bool
bigint_lt_digit_abs(const BigInt *a, BigInt_DIGIT b)
{
    return bigint_compare_digit_abs(a, b) == BIGINT_LESS;
}

bool
bigint_leq_digit_abs(const BigInt *a, BigInt_DIGIT b)
{
    return bigint_compare_digit_abs(a, b) <= BIGINT_EQUAL;
}

bool
bigint_neq_digit_abs(const BigInt *a, BigInt_DIGIT b)
{
    return bigint_compare_digit_abs(a, b) != BIGINT_EQUAL;
}

bool
bigint_gt_digit_abs(const BigInt *a, BigInt_DIGIT b)
{
    return bigint_compare_digit_abs(a, b) == BIGINT_GREATER;
}

bool
bigint_geq_digit_abs(const BigInt *a, BigInt_DIGIT b)
{
    return bigint_compare_digit_abs(a, b) >= BIGINT_EQUAL;
}

// === }}} =====================================================================
