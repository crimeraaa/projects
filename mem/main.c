#include <stdarg.h> // va_list
#include <stdio.h>  // fgetc

#include "allocator.c"
#include "arena.c"
#include "stack.c"

#include "../utils/strings.c"

static Allocator
temp_allocator;

static const char *
file_read_string(FILE *f, String_Builder *sb, size_t *n)
{
    for (;;) {
        int ch = fgetc(f);
        if (ch == EOF) {
            goto cleanup;
        } else if (ch == '\r' || ch == '\n') {
            break;
        }

        if (!string_write_char(sb, cast(char)ch)) {
            goto cleanup;
        }
    }

    // Ensure nul-termination.
    if (!string_write_char(sb, '\0')) {
cleanup:
        return NULL;
    }
    string_pop_char(sb);
    return string_to_string(sb, n);
}

int
main(void)
{
    static unsigned char buf[BUFSIZ];
    // Arena a;
    // arena_init(&a, buf, sizeof(buf));
    // temp_allocator = arena_allocator(&a);

    Stack s;
    stack_init(&s, buf, sizeof(buf));
    temp_allocator = stack_allocator(&s);

    for (;;) {
        String_Builder sb;
        String line;

        printf(">");
        string_builder_init(&sb, temp_allocator);
        line.data = file_read_string(stdin, &sb, &line.len);
        if (line.data == NULL) {
            printf("\n");
            string_builder_destroy(&sb);
            break;
        }
        printfln("'%s'", line.data);

        String_Slice list = string_split(line, temp_allocator);
        for (size_t i = 0; i < list.len; i += 1) {
            printfln("=> '%.*s'", string_expand(list.data[i]));
        }

        String lol = string_concat(list, temp_allocator);
        printfln("'%.*s'", string_expand(lol));

        printfln("Before (%zu / %zu bytes)", s.curr_offset, s.buf_len);
        array_delete(cast(char *)lol.data, lol.len, temp_allocator);
        array_delete(list.data, list.len, temp_allocator);
        string_builder_destroy(&sb);
        printfln("After (%zu / %zu bytes)", s.curr_offset, s.buf_len);
    }
    return 0;
}
