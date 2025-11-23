#include <stdarg.h> // va_list
#include <stdio.h>  // fgetc

#include "allocator.c"
#include "arena.c"
#include "stack.c"

#include "../utils/strings.c"

static Allocator
temp_allocator;

static String
file_read_string(FILE *f, String_Builder *sb)
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
cleanup:
    return string_to_string(sb);
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
        printf(">");

        String_Builder sb;
        string_builder_init(&sb, temp_allocator);

        String line = file_read_string(stdin, &sb);
        if (line.data == NULL) {
            printf("\n");
            string_builder_destroy(&sb);
            break;
        }
        printfln(STRING_QFMTSPEC, string_expand(line));

        String_Slice list = string_split(line, temp_allocator);
        for (size_t i = 0; i < list.len; i += 1) {
            printfln("=> " STRING_QFMTSPEC, string_expand(list.data[i]));
        }

        String lol = string_concat(list, temp_allocator);
        printfln(STRING_FMTSPEC, string_expand(lol));

        printfln("Before (%zu / %zu bytes)", s.curr_offset, s.buf_len);
        array_delete(cast(char *)lol.data, lol.len, temp_allocator);
        array_delete(list.data, list.len, temp_allocator);
        string_builder_destroy(&sb);
        printfln("After (%zu / %zu bytes)", s.curr_offset, s.buf_len);
    }
    return 0;
}
