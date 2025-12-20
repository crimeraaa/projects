// C
#include <stdio.h>  // fprintf
#include <stdlib.h> // atoi
#include <string.h> // strlen

// main
#include <mem/arena.c>
#include "parser.c"

static void
internal_string_builder_reverse(String_Builder *sb, size_t start, size_t stop)
{
    char *buf;
    size_t left, right;

    buf   = sb->data;
    left  = start;
    right = stop - 1;
    for (; left < right; left++, right--) {
        char tmp   = buf[left];
        buf[left]  = buf[right];
        buf[right] = tmp;
    }
}

static char
internal_int_to_char(int digit)
{
    if (0 <= digit && digit <= 9) {
        return cast(char)('0' + digit);
    } else {
        return cast(char)('a' - 10 + digit);
    }
}


/** @brief Writes at most `len - 2` characters (i.e. `buf[:len - 1]`),
 *  writing the nul character at `len - 1`. */
static const char *
i128_to_binary_string(i128 a, uint base, uint shift, Allocator allocator)
{
    String_Builder sb;
    u128 a_abs, mask;
    size_t group_size, group_iter, group_total, group_count = 0, prefix_i = 0;
    char leader_char;

    string_builder_init(&sb, allocator);
    a_abs = u128_from_i128(a);
    mask  = u128_from_u64(base - 1);
    leader_char = internal_int_to_char(i128_sign(a) ? cast(int)base - 1 : 0);

    // Write base prefix.
    switch (base) {
    case 2:
        // group_size=64 * shift=1 * group_total=2 = 128
        // group_size=32 * shift=1 * group_total=4 = 128
        // group_size=16 * shift=1 * group_total=8 = 128
        // group_size=8  * shift=1 * group_total=8 = 128
        group_size  = 8;
        group_total = 8;
        if (!string_write_literal(&sb, "0b")) {
            goto nul_terminate;
        }
        break;
    case 8:
        // group_size=21 * shift=3 * group_total=2 == 128
        // group_size=10 * shift=3 * group_total=4 == 128
        group_size  = 10;
        group_total = 2;
        if (!string_write_literal(&sb, "0o")) {
            goto nul_terminate;
        }
        break;
    case 16:
        // group_size=16 * shift=4 * group_total=2 == 128
        // group_size=8  * shift=4 * group_total=4 == 128
        group_size  = 8;
        group_total = 4;
        if (!string_write_literal(&sb, "0x")) {
            goto nul_terminate;
        }
        break;
    }
    group_iter = group_size;

    prefix_i = sb.len;
    if (u128_eq(a_abs, U128_ZERO)) {
        string_write_char(&sb, '0');
        goto add_leading_zeroes;
    }

    // Write LSD to MSD into the buffer.
    while (!u128_eq(a_abs, U128_ZERO)) {
        int digit;
        char ch;

        // digit = a % base
        // a     = a / base
        //
        // NOTE: wrong output for signed octal since 3 is not a clean
        //      quotient of 128. However, we can't arithmetic right
        //      shift because we'll never reach zero.
        digit = cast(int)u128_and(a_abs, mask).lo;
        a_abs = u128_shift_right(a_abs, shift);
        ch    = internal_int_to_char(digit);

        if (group_iter == 0) {
            if (!string_write_char(&sb, '_')) {
                goto nul_terminate;
            }
            group_iter   = group_size;
            group_count += 1;
        }
        group_iter -= 1;

        if (!string_write_char(&sb, ch)) {
            goto nul_terminate;
        }
    }

add_leading_zeroes:
    if (group_count < group_total) {
        while (group_iter > 0) {
            if (!string_write_char(&sb, leader_char)) {
                goto nul_terminate;
            }
            group_iter -= 1;
        }
    }

    // Rewrite to be MSD to LSD.
    internal_string_builder_reverse(&sb, prefix_i, sb.len);
nul_terminate:
    return string_to_cstring(&sb, NULL);
}

static const char *
i128_bin(i128 a, Allocator allocator)
{
    const uint base = 2, shift = 1;
    return i128_to_binary_string(a, base, shift, allocator);
}

static const char *
i128_oct(i128 a, Allocator allocator)
{
    const uint base = 8, shift = 3;
    return i128_to_binary_string(a, base, shift, allocator);
}

static const char *
i128_hex(i128 a, Allocator allocator)
{
    const uint base = 16, shift = 4;
    return i128_to_binary_string(a, base, shift, allocator);
}

int
main(void)
{
    Arena arena;
    Allocator allocator;
    char buf[BUFSIZ];

    arena_init(&arena, buf, sizeof(buf));
    allocator = arena_allocator(&arena);
    for (;;) {
        Parser p;
        Value v;
        String s;
        Parser_Error err;

        fputs("bigint> ", stdout);
        s.data = fgets(buf, sizeof(buf), stdin);
        if (s.data == NULL) {
            fputc('\n', stdout);
            break;
        }
        s.len = strcspn(buf, "\r\n");
        parser_init(&p, s, (Allocator){NULL, NULL});

        v.type    = VALUE_INTEGER;
        v.integer = I128_ZERO;
        err       = parser_parse(&p, &v);
        if (err == PARSER_OK) {
            switch (v.type) {
            case VALUE_BOOLEAN:
                printfln("%s", v.boolean ? "true" : "false");
                break;
            case VALUE_INTEGER:
                printfln("bin(%s)", i128_bin(v.integer, allocator));
                printfln("oct(%s)", i128_oct(v.integer, allocator));
                printfln("hex(%s)", i128_hex(v.integer, allocator));
                break;
            }
        }
        arena_free_all(&arena);
    }

    return 0;
}

#if 0

#include <stdio.h>  // fgets, fputc, printf, fprintf
#include <string.h> // strlen, strcspn
#include <stdlib.h> // realloc, free

// bigint
#include <mem/allocator.c>
#include <utils/strings.c>
#include "bigint.c"

// main
#include <mem/arena.c>
#include "lexer.c"
#include "parser.c"

static void *
default_allocator_fn(void *context,
    Allocator_Mode         mode,
    void                  *old_ptr,
    size_t                 old_size,
    size_t                 new_size,
    size_t                 align)
{
    // Not needed; the malloc family already tracks this for us.
    unused(context);
    unused(align);

    switch (mode) {
    case ALLOCATOR_ALLOC:
    case ALLOCATOR_RESIZE: {
        void *new_ptr = realloc(old_ptr, new_size);
        // Have a new region to zero out?
        if (new_ptr != NULL && old_size < new_size) {
            size_t growth  = new_size - old_size;
            char  *old_top = cast(char *)new_ptr + old_size;
            memset(old_top, 0, growth);
        }
        return new_ptr;
    }
    case ALLOCATOR_FREE:
        free(old_ptr);
        break;
    case ALLOCATOR_FREE_ALL:
        break;
    }
    return NULL;
}

static Allocator
default_allocator = {default_allocator_fn, NULL},
temp_allocator;

static Arena arena;

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

        printfln("(%zu / %zu bytes)", arena.curr_offset, arena.buf_len);
        mem_free_all(temp_allocator);
        printfln("(%zu / %zu bytes)", arena.curr_offset, arena.buf_len);
    }
    bigint_destroy(&b);
    return 0;
}

int
main(int argc, char *argv[])
{
    static char arena_buf[BUFSIZ];
    arena_init(&arena, arena_buf, sizeof(arena_buf));
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

#endif // if 0
