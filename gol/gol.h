#ifndef CONWAY_GAME_OF_LIFE_H
#define CONWAY_GAME_OF_LIFE_H

#include <stdio.h>  // fprintf, fputc
#include <string.h> // memset, memcpy

#include <mem/allocator.h>

#define GRID_LIMB_TYPE          u64
#define GRID_LIMB_TYPE_BITS     (sizeof(GRID_LIMB_TYPE) * CHAR_BIT)
#define GRID_LIMB_SHIFT         GRID_LIMB_TYPE_BITS

enum Cell_State {
    CELL_DEAD,
    CELL_ALIVE,

    /* Only ever passed to `Cell_Writer()`. */
    CELL_WRITE_NEWLINE
};

typedef GRID_LIMB_TYPE GRID_LIMB;
typedef void (*Cell_Writer)(void *user_data, enum Cell_State state);

struct Grid {
    size_t rows, cols;

    /* How many `CELL_ALIVE` are in `cells[]`? */
    int alive;

    /* Packed `Cell_State`. */
    GRID_LIMB limbs[];
};

typedef struct Grid Grid;
typedef enum Cell_State Cell_State;

Grid *
grid_make(size_t rows, size_t cols, Allocator allocator);

Grid *
grid_make_copy(const Grid *src, Allocator allocator);

void
grid_init(Grid *g);

void
grid_update(Grid *g, Grid *scratch);


/** @brief Deep copy `*dst = *src`, assuming already adequate sizes. */
void
grid_deep_copy(Grid *dst, const Grid *src);


/** @brief `return g[row][col]`. */
Cell_State
grid_at(const Grid *g, size_t row, size_t col);


/** @brief `g[row][col] = next_state`. */
void
grid_insert(Grid *g, size_t row, size_t col, Cell_State next_state);


void
grid_write(const Grid *g, Cell_Writer writer_fn, void *writer_data);

#endif /* CONWAY_GAME_OF_LIFE_H */
