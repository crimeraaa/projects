#ifndef BIGINT_REPL_H
#define BIGINT_REPL_H

#include <stddef.h>

#include "lstring.h"

typedef enum {
    TOKEN_NUMBER,
    TOKEN_PLUS,
    TOKEN_MINUS,
    TOKEN_STAR,
    TOKEN_SLASH,
    TOKEN_PERCENT,
    TOKEN_EQUALS,
    TOKEN_LESS_THAN,
    TOKEN_LESS_EQUAL,
    TOKEN_NOT_EQUAL,
    TOKEN_GREATER_THAN,
    TOKEN_GREATER_EQUAL,
    TOKEN_UNKNOWN,
    TOKEN_EOF,
} Token_Type;

typedef struct {
    Token_Type type;
    String     lexeme;
} Token;

typedef struct {
    String input;
    size_t start;
    size_t cursor;
} Lexer;

typedef struct {
    Lexer lexer;
    Token consumed;
    Token lookahead;
} Parser;

#endif /* BIGINT_REPL_H */
