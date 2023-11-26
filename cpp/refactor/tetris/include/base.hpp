#pragma once
#include "common.hpp"
// 2D coordinates
struct Position {
    int x = 0;
    int y = 0;
    Position(int fx, int fy) : x(fx), y(fy) { 
        // no code here, just need the initializer list
    }
};

// Windows Virtual key codes
struct VKey {
    unsigned char code; // Virtual key code, e.g. 0x27 for right arrow
    bool down; // Maintain state

    // Please pass a `VK_` macro for the correct Windows virtual key code,
    // else you can just pass a `char` literal.
    VKey(unsigned char vkeycode) : code(vkeycode), down(false) { 
        // no code here, just need the initializer list
    }

    // 0x8000 means get the most significant bit of a 16 bit number,
    // the Windows API defines that as the key being pressed.
    // So if MSB of 16 bits is 1, it's being pressed down. 
    void update() {
        down = (GetAsyncKeyState(code) & 0x8000);
    }
}; 
