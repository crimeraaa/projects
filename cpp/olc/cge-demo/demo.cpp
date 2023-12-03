#include "olcConsoleGameEngine.hpp"

class Demo : public olcConsoleGameEngine {
public:
    // Nothing constructor
    Demo() {}

    // Must be overriden. Just do nothing for now.
    bool OnUserCreate() override {
        return true;
    }

    // Must be overriden, is called every frame.
    // Shows a palette of all the available colors.
    // @param dt Stands for "delta time".
    bool OnUserUpdate(float dt) override {
        Fill(0, 0, ScreenWidth(), ScreenHeight(), PIXEL_SOLID, FG_BLACK);
        // https://youtu.be/u5BhrA8ED0o?si=_GTIxkrx4aL3AYEe&t=1416
        for (int c = 0; c < 16; c++) {
            int x1 = 0, y1 = c * 6;
            int x2 = 5, y2 = y1 + 5;

            Fill(x1,      y1, x2,      y2, PIXEL_QUARTER, c);
            Fill(x1 + 6,  y1, x2 + 6,  y2, PIXEL_HALF, c);
            Fill(x1 + 12, y1, x2 + 12, y2, PIXEL_THREEQUARTERS, c);
            Fill(x1 + 18, y1, x2 + 18, y2, PIXEL_SOLID, c);

            Fill(x1 + 24, y1, x2 + 24, y2, PIXEL_THREEQUARTERS, c | BG_WHITE);
            Fill(x1 + 30, y1, x2 + 30, y2, PIXEL_HALF, c | BG_WHITE);
            Fill(x1 + 36, y1, x2 + 36, y2, PIXEL_QUARTER, c | BG_WHITE);
        }
        return true;
    }
};

/*  
Unlike Pixel Game Engine, this one has minimal dependencies! 
We do, however, still need to link against `winmm.lib` and `user32.lib`.

#### GCC/CLANG
`    g++ -std=c++17 -I.. -o demo demo.cpp -luser32 -lwinmm`

#### MSVC
`    cl /EHsc /std:c++17 /I.. demo.cpp /link user32.lib winmm.lib`

#### Or use MSVC pragmas:
```c
    // In `../olcConsoleGameEngine.hpp`
    //  Note that this pragma is ONLY recognized by MSVC.
     #pragma comment(lib, "user32.lib")
     #pragma comment(lib, "winmm.lib")
```
*/
int main() {
    // system("pause"); // Hack so we can attach debug before constructors
    Demo demo;
    // Watch the font size! 8 is quite large, so 6 seems like a good compromise.
    demo.ConstructConsole(160, 120, 6, 6);
    demo.Start();
    return 0;
}
