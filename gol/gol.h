#ifndef CONWAY_GAME_OF_LIFE_H
#define CONWAY_GAME_OF_LIFE_H

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <stdint.h>

#define GRID_WIDTH  25
#define GRID_HEIGHT 25

#define CELL_SYMBOL_ALIVE       '#'
#define CELL_SYMBOL_BG  '-'

enum Cell_State {
    CELL_DEAD,
    CELL_ALIVE,
};

typedef enum Cell_State Cell_State;
typedef struct Cell Cell;

struct Cell {
    Cell_State state;
};


#endif /* CONWAY_GAME_OF_LIFE_H */
