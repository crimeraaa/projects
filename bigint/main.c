#include <stdio.h>  // fgets, fputc, printf, fprintf
#include <string.h> // strlen, strcspn
#include <stdlib.h> // realloc, free

#include "../mem/arena.c"

#include "bigint.c"
#include "lexer.c"
#include "parser.c"

static void *
stdc_allocator_fn(void *context,
    Allocator_Mode      mode,
    void               *old_memory,
    size_t              old_size,
    size_t              new_size,
    size_t              align)
{
    // Not needed; the malloc family already tracks this for us.
    unused(context);
    unused(old_size);
    unused(align);

    switch (mode) {
    case ALLOCATOR_ALLOC:
    case ALLOCATOR_RESIZE:
        return realloc(old_memory, new_size);
    case ALLOCATOR_FREE:
        free(old_memory);
    case ALLOCATOR_FREE_ALL:
        break;
    }
    return NULL;
}

static const Allocator
stdc_allocator = {stdc_allocator_fn, NULL};

static Arena
arena;

static void
bigint_print(const BigInt *b, char c)
{
    size_t n = 0;

    const char *s = bigint_to_lstring(b, &n, arena_allocator(&arena));
    printfln("%c: '%s' (%zu / %zu chars written)",
        c, s, n, bigint_string_length(b));
}

static int
unary(const char *op, const char *arg)
{
    BigInt b;
    int err = 0;
    bigint_init_string(&b, arg, stdc_allocator);
    if (op != NULL) {
        eprintfln("Invalid unary operation '%s'", op);
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
    printfln("%s %s %s => %s",
        bigint_to_string(a, arena_allocator(&arena)),
        op,
        bigint_to_string(b, arena_allocator(&arena)),
        (cmp) ? "true" : "false");
}

static int
binary(const char *arg_a, const char *op, const char *arg_b)
{
    BigInt a, b, c;
    int err = 0;
    bigint_init_string(&a, arg_a, stdc_allocator);
    bigint_init_string(&b, arg_b, stdc_allocator);
    bigint_init(&c, stdc_allocator);

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
        eprintfln("Invalid binary operation '%s'.", op);
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
    bigint_init(&ans, stdc_allocator);
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
        arena_free_all(&arena);
    }
    bigint_destroy(&ans);
    return 0;
}

int
main(int argc, char *argv[])
{
    static char buf[BUFSIZ];
    arena_init(&arena, buf, sizeof(buf));
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
        eprintfln("Usage: %s [<integer> [<operation> <integer>]]\n", argv[0]);
        return 1;
    }
}
