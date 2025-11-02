#include <stdio.h>  // fputc, fprintf
#include <stdlib.h> // malloc, free
#include <string.h> // strlen, memset

#include "bigint.h"

#define cast(T)                      (T)
#define unused(expr)                 (void)(expr)
#define make(T, n)                   (T *)malloc(sizeof(T) * (n))
#define resize(T, ptr, old_n, new_n) (T *)realloc(ptr, sizeof(T) * (new_n))
#define stub() \
    fprintf(stderr, "[STUB] %s:%i: Unimplemented\n", __FILE__, __LINE__); \
    __builtin_trap()


typedef BigInt_Digit  Digit;
typedef BigInt_Result Result;
typedef BigInt_Error  Error;

typedef enum {
    SIGN_POSITIVE = false,
    SIGN_NEGATIVE = true,
} Sign;

typedef struct {
    const char *data;
    size_t      len;
} String;

typedef struct {
    char *data;
    int len;
    int cap;
} String_Builder;

void
bigint_init(BigInt *b)
{
    b->data = NULL;
    b->len  = 0;
    b->cap  = 0;
    b->sign = SIGN_POSITIVE;
}

static void
bigint_fill_zero(BigInt *b, int start, int stop)
{
    Digit *data = &b->data[start];
    size_t len  = cast(size_t)(stop - start);
    memset(data, 0, sizeof(data[0]) * len);
}

static Error
bigint_init_len_cap(BigInt *b, int len, int cap)
{
    b->data = make(Digit, cap);
    if (b->data == NULL) {
        return BIGINT_ERROR_MEMORY;
    }
    b->len  = len;
    b->cap  = cap;
    b->sign = SIGN_POSITIVE;
    bigint_fill_zero(b, 0, cap);
    return BIGINT_OK;
}

