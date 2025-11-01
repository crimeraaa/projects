#include <stdio.h>  // fputs, fprintf, printf
#include <string.h> // strcspn

#include "bigint.h"

static int
unary(const char *op, const char *arg)
{
    BigInt b;
    int err = 0;
    char buf[256];
    bigint_init_string(&b, arg);
    if (op != NULL) {
        fprintf(stderr, "Invalid unary operation'%s'\n", op);
        err = 1;
        goto cleanup;
    }
    printf("b: '%s'\n", bigint_to_string(&b, buf, sizeof(buf)));
cleanup:
    bigint_destroy(&b);
    return err;
}

static int
binary(const char *arg_a, const char *op, const char *arg_b)
{
    BigInt a, b, c;
    int err = 0;
    char buf[256];
    bigint_init_string(&a, arg_a);
    bigint_init_string(&b, arg_b);
    bigint_init(&c);

    switch (op[0]) {
    case '+':
        bigint_add(&c, &a, &b);
        break;
    default:
        err = 1;
        fprintf(stderr, "Invalid binary operation '%s'.\n", op);
        goto cleanup;
    }

    printf("a: '%s'\n", bigint_to_string(&a, buf, sizeof(buf)));
    printf("b: '%s'\n", bigint_to_string(&b, buf, sizeof(buf)));
    printf("c: '%s'\n", bigint_to_string(&c, buf, sizeof(buf)));

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
        return unary(/*op=*/ NULL, /*arg=*/ argv[1]);
    case 3:
        return unary(/*op=*/ argv[1], /*arg_a=*/ argv[2]);
    case 4:
        return binary(/*arg_a=*/ argv[1], /*op=*/ argv[2], /*arg_b=*/ argv[3]);
    default:
        fprintf(stderr, "Usage: %s [<integer> [<operation> <integer>]]\n",
            argv[0]);
        return 1;
    }
}
