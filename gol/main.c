#include "gol.h"

static void
grid_init(Grid *g)
{
    g->alive = 0;
    memset(g->limbs, CELL_DEAD, sizeof(g->limbs));
}

static void
grid_copy(Grid *dst, const Grid *src)
{
    dst->alive = src->alive;
    memcpy(dst->limbs, src->limbs, sizeof(dst->limbs));
}


/** @brief `return g[row][col]`. */
static Cell_State
grid_at(const Grid *g, size_t row, size_t col)
{
    GRID_LIMB mask;
    size_t cell_index, limb_index, bit_index;

    assertfln(row < GRID_ROWS && col < GRID_COLS,
        "Invalid index: cells[row=%zu][col=%zu]", row, col);

    // Column major.
    cell_index = (row * GRID_COLS) + col;
    limb_index = cell_index / GRID_LIMB_SHIFT;
    bit_index  = cell_index % GRID_LIMB_SHIFT;
    mask       = (cast(GRID_LIMB)1 << cast(GRID_LIMB)bit_index);

    return cast(Cell_State)((g->limbs[limb_index] & mask) != 0);
}


/** @brief `g[row][col] = next_state`. */
static void
grid_insert(Grid *g, size_t row, size_t col, Cell_State next_state)
{
    GRID_LIMB mask;
    size_t cell_index, limb_index, bit_index;
    Cell_State prev_state;

    assertfln(row < GRID_ROWS && col < GRID_COLS,
        "Invalid index: cells[row=%zu][col=%zu]", row, col);

    // Column major.
    cell_index = (row * GRID_COLS) + col;
    limb_index = cell_index / GRID_LIMB_SHIFT;
    bit_index  = cell_index % GRID_LIMB_SHIFT;
    mask       = (cast(GRID_LIMB)1 << cast(GRID_LIMB)bit_index);
    prev_state = cast(Cell_State)((g->limbs[limb_index] & mask) != 0);

    switch (prev_state) {
    case CELL_DEAD:
        // dead -> alive
        if (next_state == CELL_ALIVE) {
            // Set the 1-bit into place.
            g->alive += 1;
            g->limbs[limb_index] |= mask;
            break;
        }
        break;
    case CELL_ALIVE:
        // alive -> dead
        if (next_state == CELL_DEAD) {
            // Copy all 1-bits for this limb except for `bit_index`.
            g->alive -= 1;
            g->limbs[limb_index] &= ~mask;
            break;
        }
        break;
    default:
        unreachable();
        break;
    }
}

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
grid_write(const Grid *g, Cell_Writer writer_fn, void *writer_data)
{
    for (size_t row = 0; row < GRID_ROWS; row += 1) {
        for (size_t col = 0; col < GRID_COLS; col += 1) {
            Cell_State state;

            state = grid_at(g, row, col);
            writer_fn(writer_data, state);
        }
        writer_fn(writer_data, CELL_WRITE_NEWLINE);
    }
}

static void
grid_update(Grid *g)
{
    Grid scratch;
    grid_copy(&scratch, g);
    for (size_t row = 0; row < GRID_ROWS; row += 1) {
        size_t north, south;

        north = (row - 1 + GRID_ROWS) % GRID_ROWS;
        south = (row + 1 + GRID_ROWS) % GRID_ROWS;
        for (size_t col = 0; col < GRID_COLS; col += 1) {
            size_t west, east;
            int count_neighbors_alive;

            west = (col - 1 + GRID_COLS) % GRID_COLS;
            east = (col + 1 + GRID_COLS) % GRID_COLS;

            count_neighbors_alive =
                  cast(int)(grid_at(g, north, west))
                + cast(int)(grid_at(g, north, col))
                + cast(int)(grid_at(g, north, east))
                + cast(int)(grid_at(g, row,   west))
                + cast(int)(grid_at(g, row,   east))
                + cast(int)(grid_at(g, south, west))
                + cast(int)(grid_at(g, south, col))
                + cast(int)(grid_at(g, south, east));

            switch (count_neighbors_alive) {
            // 1.) Any cell with fewer than 2 live neighbors dies.
            case 0:
            case 1:
            // 3.) Any live cell with more than 3 live neighbors dies,
            // as if by overpopulation.
            default:
                grid_insert(&scratch, row, col, CELL_DEAD);
                break;

            // 2.) Any live cell with 2 or 3 live neighbors live on to the
            // next generation.
            case 2:
                break;

            // 4.) Any dead cell with exactly 3 live neighbors becomes
            // a live cell, as if by reproduction.
            case 3:
                grid_insert(&scratch, row, col, CELL_ALIVE);
                break;
            }
        }
    }
    grid_copy(g, &scratch);
}

static intmax_t
msecs_to_nsecs(time_t msecs)
{
    return cast(intmax_t)msecs * 1000000;
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

    grid_write(&g, &cell_stdio_writer, stdout);
    fprintf(stdout, "Press any key to continue. ");
    fflush(stdout);
    int ch = fgetc(stdin);
    if (ch != EOF) {
        // See the output of GNU units: `units "0.5 seconds" "nanoseconds"`
        struct timespec t;
        t.tv_sec  = 0;
        t.tv_nsec = msecs_to_nsecs(TICK_SPEED);
        system("clear");

        while (g.alive != 0) {
            grid_write(&g, &cell_stdio_writer, stdout);
            grid_update(&g);
            nanosleep(&t, /*rem=*/NULL);
            system("clear");
        }
    }

    return 0;
}
