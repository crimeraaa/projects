#ifndef UTILS_STRINGS_H
#define UTILS_STRINGS_H

#include "../common.h"
#include "../mem/allocator.h"

typedef struct {
    const char *data;
    size_t len;
} String;

#define string_expand(s)    (int)(s).len, (s).data
#define string_literal(s)   {(s), sizeof(s) - 1}

typedef struct {
    String *data;
    size_t len;
} String_Slice;

typedef struct {
    String *data;
    size_t len;
    size_t cap;
    Allocator allocator;
} String_Dynamic;

typedef struct {
    char *data;
    size_t len;
    size_t cap;
    Allocator allocator;
} String_Builder;

bool
is_digit(char ch);

bool
is_upper(char ch);

bool
is_lower(char ch);

bool
is_alnum(char ch);

bool
is_space(char ch);


String
string_sub(String s, size_t start, size_t stop);

String_Slice
string_split(String s, Allocator a);

void
string_dynamic_init(String_Dynamic *d, Allocator a);

bool
string_dynamic_resize(String_Dynamic *d, size_t n);

bool
string_dynamic_append(String_Dynamic *d, String s);

void
string_dynamic_delete(String_Dynamic *d);

void
string_builder_init(String_Builder *b, Allocator a);

void
string_builder_destroy(String_Builder *b);

bool
string_append_char(String_Builder *b, char c);

char
string_pop_char(String_Builder *b);

const char *
string_to_string(String_Builder *b, size_t *n);

#endif /* UTILS_STRINGS_H */
