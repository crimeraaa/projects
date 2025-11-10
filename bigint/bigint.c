#include <stdio.h>  // fprintf
#include <string.h> // strlen, memset

#define LSTRING_IMPLEMENTATION
#include "lstring.h"
#include "bigint.h"

typedef BigInt_Digit Digit;
typedef BigInt_Word  Word;
typedef BigInt_Error Error;

typedef struct {
    const Allocator *allocator;
    char *data;
    size_t len;
    size_t cap;
} String_Builder;

void
bigint_init(BigInt *b, const Allocator *a)
{
    b->data      = NULL;
    b->allocator = a;
    b->len       = 0;
    b->cap       = 0;
    b->sign      = BIGINT_POSITIVE;
}

static void
bigint_fill_zero(BigInt *b, int start, int stop)
{
    Digit *data = &b->data[start];
    size_t len  = cast(size_t)(stop - start);
    memset(data, 0, sizeof(data[0]) * len);
}

static Error
bigint_init_len_cap(BigInt *b, int len, int cap, const Allocator *a)
{
    b->data = mem_make(Digit, cap, a);
    if (b->data == NULL) {
        return BIGINT_ERROR_MEMORY;
    }
    b->allocator = a;
    b->len       = len;
    b->cap       = cap;
    b->sign      = BIGINT_POSITIVE;
    bigint_fill_zero(b, 0, cap);
    return BIGINT_OK;
}

static int
bigint_count_digits(intmax_t *value, int base, bool *sign)
{
    intmax_t tmp = *value;
    *sign = (tmp < 0);

    // The below loop will never start.
    if (tmp == 0) {
        return 1;
    }

    if (*sign == BIGINT_NEGATIVE) {
        // Concept check: -1234 % 10 == 6; We do not want this!
        tmp    = -tmp;
        *value = tmp;
    }

    // e.g. 9   (1 iteration)
    //      10  (2 iterations)
    //      0   (0 iterations)
    int n = 0;
    while (tmp > 0) {
        tmp /= base;
        n++;
    }

    return n;
}

/** @brief Workhorse function. Do not expose as `intmax_t` is a nightmare for APIs.
 * See: https://thephd.dev/intmax_t-hell-c++-c */
static BigInt_Error
bigint_init_int_impl(BigInt *b, intmax_t value, const Allocator *a)
{
    int n_digits = bigint_count_digits(&value, BIGINT_DIGIT_BASE, &b->sign);
    Error err = bigint_init_len_cap(b, n_digits, n_digits, a);
    if (err) return err;

    for (int i = 0; i < n_digits; i++) {
        // Get the (current) least significant digit.
        // Concept check: 1234 % 10 == 4
        b->data[i] = cast(Digit)(value % BIGINT_DIGIT_BASE);

        // Pop the (current) least significant digit.
        // Concept check: 1234 // 10 == 123
        value /= BIGINT_DIGIT_BASE;
    }
    return err;
}

BigInt_Error
bigint_init_int(BigInt *b, int value, const Allocator *a)
{
    return bigint_init_int_impl(b, cast(intmax_t)value, a);
}

static BigInt_Sign
string_get_sign(String *s)
{
    BigInt_Sign sign = BIGINT_POSITIVE;
    for (; s->len > 1; s->data += 1, s->len -= 1) {
        char ch = s->data[0];
        if (ch == '+') {
            // Unary plus does nothing, e.g. +2, -+2
            continue;
        } else if (ch == '-') {
            // Unary minus flips the sign, e.g. -2, +-2, --2, -+-2, ---2
            sign = (sign == BIGINT_NEGATIVE) ? BIGINT_POSITIVE : BIGINT_NEGATIVE;
        } else if (is_space(ch)) {
            continue;
        } else {
            break;
        }
    }
    return sign;
}

static int
string_get_base(String *s)
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
string_trim(String *s)
{
    // Skip leading whitespace
    while (is_space(s->data[0])) {
        s->data += 1;
        s->data -= 1;
    }
    // Skip trailing whitespace
    while (s->len > 0 && is_space(s->data[s->len - 1])) {
        s->len -= 1;
    }
}

