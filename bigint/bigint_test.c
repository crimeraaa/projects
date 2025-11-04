#include <stdio.h>  // fputs, fprintf, printf
#include <string.h> // strcspn
#include <stdlib.h> // realloc, free

#include "bigint.h"

#define cast(T)         (T)
#define unused(expr)    ((void)(expr))

static void *
stdc_allocator_fn(void *ptr, size_t old_size, size_t new_size, void *context)
{
    // Not needed; the malloc family already tracks this for us.
    unused(old_size);
    unused(context);

    // Free request?
    if (ptr != NULL && new_size == 0) {
        free(ptr);
        return NULL;
    }
    // New allocation or resize request?
    return realloc(ptr, new_size);
}

static const BigInt_Allocator
stdc_allocator = {stdc_allocator_fn, NULL};

static void *
buffer_allocator_fn(void *ptr, size_t old_size, size_t new_size, void *context)
{
    char *buf = cast(char *)context;
    unused(old_size);

    // Free request?
    if (ptr != NULL && new_size == 0) {
        return NULL;
    }

    // New allocation OR resize request which fits in the buffer?
    if (0 <= new_size && new_size < BUFSIZ) {
        return buf;
    }
    // Otherwise, doesn't fit in the buffer.
    return NULL;
}

static void
bigint_print(const BigInt *b, char c)
{
    char buf[BUFSIZ];
    size_t n = 0;
    BigInt_Allocator a;
    a.fn      = &buffer_allocator_fn;
    a.context = buf;

    const char *s = bigint_to_lstring(b, &a, &n);
    printf("%c: '%s' (%zu / %zu chars written)\n",
        c, s, n, bigint_string_length(b));
}

static int
unary(const char *op, const char *arg)
{
    BigInt b;
    int err = 0;
    bigint_init_string(&b, arg, &stdc_allocator);
    if (op != NULL) {
        fprintf(stderr, "Invalid unary operation'%s'\n", op);
        err = 1;
        goto cleanup;
    }
    bigint_print(&b, 'b');

cleanup:
    bigint_destroy(&b);
    return err;
}

static int
binary(const char *arg_a, const char *op, const char *arg_b)
{
    BigInt a, b, c;
    int err = 0;
    bigint_init_string(&a, arg_a, &stdc_allocator);
    bigint_init_string(&b, arg_b, &stdc_allocator);
    bigint_init(&c, &stdc_allocator);

    switch (op[0]) {
    case '+':
        bigint_add(&c, &a, &b);
        break;
    case '-':
        // bigint_sub(&c, &a, &b);
        bigint_sub_digit(&c, &a, b.data[0]);
        break;
    default:
        err = 1;
        fprintf(stderr, "Invalid binary operation '%s'.\n", op);
        goto cleanup;
    }

    bigint_print(&a, 'a');
    bigint_print(&b, 'b');
    bigint_print(&c, 'c');

cleanup:
    bigint_destroy(&c);
    bigint_destroy(&b);
    bigint_destroy(&a);
    return err;
}

int
main(int argc, char *argv[])
{
    switch (argc) {
    case 2:
        return unary(/*op=*/NULL, /*arg=*/argv[1]);
    case 3:
        return unary(/*op=*/argv[1], /*arg_a=*/argv[2]);
    case 4:
        return binary(/*arg_a=*/argv[1], /*op=*/argv[2], /*arg_b=*/argv[3]);
    default:
        fprintf(stderr, "Usage: %s [<integer> [<operation> <integer>]]\n",
            argv[0]);
        return 1;
    }
}
