#include "gol.c"

// Very unsafe function!
static bool
grid_copy_file(Grid *g, FILE *f)
{
    size_t line = 0, col = 0;
    int ch;
    ch = fgetc(f);
    while (ch != EOF) {
        if (ch == '\n') {
            line++;
            col = 0;
            if (line >= GRID_ROWS) {
                eprintfln("Grid overflow: file lines=%zu >= GRID_ROWS=%i",
                    line, GRID_ROWS);
                return false;
            }
        } else if (ch == CELL_CHAR_ALIVE) {
            if (col + 1 >= GRID_COLS) {
                goto invalid_cols;
            }
            grid_insert(g, line, col++, CELL_ALIVE);
        } else if (ch == CELL_CHAR_DEAD) {
            if (col + 1 >= GRID_COLS) {
invalid_cols:
                eprintfln("Grid overflow: file cols=%zu >= GRID_COLS=%i",
                    col + 1, GRID_COLS);
                return false;
            }
            grid_insert(g, line, col++, CELL_DEAD);
        }
        ch = fgetc(f);
    }
    return true;
}

static void
cell_stdio_writer(void *user_data, Cell_State state)
{
    FILE *f;

    f = cast(FILE *)user_data;
    switch (state) {
    case CELL_DEAD:
        fputc(CELL_CHAR_DEAD, f);
        break;
    case CELL_ALIVE:
        fputc(CELL_CHAR_ALIVE, f);
        break;
    case CELL_WRITE_NEWLINE:
        fputc('\n', f);
        break;
    default:
        unreachable();
        break;
    }
}

int
main(int argc, char *argv[])
{
    Grid g;

    grid_init(&g);
    if (argc > 1) {
        const char *s;
        FILE *f;
        bool ok;

        s = argv[1];
        f = fopen(s, "r");
        if (f == NULL) {
            eprintfln("Failed to open '%s'.", s);
            return 1;
        }
        ok = grid_copy_file(&g, f);
        fclose(f);
        if (!ok) {
            return 1;
        }
    } else {
        // Create a solid block of live cells.
        for (size_t row = 0; row <= 2; row += 1) {
            for (size_t col = 1; col <= 3; col += 1) {
                grid_insert(&g, row, col, CELL_ALIVE);
            }
        }
    }

    system("clear");

    struct timespec time_pause;
    time_pause.tv_sec   = 0;
    time_pause.tv_nsec  = TICK_SPEED * 1000000; // msecs * 1e6

    while (g.alive != 0) {
        grid_write(&g, &cell_stdio_writer, stdout);
        grid_update(&g);
        nanosleep(&time_pause, /*rem=*/NULL);
        system("clear");
    }
    return 0;
}
