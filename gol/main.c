#include "gol.c"

#include "raylib.h"

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

#define SCALE        20
#define TEXT_OFFSET  2

struct Point {int x, y;};
typedef struct Point Point;

static void
cell_writer(void *user_data, Cell_State state)
{
    Point *pixel;

    pixel = cast(Point *)user_data;
    switch (state) {
    case CELL_DEAD:
        DrawRectangleLines(pixel->x, pixel->y, SCALE, SCALE, DARKGRAY);
        pixel->x += SCALE;
        break;
    case CELL_ALIVE:
        DrawRectangle(pixel->x, pixel->y, SCALE, SCALE, WHITE);
        pixel->x += SCALE;
        break;
    case CELL_WRITE_NEWLINE:
        pixel->x  = 0;
        pixel->y += SCALE;
        break;
    default:
        unreachable();
        break;
    }
}

static void
render(Grid *g)
{
    ClearBackground(BLACK);
    BeginDrawing();
    {
        const char *msg;
        Point pixel = {0, TEXT_OFFSET * SCALE};

        msg = TextFormat("FPS: %i", GetFPS());
        DrawText(msg, /*posX=*/5, /*posY=*/5, /*fontSize=*/SCALE, WHITE);
        grid_write(g, &cell_writer, &pixel);
    }
    EndDrawing();
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

    InitWindow(/*width=*/  GRID_COLS * SCALE,
               /*height=*/(GRID_ROWS + TEXT_OFFSET) * SCALE,
               "Conway's Game Of Life");
    SetTargetFPS(5);
    render(&g);
    while (!WindowShouldClose()) {
        grid_update(&g);
        render(&g);
    }
    CloseWindow();
    return 0;
}
