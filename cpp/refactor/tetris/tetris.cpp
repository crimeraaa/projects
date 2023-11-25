// PCH must always come first in order for it to work correctly.
#include "include/common.hpp"
#include "tetris.hpp"

using namespace std;

int main() {
    Tetris tetris;
    tetris.setup_screen(80, 30).setup_field(12, 18);
    tetris.dump_pieces();
    return 0;
}
