#include <stdio.h>  // fprintf, sprintf

#include "parser.h"

typedef enum {
    BIN_NONE,

    // Arith
    BIN_ADD, BIN_SUB, BIN_MUL, BIN_DIV, BIN_MOD,

    // Comparison
    BIN_EQ, BIN_NEQ, BIN_LT, BIN_LEQ, BIN_GT, BIN_GEQ,

    // Logical
    BIN_AND, BIN_OR,
} Binary_Op;

typedef struct Parser_Rule Parser_Rule;

struct Parser_Rule {
    Precedence prec;
    Binary_Op  op;
    void (*binary_fn)(Parser *p, const Parser_Rule *rule, Value *left, Value *right);
};

static const Parser_Rule *
parser_get_rule(Token_Type t);

void
parser_init(Parser *p, String input, Allocator allocator)
{
    lexer_init(&p->lexer, input);
    p->intermediates = NULL;
    p->error_code    = PARSER_OK;
    p->allocator     = allocator;
}

static void
parser_parse_expression(Parser *p, Value *dst);

static void
parser_parse_precedence(Parser *p, Precedence prec, Value *dst);

static void
parser_throw(Parser *p, Parser_Error err)
{
    // Clean up intermediate values, if any
    while (p->intermediates != NULL) {
        Value v = p->intermediates->value;
        if (value_is_integer(v)) {
            bigint_destroy(v.integer);
        }
        p->intermediates = p->intermediates->prev;
    }
    p->error_code = err;
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
    Token t      = lexer_lex(&p->lexer);
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
parser_parse(Parser *p, Value *ans)
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
parser_parse_expression(Parser *p, Value *dst)
{
    parser_parse_precedence(p, PREC_NONE, dst);
}

static void
parser_check_integer_unary(Parser *p, const Value *a, String act)
{
    if (!value_is_integer(*a)) {
        printfln("Expected <integer> in '%.*s' (got '%s')",
            string_expand(act),
            a->boolean ? "true" : "false");
        parser_throw(p, PARSER_ERROR_TYPE);
    }
}

static void
parser_check_integer_binary(Parser *p, const Value *a, const Value *b, String act)
{
    parser_check_integer_unary(p, a, act);
    parser_check_integer_unary(p, b, act);
}

static void
parser_parse_unary(Parser *p, Value *left)
{
    Token t = p->consumed;
    // Check the consumed prefix operand/operator
    switch (t.type) {
    // Essentially a no-op, but we do care if it's an integer.
    case TOKEN_PLUS:
        parser_parse_precedence(p, PREC_UNARY, left);
        parser_check_integer_unary(p, left, t.lexeme);
        break;

    // Unary negation
    case TOKEN_MINUS:
        parser_parse_precedence(p, PREC_UNARY, left);
        parser_check_integer_unary(p, left, t.lexeme);
        bigint_neg(left->integer, left->integer);
        break;

    // Number literals
    case TOKEN_NUMBER: {
        String s = p->consumed.lexeme;
        // Default type is integer anyway
        parser_check_integer_unary(p, left, t.lexeme);
        bigint_set_base_lstring(left->integer, s.data, s.len, /*base=*/0);
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
}

static void
parser_arith(Parser *p, const Parser_Rule *rule, Value *left, Value *right)
{
    Token t = p->consumed;
    parser_parse_precedence(p, rule->prec + 1, right);
    parser_check_integer_binary(p, left, right, t.lexeme);

    BigInt *dst = left->integer;
    BigInt *a   = left->integer;
    BigInt *b   = right->integer;

    BigInt_Error err = BIGINT_OK;
    switch (rule->op) {
    case BIN_ADD: err = bigint_add(dst, a, b); break;
    case BIN_SUB: err = bigint_sub(dst, a, b); break;
    case BIN_MUL: err = bigint_mul(dst, a, b); break;
    default:
        parser_syntax_error_at(p, "Unsupported binary arithmetic operation", &t);
        break;
    }

    if (err) {
        parser_throw(p, PARSER_ERROR_MEMORY);
    }
}

static void
parser_compare(Parser *p, const Parser_Rule *rule, Value *left, Value *right)
{
    Token t = p->consumed;
    parser_parse_precedence(p, rule->prec + 1, right);
    parser_check_integer_binary(p, left, right, t.lexeme);

    BigInt *a = left->integer;
    BigInt *b = right->integer;

    bool res = false;
    switch (rule->op) {
    case BIN_EQ:    res = bigint_eq(a, b);  break;
    case BIN_NEQ:   res = bigint_neq(a, b); break;
    case BIN_LT:    res = bigint_lt(a, b);  break;
    case BIN_LEQ:   res = bigint_leq(a, b); break;
    case BIN_GT:    res = bigint_gt(a, b);  break;
    case BIN_GEQ:   res = bigint_geq(a, b); break;
    default:
        parser_syntax_error_at(p, "Unsupported binary comparison operation", &t);
        break;
    }

    left->type    = VALUE_BOOLEAN;
    left->boolean = res;
}

static void
parser_logical(Parser *p, const Parser_Rule *rule, Value *left, Value *right)
{
    Token t = p->consumed;
    parser_parse_precedence(p, rule->prec + 1, right);
    if (!value_is_boolean(*left) || !value_is_boolean(*right)) {
        printfln("Expected <boolean> at '%.*s', got '<integer>'",
            string_expand(t.lexeme));
        parser_throw(p, PARSER_ERROR_TYPE);
    }

    switch (rule->op) {
    case BIN_AND: left->boolean = left->boolean && right->boolean; break;
    case BIN_OR:  left->boolean = left->boolean || right->boolean; break;
    default:
        break;
    }
}

static void
parser_parse_precedence(Parser *p, Precedence prec, Value *left)
{
    parser_advance(p);
    parser_parse_unary(p, left);

    Value_List top;
    // Push new intermediate
    top.prev          = p->intermediates;
    p->intermediates  = &top;

    BigInt tmp;
    bigint_init(&tmp, p->allocator);

    Value *right = &top.value;
    right->type    = VALUE_INTEGER;
    right->integer = &tmp;

    for (;;) {
        const Parser_Rule *rule = parser_get_rule(p->lookahead.type);
        if (prec > rule->prec || rule->binary_fn == NULL) {
            break;
        }
        parser_advance(p);
        bigint_clear(&tmp);
        rule->binary_fn(p, rule, left, right);
    }

    bigint_destroy(&tmp);

    // Pop the intermediate
    p->intermediates = top.prev;
}

static const Parser_Rule
parser_rules[TOKEN_COUNT] = {
    // Token_Type                prec             op        binary_fn
    /* TOKEN_UNKNOWN */         {PREC_NONE,       BIN_NONE, NULL},
    /* TOKEN_AND */             {PREC_AND,        BIN_AND,  parser_logical},
    /* TOKEN_OR */              {PREC_OR,         BIN_OR,   parser_logical},
    /* TOKEN_PAREN_OPEN */      {PREC_NONE,       BIN_NONE, NULL},
    /* TOKEN_PAREN_CLOSE */     {PREC_NONE,       BIN_NONE, NULL},
    /* TOKEN_PLUS */            {PREC_TERMINAL,   BIN_ADD,  parser_arith},
    /* TOKEN_MINUS */           {PREC_TERMINAL,   BIN_SUB,  parser_arith},
    /* TOKEN_STAR */            {PREC_FACTOR,     BIN_MUL,  parser_arith},
    /* TOKEN_SLASH */           {PREC_FACTOR,     BIN_DIV,  parser_arith},
    /* TOKEN_PERCENT */         {PREC_FACTOR,     BIN_MOD,  parser_arith},
    /* TOKEN_EQUALS */          {PREC_EQUALITY,   BIN_EQ,   parser_compare},
    /* TOKEN_NOT_EQUAL */       {PREC_EQUALITY,   BIN_NEQ,  parser_compare},
    /* TOKEN_LESS_THAN */       {PREC_COMPARISON, BIN_LT,   parser_compare},
    /* TOKEN_LESS_EQUAL */      {PREC_COMPARISON, BIN_LEQ,  parser_compare},
    /* TOKEN_GREATER_THAN */    {PREC_COMPARISON, BIN_GT,   parser_compare},
    /* TOKEN_GREATER_EQUAL */   {PREC_COMPARISON, BIN_GEQ,  parser_compare},
    /* TOKEN_NUMBER */          {PREC_NONE,       BIN_NONE, NULL},
    /* TOKEN_EOF */             {PREC_NONE,       BIN_NONE, NULL},
};

static const Parser_Rule *
parser_get_rule(Token_Type t)
{
    return &parser_rules[t];
}
