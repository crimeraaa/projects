#include <stdio.h>  // printf
#include <stdlib.h> // abort

#include "bigint.h"
#include "string_builder.h"

static void
string_append(String_Builder *sb, char ch)
{
    if (sb->len + 1 < sb->cap) {
        sb->len += 1;
        sb->data[sb->len - 1] = ch;
        return;
    }
    fprintf(stderr, "[FATAL] Out of bounds index %zu / %zu", sb->len, sb->cap);
    abort();
}

static const char *
string_reverse(String_Builder *sb)
{
    size_t left  = 0;
    size_t right = sb->len;
    for (; left < right; left += 1, right -= 1) {
        char tmp = sb->data[left];
        sb->data[left] = sb->data[right - 1];
        sb->data[right - 1] = tmp;
    }
    return sb->data;
}

static char
string_pop(String_Builder *sb)
{
    char ch = sb->data[sb->len - 1];
    sb->len -= 1;
    return ch;
}

static void
digit_slice_print(Digit_Slice d, size_t offset)
{
    static int counter = 0;
    printf("%2i [%zu:] = {", counter++, offset);
    for (size_t i = 0; i < d.len; i += 1) {
        if (i > 0) {
            printf(", ");
        }
        printf("%10u", d.data[i]);
    }
    printf("}\n");
}

// Form a decimal string.
// https://stackoverflow.com/questions/71143129/is-there-a-way-to-convert-a-base-264-number-to-its-base10-value-in-string-form
// https://stackoverflow.com/a/71144161
static const char *
convert(Digit_Slice d, String_Builder *sb)
{
    // Skip leading zeroes
    size_t offset = 0;
    while (offset < d.len && d.data[offset] == 0) {
        offset += 1;
    }

    // Still have digits to process?
    while (offset < d.len) {
        unsigned char carry = 0;

        digit_slice_print(d, offset);

        // Repeately mod-10 the array the find the next least-significant-digit
        // (LSD) then divide by 10. Repeat as needed because base-2 to base-10
        // conversion is quite costly.
        for (size_t i = offset; i < d.len; i += 1) {
            Word sum = d.data[i] + (cast(Word)carry * BIGINT_DIGIT_BASE);

            d.data[i] = cast(Digit)(sum / 10u);
            carry = cast(unsigned char)(sum % 10u);

            // Continue: keep going as we propagate `rem`.
        }

        string_append(sb, cast(char)(carry + '0'));
        // Current MSD has been exhausted, so we know there is no more
        // "overflow" from it.
        if (d.data[offset] == 0) {
            offset += 1;
        }
    }
    string_append(sb, '\0');
    string_pop(sb);
    // Account for data being written LSD to MSD.
    return string_reverse(sb);
}

int
main(void)
{
    // Base 2^64: uint64_t big_num[] = {77748, 656713, 872};
    // Base 2^32: (below)
    // Base 10: 26_364_397_224_300_470_284_329_554_475_476_558_257_587_048
    Digit big_num[] = {0, 77478, 0, 656713, 0, 872};

    char buf[256];
    String_Builder sb = {buf, 0, sizeof(buf)};
    Digit_Slice digits = {big_num, count_of(big_num)};
    printf("<%s>\n", convert(digits, &sb));
    // Unreversed: <84078575285567457445592348207400342279346362>
    // Reversed     26364397224300470284329554475476558257587048
    return 0;
}
