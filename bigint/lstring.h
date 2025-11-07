#ifndef LSTRING_H
#define LSTRING_H

#include <stddef.h>

typedef struct {
    const char *data;
    size_t      len;
} String;

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

inline int
is_digit(char ch)
{
    return '0' <= ch && ch <= '9';
}

inline int
is_upper(char ch)
{
    return 'A' <= ch && ch <= 'Z';
}

inline int
is_lower(char ch)
{
    return 'a' <= ch && ch <= 'z';
}

inline int
is_alnum(char ch)
{
    return is_digit(ch) || is_upper(ch) || is_lower(ch);
}

inline int
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
