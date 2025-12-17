#ifndef BIGINT_PARSER_H
#define BIGINT_PARSER_H

#include <setjmp.h>     // jmp_buf, setjmp, longjmp

// #include "bigint.h"
#include "i128.h"
#include "lexer.h"
#include "value.h"

typedef struct Value_List Value_List;
struct Value_List {
    Value_List *prev;
    Value       value;
};

typedef enum {
    PARSER_OK,
    PARSER_ERROR_SYNTAX,
    PARSER_ERROR_TYPE,
    PARSER_ERROR_MEMORY,
} Parser_Error;

typedef struct {
    Lexer                 lexer;
    Value_List           *intermediates;
    Token                 consumed;
    Token                 lookahead;
    jmp_buf               error_handler;
    volatile Parser_Error error_code;
    Allocator             allocator; // Used to allocate for temporaries.
} Parser;

typedef enum {
    PREC_NONE,
    PREC_NUMBER,
    PREC_OR,         // or
    PREC_AND,        // and
    PREC_EQUALITY,   // == !=
    PREC_COMPARISON, // < <= >= >
    PREC_TERMINAL,   // + -
    PREC_FACTOR,     // * / %
    PREC_UNARY,      // + -
} Precedence;

void
parser_init(Parser *p, String input, Allocator allocator);

Parser_Error
parser_parse(Parser *p, Value *dst);

#endif /* BIGINT_PARSER_H */