static int
char_to_int(char ch, int base)
{
    int i = -1;
    // Convert character to potential integer representation up to base-36
    if (is_digit(ch)) {
        i = ch - '0';
    } else if (is_lower(ch)) {
        i = ch - 'a' + 10;
    } else if (is_upper(ch)) {
        i = ch - 'A' + 10;
    }

    // Ensure above conversion is valid
    if (0 <= i && i < base) {
        return i;
    }
    return -1;
}

BigInt_Error
bigint_init_base_lstring(BigInt *b, const char *s, size_t n, int base,
    const Allocator *a)
{
    bigint_init(b, a);
    return bigint_set_base_lstring(b, s, n, base);
}

BigInt_Error
bigint_set_base_lstring(BigInt *b, const char *s, size_t n, int base)
{
    String m;
    m.data = s;
    m.len  = n;
    string_trim(&m);

    Error err = BIGINT_OK;

    // Check for unary minus or unary plus. Don't set the sign yet;
    // Because the intermediate calculations will undo it anyway.
    // Delaying this also accounts for inputs like "-0".
    BigInt_Sign sign = string_get_sign(&m);

    // No explicit base given, try to check for a prefix.
    if (base == 0) {
        base = string_get_base(&m);
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
        char ch = *it;
        if (ch == '_' || ch == ',') {
            continue;
        }
        int digit = char_to_int(ch, base);
        if (digit != -1) {
            bigint_mul_digit(b, b, cast(Digit)base);
            bigint_add_digit(b, b, digit);
        } else {
            err = BIGINT_ERROR_DIGIT;
fail:
            bigint_destroy(b);
            return err;
        }
    }

    // Finalize the sign after all the intermediate calculations.
    // We assume all the above calculations already clamped us.
    b->sign = bigint_is_zero(b) ? BIGINT_POSITIVE : sign;
    return BIGINT_OK;

}

static void
string_builder_init(String_Builder *sb, const Allocator *a)
{
    sb->allocator = a;
    sb->data = NULL;
    sb->len  = 0;
    sb->cap  = 0;
}

static Error
string_append_char(String_Builder *sb, char ch)
{
    if (sb->len + 1 > sb->cap) {
        // Use powers of 2 for simplicity.
        size_t n = (sb->len < 8) ? 8 : sb->cap * 2;
        char *ptr = mem_resize(char, sb->data, sb->cap, n, sb->allocator);
        if (ptr == NULL) {
            return BIGINT_ERROR_MEMORY;
        }
        sb->data = ptr;
        sb->cap  = n;
    }
    sb->len += 1;
    sb->data[sb->len - 1] = ch;
    return BIGINT_OK;
}

static Error
string_append_lstring(String_Builder *sb, const char *s, size_t n)
{
    Error err = BIGINT_OK;
    for (size_t i = 0; i < n; i += 1) {
        char c = s[i];
        err = string_append_char(sb, c);
        if (err) {
            break;
        }
    }
    return err;
}

#define string_append_literal(sb, s)  string_append_lstring(sb, s, sizeof(s) - 1)


/** @brief Gets the place-value of the MSD of `d`, e.g. 1234 returns 1000. */
static Digit
digit_place_value(Digit d, Word base)
{
    // Use Result type in case of Digit overflow.
    Word place = 1;

    // Check the next place value; may be the one we are looking for.
    // E.g. 9 returns 1 but 10 returns 10.
    while (place * base <= cast(Word)d) {
        place *= base;
    }
    return cast(Digit)place;
}

static int
digit_count_digits(Digit d, Digit base)
{
    int n = 0;
    if (d == 0) {
        return 1;
    }

    // Since `Digit` must be unsigned, we can never have negative values.
    while (d > 0) {
        n += 1;
        d /= base;
    }
    return n;
}


/** @brief Get the maximum number of base-`base` digits that would fit in a
 * base-`DIGIT_BASE` number. */
