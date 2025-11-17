#ifndef LSTRING_H
#define LSTRING_H

#include "../common.h"

typedef struct {
    const char *data;
    size_t      len;
} String;

#define string_expand(s)    (int)(s).len, (s).data
#define string_literal(s)   {(s), sizeof(s) - 1}

int
is_digit(char ch);

int
is_upper(char ch);

int
is_lower(char ch);

int
is_alnum(char ch);

int
is_space(char ch);

#ifdef LSTRING_IMPLEMENTATION

int
is_digit(char ch)
{
    return '0' <= ch && ch <= '9';
}

int
is_upper(char ch)
{
    return 'A' <= ch && ch <= 'Z';
}

int
is_lower(char ch)
{
    return 'a' <= ch && ch <= 'z';
}

int
is_alnum(char ch)
{
    return is_digit(ch) || is_upper(ch) || is_lower(ch);
}

int
is_space(char ch)
{
    switch (ch) {
    case ' ':
    case '\t':
    case '\n':
    case '\v':
    case '\r':
        return 1;
    }
    return 0;
}

#endif /* LSTRING_IMPLEMENTATION */

#endif /* LSTRING_H */
