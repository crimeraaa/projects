#ifndef CONWAY_GAME_OF_LIFE_H
#define CONWAY_GAME_OF_LIFE_H

#include <stdio.h>  // fprintf, fputc
#include <stdlib.h> // system
#include <string.h> // memset, memcpy
#include <time.h>   // struct timespec, nanosleep

#include "../common.h"


/** @brief In milliseconds. */
#define TICK_SPEED              100
#define GRID_COLS               50
#define GRID_ROWS               25
#define GRID_AREA               (GRID_ROWS * GRID_COLS)
#define CELL_CHAR_ALIVE         'E'
#define CELL_CHAR_DEAD          '-'
#define GRID_LIMB_TYPE          uint64_t
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
    /* How many `CELL_ALIVE` are in `cells[]`? */
    int alive;

    /* Packed `Cell_State`. */
    GRID_LIMB limbs[GRID_AREA / GRID_LIMB_TYPE_BITS + 1];
};

typedef struct Grid Grid;
typedef enum Cell_State Cell_State;

void
grid_init(Grid *g);

void
grid_update(Grid *g);

void
grid_copy(Grid *dst, const Grid *src);

/** @brief `return g[row][col]`. */
Cell_State
grid_at(const Grid *g, size_t row, size_t col);


/** @brief `g[row][col] = next_state`. */
void
grid_insert(Grid *g, size_t row, size_t col, Cell_State next_state);


void
grid_write(const Grid *g, Cell_Writer writer_fn, void *writer_data);

#endif /* CONWAY_GAME_OF_LIFE_H */
