#include <stdio.h>  // fputs, fprintf, printf
#include <string.h> // strcspn
#include <stdlib.h> // realloc, free

#include "lstring.h"
#include "bigint.h"
#include "parser.h"
#include "stack.h"

static void *
stdc_allocator_fn(void *ptr, size_t old_size, size_t new_size, void *context)
{
    // Not needed; the malloc family already tracks this for us.
    unused(old_size);
    unused(context);

    // Free request?
    if (new_size == 0) {
        free(ptr);
        return NULL;
    }

    // New allocation or resize request?
    return realloc(ptr, new_size);
}

static const Allocator
stdc_allocator = {stdc_allocator_fn, NULL};

static Stack
stack;

static const Allocator
stack_allocator = {stack_allocator_fn, &stack};

static void
bigint_print(const BigInt *b, char c)
{
    size_t n = 0;

    const char *s = bigint_to_lstring(b, &stack_allocator, &n);
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
        eprintfln("Invalid unary operation '%s'\n", op);
        err = 1;
        goto cleanup;
    }
    bigint_print(&b, 'b');

cleanup:
    bigint_destroy(&b);
    return err;
}

static void
print_compare(const BigInt *a, const char *op, const BigInt *b, bool cmp)
{
    printf("%s %s %s => %s\n",
        bigint_to_string(a, &stack_allocator), op,
        bigint_to_string(b, &stack_allocator), (cmp) ? "true" : "false");
}

static int
binary(const char *arg_a, const char *op, const char *arg_b)
{
    BigInt a, b, c;
    int err = 0;
    bigint_init_string(&a, arg_a, &stdc_allocator);
    bigint_init_string(&b, arg_b, &stdc_allocator);
    bigint_init(&c, &stdc_allocator);

    // The following is absolutely abysmal
    switch (strlen(op)) {
    case 1: {
        switch (op[0]) {
        case '+': bigint_add(&c, &a, &b); break;
        case '-': bigint_sub(&c, &a, &b); break;
        case '<': print_compare(&a, op, &b, bigint_lt(&a, &b)); break;
        case '>': print_compare(&a, op, &b, bigint_gt(&a, &b)); break;
        default:  goto error;
        }
        break;
    }
    case 2: {
        if (op[1] != '=') {
            goto error;
        }
        switch (op[0]) {
        case '!': print_compare(&a, op, &b, bigint_neq(&a, &b)); break;
        case '=': print_compare(&a, op, &b, bigint_eq(&a, &b));  break;
        case '<': print_compare(&a, op, &b, bigint_leq(&a, &b)); break;
        case '>': print_compare(&a, op, &b, bigint_geq(&a, &b)); break;
        default:  goto error;
        }
        break;
    }
    default:
error:
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

static void
evaluate(String input, BigInt *ans)
{
    Parser p;
    parser_init(&p, input);
    Parser_Error err = parser_parse(&p, ans);
    if (err == PARSER_OK) {
        bigint_print(ans, 'a');
    }
}

static int
repl(void)
{
    char buf[256];
    BigInt ans;
    bigint_init(&ans, &stdc_allocator);
    for (;;) {
        String s;
        printf(">");
        s.data = fgets(buf, sizeof(buf), stdin);
        if (s.data == NULL) {
            fputc('\n', stdout);
            break;
        }
        s.len = strcspn(s.data, "\r\n");

        bigint_clear(&ans);
        evaluate(s, &ans);
        stack_free_all(&stack);
    }
    bigint_destroy(&ans);
    return 0;
}

int
main(int argc, char *argv[])
{
    switch (argc) {
    case 1:
        return repl();
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
