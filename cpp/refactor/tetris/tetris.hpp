#pragma once
#include "include/common.hpp"
#include "include/display.hpp"
#include "include/playingfield.hpp"

#include <array>
#include <thread>

class Tetris {
private:
    // We should have exactly 7 tetrominos and 4 possible keys to press.
    static constexpr int PIECE_COUNT = 7, KEYS_COUNT = 4;

    // `pc` is short for `piece`, as in Tetromino piece.
    static constexpr int PIECE_WIDTH = 4, PIECE_HEIGHT = 4;

    // Needed for the 90 degree and 180 degree angle calculations.
    static constexpr int PIECE_AREA = PIECE_WIDTH * PIECE_HEIGHT;

    Display m_display; // initialize this one first
    PlayingField m_pfield; // initialize after `m_display`

    // Copies so we don't need to call getters.
    // `scr` short for `display screen`, 
    // `pf` short for `playing field`.
    const int m_scr_width, m_scr_height, m_pf_width, m_pf_height;
    
    // List of valid tetris pieces, from indexes 0-6. 
    // Each tetronimo is a 4x4 block but stored as a 1D array of `wchar_t`.
    const std::array<std::wstring, PIECE_COUNT> m_tetrominos = {
        L"..X...X...X...X.", // [0] 4x1: vertical line
        L"..X..XX...X.....", // [1] 3+1: T shape
        L".....XX..XX.....", // [2] 2x2: square
        L"..X..XX..X......", // [3] 2+2: Z shape
        L".X...XX...X.....", // [4] 2+2: S shape
        L".X...X...XX.....", // [5] 3+1: L shape
        L"..X...X..XX....."  // [6] 3+1: Mirror L
    };

    /**
     * Get the correct index into a piece based on its rotation.
     * @param target_x `piece_x`, desired piece's x-axis cell, from the caller loop.
     * @param target_y `piece_y`, desired piece's y-axis cell, from the caller loop.
     * @param rotation Value of rotation. We modulo by 4 internally.
     */
    int rotate(int target_x, int target_y, int rotation);

    /**
     * "Transform" a tetromino array to be used in the field array.
     * @param tetromino_id ID to be used as index into global `tetromino` array.
     * @param rotation Current rotation of the player's piece.
     * @param field_x Current playing position's x-axis (column#).
     * @param field_y Current playing position's y-axis (row#).
     */
    bool piece_fits(int tetromino_id, int rotation, int field_x, int field_y);
public:
    /**
     * Instantiates the members `m_display (Display)` and `m_pfield (PlayingField)`.
     * @warning Please pass 0 arguments or all 4, nothing in between,
     * unless you know what you are doing.
     * Dire things may happen if you pass some but others use the defaults.
     * @note `scr` short for `display screen`.
     * @note `pf` short for `playing field`.
     * @note Default values should be left at declaration.
     */
    Tetris(int scr_width = 80, int scr_height = 30, int pf_width = 12, int pf_height = 18);

    // Just to get a feel for the variables before I refactor things.
    void game_loop();

    /**
     * Draw the current state of the playing field.
     * @note Pokes at `m_pfield` and `m_display`, so need in the `Tetris` scope.
     */ 
    void draw_field();
};

/*******************************************************************************
******************************** IMPLEMENTATION ********************************
*******************************************************************************/

Tetris::Tetris(int scr_width, int scr_height, int pf_width, int pf_height) 
:   m_display(scr_width, scr_height), 
    m_pfield(pf_width, pf_height),
    m_scr_width(scr_width),
    m_scr_height(scr_height),
    m_pf_width(pf_width),
    m_pf_height(pf_height)
    {
    // no code here, unless you want to do something important
}