static int
count_digits_intmax_t(intmax_t *value, int base, bool *sign)
{
    intmax_t tmp = *value;
    *sign = (tmp < 0);

    // The below loop will never start.
    if (tmp == 0) {
        return 1;
    }

    if (*sign) {
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
bigint_init_intmax_t(BigInt *b, intmax_t value)
{
    int n_digits = count_digits_intmax_t(&value, BIGINT_DIGIT_BASE, &b->sign);
    Error err = bigint_init_len_cap(b, n_digits, n_digits);
    if (err != BIGINT_OK) {
        return err;
    }

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
bigint_init_int(BigInt *b, int value)
{
    return bigint_init_intmax_t(b, cast(intmax_t)value);
}

static bool
is_decimal(char c)
{
    return '0' <= c && c <= '9';
}

static bool
is_upper(char ch)
{
    return 'A' <= ch && ch <= 'Z';
}

static bool
is_lower(char ch)
{
    return 'a' <= ch && ch <= 'z';
}

static bool
is_space(char ch)
{
    switch (ch) {
    case ' ':
    case '\t':
    case '\n':
    case '\v':
    case '\r':
        return true;
    }
    return false;
}

static Sign
string_get_sign(String *s)
{
    Sign sign = SIGN_POSITIVE;
    if (s->len > 1) {
        char ch = s->data[0];
        if (ch == '+') {
            goto skip_sign;
        } else if (ch == '-') {
            sign = SIGN_NEGATIVE;
skip_sign:
            s->data += 1;
            s->len -= 1;
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
        case 'b':
        case 'B':
            base = 2;
            break;
        case 'd':
        case 'D':
            base = 10;
            break;
        case 'o':
        case 'O':
            base = 8;
            break;
        case 'X':
        case 'x':
            base = 16;
            break;
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
    // Convert character to potential integer representation up to base-16
    if (is_decimal(ch)) {
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
bigint_init_string(BigInt *b, const char *s)
{
    String m;
    m.data = s;
    m.len  = strlen(s);
    string_trim(&m);

    // Start with 0 so we can build on top of it.
    Error err = bigint_init_int(b, 0);
    if (err != BIGINT_OK) {
        return err;
    }

    // Check for unary minus or unary plus. Don't set the sign yet;
    // Introducing negatives at this point makes handling difficult.
    Sign sign = string_get_sign(&m);
    // b->sign = sign;

    // Check for leading base prefix.
    int base = string_get_base(&m);
    if (base == -1) {
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
    b->sign = sign;
    return BIGINT_OK;
}

static void
string_init(String_Builder *sb, char *buf, int size)
{
    sb->data = buf;
    sb->len  = 0;
    sb->cap  = size;
}

static void
string_append(String_Builder *sb, char ch)
{
    if (0 <= sb->len && sb->len < sb->cap) {
        sb->len += 1;
        sb->data[sb->len - 1] = ch;
    } else {
        fprintf(stderr, "[FATAL] Out of bounds index %i / %i\n", sb->len, sb->cap);
        __builtin_trap();
    }
}

/** @brief Gets the place-value of the MSD of `d`, e.g. 1234 returns 1000. */
static Digit
digit_place_value(Digit d, Result base)
{
    // Use Result type in case of Digit overflow.
    Result place = 1;

    // Check the next place value; may be the one we are looking for.
    // E.g. 9 returns 1 but 10 returns 10.
    while (place * base <= cast(Result)d) {
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

    while (d > 0) {
        n += 1;
        d /= base;
    }
    return n;
}

size_t
bigint_string_length(const BigInt *b, int base)
{
    if (b->len == 0) {
        return 1;
    }
    
    size_t n_chars = 0;
    if (b->sign) {
        n_chars += 1;
    }
    
    int msd_index = b->len - 1;

    // MSD has variable width with no leading zeroes.
    n_chars += digit_count_digits(b->data[msd_index], cast(Digit)base);

    // Beyond MSD, all remaining digits have fixed width.
    // They may have leading zeroes.
    // for (int i = msd_index - 1; i >= 0; i -= 1) {
    //     Digit d = b->data[i];
    // }
    return n_chars;
}


/** @brief Writes all significant digits from MSD to LSD. */
static void
string_append_digit(String_Builder *sb, Digit digit, Digit base)
{
    Digit pv = digit_place_value(digit, base);
    for (;;) {
        // Get the leftmost digit (MSD), e.g. 1 in 1234.
        Digit msd = digit / pv;
        string_append(sb, cast(char)msd + '0');
        // "Trim" the MSD's magnitude, e.g. remove 1000 from 1234.
        digit -= msd * pv;
        pv /= base;
        // No more digits to process? (Would also cause division by zero!)
        if (pv == 0) {
            break;
        }
    }
}

const char *
bigint_to_lstring(const BigInt *b, char *buf, size_t cap, size_t *len)
{
    String_Builder sb;
    string_init(&sb, buf, cast(int)cap);

    // Can't fit desired big integer string representation in the buffer?
    if (cast(size_t)b->len + b->sign >= cap) {
        return NULL;
    }

    // No digits to work with?
    if (b->len == 0) {
        string_append(&sb, '0');
        goto nul_terminate;
    }

    if (b->sign) {
        string_append(&sb, '-');
    }
    
    // Write the MSD. It will never have leading zeroes.
    int msd_index = b->len - 1;
    string_append_digit(&sb, b->data[msd_index], /*base=*/10);
    
    // Write everything past the MSD. They may have leading zeroes.
    for (int i = msd_index - 1; i >= 0; i -= 1) {
        Digit digit = b->data[i];

        // For each digit, write its base-10 representation from most-to-least
        // significant using the current maximum place value (`10**n`).
        Result tmp = cast(Result)digit;
        // E.g. in base-100, we want to pad '1' with 1 zero to get 01 in 1801.
        while (tmp * 10 < BIGINT_DIGIT_BASE) {
            string_append(&sb, '0');
            tmp *= 10;
        }
        string_append_digit(&sb, digit, /*base=*/10);
    }

nul_terminate:
    if (len != NULL) {
        *len = cast(size_t)sb.len;
    }
    string_append(&sb, '\0');
    return buf;
}

void
bigint_destroy(BigInt *b)
{
    free(b->data);
}

static int
max_int(int a, int b)
{
    return (a > b) ? a : b;
}

// static int
// min_int(int a, int b)
// {
//     return (a < b) ? a : b;
// }

static Digit
bigint_safe_at(const BigInt *b, int i)
{
    if (0 <= i && i < b->len) {
        return b->data[i];
    }
    return 0;
}

static bool
bigint_resize(BigInt *b, int n)
{
    if (n > b->cap) {
        Digit *ptr = resize(Digit, b->data, b->cap, n);
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

#define bigint_resize(b, n) if (!bigint_resize(b, n)) return BIGINT_ERROR_MEMORY


/** @brief Split `r` into digit and carry. Works even if well below `base`. */
static Digit
result_split(Result r, Digit *carry)
{
    // E.g. 9 + 3 = 12
    *carry = r / BIGINT_DIGIT_BASE; // 12 // 10 == 1
    return cast(Digit)(r % BIGINT_DIGIT_BASE); // 12 %  10 == 2
}

static Digit
digit_add_carry(Digit a, Digit b, Digit *carry)
{
    Result sum = cast(Result)a + cast(Result)b + cast(Result)*carry;
    return result_split(sum, carry);
}

static Error
bigint_add_carry(BigInt *b, Digit carry)
{
    if (carry > 0) {
        bigint_resize(b, b->len + 1);
        b->data[b->len - 1] = carry;
    }
    return BIGINT_OK;
}


/** @brief `out = a + b` without considering their signedness. */
static Error
bigint_add_unsigned(BigInt *out, const BigInt *a, const BigInt *b)
{
    // Save in case `out` aliases either `a` or `b`
    int max_n = max_int(a->len, b->len);
    bigint_resize(out, max_n);

    Digit carry = 0;
    for (int i = 0; i < max_n; i++) {
        // Get the current least-significant-digits of each operand.
        Digit arg_a  = bigint_safe_at(a, i);
        Digit arg_b  = bigint_safe_at(b, i);
        out->data[i] = digit_add_carry(arg_a, arg_b, &carry);
    }
    return bigint_add_carry(out, carry);
}

static Error
bigint_sub_unsigned(BigInt *out, const BigInt *a, const BigInt *b)
{
    unused(out);
    unused(a);
    unused(b);
    stub();
    return BIGINT_OK;
}

BigInt_Error
bigint_add(BigInt *out, const BigInt *a, const BigInt *b)
{
    // 1.) One of the operands is negative and the other is positive?
    if (a->sign != b->sign) {
        // 1.1.) -a + b == b - a
        if (a->sign) {
            return bigint_sub_unsigned(out, b, a);
        }

        // 1.2.) a + -b == a - b
        return bigint_sub_unsigned(out, a, b);
    }

    // 2.) Both operands are both negative or both positive?
    // 2.1.) -a + -b == -a - b == -(a + b)
    // 2.2.) +a + +b == +(a + b)
    out->sign = a->sign;
    return bigint_add_unsigned(out, a, b);
}

BigInt_Error
bigint_add_digit(BigInt *out, const BigInt *a, BigInt_Digit b)
{
    // -a + b == b - a == -(a - b)
    if (a->sign) {
        Error err = bigint_sub_digit(out, a, b);
        out->sign = !out->sign;
        return err;
    }
    bigint_resize(out, a->len);

    // For our purposes `b` will also hold the carry.
    for (int i = 0; i < a->len; i++) {
        out->data[i] = digit_add_carry(a->data[i], 0, &b);
        if (b == 0) {
            break;
        }
    }
    return bigint_add_carry(out, b);
}

BigInt_Error
bigint_sub(BigInt *out, const BigInt *a, const BigInt *b)
{
    // 1.) One of the operands is negative and the other is positive?
    if (a->sign != b->sign) {
        // 1.1.) -a - b == -(a + b)
        if (a->sign) {
            Error e = bigint_add_unsigned(out, a, b);
            out->sign = !bigint_is_zero(out);
            return e;
        }
        // 1.2.) a - -b == a + b
        return bigint_add_unsigned(out, a, b);
    }

    // 2.) Both of the operands are negative or positive?
    // 2.1.) a - b
    // 2.2.) -a - -b == -a + b == -(a - b)
    stub();
    return BIGINT_OK;
}

BigInt_Error
bigint_sub_digit(BigInt *out, const BigInt *a, BigInt_Digit b)
{
    // Save in case of aliasing.
    int n = a->len;
    unused(b);
    bigint_resize(out, n);
    // -a - b <= 0
    //  a - b >= 0
    out->sign = a->sign;
    
    // Digit crossed = 0;
    // for (int i = 0; i < n; i++) {

    // }
    return BIGINT_OK;
}

BigInt_Error
bigint_mul(BigInt *restrict out, const BigInt *a, const BigInt *b)
{
    // 1.1.) +a * +b >= 0
    // 1.2.) +a * -b <= 0
    // 1.3.) -a * -b <= 0
    // 1.4.) -a * -b >= 0
    out->sign = (a->sign == b->sign);
    stub();
    return BIGINT_OK;
}

static Digit
digit_mul_carry(Digit a, Digit b, Digit *carry)
{
    Result product = (cast(Result)a * cast(Result)b) + cast(Result)*carry;
    return result_split(product, carry);
}

static Error
bigint_mul_carry(BigInt *b, Digit carry)
{
    if (carry > 0) {
        bigint_resize(b, b->len + 1);
        /** @todo(2025-11-02): Should be += or just = ? */
        b->data[b->len - 1] = carry;
    }
    return BIGINT_OK;
}

BigInt_Error
bigint_mul_digit(BigInt *out, const BigInt *a, BigInt_Digit b)
{
    // Save in case `out` aliases `a`
    int n = a->len;
    bigint_resize(out, n);

    // if a >= 0 then a * digit >= 0
    // if a  < 0 then a * digit < 0
    out->sign = a->sign;

    Digit carry = 0;
    for (int i = 0; i < n; i++) {
        out->data[i] = digit_mul_carry(a->data[i], b, &carry);
    }
    return bigint_mul_carry(out, carry);
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
    // We are only storing the zero digit?
    return (b->len == 1) && (b->data[0] == 0);
}

bool
bigint_eq(const BigInt *a, const BigInt *b)
{
    // Fast paths
    if (a->sign != b->sign || a->len != b->len) {
        return false;
    }
    
    // Assumes a->len == b->len
    for (int i = 0; i < a->len; i++) {
        // Found some digit that differs?
        if (a->data[i] != b->data[i]) {
            return false;
        }
    }
    return true;
}

typedef enum {
    FAST_EQUAL,
    FAST_LESS,
    FAST_GREATER,
} Fast_Comparison;

static Fast_Comparison
bigint_fast_compare(const BigInt *a, const BigInt *b)
{
    // 1.) One is positive while the other is negative?
    if (a->sign != b->sign) {
        // 1.1.) -a < +b
        // 1.2.) +a < -b
        return a->sign ? FAST_LESS : FAST_GREATER;
    }

    // 2.) Same signs but differing lengths.
    if (a->len != b->len) {
        // When `a` is negative, it is only lesser if it has more digits.
        if (a->sign) {
            return a->len > b->len ? FAST_LESS : FAST_GREATER;
        }
        // Otherwise `a` is positive; it is only lesser if it has less digits.
        else {
            return a->len < b->len ? FAST_LESS : FAST_GREATER;
        }
    }
    return FAST_EQUAL;
}

bool
bigint_lt(const BigInt *a, const BigInt *b)
{
    Fast_Comparison fast = bigint_fast_compare(a, b);
    if (fast != FAST_EQUAL) {
        return fast == FAST_LESS;
    }

    // Same signs and same lengths; Need to do a digit-by-digit comparison.
    for (int i = 0; i < a->len; i++) {
        // Found some digit that is the opposite of less-than?
        if (a->data[i] >= b->data[i]) {
            return false;
        }
    }
    return true;
}

bool
bigint_leq(const BigInt *a, const BigInt *b)
{
    Fast_Comparison fast = bigint_fast_compare(a, b);
    if (fast != FAST_EQUAL) {
        return fast == FAST_LESS;
    }

    // Same signs and same lengths; Need to do a digit-by-digit comparison.
    for (int i = 0; i < a->len; i++) {
        // Found some digit that is the opposite of less-equal?
        if (a->data[i] > b->data[i]) {
            return false;
        }
    }
    return true;
}