static int
digit_length_in_base(int base)
{
    // Check the precalculated ones
    switch (base) {
    case 2:  return BIGINT_DIGIT_BASE2_LENGTH;
    case 8:  return BIGINT_DIGIT_BASE8_LENGTH;
    case 10: return BIGINT_DIGIT_BASE10_LENGTH;
    case 16: return BIGINT_DIGIT_BASE16_LENGTH;
    }
    // Always works, but marginally slower- not O(1) time.
    return digit_count_digits(BIGINT_DIGIT_MAX, base);
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

    int msd_index = b->len - 1;

    // MSD has variable width with no leading zeroes.
    n_chars += digit_count_digits(b->data[msd_index], cast(Digit)base);

    // Beyond MSD, all remaining digits have fixed width.
    n_chars += (b->len - 1) * digit_length_in_base(base);
    return n_chars;
}


/** @brief Writes all significant digits from MSD to LSD. */
static Error
string_append_digit(String_Builder *sb, Digit digit, Digit base)
{
    Digit pv  = digit_place_value(digit, base);
    Error err = BIGINT_OK;
    for (;;) {
        // Get the leftmost digit (MSD), e.g. 1 in 1234.
        Digit msd = digit / pv;
        err = string_append_char(sb, cast(char)msd + '0');
        if (err) return err;

        // "Trim" the MSD's magnitude, e.g. remove 1000 from 1234.
        digit -= msd * pv;
        pv /= base;
        // No more digits to process? (Would also cause division by zero!)
        if (pv == 0) {
            break;
        }
    }
    return err;
}

static Error
string_append_base_prefix(String_Builder *sb, int base)
{
    Error err = BIGINT_OK;
    switch (base) {
    case 2:  err = string_append_literal(sb, "0b"); break;
    case 8:  err = string_append_literal(sb, "0o"); break;
    case 16: err = string_append_literal(sb, "0x"); break;
    }
    return err;
}

const char *
bigint_to_base_lstring(const BigInt *b, const Allocator *a, int base,
    size_t *len)
{
    String_Builder sb;
    string_builder_init(&sb, a);

    Error err = BIGINT_OK;

    // No digits to work with?
    if (b->len == 0) {
        err = string_append_char(&sb, '0');
        if (err) goto fail;

        goto nul_terminate;
    }

    if (bigint_is_neg(b)) {
        err = string_append_char(&sb, '-');
        if (err) goto fail;
    }

    err = string_append_base_prefix(&sb, base);
    if (err) goto fail;

    // Write the MSD. It will never have leading zeroes.
    int msd_index = b->len - 1;
    err = string_append_digit(&sb, b->data[msd_index], base);
    if (err) goto fail;

    // Write everything past the MSD. They may have leading zeroes.
    for (int i = msd_index - 1; i >= 0; i -= 1) {
        Digit digit = b->data[i];

        // For each digit, write its base-10 representation from most-to-least
        // significant using the current maximum place value (`10**n`).
        Word tmp = cast(Word)digit;
        // E.g. in base-100, we want to pad '1' with 1 zero to get 01 in 1801.
        while (tmp * cast(Word)base < BIGINT_DIGIT_BASE) {
            err = string_append_char(&sb, '0');
            if (err) goto fail;

            tmp *= cast(Word)base;
        }
        err = string_append_digit(&sb, digit, cast(Digit)base);
        if (err) goto fail;
    }

nul_terminate:
    if (len != NULL) {
        *len = cast(size_t)sb.len;
    }
    err = string_append_char(&sb, '\0');

    if (err) {
fail:
        return NULL;
    }
    return sb.data;

}

void
bigint_destroy(BigInt *b)
{
    mem_free(b->data, b->cap, b->allocator);
}

void
bigint_clear(BigInt *b)
{
    b->sign = BIGINT_POSITIVE;
    b->len  = 0;
}

static bool
bigint_resize(BigInt *b, int n)
{
    if (n > b->cap) {
        Digit *ptr = mem_resize(Digit, b->data, b->cap, n, b->allocator);
        // Don't free `b->data` because the outermost caller still owns it.
        if (ptr == NULL) {
            return false;
        }
        b->data = ptr;
        bigint_fill_zero(b, /*start=*/b->cap, /*stop=*/n);
        b->cap = n;
    }
    // Resizing always changes the user-facing length.
    b->len = n;
    return true;
}

