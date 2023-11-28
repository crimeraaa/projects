#pragma once
#include "common.hpp"

// 2D coordinates into a 2D tile map or grid.
// @note Assume `(x=0,y=0)` is top left corner. This is why we use `size_t`.
struct Position {
    size_t x = 0; // Index into width (`column#`).
    size_t y = 0; // Index into height (`row#`).
    Position(size_t fx, size_t fy) : x(fx), y(fy) {}
};

// Windows Virtual key codes
struct VKey {
    unsigned char code; // Virtual key code---use a `VK_` prefixed macro.
    bool down; // Maintain state between player's input calls.

    /**
     * @note Please pass a `VK_` macro for the correct Windows virtual key code,
     * else you can just pass a `char` literal.
     */
    VKey(unsigned char vkeycode) : code(vkeycode), down(false) {}

    /**
     * Using bitwise `&`, we determine if the bit is 1.
     * If it is, the key is being pressed down.
     * 
     * @note `<val> & 0x8000` means get most significant bit of a 16 bit number.
     * Windows API defines that as the state of the key being pressed.
     */
    void update() {
        down = (GetAsyncKeyState(code) & 0x8000);
    }
};

class Player {
    public: // SYMBOLIC CONSTANTS
    // Symbolic constants for indexes into the `controls` array.
    // @note Need a name for this enum to refer to.
    enum Keys {
        KEY_RIGHT = 0,
        KEY_LEFT,
        KEY_DOWN,
        KEY_Z,
        KEYS_COUNT
    };

    public: // EXPOSED MEMBERS
    int piece_id; // Index into tetromino array.
    int rotation; // Current rotation to be modulo by 4. Used for calculating true index.
    Position position; // Indexes into playing field buffer.
    bool hold; // Stagger to avoid super fast repeating rotations.

    // List of valid keys that we listen for.
    // @warning Please keep this in the same order as `enum Keys`!
    std::array<VKey, KEYS_COUNT> controls = {
        VKey(VK_RIGHT), VKey(VK_LEFT), VKey(VK_DOWN), VKey('Z')
    };

    public: // CONSTRUCTOR
    // Need playing field width so we can start the player off in the middle.
    Player(size_t fwidth)
        : piece_id(0)
        , rotation(0)
        , position(fwidth / 2, 0)
        , hold(false) { /* no code here, just need the initializer list */
    }
};
