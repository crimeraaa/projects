#ifndef BIGINT_LEXER_H
#define BIGINT_LEXER_H

#include <stddef.h>
#include <stdbool.h>

#include "../utils/strings.h"

typedef enum {
    TOKEN_UNKNOWN,

    // Balanced pairs
    TOKEN_PAREN_OPEN, TOKEN_PAREN_CLOSE, // ( )

    // Arithmetic
    TOKEN_PLUS, TOKEN_MINUS,                // + -
    TOKEN_STAR, TOKEN_SLASH, TOKEN_PERCENT, // * / %

    // Comparison
    TOKEN_EQUALS,       TOKEN_NOT_EQUAL,        // == !=
    TOKEN_LESS_THAN,    TOKEN_LESS_EQUAL,       // <  <=
    TOKEN_GREATER_THAN, TOKEN_GREATER_EQUAL,    // >  >=

    // Literals
    TOKEN_NUMBER,
    TOKEN_EOF,
} Token_Type;

#define TOKEN_COUNT     (TOKEN_EOF + 1)

extern const String
token_lstrings[TOKEN_COUNT];

typedef struct {
    Token_Type type;
    String     lexeme;
} Token;

typedef struct {
    String input;
    size_t start;
    size_t cursor;
} Lexer;

void
lexer_init(Lexer *x, String input);

Token
lexer_lex(Lexer *x);

#endif /* BIGINT_LEXER_H */
