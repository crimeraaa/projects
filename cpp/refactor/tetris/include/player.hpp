#pragma once
#include "common.hpp"

class TetrisPlayer {
public: // SYMBOLIC CONSTANTS
     // Symbolic constants for indexes into the `m_controls` array.
    enum Keys {
        KEY_RIGHT = 0,
        KEY_LEFT,
        KEY_DOWN,
        KEY_Z,
        KEYS_COUNT
    };

private: // INTERNAL STRUCT DEFINITIONS
    // All public members, this is internal anyway.
    struct Position {
        int x = 0;
        int y = 0;
        Position(int fx, int fy) : x(fx), y(fy) { 
            // no code here, just need the initializer list
        }
    };

    // All public members, this is internal anyway.
    struct VKey {
        unsigned char code;
        bool down;
        
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

private: // MEMBER VARIABLES
    int m_piece_id; // To be used as index into tetromino array
    int m_rotation; // Current rotation to be modulo by 4
    Position m_position; // coordinates into the playing field buffer

    // List of valid keys that we listen for.
    // @warning Please keep this in the same order as `enum Keys`!
    std::array<VKey, KEYS_COUNT> m_controls = {
        VKey(VK_RIGHT), 
        VKey(VK_LEFT), 
        VKey(VK_DOWN), 
        VKey('Z')
    }; 

private: // HELPER FUNCTIONS

public: // CONSTRUCTOR
    TetrisPlayer(int fwidth) 
    :   m_piece_id(0),
        m_rotation(0),
        m_position(fwidth / 2, 0)
    {
        // no code here, just need the initializer list
    }

public: // GETTERS
    // Readable and writeable reference to the current rotation value.
    int &rotation() {
        return m_rotation;
    }

    // Readable and writeable reference to the ID of the current tetromino piece.
    int &piece_id() {
        return m_piece_id;
    }

    // Readable and writeable reference to the player's current x-axis index
    // into the playing field.
    int &px() {
        return m_position.x;
    }

    // Readable and writeable reference to the player's current y-axis index
    // into the playing field.
    int &py() {
        return m_position.y;
    }
    
public: // METHODS
    /**
     * Calls `VKey::update()`, which asynchronously listens for key presses
     * via the Windows API call `GetAsyncKeyState`.
     * 
     * @note This updates the `this->m_controls[i]` array.
     */
    void input() {
        for (int i = 0; i < KEYS_COUNT; i++) {
            m_controls[i].update();
        }
    }

    bool is_pressing(enum Keys k) {
        return m_controls[k].down;
    }
};
