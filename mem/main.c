#include <stdarg.h> // va_list
#include <stdio.h>  // fgetc

#include "allocator.c"
#include "arena.c"

#include "../utils/strings.c"

static const char *
file_read_string(FILE *f, size_t *n, Allocator a)
{
    String_Builder b;
    string_builder_init(&b, a);
    for (;;) {
        int ch = fgetc(f);
        if (ch == EOF) {
            goto cleanup;
        } else if (ch == '\r' || ch == '\n') {
            break;
        }

        if (!string_append_char(&b, cast(char)ch)) {
            goto cleanup;
        }
    }

    // Ensure nul-termination.
    if (!string_append_char(&b, '\0')) {
cleanup:
        string_builder_destroy(&b);
        return NULL;
    }
    string_pop_char(&b);
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
        size_t line_len = 0;
        const char *line = file_read_string(stdin, &line_len, arena_allocator(&a));
        if (line == NULL) {
            printf("\n");
            break;
        }
        printf("'%s'\n", line);

        String_Slice list = string_split((String){line, line_len}, arena_allocator(&a));
        for (size_t i = 0; i < list.len; i += 1) {
            printf("=> '%.*s'\n", string_expand(list.data[i]));
        }
        printf("(Mem: %zu / %zu bytes)\n", a.curr_offset, a.buf_len);
        arena_free_all(&a);
    }
    return 0;
}
