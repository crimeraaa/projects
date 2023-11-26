#pragma once
#include "common.hpp"

// 2D coordinates
struct Position {
    size_t x = 0;
    size_t y = 0;
    Position(size_t fx, size_t fy) : x(fx), y(fy) { 
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

    /**
     * @note `0x8000` means get the most significant bit of a 16 bit number.
     * Windows API defines that as the key being pressed.
     * 
     * @note Meaning, if that bit is 1, key is being pressed down. 
     */
    void update() {
        down = (GetAsyncKeyState(code) & 0x8000);
    }
}; 
