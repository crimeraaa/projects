#include <string.h>

#include "strings.h"

bool
char_is_alpha(char ch)
{
    return char_is_upper(ch) || char_is_lower(ch);
}

bool
char_is_alnum(char ch)
{
    return char_is_alpha(ch) || char_is_digit(ch);
}

bool
char_is_digit(char ch)
{
    return '0' <= ch && ch <= '9';
}

bool
char_is_upper(char ch)
{
    return 'A' <= ch && ch <= 'Z';
}

bool
char_is_lower(char ch)
{
    return 'a' <= ch && ch <= 'z';
}

bool
char_is_space(char ch)
{
    switch (ch) {
    case ' ':
    case '\t':
    case '\n':
    case '\v':
    case '\r':
        return true;
    }
    return false;
}


String
string_sub(String s, size_t start, size_t stop)
{
    assert(start <= stop);
    String t = {s.data + start, stop - start};
    return t;
}

String_Slice
string_split(String s, Allocator allocator)
{
    String_Dynamic d;
    string_dynamic_init(&d, allocator);

    size_t start = 0;
    for (size_t stop = 0; stop < s.len; stop += 1) {
        char c = s.data[stop];
        // Split on whitespaces.
        if (char_is_space(c)) {
            // Mark substring (if any) before splitting the whitespace.
            if (start != stop) {
                if (!string_dynamic_append(&d, string_sub(s, start, stop))) {
                    goto fail;
                }
            }
            start = stop + 1;
        }
    }

    // Have a final substring?
    if (start < s.len) {
        if (!string_dynamic_append(&d, string_sub(s, start, s.len))) {
fail:
            string_dynamic_delete(&d);
            return (String_Slice){NULL, 0};
        }
    }

    // Resize list to be exact
    string_dynamic_resize(&d, d.len);
    return (String_Slice){d.data, d.len};
}

String
string_concat(String_Slice list, Allocator allocator)
{
    String_Builder sb;
    string_builder_init(&sb, allocator);

    for (size_t i = 0; i < list.len; i += 1) {
        String s = list.data[i];
        string_write_string(&sb, s.data, s.len);
    }

    // Must be nul-terminated.
    String res;
    res.data = string_to_cstring(&sb, &res.len);
    return res;
}

// STRING DYNAMIC ========================================================== {{{

void
string_dynamic_init(String_Dynamic *d, Allocator allocator)
{
    d->data = NULL;
    d->len  = 0;
    d->cap  = 0;
    d->allocator = allocator;
}

bool
string_dynamic_resize(String_Dynamic *d, size_t n)
{
    String *tmp = array_resize(String, d->data, d->cap, n, d->allocator);
    if (tmp == NULL) {
        return false;
    }
    d->data = tmp;
    d->cap  = n;
    return true;
}

bool
string_dynamic_append(String_Dynamic *d, String s)
{
    if (d->len + 1 > d->cap) {
        size_t new_cap = (d->cap < 8) ? 8 : (d->cap * 2);
        if (!string_dynamic_resize(d, new_cap)) {
            return false;
        }
    }
    d->data[d->len] = s;
    d->len += 1;
    return true;
}

void
string_dynamic_delete(String_Dynamic *d)
{
    array_delete(d->data, d->cap, d->allocator);
}

// === }}} =====================================================================

// STRING BUILDER ========================================================== {{{

void
string_builder_init(String_Builder *sb, Allocator allocator)
{
    sb->data = NULL;
    sb->len  = 0;
    sb->cap  = 0;
    sb->allocator = allocator;
}

void
string_builder_destroy(String_Builder *sb)
{
    array_delete(sb->data, sb->cap, sb->allocator);
}

bool
string_write_string(String_Builder *sb, const char *data, size_t len)
{
    size_t new_len = sb->len + len;

    // Ensure append is within bounds.
    if (new_len > sb->cap) {
        size_t new_cap = sb->cap * 2;
        if (new_cap < 8) {
            new_cap = 8;
        } else if (new_len > new_cap) {
            new_cap = new_len;
        }

        char *tmp = array_resize(char, sb->data, sb->cap, new_cap, sb->allocator);
        if (tmp == NULL) {
            return false;
        }
        sb->data = tmp;
        sb->cap  = new_cap;
    }
    memcpy(&sb->data[sb->len], data, len);
    sb->len = new_len;
    return true;
}

bool
string_write_char(String_Builder *sb, char c)
{
    // Ensure append is within bounds.
    if (sb->len + 1 > sb->cap) {
        size_t new_cap = (sb->cap < 8) ? 8 : (sb->cap * 2);
        char *tmp = array_resize(char, sb->data, sb->cap, new_cap, sb->allocator);
        if (tmp == NULL) {
            return false;
        }
        sb->data = tmp;
        sb->cap  = new_cap;
    }

    sb->data[sb->len] = c;
    sb->len += 1;
    return true;
}

char
string_pop_char(String_Builder *sb)
{
    if (sb->len == 0) {
        return '\0';
    }

    char c = sb->data[sb->len - 1];
    sb->len -= 1;
    return c;
}

String
string_to_string(const String_Builder *sb)
{
    String s = {sb->data, sb->len};
    return s;
}

const char *
string_to_cstring(String_Builder *sb, size_t *n)
{
    // Retain NULL on failure to nul-terminate.
    char *res = NULL;
    if (string_write_char(sb, '\0')) {
        string_pop_char(sb);
        res = sb->data;
    }

    if (n != NULL) {
        *n = sb->len;
    }
    return res;
}

// === }}} =====================================================================
