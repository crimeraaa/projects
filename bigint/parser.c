#include <stdio.h>  // fprintf, sprintf

#include "parser.h"

void
parser_init(Parser *p, String input)
{
    lexer_init(&p->lexer, input);
    p->intermediates = NULL;
    p->error_code    = PARSER_OK;
}

static void
parser_parse_expression(Parser *p, BigInt *out);

static void
parser_parse_precedence(Parser *p, Precedence prec, BigInt *out);

static void
parser_throw(Parser *p, Parser_Error err)
{
    // Clean up intermediate values, if any
    BigInt_List *it = p->intermediates;
    while (it != NULL) {
        bigint_destroy(&it->value);
        it = it->prev;
    }
    p->intermediates = NULL;
    p->error_code    = err;
    longjmp(p->error_handler, 1);
}

static void
parser_syntax_error_at(Parser *p, const char *info, const Token *where)
{
    String s = where->lexeme;
    if (s.len == 0) {
        s = token_lstrings[where->type];
    }
    eprintfln("%s at '%.*s'.", info, string_expand(s));
    parser_throw(p, PARSER_ERROR_SYNTAX);
}

// Generally we want to throw errors for the lookahead token.
static void
parser_syntax_error(Parser *p, const char *info)
{
    parser_syntax_error_at(p, info, &p->lookahead);
}

static void
parser_syntax_error_consumed(Parser *p, const char *info)
{
    parser_syntax_error_at(p, info, &p->consumed);
}

static void
parser_advance(Parser *p)
{
    Token t = lexer_lex(&p->lexer);
    p->consumed  = p->lookahead;
    p->lookahead = t;
    if (t.type == TOKEN_UNKNOWN) {
        parser_syntax_error(p, "Unexpected token");
    }
}

static bool
parser_match(Parser *p, Token_Type t)
{
    return p->lookahead.type == t;
}

static void
parser_expect(Parser *p, Token_Type t)
{
    if (!parser_match(p, t)) {
        char tmp[64];
        sprintf(tmp, "Expected '%s'", token_lstrings[t].data);
        parser_syntax_error(p, tmp);
    }
    // Store the lookahead in the consumed since we now know it's valid.
    parser_advance(p);
}

Parser_Error
parser_parse(Parser *p, BigInt *ans)
{
    if (setjmp(p->error_handler) == 0) {
        // Store first token to be consumed in the lookahead
        parser_advance(p);
        parser_parse_expression(p, ans);
        parser_expect(p, TOKEN_EOF);
    }
    return p->error_code;
}

static void
parser_parse_expression(Parser *p, BigInt *out)
{
    parser_parse_precedence(p, PREC_NONE, out);
}

typedef enum {
    BIN_NONE,

    // Arith
    BIN_ADD, BIN_SUB, BIN_MUL, BIN_DIV, BIN_MOD,

    // Comparison
    BIN_EQ, BIN_NEQ, BIN_LT, BIN_LEQ, BIN_GT, BIN_GEQ,
} Binary_Op;

static Precedence
parser_binary_rule(Token_Type t, Binary_Op *op)
{
    switch (t) {
    // Arithmetic
    case TOKEN_PLUS:    *op = BIN_ADD; return PREC_TERMINAL;
    case TOKEN_MINUS:   *op = BIN_SUB; return PREC_TERMINAL;
    case TOKEN_STAR:    *op = BIN_MUL; return PREC_FACTOR;
    case TOKEN_SLASH:   *op = BIN_DIV; return PREC_FACTOR;
    case TOKEN_PERCENT: *op = BIN_MOD; return PREC_FACTOR;

    // Comparison
    case TOKEN_EQUALS:          *op = BIN_EQ;   break;
    case TOKEN_NOT_EQUAL:       *op = BIN_NEQ;  break;
    case TOKEN_LESS_THAN:       *op = BIN_LT;   break;
    case TOKEN_LESS_EQUAL:      *op = BIN_LEQ;  break;
    case TOKEN_GREATER_THAN:    *op = BIN_GT;   break;
    case TOKEN_GREATER_EQUAL:   *op = BIN_GEQ;  break;
    default:
        break;
    }
    return PREC_NONE;
}

static void
parser_parse_precedence(Parser *p, Precedence prec, BigInt *left)
{
    parser_advance(p);

    // Check the consumed prefix operand/operator
    switch (p->consumed.type) {
    // Essentially a no-op
    case TOKEN_PLUS:
        parser_parse_precedence(p, PREC_UNARY, left);
        break;

    // Unary negation
    case TOKEN_MINUS:
        parser_parse_precedence(p, PREC_UNARY, left);
        bigint_neg(left, left);
        break;

    // Number literals
    case TOKEN_NUMBER: {
        String s = p->consumed.lexeme;
        bigint_set_base_lstring(left, s.data, s.len, /*base=*/0);
        break;
    }

    // Groupings
    case TOKEN_PAREN_OPEN:
        parser_parse_expression(p, left);
        parser_expect(p, TOKEN_PAREN_CLOSE);
        break;

    default:
        parser_syntax_error_consumed(p, "Expected an expression");
        break;
    }

    BigInt_List tmp;
    // Push new intermediate
    tmp.prev          = p->intermediates;
    p->intermediates  = &tmp;

    BigInt *right = &tmp.value;
    bigint_init(right, left->allocator);

    for (;;) {
        Binary_Op  op     = BIN_NONE;
        Precedence op_prec = parser_binary_rule(p->lookahead.type, &op);
        if (prec > op_prec || op == BIN_NONE) {
            break;
        }
        parser_advance(p);

        bigint_clear(right);
        switch (op) {
        case BIN_ADD:
            parser_parse_precedence(p, op_prec + 1, right);
            if (bigint_add(left, left, right) != BIGINT_OK) {
                parser_throw(p, PARSER_ERROR_MEMORY);
            }
            break;
        case BIN_SUB:
            parser_parse_precedence(p, op_prec + 1, right);
            if (bigint_sub(left, left, right) != BIGINT_OK) {
                parser_throw(p, PARSER_ERROR_MEMORY);
            }
            break;
        default:
            parser_syntax_error_consumed(p, "Unsupported binary operation");
            break;
        }
    }

    bigint_destroy(right);

    // Pop the intermediate
    p->intermediates = tmp.prev;
}
