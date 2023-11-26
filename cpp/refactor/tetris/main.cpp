// PCH must always come first in order for it to work correctly.
#include "include/common.hpp"
#include "include/tetris.hpp"

using namespace std;

int main() {
    Tetris tetris = Tetris();
    bool is_gameover = false;

    // The 4 stages of most game loops
    while (!is_gameover) {
        /*************************** GAME TIMING ******************************/
        // milliseconds suffix for literals only available with C++14 and above.
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
        /*************************** GAME INPUT ******************************/
        tetris.input();

        /*************************** GAME LOGIC *******************************/
        // Shapes falling collision detection and scoring


        /************************** RENDER OUTPUT *****************************/
        tetris.render();
    }
    return 0;
}
