#include <stdio.h>  // fgets, fputc, printf, fprintf
#include <string.h> // strlen, strcspn
#include <stdlib.h> // realloc, free

// bigint
#include "../mem/allocator.c"
#include "../utils/strings.c"
#include "bigint.c"

// main
#include "../mem/arena.c"
#include "lexer.c"
#include "parser.c"

static void *
default_allocator_fn(void *context,
    Allocator_Mode         mode,
    void                  *old_memory,
    size_t                 old_size,
    size_t                 new_size,
    size_t                 align)
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

static Allocator
default_allocator = {default_allocator_fn, NULL},
temp_allocator;


static void
bigint_print(const BigInt *b, char c)
{
    size_t n = 0;

    const char *s = bigint_to_lstring(b, &n, temp_allocator);
    printfln("%c: '%s' (%zu / %zu chars written)",
        c, s, n, bigint_string_length(b));
}

static int
unary(const char *op, const char *arg)
{
    BigInt b;
    int err = 0;
    bigint_init_string(&b, arg, default_allocator);
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
        bigint_to_string(a, temp_allocator),
        op,
        bigint_to_string(b, temp_allocator),
        (cmp) ? "true" : "false");
}

static int
binary(const char *arg_a, const char *op, const char *arg_b)
{
    BigInt a, b, c;
    int err = 0;
    bigint_init_string(&a, arg_a, temp_allocator);
    bigint_init_string(&b, arg_b, temp_allocator);
    bigint_init(&c, temp_allocator);

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
evaluate(String input, Value *ans)
{
    Parser p;
    parser_init(&p, input, temp_allocator);
    Parser_Error err = parser_parse(&p, ans);
    if (err == PARSER_OK) {
        switch (value_type(*ans)) {
        case VALUE_INTEGER:
            bigint_print(ans->integer, 'a');
            break;
        case VALUE_BOOLEAN:
            println(ans->boolean ? "true" : "false");
            break;
        }
    }
}

static int
repl(void)
{
    char buf[256];
    BigInt b;
    bigint_init(&b, default_allocator);
    for (;;) {
        String s;
        printf(">");
        s.data = fgets(buf, sizeof(buf), stdin);
        if (s.data == NULL) {
            fputc('\n', stdout);
            break;
        }
        s.len = strcspn(s.data, "\r\n");

        bigint_clear(&b);
        Value ans;
        ans.type    = VALUE_INTEGER;
        ans.integer = &b;
        evaluate(s, &ans);
        mem_free_all(temp_allocator);
    }
    bigint_destroy(&b);
    return 0;
}

int
main(int argc, char *argv[])
{
    static char buf[BUFSIZ];
    static Arena arena;
    arena_init(&arena, buf, sizeof(buf));
    temp_allocator = arena_allocator(&arena);
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
