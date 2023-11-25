// PCH must always come first in order for it to work correctly.
#include "include/common.hpp"
#include "tetris.hpp"

using namespace std;

int main() {
    Tetris tetris;
    tetris.game_loop();
    return 0;
}
