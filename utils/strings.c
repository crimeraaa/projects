#include "strings.h"

bool
is_digit(char ch)
{
    return '0' <= ch && ch <= '9';
}

bool
is_upper(char ch)
{
    return 'A' <= ch && ch <= 'Z';
}

bool
is_lower(char ch)
{
    return 'a' <= ch && ch <= 'z';
}

bool
is_alnum(char ch)
{
    return is_digit(ch) || is_upper(ch) || is_lower(ch);
}

bool
is_space(char ch)
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
string_split(String s, Allocator a)
{
    String_Dynamic d;
    string_dynamic_init(&d, a);

    size_t start = 0;
    for (size_t stop = 0; stop < s.len; stop += 1) {
        char c = s.data[stop];
        // Skip whitespace
        switch (c) {
        case ' ':
        case '\r':
        case '\n':
        case '\t':
            // Mark substring (if any) before skipping the whitespace.
            if (start != stop) {
                if (!string_dynamic_append(&d, string_sub(s, start, stop))) {
                    goto fail;
                }
            }
            start = stop + 1;
            break;
        default:
            break;
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

// STRING DYNAMIC ========================================================== {{{

void
string_dynamic_init(String_Dynamic *d, Allocator a)
{
    d->data = NULL;
    d->len  = 0;
    d->cap  = 0;
    d->allocator = a;
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
string_builder_init(String_Builder *b, Allocator a)
{
    b->data = NULL;
    b->len  = 0;
    b->cap  = 0;
    b->allocator = a;
}

void
string_builder_destroy(String_Builder *b)
{
    array_delete(b->data, b->cap, b->allocator);
}

bool
string_append_char(String_Builder *b, char c)
{
    // Ensure append is within bounds.
    if (b->len + 1 > b->cap) {
        size_t new_cap = (b->cap < 8) ? 8 : (b->cap * 2);
        char *tmp = array_resize(char, b->data, b->cap, new_cap, b->allocator);
        if (tmp == NULL) {
            return false;
        }
        b->data = tmp;
        b->cap  = new_cap;
    }

    b->data[b->len] = c;
    b->len += 1;
    return true;
}

char
string_pop_char(String_Builder *b)
{
    if (b->len == 0) {
        return '\0';
    }

    char c = b->data[b->len - 1];
    b->len -= 1;
    return c;
}

const char *
string_to_string(String_Builder *b, size_t *n)
{
    if (n != NULL) {
        *n = b->len;
    }
    return b->data;
}

// === }}} =====================================================================
