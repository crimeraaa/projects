#pragma once
#include "include/common.hpp"

#include <vector>

#include "include/display.hpp"
#include "include/playingfield.hpp"

// For setup functions: think in terms of `WIDTH` times `HEIGHT`.
class Tetris {
private:
    static constexpr int PIECE_WIDTH = 4;
    static constexpr int PIECE_HEIGHT = 4;
    static constexpr int PIECE_AREA = PIECE_WIDTH * PIECE_HEIGHT;

    Display m_display; // initialize this one first
    PlayingField m_pfield; // initialize after `m_display`
    
    // List of valid tetris pieces. Each tetronimo is a 4x4 block.
    const std::vector<std::wstring> m_tetrominos = {
        // [0] 4x1: vertical line
        L"..X."
         "..X."
         "..X."
         "..X.",
        // [1] 3+1: T shape
        L"..X."
         ".XX."
         "..X."
         "....", 
        // [2] 2x2: square
        L"...."
         ".XX."
         ".XX."
         "....", 
        // [3] 2+2: Z shape
        L"..X."
         ".XX."
         ".X.."
         "....",
        // [4] 2+2: S shape
        L".X.."
         ".XX."
         "..X."
         "....",
        // [5] 3+1: L shape
        L".X.."
         ".X.."
         ".XX."
         "....",
        // [6] 3+1: Mirror L
        L"..X."
         "..X."
         ".XX."
         "...."
    };

    /**
     * Get the correct index into a piece based on its rotation.
     * @param target_x `px`, desired piece's x-axis cell, from the caller loop.
     * @param target_y `py`, desired piece's y-axis cell, from the caller loop.
     * @param rotation Value of rotation. We modulo by 4 internally.
     */
    int rotate(int target_x, int target_y, int rotation);

    /**
     * @param tetromino_id ID to be used as index into global `tetromino` array.
     * @param rotation Value of rotation to be passed to the `rotate` function.
     * @param field_x Piece X-position in playing field, may add/subtract 1.
     * @param field_y Piece Y-position in playing field, may add/subtract 1.
     */
    bool piece_fits(int tetromino_id, int rotation, int field_x, int current_y) {
        // Check each cell of the reference tetromino piece
        for (int px = 0; px < 4; px++) {
            for (int py = 0; py < 4; py++) {
                // Get index into the reference piece given the rotation
                int pi = rotate(px, py, rotation);
                // Get index into the field
            }
        }
        return false;
    }
public:
    /**
     * @warning Please pass 0 arguments or all 4, nothing in between,
     * unless you know what you are doing.
     * Dire things may happen if you pass some but others use the defaults.
     * @note `scr` short for `display screen`.
     * @note `pf` short for `playing field`.
     * @note Default values should be left at declaration.
     */
    Tetris(int scr_width = 80, int scr_height = 30, int pf_width = 12, int pf_height = 18);

    // testing function
    void dump_pieces() {
        std::wcout << m_tetrominos.size() << L" elements.\n";
        for (auto &ws : m_tetrominos) {
            // std::wcout << ws << L"\n";
            for (int i = 0, limit = ws.size(); i < limit; i++) {
                if (i > 0 && ((i % 4) == 0)) {
                    std::cout << "\n";
                }
                std::cout << (char) ws[i];
            }
            std::cout << "\n\n";
        }
    }
};

/*******************************************************************************
******************************** IMPLEMENTATION ********************************
*******************************************************************************/


Tetris::Tetris(int scr_width, int scr_height, int pf_width, int pf_height) 
:   m_display(scr_width, scr_height), 
    m_pfield(pf_width, pf_height) {
    // no code here, unless you want to do something important
}


int Tetris::rotate(int target_x, int target_y, int rotation) {
    switch (rotation % 4) {
        /** 0 DEGREES:       0  1  2  3
                             4  5  6  7
                                8  9 10 11 
                            12 13 14 15 */
        case 0: return (target_y * PIECE_HEIGHT) + target_x;
        /** 90 DEGREES:     12  8  4  0
                            13  9  5  1
                            14 10  6  2 
                            15 11  7  3 */
        case 1: return (PIECE_AREA - PIECE_WIDTH) + target_y - (target_x * PIECE_WIDTH);
        /** 180 DEGREES:    15 14 13 12
                            11 10  9  8
                                7  6  5  4
                                3  2  1  0 */
        case 2: return (PIECE_AREA - 1) - (target_y * PIECE_WIDTH) - target_x;
        /** 270 DEGREES:    3  7 11 15
                            2  6 10 14
                            1  5  9 13
                            0  4  8 12 */
        case 3: return (PIECE_WIDTH - 1) - target_y + (target_x * PIECE_WIDTH);
    }
    return 0;
}
