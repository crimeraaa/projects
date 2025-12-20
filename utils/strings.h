#ifndef UTILS_STRINGS_H
#define UTILS_STRINGS_H

#include <projects.h>
#include <mem/allocator.h>

typedef struct String String;
struct String {
    const char *data;
    size_t len;
};

#define string_expand(s)    (int)(s).len, (s).data
#define string_literal(s)   {(s), sizeof(s) - 1}
#define string_delete(string, allocator)                                       \
    array_delete((char *)(string).data,                                        \
        /*len=*/         (string).len,                                         \
        /*allocator=*/   allocator)

#define STRING_FMTSPEC      "%.*s"
#define STRING_QFMTSPEC     "'" STRING_FMTSPEC "'"

typedef struct String_Slice String_Slice;
struct String_Slice {
    String *data;
    size_t  len;
};

typedef struct String_Dynamic String_Dynamic;
struct String_Dynamic {
    String   *data;
    size_t    len;
    size_t    cap;
    Allocator allocator;
};

typedef struct String_Builder String_Builder;
struct String_Builder {
    char     *data;
    size_t    len;
    size_t    cap;
    Allocator allocator;
};

bool
char_is_alpha(char ch);

bool
char_is_alnum(char ch);

bool
char_is_digit(char ch);

bool
char_is_upper(char ch);

bool
char_is_lower(char ch);

bool
char_is_space(char ch);

String
string_sub(String s, size_t start, size_t stop);

String_Slice
string_split(String s, Allocator allocator);

String
string_concat(String_Slice list, Allocator allocator);

void
string_dynamic_init(String_Dynamic *d, Allocator allocator);

bool
string_dynamic_resize(String_Dynamic *d, size_t n);

bool
string_dynamic_append(String_Dynamic *d, String s);

void
string_dynamic_delete(String_Dynamic *d);

void
string_builder_init(String_Builder *sb, Allocator allocator);

void
string_builder_destroy(String_Builder *sb);

#define string_write_literal(sb, s) string_write_string(sb, s, sizeof(s) - 1)

bool
string_write_string(String_Builder *sb, const char *data, size_t len);

bool
string_write_char(String_Builder *sb, char c);

char
string_pop_char(String_Builder *sb);

String
string_to_string(const String_Builder *sb);

const char *
string_to_cstring(String_Builder *sb, size_t *n);

#endif /* UTILS_STRINGS_H */
