#include <stdio.h>  // fputs, fprintf, printf
#include <string.h> // strcspn
#include <stdlib.h> // realloc, free

#include "lstring.h"
#include "bigint.h"
#include "bigint_repl.h"

#define cast(T)         (T)
#define unused(expr)    ((void)(expr))

static Token
lexer_lex(Lexer *x);

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

static void
evaluate(String input)
{
    Parser p;
    p.lexer.input  = input;
    p.lexer.start  = 0;
    p.lexer.cursor = 0;

    p.consumed.type   = TOKEN_EOF;
    p.consumed.lexeme = input;

    BigInt ans;
    bigint_init(&ans, &stdc_allocator);
    for (;;) {
        Token t = lexer_lex(&p.lexer);
        if (t.type == TOKEN_EOF) {
            break;
        }
        printf("Token(%02i): '%.*s'\n", cast(int)t.type,
            cast(int)t.lexeme.len, t.lexeme.data);
    }
    bigint_destroy(&ans);
}

static int
repl(void)
{
    char buf[512];
    for (;;) {
        printf(">");
        String s;
        s.data = fgets(buf, sizeof(buf), stdin);
        if (s.data == NULL) {
            fputc('\n', stdout);
            break;
        }
        s.len = strcspn(s.data, "\r\n");
        evaluate(s);
    }
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

static bool
lexer_is_eof(const Lexer *x)
{
    return x->cursor >= x->input.len;
}

// Returns the character at the current cursor.
static char
lexer_peek(Lexer *x)
{
    return x->input.data[x->cursor];
}

// Increments the cursor.
static void
lexer_advance(Lexer *x)
{
    x->cursor += 1;
}

// Advances the cursor only if it matches `ch`.
static bool
lexer_match(Lexer *x, char ch)
{
    if (lexer_peek(x) == ch) {
        lexer_advance(x);
        return true;
    }
    return false;
}

// Trim leading whitespace
static void
lexer_skip_whitespace(Lexer *x)
{
    while (!lexer_is_eof(x) && is_space(lexer_peek(x))) {
        lexer_advance(x);
    }
}

static String
string_slice(String s, size_t start, size_t stop)
{
    String r;
    r.data = &s.data[start];
    r.len  = stop - start;
    return r;
}

static Token
lexer_make_token(Lexer *x, Token_Type t)
{
    Token k;
    k.type   = t;
    k.lexeme = string_slice(x->input, x->start, x->cursor);
    return k;
}

static Token
lexer_lex(Lexer *x)
{
    lexer_skip_whitespace(x);
    x->start = x->cursor;
    if (lexer_is_eof(x)) {
        return lexer_make_token(x, TOKEN_EOF);
    }

    char ch = lexer_peek(x);
    lexer_advance(x);
    if (is_alnum(ch)) {
        ch = lexer_peek(x);
        while (is_alnum(ch) || ch == ',' || ch == '_') {
            lexer_advance(x);
            ch = lexer_peek(x);
        }
        return lexer_make_token(x, TOKEN_NUMBER);
    }

    Token_Type t = TOKEN_UNKNOWN;
    switch (ch) {
    case '+': t = TOKEN_PLUS;    break;
    case '-': t = TOKEN_MINUS;   break;
    case '*': t = TOKEN_STAR;    break;
    case '/': t = TOKEN_SLASH;   break;
    case '%': t = TOKEN_PERCENT; break;

    case '=':
        if (lexer_match(x, '='))
            t = TOKEN_EQUALS;
        break;
    case '<': t = lexer_match(x, '=') ? TOKEN_LESS_EQUAL    : TOKEN_GREATER_EQUAL; break;
    case '>': t = lexer_match(x, '=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER_THAN;  break;
    case '!':
        if (lexer_match(x, '='))
            t = TOKEN_NOT_EQUAL;
        break;
    }

    return lexer_make_token(x, t);
}