static void
bigint_swap(const BigInt **a, const BigInt **b)
{
    const BigInt *tmp = *a;
    *a = *b;
    *b = tmp;
}

/** @brief Removes leading zeroes. If `|b| == 0`, then it is set positive.
 * Otherwise the sign is not touched. */
static Error
bigint_clamp(BigInt *b)
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


// === ARITHMETIC ========================================================== {{{


/** @brief Split `r` into digit and carry if it is too large. */
static Digit
split_sum(Word sum, Digit *carry)
{
    // Branched version but using cheaper operations.
    if (sum > BIGINT_DIGIT_MAX) {
        sum -= BIGINT_DIGIT_BASE;
        // Notice how whenever we carry in addition, it is only 1 at most.
        // E.g. (base-10) 9 + 9 = 18 (8 carry 1).
        // This is of course not true in multiplication.
        *carry = 1;
    } else {
        *carry = 0;
    }
    return cast(Digit)sum;
}


/** @brief `out = |a| + |b|` without considering their signedness. */
static Error
bigint_add_unsigned(BigInt *out, const BigInt *a, const BigInt *b)
{
    // Swap so we can assume a >= b (loosely)
    if (a->len < b->len) {
        bigint_swap(&a, &b);
    }

    int max_used = a->len;
    int min_used = b->len;
    if (!bigint_resize(out, max_used + 1)) {
        return BIGINT_ERROR_MEMORY;
    }

    Digit carry = 0;
    int i = 0;

    // Add up the digit places common to both numbers.
    for (; i < min_used; i += 1) {
        Digit sum = a->data[i] + b->data[i] + carry;
        out->data[i] = split_sum(sum, &carry);
    }

    // Copy over unadded digit places, propagating the carry.
    for (; i < max_used; i += 1) {
        Digit sum = a->data[i] + carry;
        out->data[i] = split_sum(sum, &carry);
    }

    // Carry may be 0 at this point. Clamp will get rid of it.
    out->data[max_used] = carry;
    return bigint_clamp(out);
}

static Digit
split_diff(Word diff, Digit *borrow)
{
    // Need to borrow?
    if (diff < 0) {
        // Recall when borrowing we only ever "cross out" 1 at a time.
        // This will be subtracted in subsequent iterations.
        *borrow = 1;

        // This is the actual "borrowing". Recall how when we "cross out"
        // a digit to the left, the target digit has its base added to it.
        diff += BIGINT_DIGIT_BASE;
    } else {
        *borrow = 0;
    }
    return cast(Digit)diff;
}


/** @brief `out = |a| - |b|` where `|a| >= |b|`. */
static Error
bigint_sub_unsigned(BigInt *out, const BigInt *a, const BigInt *b)
{
    int max_used = a->len;
    int min_used = b->len;
    if (!bigint_resize(out, max_used + 1)) {
        return BIGINT_ERROR_MEMORY;
    }

    Digit borrow = 0;
    int i = 0;

    // Subtract the digit places common to both `a` and `b`, propagating the
    // carry as a negative offset.
    for (; i < min_used; i += 1) {
        Word diff = cast(Word)a->data[i] - cast(Word)b->data[i];
        diff -= cast(Word)borrow;
        out->data[i] = split_diff(diff, &borrow);
    }

    // Copy over the digit places in `a` that are not in `b`.
    for (; i < max_used; i += 1) {
        Word diff = cast(Word)a->data[i] - cast(Word)borrow;
        out->data[i] = split_diff(diff, &borrow);
    }

    out->data[max_used] = borrow;
    return bigint_clamp(out);
}


