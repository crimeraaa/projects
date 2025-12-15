#include "gol.h"

Grid *
grid_make(size_t rows, size_t cols, Allocator allocator)
{
    Grid *g;
    size_t limb_count, array_size;

    limb_count = ((rows * cols) / GRID_LIMB_TYPE_BITS) + 1;
    array_size = sizeof(g->limbs[0]) * limb_count;

    // Assume success
    g        = cast(Grid *)mem_alloc(sizeof(*g) + array_size, allocator);
    g->rows  = rows;
    g->cols  = cols;
    g->alive = 0;
    memset(g->limbs, 0, array_size);
    return g;
}

Grid *
grid_make_copy(const Grid *src, Allocator allocator)
{
    Grid *dst;

    // Assume success
    dst = grid_make(src->rows, src->cols, allocator);
    grid_deep_copy(dst, src);
    return dst;
}

void
grid_deep_copy(Grid *dst, const Grid *src)
{
    size_t limb_count;

    limb_count = ((src->rows * src->cols) / GRID_LIMB_TYPE_BITS) + 1;
    dst->rows  = src->rows;
    dst->cols  = src->cols;
    dst->alive = src->alive;
    memcpy(dst->limbs, src->limbs, sizeof(dst->limbs[0]) * limb_count);
}

void
grid_update(Grid *g, Grid *scratch)
{
    size_t row_count, col_count;

    row_count = g->rows;
    col_count = g->cols;
    grid_deep_copy(scratch, g);

    for (size_t row = 0; row < row_count; row += 1) {
        size_t north, south;

        north = (row - 1 + row_count) % row_count;
        south = (row + 1 + row_count) % row_count;
        for (size_t col = 0; col < col_count; col += 1) {
            size_t west, east;
            int alive_neighbor_count;

            west = (col - 1 + col_count) % col_count;
            east = (col + 1 + col_count) % col_count;

            // Unrolled loop of iterating through all 8 neighbors.
            alive_neighbor_count =
                  cast(int)grid_at(g, north, west)
                + cast(int)grid_at(g, north, col)
                + cast(int)grid_at(g, north, east)
                + cast(int)grid_at(g, row,   west)
                + cast(int)grid_at(g, row,   east)
                + cast(int)grid_at(g, south, west)
                + cast(int)grid_at(g, south, col)
                + cast(int)grid_at(g, south, east);

            switch (alive_neighbor_count) {
            // 1.) Any cell with fewer than 2 live neighbors dies.
            case 0:
            case 1:
            // 3.) Any live cell with more than 3 live neighbors dies,
            // as if by overpopulation.
            default:
                grid_insert(scratch, row, col, CELL_DEAD);
                break;

            // 2.) Any live cell with 2 or 3 live neighbors live on to the
            // next generation.
            case 2:
                break;

            // 4.) Any dead cell with exactly 3 live neighbors becomes
            // a live cell, as if by reproduction.
            case 3:
                grid_insert(scratch, row, col, CELL_ALIVE);
                break;
            }
        }
    }
    grid_deep_copy(g, scratch);
}

Cell_State
grid_at(const Grid *g, size_t row, size_t col)
{
    GRID_LIMB mask;
    size_t cell_index, limb_index, bit_index;

    assertfln(row < g->rows && col < g->cols,
        "Invalid index: cells[row=%zu][col=%zu]", row, col);

    // Column major.
    cell_index = (row * g->cols) + col;
    limb_index = cell_index / GRID_LIMB_SHIFT;
    bit_index  = cell_index % GRID_LIMB_SHIFT;
    mask       = (cast(GRID_LIMB)1 << cast(GRID_LIMB)bit_index);

    return cast(Cell_State)((g->limbs[limb_index] & mask) != 0);
}


/** @brief `g[row][col] = next_state`. */
void
grid_insert(Grid *g, size_t row, size_t col, Cell_State next_state)
{
    GRID_LIMB mask;
    size_t cell_index, limb_index, bit_index;
    Cell_State prev_state;

    assertfln(row < g->rows && col < g->cols,
        "Invalid index: cells[row=%zu][col=%zu]", row, col);

    // Column major.
    cell_index = (row * g->cols) + col;
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

void
grid_write(const Grid *g, Cell_Writer writer_fn, void *writer_data)
{
    size_t row_count, col_count;

    row_count = g->rows;
    col_count = g->cols;
    for (size_t row = 0; row < row_count; row += 1) {
        for (size_t col = 0; col < col_count; col += 1) {
            Cell_State state;

            state = grid_at(g, row, col);
            writer_fn(writer_data, state);
        }
        writer_fn(writer_data, CELL_WRITE_NEWLINE);
    }
}
