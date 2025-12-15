// gol
#include "gol.c"

// main
#include <mem/allocator.c>
#include <mem/arena.c>
#include "raylib.h"

// 5:4 ratio
#define GRID_COLS       50
#define GRID_ROWS       40
#define CELL_CHAR_ALIVE 'E'
#define CELL_CHAR_DEAD  '-'
#define POINT_SCALE     16
#define TEXT_OFFSET     24

// 0.2 seconds per tick
#define TARGET_FPS 12


// Grid points are 2D coordinates, in pixels, adjusted for `POINT_SCALE`
// and `TEXT_OFFSET`.
typedef struct Grid_Point Grid_Point;
struct Grid_Point {
    int x, y;
};

typedef struct Grid_Line Grid_Line;
struct Grid_Line {
    Grid_Point start, end;
};


// Very unsafe function!
static bool
grid_copy_file(Grid *g, FILE *f)
{
    size_t row_count, col_count, line = 0, col = 0;

    row_count = g->rows;
    col_count = g->cols;
    // The following loop body is atrocious, but works
    for (int ch = fgetc(f); ch != EOF; ch = fgetc(f)) {
        if (ch == '\n') {
            if (line + 1 > row_count) {
                eprintfln("[ERROR]: Grid overflow: file lines=%zu + 1 > g->rows=%zu",
                    line, row_count);
                return false;
            }
            line++;
            col = 0;
        } else if (ch == CELL_CHAR_ALIVE) {
            if (col + 1 > col_count) {
                goto invalid_cols;
            }
            grid_insert(g, line, col++, CELL_ALIVE);
        } else if (ch == CELL_CHAR_DEAD) {
            if (col + 1 > col_count) {
invalid_cols:
                eprintfln("[ERROR]: Grid overflow: file cols=%zu + 1 > g->cols=%zu",
                    col, col_count);
                return false;
            }
            grid_insert(g, line, col++, CELL_DEAD);
        } else {
            eprintfln("[ERROR]: Unknown grid character '%c' (line %zu, col %zu)",
                cast(char)ch, line + 1, col + 1);
            return false;
        }
    }
    return true;
}

static Grid_Point
grid_point_make(int x, int y)
{
    Grid_Point p;
    p.x = x * POINT_SCALE;
    p.y = y * POINT_SCALE + TEXT_OFFSET;
    return p;
}

static void
grid_point_draw(Grid_Point p)
{
    DrawRectangle(p.x, p.y, /*width=*/POINT_SCALE, /*height=*/POINT_SCALE, RAYWHITE);
}

static void
cell_writer(void *user_data, Cell_State state)
{
    Grid_Point *p;

    p = cast(Grid_Point *)user_data;
    switch (state) {
    case CELL_DEAD:
        p->x += POINT_SCALE;
        break;
    case CELL_ALIVE:
        grid_point_draw(*p);
        p->x += POINT_SCALE;
        break;
    case CELL_WRITE_NEWLINE:
        p->x = 0;
        p->y += POINT_SCALE;
        break;
    default:
        unreachable();
        break;
    }
}

static Grid_Line
grid_line_make(int x_start, int y_start, int x_end, int y_end)
{
    Grid_Line l;
    l.start = grid_point_make(x_start, y_start);
    l.end   = grid_point_make(x_end, y_end);
    return l;
}

static void
grid_line_draw(Grid_Line line)
{
    DrawLine(line.start.x, line.start.y, line.end.x, line.end.y, DARKGRAY);
}

static void
grid_draw_grid_lines(const Grid *g)
{
    // Draw Grid x-axis
    size_t row_count, col_count;
    int y_end;

    row_count = g->rows;
    col_count = g->cols;
    y_end     = cast(int)row_count;
    for (size_t row = 0; row < row_count + 1; row += 1) {
        Grid_Line x_axis;
        int y;

        y      = cast(int)row;
        x_axis = grid_line_make(0, y, cast(int)col_count, y);
        grid_line_draw(x_axis);

        // Draw Grid y-axis
        for (size_t col = 0; col < col_count + 1; col += 1) {
            Grid_Line y_axis;

            y_axis = grid_line_make(cast(int)col, 0, cast(int)col, y_end);
            grid_line_draw(y_axis);
        }
    }
}

static void
render(Grid *g, int *ticks)
{
    ClearBackground(BLACK);
    BeginDrawing();
    {
        const char *debug_info, *help_msg;
        Grid_Point p, center;

        p           = grid_point_make(0, 0);
        center      = grid_point_make(GRID_COLS / 2 - 4, 0);
        debug_info  = TextFormat("FPS: %i, Generations: %i", GetFPS(), *ticks);
        help_msg    = "Controls: S (start), P (pause), N (next)";
        *ticks += 1;

        DrawText(debug_info, /*posX=*/4, /*posY=*/4, /*fontSize=*/20, RAYWHITE);
        DrawText(help_msg, /*posX=*/center.x, /*posY=*/4, /*fontSize=*/20, RAYWHITE);
        grid_write(g, &cell_writer, &p);
        // Grid must overlay the pixels.
        grid_draw_grid_lines(g);
    }
    EndDrawing();
}

int
main(int argc, char *argv[])
{
    // int x = (((GRID_ROWS * GRID_COLS) / GRID_LIMB_TYPE_BITS) + 1) * sizeof(GRID_LIMB_TYPE) * 2;
    char arena_buf[BUFSIZ];
    Arena arena;
    Grid *g, *scratch;

    arena_init(&arena, arena_buf, sizeof(arena_buf));
    g       = grid_make(GRID_ROWS, GRID_COLS, arena_allocator(&arena));
    scratch = grid_make_copy(g, arena_allocator(&arena));

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
        ok = grid_copy_file(g, f);
        fclose(f);
        if (!ok) {
            return 1;
        }
    } else {
        // Create a solid block of live cells.
        for (size_t row = 0; row <= 2; row += 1) {
            for (size_t col = 1; col <= 3; col += 1) {
                grid_insert(g, row, col, CELL_ALIVE);
            }
        }
    }

    Grid_Point dims = grid_point_make(cast(int)g->cols, cast(int)g->rows);
    InitWindow(/*width=*/dims.x, /*height=*/dims.y, "Conway's Game Of Life");
    SetTargetFPS(TARGET_FPS);

    int ticks = 0;
    bool is_paused = true;
    render(g, &ticks);
    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_S)) {
            is_paused = false;
        } else if (IsKeyPressed(KEY_P)) {
            is_paused = true;
        }

        if (is_paused && !IsKeyDown(KEY_N)) {
            ticks -= 1;
        } else {
            grid_update(g, scratch);
        }
        render(g, &ticks);
    }
    CloseWindow();
    return 0;
}
