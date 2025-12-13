#include "gol.h"

#define GRID_WIDTH  25
#define GRID_HEIGHT 25

static Cell
grid[GRID_HEIGHT][GRID_WIDTH];

static void
grid_init(void)
{
    for (int row = 0; row < GRID_HEIGHT; row += 1) {
        for (int col = 0; col < GRID_WIDTH; col += 1) {
            grid[row][col].state = CELL_DEAD;
        }
    }
}

static int
grid_print(void)
{
    int alive_count = 0;
    for (int row = 0; row < GRID_HEIGHT; row += 1) {
        for (int col = 0; col < GRID_WIDTH; col += 1) {
            if (grid[row][col].state == CELL_ALIVE) {
                alive_count += 1;
                printf("%c", CELL_SYMBOL_ALIVE);
            } else {
                printf("%c", CELL_SYMBOL_BG);
            }
        }
        printf("\n");
    }
    return alive_count;
}

static void
grid_update(void)
{
    for (int row = 0; row < GRID_HEIGHT; row += 1) {
        for (int col = 0; col < GRID_WIDTH; col += 1) {
            // Iterate over the neighboring cells.
            int alive_count = 0;
            for (int row_offset = -1; row_offset <= 1; row_offset += 1) {
                for (int col_offset = -1; col_offset <= 1; col_offset += 1) {
                    // No offset means we are poking at the same cell.
                    if (row_offset == 0 && col_offset == 0) {
                        continue;
                    }

                    // Bounds check.
                    int i = row + row_offset;
                    int j = col + col_offset;
                    if (!(0 <= i && i < GRID_HEIGHT) || !(0 <= j && j < GRID_HEIGHT)) {
                        continue;
                    }

                    if (grid[i][j].state == CELL_ALIVE) {
                        alive_count += 1;
                    }
                }
            }

            switch (alive_count) {
            // 1.) Any cell with fewer than 2 live neighbors dies.
            case 0:
            case 1:
                grid[row][col].state = CELL_DEAD;
                break;

            // 2.) Any live cell with 2 or 3 live neighbors live on to the
            // next generation.
            case 2:
                break;

            // 4.) Any dead cell with exactly 3 live neighbors becomes
            // a live cell, as if by reproduction.
            case 3:
                grid[row][col].state = CELL_ALIVE;
                break;

            // 3.) Any live cell with more than 3 live neighbors dies,
            // as if by overpopulation.
            default:
                grid[row][col].state = CELL_DEAD;
                break;
            }
        }
    }
}

static intmax_t
msecs_to_nsecs(time_t msecs)
{
    return msecs * (intmax_t)1e+06;
}


int
main(void)
{
    system("clear");
    grid_init();

    // Create a solid block of live cells.
    for (int row = 0; row < GRID_HEIGHT / 5; row += 1) {
        for (int col = 0; col < GRID_WIDTH / 5;  col += 1) {
            grid[row][col].state = CELL_ALIVE;
        }
    }

    // See the output of GNU units: `units "0.5 seconds" "nanoseconds"`
    struct timespec pause;
    pause.tv_sec  = 0;
    pause.tv_nsec = msecs_to_nsecs(50);
    while (grid_print() != 0) {
        grid_update();
        nanosleep(&pause, /*rem=*/NULL);
        system("clear");
    }
    return 0;
}