BigInt_Error
bigint_add(BigInt *out, const BigInt *a, const BigInt *b)
{
    out->sign = a->sign;

    // 1.) One of the operands is negative and the other is positive?
    if (a->sign != b->sign) {
        // 1.1.) -a + b == -(a - b)
        if (bigint_is_neg(a)) {
            // 1.1.1.) -a + b >= 0
            //  where |a| < |b|
            //
            // Concept check: (-2) + 3 == 1
            if (bigint_lt_abs(a, b)) {
                goto use_b_sign;
            }

            // 1.1.2.) -a + b < 0
            //  where |a| > |b|
            //
            // Concept check: (-3) + 2 == (-1)
            //                (-4) + 4 == 0
            goto use_a_sign;
        }

        // 1.2.) a + (-b) == a - b
        // 1.2.1.) a + (-b) >= 0
        //  when |a| >= |b|
        if (bigint_geq_abs(a, b)) {
use_a_sign:
            return bigint_sub_unsigned(out, a, b);
        }

        // 1.2.2.) a + (-b)  < 0
        //  when |a| < |b|
use_b_sign:
        out->sign = b->sign;
        return bigint_sub_unsigned(out, b, a);
    }

    // 2.) Both operands are both negative or positive?
    // 2.1.) (-a) + (-b) == -a - b == -(a + b)
    // 2.2.) a + b >= 0
    return bigint_add_unsigned(out, a, b);
}

BigInt_Error
bigint_sub(BigInt *out, const BigInt *a, const BigInt *b)
{
    // Propagate the left hand side sign by default.
    out->sign = a->sign;

    // 1.) One of the operands is negative and the other is positive?
    if (a->sign != b->sign) {
        // Use the sign of `a` no matter what.
        //
        // 1.1.) (-a) - b < 0
        //  where a < 0
        //    and b >= 0
        //
        // Concept check:   (-2) - 3  == -(2 + 3) == -5
        //                  (-3) - 2  == -(3 + 2) == -5
        //
        // 1.2.) a - (-b) >= 0
        //  where a >= 0
        //    and b < 0
        //
        // Concept check:   2 - (-3) ==  2 + 3 == 5
        //                  3 - (-2) ==  3 + 2 == 5
        return bigint_add_unsigned(out, a, b);
    }

    // 2.) Both operands are negative or positive?
    // Ensure |a| >= |b| so that we can do unsigned subtraction.
    if (a->len < b->len) {
        bigint_swap(&a, &b);
    }

    // 2.1.) a - b < 0
    //  where a < b
    //    and a >= 0
    //    and b >= 0
    //
    // Concept check:   2  -   3  == -(3 - 2)  == -1
    //                (-2) - (-3) ==  (-2) + 3 ==  3 - 2 == 1
    if (bigint_lt_abs(a, b)) {
        out->sign = !a->sign;
        return bigint_sub_unsigned(out, b, a);
    }

    // 2.2.) a - b >= 0
    //  where a >= b
    //    and a >= 0
    //    and b >= 0
    //
    // Concept check:   3  -   2  == 1
    //                (-3) - (-2) == (-3) + 2 == -1
    return bigint_sub_unsigned(out, a, b);
}

BigInt_Error
bigint_mul(BigInt *restrict out, const BigInt *a, const BigInt *b)
{
    // 1.1.) +a * +b >= 0
    // 1.2.) +a * -b <  0
    // 1.3.) -a * -b <  0
    // 1.4.) -a * -b >= 0
    out->sign = (a->sign != b->sign);

    if (a->len < b->len) {
        bigint_swap(&a, &b);
    }

    stub();
    return BIGINT_OK;
}

/** @brief `out = |a| + |b|` */
static BigInt_Error
bigint_add_digit_unsigned(BigInt *out, const BigInt *a, BigInt_Digit b)
{
    int used = a->len;
    if (!bigint_resize(out, used + 1)) {
        return BIGINT_ERROR_MEMORY;
    }

    Digit carry = b;
    for (int i = 0; i < used; i++) {
        Digit sum = a->data[i] + carry;
        out->data[i] = split_sum(sum, &carry);
        if (carry == 0) {
            break;
        }
    }
    out->data[used] = carry;
    return bigint_clamp(out);
}


/** @brief `out = |a| - |b|` where `|a| >= |b|`. */
static BigInt_Error
bigint_sub_digit_unsigned(BigInt *out, const BigInt *a, BigInt_Digit b)
{
    int used = a->len;
    if (!bigint_resize(out, used + 1)) {
        return BIGINT_ERROR_MEMORY;
    }

    Digit carry = b;
    for (int i = 0; i < used; i += 1) {
        Word diff = cast(Word)a->data[i] - cast(Word)carry;
        out->data[i] = split_diff(diff, &carry);
        if (carry == 0) {
            break;
        }
    }
    out->data[used] = carry;
    return bigint_clamp(out);
}

