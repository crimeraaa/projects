#ifndef BIGINT_LEXER_H
#define BIGINT_LEXER_H

#include <stddef.h>
#include <stdbool.h>

#include <utils/strings.h>

enum Token_Type {
    TOKEN_UNKNOWN,

    // Keywords
    TOKEN_AND, TOKEN_FALSE, TOKEN_OR, TOKEN_TRUE,

    // Balanced pairs
    TOKEN_PAREN_OPEN, TOKEN_PAREN_CLOSE, // ( )

    // Bitwise
    TOKEN_AMPERSAND,    // &
    TOKEN_PIPE,         // |
    TOKEN_CARET,        // ^
    TOKEN_SHIFT_LEFT,   // <<
    TOKEN_SHIFT_RIGHT,  // >>
    TOKEN_TILDE,        // ~

    // Arithmetic
    TOKEN_PLUS, TOKEN_MINUS,                // + -
    TOKEN_STAR, TOKEN_SLASH, TOKEN_PERCENT, // * / %

    // Comparison
    TOKEN_EQUALS,       TOKEN_NOT_EQUAL,        // == !=
    TOKEN_LESS_THAN,    TOKEN_LESS_EQUAL,       // <  <=
    TOKEN_GREATER_THAN, TOKEN_GREATER_EQUAL,    // >  >=

    // Literals
    TOKEN_NUMBER,       // <number>     ::= [0-9][0-9,_ ]*
    TOKEN_IDENTIFIER,   // <identifier> ::= [a-zA-Z_][0-9a-zA-Z_]*
    TOKEN_EOF,
};

#define TOKEN_COUNT     (TOKEN_EOF + 1)

extern const String
TOKEN_STRINGS[TOKEN_COUNT];

typedef enum Token_Type Token_Type;
typedef struct Token Token;
struct Token {
    Token_Type type;
    String lexeme;
};

typedef struct Lexer Lexer;
struct Lexer {
    String input;
    size_t start;
    size_t cursor;
};

void
lexer_init(Lexer *x, String input);

Token
lexer_lex(Lexer *x);

#endif /* BIGINT_LEXER_H */
