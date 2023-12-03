// Macro definitions for UNICODE and friends are in our PCH now
#include "olcPixelGameEngine.hpp"

class Example : public olc::PixelGameEngine {
public:
    bool OnUserCreate() override {
        // Called once at the start, so create things here
        return true;
    }

    // elapsed time is like wall time
    bool OnUserUpdate(float elapsed_time) override {
        // Called once per frame
        // for (int x = 0; x < ScreenWidth(); x++) {
        //     for (int y = 0; y < ScreenHeight(); y++) {
        //         // Fill screen with black pixels so my eyes don't hurt
        //         Draw(x, y, olc::BLACK);
        //     }
        // }
        return true;
    }
};

// On MinGW (w64, installed from MSYS2), compile with the following:
// 
// COMPILE FLAGS
//      g++ -std=c++17 -o demo demo.cpp 
//      -luser32 -lgdi32 -lopengl32 -lgdiplus -lShlwapi -ldwmapi -lstdc++fs
// 
// C++ STANDARD:
//      C++14 or C++17
// 
// LINK AGAINST:
//      user32 gdi32 opengl32 gdiplus Shlwapi dwmapi stdc++fs
// 
// NOTE:
//      When compiling a precompiled header, make sure all the compile flags,
//      preprocessor directives and such are the exact same as the main sources.
//      Otherwise, if something is out of sync, the PCH won't be used!
int main() {
    Example demo;
    // If window was constructed successfully, start the thread in the background!
    if (demo.Construct(120, 90, 8, 8)) {
        demo.Start();
    }
    return 0;
}
