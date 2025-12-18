#ifndef PROJECT_CHARS_H
#define PROJECT_CHARS_H

#include <projects.h>

#define BIT_FIELD(N)    (1 << (N))

enum Char_Trait {
    CHAR_LOWER  = BIT_FIELD(0),
    CHAR_UPPER  = BIT_FIELD(1),
    CHAR_DIGIT  = BIT_FIELD(2),
    CHAR_XDIGIT = BIT_FIELD(3),
    CHAR_CNTRL  = BIT_FIELD(4),
    CHAR_GRAPH  = BIT_FIELD(5),
    CHAR_SPACE  = BIT_FIELD(6),
    CHAR_BLANK  = BIT_FIELD(7),
    CHAR_PRINT  = BIT_FIELD(8),
    CHAR_PUNCT  = BIT_FIELD(9),

    // Utility bitsets
    CHAR_ALPHA  = CHAR_LOWER | CHAR_UPPER,
    CHAR_ALNUM  = CHAR_ALPHA | CHAR_DIGIT,
};

extern const u16
ASCII_CHAR_TRAITS[CHAR_MAX + 1];

#define ascii_has_trait(ch, trait)  (ASCII_CHAR_TRAITS[(int)ch] & (trait))

#define ascii_is_digit(ch)  ascii_has_trait(ch, CHAR_DIGIT)
#define ascii_is_upper(ch)  ascii_has_trait(ch, CHAR_UPPER)
#define ascii_is_lower(ch)  ascii_has_trait(ch, CHAR_LOWER)

#endif /* PROJECT_CHARS_H */
