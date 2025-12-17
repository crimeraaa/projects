// C
#include <stdio.h>  // fprintf
#include <stdlib.h> // atoi
#include <string.h> // strlen

// projects
#include <mem/allocator.c>
#include <utils/strings.c>

// main
#include "i128.h"
#include "lexer.c"
#include "parser.c"

/** @brief Writes at most `len - 2` characters (i.e. `buf[:len - 1]`),
 *  writing the nul character at `len - 1`.
 */

const char *
i128_binary_base_string(i128 a, char *buf, size_t len, unsigned int shift)
{
    i128 zero, mask;
    size_t buf_i = 0;

    zero = i128_from_u64(0);
    mask = i128_from_u64((1 << shift) - 1);

    // Check if we can accomodate the base prefix.
    if (buf_i + 2 > len - 1) {
        goto nul_terminate;
    }

    // Write base prefix.
    buf[buf_i++] = '0';
    switch (1 << shift) {
    case 2:  buf[buf_i++] = 'b'; break;
    case 8:  buf[buf_i++] = 'o'; break;
    case 16: buf[buf_i++] = 'x'; break;
    }

    if (i128_eq(a, zero)) {
        // Check if we can accomodate this character.
        if (buf_i + 1 <= len - 1) {
            buf[buf_i++] = '0';
        }
        goto nul_terminate;
    }

    // Write LSD to MSD into the buffer.
    for (; buf_i < len - 1 && !i128_eq(a, zero); buf_i++) {
        uint64_t digit;

        // digit = a % base
        // a     = a / base
        digit = i128_and(a, mask).lo;
        a     = i128_shift_right_logical(a, shift);
        if (digit < 10) {
            buf[buf_i] = cast(char)digit + '0';
        } else {
            buf[buf_i] = cast(char)digit - 10 + 'a';
        }
    }

    // Rewrite to be MSD to LSD.
    for (size_t left = 2, right = buf_i - 1; left < right; left++, right--) {
        char tmp;

        tmp        = buf[left];
        buf[left]  = buf[right];
        buf[right] = tmp;
    }
nul_terminate:
    buf[buf_i] = '\0';
    return buf;
}

static const char *
i128_bin(i128 a, char *buf, size_t len)
{
    return i128_binary_base_string(a, buf, len, 1);
}

static const char *
i128_oct(i128 a, char *buf, size_t len)
{
    return i128_binary_base_string(a, buf, len, 3);
}

static const char *
i128_hex(i128 a, char *buf, size_t len)
{
    return i128_binary_base_string(a, buf, len, 4);
}

i128
i128_from_lstring(const char *restrict s, size_t n, const char **restrict end_ptr, int base)
{
    i128 dst, base_i128;
    size_t i = 0;

    dst = i128_from_u64(0);
    if (n > 2 && s[i] == '0') {
        int string_base = 0;

        i++;
        switch (s[i]) {
        case 'b': case 'B': string_base = 2;  i++; break;
        case 'o': case 'O': string_base = 8;  i++; break;
        case 'd': case 'D': string_base = 10; i++; break;
        case 'x': case 'X': string_base = 16; i++; break;
        }

        // Didn't know the base beforehand, so we have it now.
        if (base == 0 && string_base != 0) {
            base = string_base;
        }
        // Inconsistent base received?
        else if (base != string_base) {
            goto finish;
        }
    } else if (base == 0) {
        base = 10;
    }

    base_i128 = i128_from_u64(cast(uint64_t)base);

    for (; i < n; i += 1) {
        i128 digit_i128;
        uint64_t digit = 0;
        char ch;

        ch = s[i];
        if (ch == '_' || ch == ',' || is_space(ch)) {
            continue;
        }

        if (is_digit(ch)) {
            digit = cast(uint64_t)(ch - '0');
        } else if (is_upper(ch)) {
            digit = cast(uint64_t)(ch - 'A' + 10);
        } else if (is_lower(ch)) {
            digit = cast(uint64_t)(ch - 'a' + 10);
        } else {
            break;
        }

        // Not a valid digit in this base?
        if (digit >= cast(uint64_t)base) {
            break;
        }

        // dst *= base
        // dst += digit
        digit_i128 = i128_from_u64(digit);
        dst        = i128_mul_unsigned(dst, base_i128);
        dst        = i128_add_unsigned(dst, digit_i128);
    }

finish:
    if (end_ptr) {
        *end_ptr = &s[i];
    }
    return dst;
}

int
main(void)
{
    char buf[BUFSIZ];
    for (;;) {
        Parser p;
        Value v;
        const char *s;
        size_t n;
        Parser_Error err;

        fputs(">>> ", stdout);
        if (fgets(buf, sizeof(buf), stdin) == NULL) {
            fputc('\n', stdout);
            break;
        }
        s = buf;
        n = strcspn(buf, "\r\n");

        parser_init(&p, (String){s, n}, (Allocator){NULL, NULL});

        v.type    = VALUE_INTEGER;
        v.integer = i128_from_u64(0);
        err       = parser_parse(&p, &v);
        if (err == PARSER_OK) {
            switch (v.type) {
            case VALUE_BOOLEAN:
                printfln("%s", v.boolean ? "true" : "false");
                break;
            case VALUE_INTEGER:
                printfln("bin(%s)", i128_bin(v.integer, buf, sizeof(buf)));
                printfln("oct(%s)", i128_oct(v.integer, buf, sizeof(buf)));
                printfln("hex(%s)", i128_hex(v.integer, buf, sizeof(buf)));
                break;
            }
        }
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
