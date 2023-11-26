#pragma once
#include "common.hpp"
#include "base.hpp"

class TetrisPlayer {
public: // SYMBOLIC CONSTANTS
     // Symbolic constants for indexes into the `controls` array.
    enum Keys {
        KEY_RIGHT = 0,
        KEY_LEFT,
        KEY_DOWN,
        KEY_Z,
        KEYS_COUNT
    };

public: // EXPOSED MEMBERS

    int piece_id; // index into tetromino array
    int rotation; // current rotation to be modulo by 4
    Position position; // 2D coordinates (is index into playing field buffer)

    // List of valid keys that we listen for.
    // @warning Please keep this in the same order as `enum Keys`!
    std::array<VKey, KEYS_COUNT> controls = {
        VKey(VK_RIGHT), 
        VKey(VK_LEFT), 
        VKey(VK_DOWN), 
        VKey('Z')
    }; 

public: // CONSTRUCTOR

    TetrisPlayer(int fwidth) 
    :   piece_id(0),
        rotation(0),
        position(fwidth / 2, 0)
    {
        // no code here, just need the initializer list
    }
    
public: // METHODS
    /**
     * Calls `VKey::update()`, which asynchronously listens for key presses
     * via the Windows API call `GetAsyncKeyState`.
     * 
     * @note This updates the `this->controls[i]` array.
     */
    void input() {
        for (int i = 0; i < KEYS_COUNT; i++) {
            controls[i].update();
        }
    }

    bool is_pressing(enum Keys k) {
        return controls[k].down;
    }
};