void Tetris::game_loop() {
    bool is_gameover = false;

    // TODO: Refactor player state and keyboard inputs.
    int player_piece = 0;
    int player_rotation = 0;
    int player_x = m_pf_width / 2; // starts at the middle
    int player_y = 0; // starts at the top

    std::array<bool, KEYS_COUNT> keys_pressed;

    // The 4 stages of most game loops
    while (!is_gameover) {
        /*************************** GAME TIMING ******************************/
        // milliseconds suffix for literals only available with C++14 and above.
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        /*************************** GAME INPUT ******************************/
        // "Listen" for input via the Windows virtual key codes.
        // TODO: Refactor player state and keyboard inputs.
        for (int i = 0; i < KEYS_COUNT; i++) {                  //R   L   D Z
            SHORT key_state = GetAsyncKeyState((unsigned char)("\x27\x25\x28Z"[i]));
            keys_pressed.at(i) = (key_state & 0x8000) != 0;
        }

        // Right Arrow Key
        if (keys_pressed.at(0)) {
            if (piece_fits(player_piece, player_rotation, player_x + 1, player_y)) {
                player_x++;
            }
        }
        // Left Arrow Key
        if (keys_pressed.at(1)) {
            if (piece_fits(player_piece, player_rotation, player_x - 1, player_y)) {
                player_x--;
            }
        }
        // Down Arrow Key
        // Z Key (Rotation)

        /*************************** GAME LOGIC *******************************/
        // Shapes falling collision detection and scoring


        /************************** RENDER OUTPUT *****************************/
        // Based on our `m_pfield` buffer, update the `m_display` buffer.
        draw_field();

        // Current piece isn't yet part of the field, so draw it separately.
        // TODO: Refactor player state and keyboard inputs.
        for (int piece_x = 0; piece_x < PIECE_WIDTH; piece_x++) {
            for (int piece_y = 0; piece_y < PIECE_HEIGHT; piece_y++) {
                int piece_index = rotate(piece_x, piece_y, player_rotation);
                auto letter = m_tetrominos.at(player_piece).at(piece_index);

                // Don't write parts of the tetromino reference that aren't occupied
                if (letter != L'X') {
                    continue;
                }

                // Offset by current position and 2.
                int screen_index = (player_y + piece_y + 2) * m_scr_width + (player_x + piece_x + 2);

                // Update our output console buffer.
                // player_piece + 'A' just gets the capital letter representation,
                // since each tetromino is represented by one particular letter.
                m_display.set_screen_at(screen_index, player_piece + 'A');
            }
        }
        // Actually write the display frame to the console output.
        m_display.render();
    }
}

void Tetris::draw_field() {
    // Loop through each cell in our playing field
    for (int x = 0; x < m_pf_width; x++) {
        for (int y = 0; y < m_pf_height; y++) {
            // Index into the display screen to be updated.
            int screen_index = (y + 2) * m_scr_width + (x + 2);

            // What character the field has at this spot,
            // to be used as an index into the literal `" ABCDEFG=#"`.
            auto tile = m_pfield.get_field_at((y * m_pf_width) + x);

            /**
             * Update the main display output
             * [0]      = L' ':         empty space, 
             * [1...7]: = L"ABCDEFG":   each letter represents a tetris piece.
             * [8]      = L'=':         completed lines/tetris.
             * [9]:     = L'#':         border/wall.
             */
            m_display.set_screen_at(screen_index, L" ABCDEFG=#"[tile]);
        }
    }
}

bool Tetris::piece_fits(int tetromino_id, int rotation, int field_x, int field_y) {
    // Check each cell of the reference tetromino piece (based on given ID).
    for (int piece_x = 0; piece_x < PIECE_WIDTH; piece_x++) {
        for (int piece_y = 0; piece_y < PIECE_HEIGHT; piece_y++) {
            // Get correct index into the reference tetromino
            int piece_index = rotate(piece_x, piece_y, rotation);

            // Spot in playing field where we want to check for collisions.
            int field_index = (field_y + piece_y) * m_pf_width + (field_x + piece_x);

            // If out of bounds, ignore. Else, still need to check collisions.
            if (!m_pfield.is_in_bounds(field_x + piece_x, field_y + piece_y)) {
                continue;
            }

            // @note Reference tetrominos are mode of `L'X'` and `L'.'` only.
            auto piece_cell = m_tetrominos.at(tetromino_id).at(piece_index);
            /** 
             * Recall what characters the playing field is made of:
             * ```cpp
             * L" ABCDEFG=#"[0] = L' ';
             * ```
            */
            auto field_cell = m_pfield.get_field_at(field_index);

            // 0 indicates an empty space in the field. 
            if (piece_cell != L'.' && field_cell != 0) {
                // Collided with/hit something! We can't fit here.
                return false;
            }
        }
    }
    return true;
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
