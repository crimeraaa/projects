#ifndef BIGINT_PARSER_H
#define BIGINT_PARSER_H

#include <setjmp.h>     // jmp_buf, setjmp, longjmp

#include "bigint.h"
#include "lexer.h"

typedef struct BigInt_List BigInt_List;
struct BigInt_List {
    BigInt_List *prev;
    BigInt       value;
};

typedef enum {
    PARSER_OK,
    PARSER_ERROR_SYNTAX,
    PARSER_ERROR_MEMORY,
} Parser_Error;

typedef struct {
    Lexer        lexer;
    BigInt_List *intermediates;
    Token        consumed;
    Token        lookahead;
    jmp_buf      error_handler;
    Parser_Error error_code;
} Parser;

typedef enum {
    PREC_NONE,
    PREC_NUMBER,
    PREC_TERMINAL, // + -
    PREC_FACTOR,   // * / %
    PREC_UNARY,    // + -
} Precedence;

void
parser_init(Parser *p, String input);

Parser_Error
parser_parse(Parser *p, BigInt *ans);

#endif /* BIGINT_PARSER_H */
