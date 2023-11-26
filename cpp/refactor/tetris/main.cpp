// PCH must always come first, for all translation units.
// If at least one is missing it, it won't work correctly.
// 
// Try to verify the include paths via the -H flag.
// 1. creating the object files
//      g++ -Wall -Wextra -Werror -H -c -o main.o main.cpp
//      g++ -Wall -Wextra -Werror -H -c -o tetris.o tetris.cpp
// 2. compiling executable directly from source
//      g++ -Wall -Wextra -Werror -H -o main main.cpp tetris.cpp
// 
// You can verify the preprocessor output. Note that it can be MASSIVE.
//      Because of that, I recommend sending hte output to a file.
// Pass -E to stop at the preprocessing stage. By default it writes to stdout.
//      We can use the shell's redirect operator '>` to send the output elsewhere.
// Pass -P right after to strip line markers and comments.
//
//      g++ -Wall -Wextra -Werror -E -P main.cpp > dump.txt
#include "include/common.hpp"
#include "tetris.hpp"

using namespace std;

/**
 * @note See here for how to inline member functions separately:
 * https://stackoverflow.com/a/9370630
 * 
 * @note Basically declare function definitions outside of class declaration
 * as `inline`.
 * 
 * @warning Publicly exposed functions defined in a separate translation unit
 * must not be inlined it seems. E.g. `Tetris::input` and `Tetris::render`.
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
        // TODO: Shapes falling collision detection and scoring


        /************************** RENDER OUTPUT *****************************/
        tetris.render();
    }
    return 0;
}
