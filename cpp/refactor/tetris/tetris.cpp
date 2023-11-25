// PCH must always come first in order for it to work correctly.
#include "include/common.hpp"
#include "tetris.hpp"

using namespace std;

int main() {
    Tetris tetris = Tetris(12, 18, 80, 30);
    tetris.dump_pieces();
    return 0;
}