BigInt_Error
bigint_add_digit(BigInt *out, const BigInt *a, BigInt_Digit b)
{
    out->sign = BIGINT_POSITIVE;

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
            Error err = bigint_sub_digit_unsigned(out, a, b);
            if (err == BIGINT_OK) {
                bigint_neg(out, out);
            }
            return err;
        }

        // 1.2.) -a + b > 0
        //  where |a| < |b|
        //
        // Concept check: -2 + 3 = 1
        return bigint_sub_digit_unsigned(out, a, b);
    }

    // 2.) a + b
    // where a >= b
    //   and a >= 0
    //   and b >= 0
    return bigint_add_digit_unsigned(out, a, b);
}

BigInt_Error
bigint_sub_digit(BigInt *out, const BigInt *a, BigInt_Digit b)
{
    out->sign = a->sign;

    // Subtracting from a negative is the same as negating an addition.
    //
    // 1.) -a - b == -(a + b)
    //  where a < 0
    //    and a < b
    if (bigint_is_neg(a)) {
        // 1.1.) -a - b <= 0
        // Concept check:   (-3) - 2 == -5
        //                  (-2) - 2 == -4
        return bigint_add_digit_unsigned(out, a, b);
    }

    // Simple because we are only working with one digit.
    //
    // 2.) a - b < 0 == -(-a + b) == -(b - a)
    //  where a < b
    //    and a >= 0
    //    and b >= 0
    if (bigint_lt_digit_abs(a, b)) {
        bigint_resize(out, a->len + 1);
        Word diff = b - a->data[0];
        out->data[0] = diff;
        bigint_neg(out, out);
        return bigint_clamp(out);
    }

    // 3.) a - b >= 0
    //  where a >= b
    //    and a >= 0
    //    and b >= 0
    return bigint_sub_digit_unsigned(out, a, b);
}

/** @brief Splits a product into its carry and actual writeable portions. */
static Digit
split_product(Word prod, Digit *carry)
{
    // Adjust for any previous carries.
    prod += cast(Word)*carry;

    // New carry is the overflow from this product.
    // Unlike addition and subtraction, this may be > 1.
    *carry = cast(Digit)(prod / BIGINT_DIGIT_BASE);

    // Actual result digit is the portion that fits in the given base.
    return cast(Digit)(prod % BIGINT_DIGIT_BASE);
}


/** @brief `out = |a| * |b|` */
static BigInt_Error
bigint_mul_digit_unsigned(BigInt *out, const BigInt *a, BigInt_Digit b)
{
    // Save in case `out` aliases `a`
    int used = a->len;

    // Add 1 because multiplication results in at most 1 extra digit.
    // Concept check (base-10): 9*9 = 81
    if (!bigint_resize(out, used + 1)) {
        return BIGINT_ERROR_MEMORY;
    }

    // Multiplication by a single digit is simple: we multiple each digit
    // of `a` with `b`.
    Digit carry = 0;
    for (int i = 0; i < used; i++) {
        Word prod = cast(Word)a->data[i] * cast(Word)b;
        out->data[i] = split_product(prod, &carry);
    }
    out->data[used] = carry;
    return bigint_clamp(out);
}

BigInt_Error
bigint_mul_digit(BigInt *out, const BigInt *a, BigInt_Digit b)
{
    // 1.) if (a >= 0) then (a * digit) >= 0
    // 2.) if (a  < 0) then (a * digit)  < 0
    out->sign = a->sign;
    return bigint_mul_digit_unsigned(out, a, b);
}

// === }}} =====================================================================

// === COMPARISON ========================================================== {{{


