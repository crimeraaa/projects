#include <stdarg.h> // va_list
#include <stdio.h>  // fgetc

#include "allocator.c"
#include "arena.c"

typedef struct {
    char  *data;
    size_t len;
    size_t cap;
    Allocator allocator;
} String_Builder;

static String_Builder
string_builder_make(Allocator a)
{
    String_Builder b;
    b.data = NULL;
    b.len  = 0;
    b.cap  = 0;
    b.allocator = a;
    return b;
}

static void
string_builder_destroy(String_Builder *b)
{
    slice_delete(b->data, b->cap, b->allocator);
}

static bool
string_append(String_Builder *b, char c)
{
    // Ensure append is within bounds.
    if (b->len + 1 > b->cap) {
        size_t new_cap = (b->cap < 8) ? 8 : (b->cap * 2);
        char *tmp = slice_resize(char, b->data, b->cap, new_cap, b->allocator);
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

static char
string_pop(String_Builder *b)
{
    if (b->len == 0) {
        return '\0';
    }

    char c = b->data[b->len - 1];
    b->len -= 1;
    return c;
}

static const char *
string_to_string(String_Builder *b, size_t *n)
{
    if (n != NULL) {
        *n = b->len;
    }
    return b->data;
}

static const char *
file_read_string(FILE *f, size_t *n, Allocator a)
{
    String_Builder b = string_builder_make(a);
    for (;;) {
        int ch = fgetc(f);
        if (ch == EOF) {
            goto cleanup;
        } else if (ch == '\r' || ch == '\n') {
            break;
        }

        if (!string_append(&b, cast(char)ch)) {
            goto cleanup;
        }
    }

    // Ensure nul-termination.
    if (!string_append(&b, '\0')) {
cleanup:
        string_builder_destroy(&b);
        return NULL;
    }
    string_pop(&b);
    return string_to_string(&b, n);
}

int
main(void)
{
    static unsigned char buf[BUFSIZ];
    Arena a;
    arena_init(&a, buf, sizeof(buf));

    for (;;) {
        printf(">");
        size_t n = 0;
        const char *s = file_read_string(stdin, &n, arena_allocator(&a));
        if (s == NULL) {
            printf("\n");
            break;
        }
        printf("%s\n", s);
        printf("(%zu / %zu bytes)\n", a.curr_offset, a.buf_len);
        arena_free_all(&a);
    }
    return 0;
}
