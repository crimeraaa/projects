#include <stdio.h>  // fputc, fprintf
#include <stdlib.h> // malloc, free
#include <string.h> // memset

#include "bigint.h"

#define cast(T)     (T)

static int
string_to_int(const char *s)
{
    int value = 0;
    for (const char *p = s; *p != '\0'; p++) {
        value *= 10;
        value += *p - '0';
    }
    return value;
}

int
main(int argc, char *argv[])
{
    if (argc != 4) {
        fprintf(stderr, "Usage: %s <integer> <operation> <integer>\n", argv[0]);
        return 1;
    }

    Big_Int a, b, c;
    int err = 0;
    big_int_init_int(&a, string_to_int(argv[1]));
    big_int_init_int(&b, string_to_int(argv[3]));

    switch (argv[2][0]) {
    case '+':
        big_int_add(&a, &b, &c);
        break;
    default:
        err = 1;
        fprintf(stderr, "Invalid operation '%s'.\n", argv[2]);
        goto cleanup;
    }

    char buf[64];
    printf("a: '%s'\n", big_int_to_string(&a, buf, sizeof(buf)));
    printf("b: '%s'\n", big_int_to_string(&b, buf, sizeof(buf)));
    printf("c: '%s'\n", big_int_to_string(&c, buf, sizeof(buf)));

    big_int_destroy(&c);
cleanup:
    big_int_destroy(&b);
    big_int_destroy(&a);
    return err;
}

void
big_int_init(Big_Int *b)
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
    // The below loop will never start.
    if (tmp == 0) {
        return 1;
    }

    *sign = (tmp < 0);
    if (*sign) {
        // Concept check: -1234 % 10 == 6; We do not want this!
        *value = -tmp;
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

#define make(T, n)  cast(T *)malloc(sizeof(T) * (n))

static void
big_int_init_len_cap(Big_Int *b, int len, int cap)
{
    b->data = make(Big_Int_Digit, cap);
    b->len  = len;
    b->cap  = cap;
    memset(b->data, 0, cap);
}

void
big_int_init_int(Big_Int *b, int value)
{
    int n_digits = count_digits_int(&value, BIGINT_DIGIT_BASE, &b->sign);
    big_int_init_len_cap(b, n_digits, n_digits);

    for (int i = 0; i < n_digits; i++) {
        // Get the (current) least significant digit.
        // Concept check: 1234 % 10 == 4
        b->data[i] = value % BIGINT_DIGIT_BASE;

        // Pop the (current) least significant digit.
        // Concept check: 1234 // 10 == 123
        value /= BIGINT_DIGIT_BASE;
    }
}

static char *
string_append(char *buf, int cap, int *len)
{
    if (0 <= *len && *len < cap) {
        *len += 1;
        return &buf[*len - 1];
    }
    fprintf(stderr, "[FATAL] Out of bounds index %i / %i\n", *len, cap);
    __builtin_trap();
}

static int
count_digits_digit(Big_Int_Digit d, int base)
{
    int n = 0;
    while (d > 0) {
        n++;
        d /= base;
    }
    return n;
}

const char *
big_int_to_string(const Big_Int *b, char *buf, size_t cap)
{
    int n_written = 0;

    // Can't fit desired big integer string representation in the buffer?
    if (cast(size_t)b->len + b->sign >= cap) {
        return NULL;
    }

    if (b->len == 0) {
        *string_append(buf, cap, &n_written) = '0';
        goto nul_terminate;
    }

    if (b->sign) {
        *string_append(buf, cap, &n_written) = '-';
    }

    // Write the most significant digits first.
    for (int i = b->len - 1; i >= 0; --i) {
        Big_Int_Digit digit = b->data[i];
        int n = count_digits_digit(digit, 10);

        // For each digit, write its base-10 representation from most-to-least
        // significant using the current maximum place value (`10**n`).
        Big_Int_Digit place = 1;
        for (int j = 1; j < n; j++) {
            place *= 10;
        }

        for (;;) {
            char ch = (digit / place) + '0';
            *string_append(buf, cap, &n_written) = ch;
            digit -= place;
            place /= 10;
            if (place == 0) {
                break;
            }
        }
    }

nul_terminate:
    *string_append(buf, cap, &n_written) = '\0';
    return buf;
}

void
big_int_destroy(Big_Int *b)
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

static Big_Int_Digit
big_int_safe_at(const Big_Int *b, int i)
{
    if (0 <= i && i < b->len) {
        return b->data[i];
    }
    return 0;
}

void
big_int_add(const Big_Int *restrict a, const Big_Int *restrict b,
    Big_Int *restrict out)
{
    int out_len = max_int(a->len, b->len);
    // +1 to cap for pessimistic assumption.
    // Concept check: 1 (len=1) + 2 (len=1) = 3  (len=1)
    //                9 (len=1) + 1 (len=1) = 10 (len=2)
    big_int_init_len_cap(out, out_len, out_len + 1);

    Big_Int_Digit carry = 0;
    for (int i = 0; i < out_len; i++) {
        // Get the current least-significant-digits of each operand.
        Big_Int_Digit  da  = big_int_safe_at(a, i);
        Big_Int_Digit  db  = big_int_safe_at(b, i);
        Big_Int_Result sum = da + db + carry;
        // E.g. 9 + 3 = 12
        if (sum >= BIGINT_DIGIT_BASE) {
            carry = sum / BIGINT_DIGIT_BASE; // 12 // 10 == 1
            sum %= BIGINT_DIGIT_BASE;        // 12 %  10 == 2
        }
        out->data[i] = sum;
    }

    if (carry > 0) {
        out->data[out->len] = carry;
        out->len++;
    }
}
