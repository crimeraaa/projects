// PCH must always come first in order for it to work correctly.
#include "include/common.hpp"
#include "include/tetris.hpp"

using namespace std;

/**
 * To verify preprocessing, try:
 * - `g++ -I. -E -P ./main.cpp > ./dump.cpp`
 * 1. `-I` means uses the specified directory for headers. and `-I.` means include the current directory.
 * 2. `-E` means stop at the preprocessing stage. By default we output to stdout.
 * 3. `-P` means omit linemarkers from the preprocessor output.
 * 
 * @note See: https://stackoverflow.com/a/30033131, https://gcc.gnu.org/onlinedocs/cpp/Invocation.html#index-P
*/
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
