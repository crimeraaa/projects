#include "lexer.h"

void
lexer_init(Lexer *x, String input)
{
    x->input  = input;
    x->start  = 0;
    x->cursor = 0;
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

Token
lexer_lex(Lexer *x)
{
    lexer_skip_whitespace(x);
    x->start = x->cursor;
    if (lexer_is_eof(x)) {
        return lexer_make_token(x, TOKEN_EOF);
    }

    char ch = lexer_peek(x);
    lexer_advance(x);
    if (is_digit(ch)) {
        ch = lexer_peek(x);
        while (is_alnum(ch) || ch == ',' || ch == '_') {
            lexer_advance(x);
            ch = lexer_peek(x);
        }
        return lexer_make_token(x, TOKEN_NUMBER);
    }

    Token_Type t = TOKEN_UNKNOWN;
    switch (ch) {
    case '(': t = TOKEN_PAREN_OPEN;  break;
    case ')': t = TOKEN_PAREN_CLOSE; break;
    case '+': t = TOKEN_PLUS;        break;
    case '-': t = TOKEN_MINUS;       break;
    case '*': t = TOKEN_STAR;        break;
    case '/': t = TOKEN_SLASH;       break;
    case '%': t = TOKEN_PERCENT;     break;

    case '=':
        if (lexer_match(x, '=')) {
            t = TOKEN_EQUALS;
        }
        break;
    case '<': t = lexer_match(x, '=') ? TOKEN_LESS_EQUAL    : TOKEN_GREATER_EQUAL; break;
    case '>': t = lexer_match(x, '=') ? TOKEN_GREATER_EQUAL : TOKEN_GREATER_THAN;  break;
    case '!':
        if (lexer_match(x, '=')) {
            t = TOKEN_NOT_EQUAL;
        }
        break;
    }

    return lexer_make_token(x, t);
}

#define s_ string_literal

const String
token_lstrings[TOKEN_COUNT] = {
    /* TOKEN_UNKNOWN */         s_("<unknown>"),
    /* TOKEN_PAREN_OPEN */      s_("("),
    /* TOKEN_PAREN_CLOSE */     s_(")"),
    /* TOKEN_PLUS */            s_("+"),
    /* TOKEN_MINUS */           s_("-"),
    /* TOKEN_STAR */            s_("*"),
    /* TOKEN_SLASH */           s_("/"),
    /* TOKEN_PERCENT */         s_("%"),
    /* TOKEN_EQUALS */          s_("=="),
    /* TOKEN_NOT_EQUAL */       s_("!="),
    /* TOKEN_LESS_THAN */       s_("<"),
    /* TOKEN_LESS_EQUAL */      s_("<="),
    /* TOKEN_GREATER_THAN */    s_(">"),
    /* TOKEN_GREATER_EQUAL */   s_(">="),
    /* TOKEN_NUMBER */          s_("<number>"),
    /* TOKEN_EOF */             s_("<eof>"),
};

#undef s_
