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
    b->sign = false;
}

static int
count_digits_int(int *value, int base, bool *sign)
{
    int tmp = *value;
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

static void
bigint_zero(BigInt *b, int start, int stop)
{
    Digit *data = &b->data[start];
    size_t len  = cast(size_t)(stop - start);
    memset(data, 0, sizeof(data[0]) * len);
}

static void
bigint_init_len_cap(BigInt *b, int len, int cap)
{
    b->data = make(Digit, cap);
    b->len  = len;
    b->cap  = cap;
    b->sign = false;
    bigint_zero(b, 0, cap);
}

void
bigint_init_int(BigInt *b, int value)
{
    int n_digits = count_digits_int(&value, BIGINT_DIGIT_BASE, &b->sign);
    bigint_init_len_cap(b, n_digits, n_digits);

    for (int i = 0; i < n_digits; i++) {
        // Get the (current) least significant digit.
        // Concept check: 1234 % 10 == 4
        b->data[i] = value % BIGINT_DIGIT_BASE;

        // Pop the (current) least significant digit.
        // Concept check: 1234 // 10 == 123
        value /= BIGINT_DIGIT_BASE;
    }
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

static bool
string_get_sign(String *s)
{
    bool sign = false;
    if (s->len > 1) {
        char ch = s->data[0];
        if (ch == '+') {
            goto skip_sign;
        } else if (ch == '-') {
            sign = true;
skip_sign:
            s->data += 2;
            s->len -= 2;
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

void
bigint_init_string(BigInt *b, const char *s)
{
    String m;
    m.data = s;
    m.len  = strlen(s);
    string_trim(&m);

    // Start with 0 so we can build on top of it.
    bigint_init_int(b, 0);

    // Check for unary minus or unary plus
    b->sign = string_get_sign(&m);

    // Check for leading base prefix
    int base = string_get_base(&m);

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
        }
    }
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

const char *
bigint_to_lstring(const BigInt *b, char *buf, size_t cap, size_t *len)
{
    String_Builder sb;
    string_init(&sb, buf, cast(int)cap);

    // Can't fit desired big integer string representation in the buffer?
    if (cast(size_t)b->len + b->sign >= cap) {
        return NULL;
    }

    if (b->len == 0) {
        string_append(&sb, '0');
        goto nul_terminate;
    }

    if (b->sign) {
        string_append(&sb, '-');
    }

    // Write the most significant digits first.
    int msd_index = b->len - 1;
    for (int i = msd_index; i >= 0; --i) {
        Digit digit = b->data[i];

        // For each digit, write its base-10 representation from most-to-least
        // significant using the current maximum place value (`10**n`).
        Digit place = digit_place_value(digit, /*base=*/ 10);

        // Do we require leading zeroes for non-most-significant-digits?
        if (i < msd_index) {
            Result tmp = cast(Result)digit;
            // E.g. in base-100, we want to pad '1' with 1 zero to get 01 in 1801.
            while (tmp * 10 < BIGINT_DIGIT_BASE) {
                string_append(&sb, '0');
                tmp *= 10;
            }
        }

        // Write all the non-zero (significant) digits.
        for (;;) {
            Digit first = digit / place;
            string_append(&sb, cast(char)first + '0');
            digit -= first * place;
            place /= 10;
            if (place == 0) {
                break;
            }
        }
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

static void
bigint_resize(BigInt *b, int n)
{
    if (n > b->cap) {
        b->data = resize(Digit, b->data, b->cap, n);
        bigint_zero(b, /*start=*/ b->cap, /*stop=*/ n);
        b->cap = n;
    }
    // Resizing always changes the user-facing length.
    b->len = n;
}

void
bigint_add(BigInt *out, const BigInt *a, const BigInt *b)
{
    // One of the operands is negative and the other is positive?
    if (a->sign != b->sign) {
        // -a + b == b - a
        if (a->sign) {
            bigint_sub(out, b, a);
            return;
        }
        //  a + -b == a - b
        if (b->sign) {
            bigint_sub(out, a, b);
            return;
        }
    }
    // Save in case `out` aliases either `a` or `b`
    int max_n = max_int(a->len, b->len);

    // +1 to cap for pessimistic assumption.
    // Concept check:  1 (len=1) +  2 (len=1) =   3 (len=1)
    //                 9 (len=1) +  1 (len=1) =  10 (len=2)
    //                99 (len=2) + 99 (len=2) = 198 (len=3)
    bigint_resize(out, max_n + 1);

    Digit carry = 0;
    for (int i = 0; i < max_n; i++) {
        // Get the current least-significant-digits of each operand.
        Digit  da  = bigint_safe_at(a, i);
        Digit  db  = bigint_safe_at(b, i);
        Result sum = da + db + carry;
        // E.g. 9 + 3 = 12
        if (sum >= BIGINT_DIGIT_BASE) {
            carry = sum / BIGINT_DIGIT_BASE; // 12 // 10 == 1
            sum %= BIGINT_DIGIT_BASE;        // 12 %  10 == 2
        } else {
            carry = 0;
        }
        out->data[i] = sum;
    }

    // Pessimistic assumption was satisfied?
    if (carry > 0) {
        out->data[out->len - 1] = carry;
    } else {
        out->len -= 1;
    }

    // Assumes a->sign == b->sign and both are negative
    // -a + -b == -a - b == -(a + b)
    if (a->sign) {
        // Only negate non-zero results
        if (out->len == 1 && out->data[0] == 0) {
            out->sign = false;
        } else {
            out->sign = true;
        }
    } else {
        out->sign = false;
    }

}

void
bigint_add_digit(BigInt *out, const BigInt *a, BigInt_Digit digit)
{
    // -a + b == b - a
    if (a->sign) {
        stub();
    }
    // +1 to cap for pessimistic assumption.
    bigint_resize(out, a->len + 1);

    Digit carry = digit;
    for (int i = 0; i < a->len; i++) {
        Digit  tmp = a->data[i];
        Result sum = tmp + carry;
        if (sum >= BIGINT_DIGIT_BASE) {
            carry = sum / BIGINT_DIGIT_BASE;
            sum  %= BIGINT_DIGIT_BASE;
        } else {
            carry = 0;
        }
        out->data[i] = sum;
        if (carry == 0) {
            break;
        }
    }

    // Pessimistic assumption was satisfied?
    if (carry > 0) {
        out->data[out->len - 1] = carry;
    } else {
        out->len -= 1;
    }
}

void
bigint_sub(BigInt *out, const BigInt *a, const BigInt *b)
{
    unused(out);
    unused(a);
    unused(b);
    stub();
}

void
bigint_mul(BigInt *restrict out, const BigInt *a, const BigInt *b)
{
    unused(out);
    unused(a);
    unused(b);
    stub();
}

void
bigint_mul_digit(BigInt *out, const BigInt *a, BigInt_Digit digit)
{
    // Save in case `out` aliases `a`
    int n = a->len;
    bigint_resize(out, n + 1);

    // if a >= 0 then a * digit >= 0
    // if a  < 0 then a * digit < 0
    out->sign = a->sign;

    Result carry = 0;
    for (int i = 0; i < n; i++) {
        Result product = (cast(Result)a->data[i] * cast(Result)digit) + carry;
        if (product >= BIGINT_DIGIT_BASE) {
            carry = product / BIGINT_DIGIT_BASE;
            product %= BIGINT_DIGIT_BASE;
        } else {
            carry = 0;
        }

        out->data[i] = product;
    }

    // Pessimistic assumption was satisfied?
    if (carry > 0) {
        out->data[out->len - 1] += carry;
    } else {
        out->len -= 1;
    }
}

void
bigint_div(BigInt *restrict out, const BigInt *a, const BigInt *b)
{
    unused(out);
    unused(a);
    unused(b);
    stub();
}

void
bigint_mod(BigInt *restrict out, const BigInt *a, const BigInt *b)
{
    unused(out);
    unused(a);
    unused(b);
    stub();
}