// Never fails when `out == a`.
BigInt_Error
bigint_neg(BigInt *out, const BigInt *a)
{
    BigInt_Sign sign = BIGINT_NEGATIVE;
    // 1.) `out = -0`
    // 2.) `out = -(-a)`
    if (bigint_is_zero(a) || bigint_is_neg(a)) {
        sign = BIGINT_POSITIVE;
    }

    // 3.) `out = -out`
    if (out == a) {
        out->sign = sign;
        return BIGINT_OK;
    }

    // 4.) `out = -a` where `out` does not alias `a`
    // Need to copy `a` into `out` with its sign flipped.
    stub();
}

BigInt_Error
bigint_div(BigInt *restrict out, const BigInt *a, const BigInt *b)
{
    // 1.1.) +a // +b >= 0
    // 1.2.) +a // -b <= 0
    // 1.3.) -a // -b <= 0
    // 1.4.) -a // -b >= 0
    out->sign = (a->sign == b->sign);
    stub();
    return BIGINT_OK;
}

BigInt_Error
bigint_mod(BigInt *restrict out, const BigInt *a, const BigInt *b)
{
    unused(out);
    unused(a);
    unused(b);
    stub();
    return BIGINT_OK;
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

static BigInt_Comparison
bigint_compare_fast_abs(const BigInt *a, const BigInt *b)
{
    // 2.) Same signs but differing lengths.
    if (a->len != b->len) {
        // When `a` is negative, it is only lesser if it has more digits.
        if (bigint_is_neg(a)) {
            return (a->len > b->len) ? BIGINT_LESS : BIGINT_GREATER;
        }
        // Otherwise `a` is positive; it is only lesser if it has less digits.
        else {
            return (a->len < b->len) ? BIGINT_LESS : BIGINT_GREATER;
        }
    }
    return BIGINT_EQUAL;
}

static BigInt_Comparison
bigint_compare_fast(const BigInt *a, const BigInt *b)
{
    // 1.) One is positive while the other is negative?
    if (a->sign != b->sign) {
        // 1.1.) -a < +b
        // 1.2.) +a > -b
        return bigint_is_neg(a) ? BIGINT_LESS : BIGINT_GREATER;
    }
    return bigint_compare_fast_abs(a, b);
}

BigInt_Comparison
bigint_compare(const BigInt *a, const BigInt *b)
{
    BigInt_Comparison fast = bigint_compare_fast(a, b);
    // 1.) Can already tell the ordering based on sign and/or digit count?
    if (fast != BIGINT_EQUAL) {
        return fast;
    }

    // 2.) Same signs, same lengths. Compare MSD to LSD.
    bool negative = bigint_is_neg(a);
    for (int i = a->len - 1; i >= 0; i -= 1) {
        // 2.1.) |a[i]| < |b[i]|
        //
        // Concept check:   2  <   3
        //                |-2| < |-3|
        if (a->data[i] < b->data[i]) {
            return (negative) ? BIGINT_GREATER : BIGINT_LESS;
        }
        // 2.1.) |a[i]| > |b[i]|
        //
        // Concept check:   5  >   1
        //                |-3| > |-2|
        else if (a->data[i] > b->data[i]) {
            return (negative) ? BIGINT_LESS : BIGINT_GREATER;
        }
    }
    return BIGINT_EQUAL;
}

BigInt_Comparison
bigint_compare_abs(const BigInt *a, const BigInt *b)
{
    BigInt_Comparison fast = bigint_compare_fast_abs(a, b);
    // 1.) Can already tell the ordering based on length?
    if (fast != BIGINT_EQUAL) {
        return fast;
    }

    // 2.) Same lengths. Do not consider signedness. Compare MSD to LSD.
    for (int i = a->len - 1; i >= 0; i -= 1) {
        if (a->data[i] < b->data[i]) {
            return BIGINT_LESS;
        }
        else if (a->data[i] > b->data[i]) {
            return BIGINT_GREATER;
        }
    }
    return BIGINT_EQUAL;
}

BigInt_Comparison
bigint_compare_digit(const BigInt *a, BigInt_Digit b)
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

BigInt_Comparison
bigint_compare_digit_abs(const BigInt *a, BigInt_Digit b)
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


// === }}} =====================================================================

// Macro cleanup in case of unity builds
#undef mem_make
#undef mem_resize
#undef mem_free

#undef string_append_literal
