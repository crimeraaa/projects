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
    while (!lexer_is_eof(x) && char_is_space(lexer_peek(x))) {
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
lexer_make_number(Lexer *x)
{
    char ch = lexer_peek(x);
    while (char_is_alnum(ch) || ch == ',' || ch == '_') {
        lexer_advance(x);
        ch = lexer_peek(x);
    }
    return lexer_make_token(x, TOKEN_NUMBER);
}

static Token_Type
lexer_check_keyword_or_identifier(String s)
{
    switch (s.len) {
    case 2:
        if (s.data[0] == 'o' && s.data[1] == 'r') {
            return TOKEN_OR;
        }
    case 3:
        if (s.data[0] == 'a' && s.data[1] == 'n' && s.data[2] == 'd') {
            return TOKEN_AND;
        }
    default:
        break;
    }
    return TOKEN_IDENTIFIER;
}

static Token
lexer_make_keyword_or_identifier(Lexer *x)
{
    String s;
    Token t;
    char ch;

    ch = lexer_peek(x);
    while (char_is_alnum(ch) || ch == '_') {
        lexer_advance(x);
        ch = lexer_peek(x);
    }
    s        = string_slice(x->input, /*start=*/x->start, /*stop=*/x->cursor);
    t.type   = lexer_check_keyword_or_identifier(s);
    t.lexeme = s;
    return t;
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
    if (char_is_digit(ch)) {
        return lexer_make_number(x);
    } else if (char_is_alpha(ch) || ch == '_') {
        return lexer_make_keyword_or_identifier(x);
    }

    Token_Type t = TOKEN_UNKNOWN;
    switch (ch) {
    case '(': t = TOKEN_PAREN_OPEN;  break;
    case ')': t = TOKEN_PAREN_CLOSE; break;
    case '&': t = TOKEN_AMPERSAND;   break;
    case '|': t = TOKEN_PIPE;        break;
    case '^': t = TOKEN_CARET;       break;
    case '~': t = TOKEN_TILDE;       break;
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
    case '<':
        if (lexer_match(x, '=')) {
            t = TOKEN_LESS_EQUAL;
        } else if (lexer_match(x, '<')) {
            t = TOKEN_SHIFT_LEFT;
        } else {
            t = TOKEN_LESS_THAN;
        }
        break;
    case '>':
        if (lexer_match(x, '=')) {
            t = TOKEN_GREATER_EQUAL;
        } else if (lexer_match(x, '>')) {
            t = TOKEN_SHIFT_RIGHT;
        } else {
            t = TOKEN_GREATER_THAN;
        }
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
TOKEN_STRINGS[TOKEN_COUNT] = {
    /* TOKEN_UNKNOWN */         s_("<unknown>"),
    /* TOKEN_AND */             s_("and"),
    /* TOKEN_OR */              s_("or"),
    /* TOKEN_PAREN_OPEN */      s_("("),
    /* TOKEN_PAREN_CLOSE */     s_(")"),
    /* TOKEN_AMPERSAND */       s_("&"),
    /* TOKEN_PIPE */            s_("|"),
    /* TOKEN_CARET */           s_("^"),
    /* TOKEN_SHIFT_LEFT */      s_("<<"),
    /* TOKEN_SHIFT_RIGHT */     s_(">>"),
    /* TOKEN_TILDE */           s_("~"),
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
    /* TOKEN_IDENTIFIER */      s_("<identifier>"),
    /* TOKEN_EOF */             s_("<eof>"),
};

#undef s_
