#pragma once
#include "include/common.hpp"
#include "include/display.hpp"
#include "include/playingfield.hpp"
#include "include/player.hpp"

#include <array>
#include <thread>

class Tetris {
private: // COMPILE-TIME CONSTANTS

    // We should have exactly 7 tetrominos and 4 possible keys to press.
    static constexpr int PIECE_COUNT = 7, KEYS_COUNT = 4;

    // `pc` is short for `piece`, as in Tetromino piece.
    // Although the arrays are 1D, we treat them as if they were squares.
    static constexpr int PIECE_WIDTH = 4, PIECE_HEIGHT = 4;

    // Needed for the 90 degree and 180 degree rotation calculations.
    static constexpr int PIECE_AREA = PIECE_WIDTH * PIECE_HEIGHT;

private: // MEMBER VARIABLES
    /** 
     * Direct instance (i.e, not a pointer!).
     * Use this to handle writing to the console screen buffer.
     * 
     * @note Goes first in the initializer list, before `this->m_pfield`. 
    */
    Display m_display; 

    /**
     * Direct instance (i.e, not a pointer!). Used as a sort of game world buffer.
     * 
     * @note Goes second in the initializer list, after `this->m_display`. 
    */
    PlayingField m_pfield; 
    
    /**
     * Direct instance (i.e, not a pointer!). Used as a sort of game world buffer.
     * 
     * @note Goes third in the initializer list, after `this->m_pfield`,
     * especially since it relies on playing field width.
    */
    TetrisPlayer m_player;

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

public: // CONSTRUCTOR
    /**
     * Instantiates the members `m_display (Display)` and `m_pfield (PlayingField)`.
     * 
     * @warning Please pass 0 arguments or all 4, nothing in between,
     * unless you know what you are doing.
     * Dire things may happen if you pass some but others use the defaults.
     * 
     * @note `scr` short for `display screen`.
     * @note `pf` short for `playing field`.
     * @note Default values should be left at declaration.
     */
    Tetris(int scr_width = 80, int scr_height = 30, int pf_width = 12, int pf_height = 18);

public: // METHODS
    // Just to get a feel for the variables before I refactor things.
    void game_loop();

    /**
     * "Listens" for input via the Windows virtual key codes, and handles
     * successive calls to `m_player.is_pressing` and `piece_fits`.
     * 
     * @note This updates the player's tetromino position on the playing field.
     */
    void input();

    /**
     * Updates console screen buffer and playing field buffer,
     * then writes the output to the active console window.
     * 
     * @note Calls the private member functions `draw_field` and `draw_piece`.
     */
    void render();

private: // GENERAL TETRIS HELPER METHODS
    /**
     * Get the correct index into a piece based on its rotation.
     * 
     * @param tx Desired (reference) piece's x-axis cell, usually called `px`.
     * @param ty Desired (reference) piece's y-axis cell, usually called `py`.
     * 
     * @param rotation Value of rotation. We modulo by 4 internally.
     */
    int rotate(int tx, int ty, int rotation);

    /**
     * "Transform" a tetromino array to be used in the field array.
     * We Check each cell of the reference tetromino piece (based on given ID).
     * 
     * @param offset_x Offset into the player's target x-axis index (column#)
     * @param offset_y Offset into the player's target y-axis index (row#)
     * @param or Offset into the player's rotation value.
     * 
     * @note You can call with no arguments to just check the current position.
     */
    bool piece_fits(int offset_x = 0, int offset_y = 0, int offset_rotation = 0);

private: // RENDER OUTPUT HELPER METHODS
    /**
     * Draw the current state of the playing field, and just the field.
     * 
     * @note Pokes at `m_pfield` and `m_display`, so need in the `Tetris` scope.
     */ 
    void draw_field();

    /**
     * Current piece is separate from the field, so draw it separately. 
     * 
     * @note Pokes at `m_player` and `m_display`, so need in the `Tetris` scope.
     */
    void draw_piece();
};

/*******************************************************************************
******************************** IMPLEMENTATION ********************************
*******************************************************************************/

Tetris::Tetris(int scr_width, int scr_height, int pf_width, int pf_height) 
:   m_display(scr_width, scr_height), 
    m_pfield(pf_width, pf_height),
    m_player(pf_width)
{
    // no code here, unless you want to do something important
}

void Tetris::game_loop() {
    bool is_gameover = false;
    bool hold_rotate = false;

    // The 4 stages of most game loops
    while (!is_gameover) {
        /*************************** GAME TIMING ******************************/
        // milliseconds suffix for literals only available with C++14 and above.
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
        /*************************** GAME INPUT ******************************/
        input();

        // TODO make `hold_rotate` more internal.
        // Separate from the `input` function as need `hold_rotate` to exist
        // between calls.
        if (m_player.is_pressing(m_player.KEY_Z)) {
            // true = 1, false = 0, so we can use them as integers too
            m_player.rotation() += hold_rotate && piece_fits(0, 0, 1);
            hold_rotate = false;
        }
        else {
            hold_rotate = true;
        }

        /*************************** GAME LOGIC *******************************/
        // Shapes falling collision detection and scoring


        /************************** RENDER OUTPUT *****************************/
        render();
    }
}

/*******************************************************************************
***************************** GAME INPUT AND LOGIC *****************************
*******************************************************************************/

void Tetris::input() {
    // Asynchronously updates the list of pressed keys.
    m_player.input();

    // ? Use booleans to our advantage: true = 1, false = 0, so we can use that!
    // `offset_x = 1`: Only check the cell to our right.
    m_player.px() += m_player.is_pressing(m_player.KEY_RIGHT) && piece_fits(1);

    // `offset_x = -1`: Only check the cell to our left.
    m_player.px() -= m_player.is_pressing(m_player.KEY_LEFT) && piece_fits(-1);

    // `offset_x = 0, offset_y = 1`: Only check the cells below us.
    // Use use positive 1 since topleft is 0,0, meaning y-axis grows down.
    m_player.py() += m_player.is_pressing(m_player.KEY_DOWN) && piece_fits(0, 1);
}

/*******************************************************************************
******************************** RENDER OUTPUT *********************************
*******************************************************************************/

void Tetris::render() {
    // Update the `m_display` buffer. based on our `m_pfield` buffer, 
    draw_field();

    // Update the `m_pfield` buffer with the player's current tetromino.
    draw_piece();

    // Actually write the display frame to the console output.
    m_display.render();
}

void Tetris::draw_field() {
    // `pf` is short for Playing Field.
    const int pf_width = m_pfield.get_width(), pf_height = m_pfield.get_height();

    // `scr` is short for Display Screen.
    const int scr_width = m_display.get_width();

    // Loop through each cell in our playing field.
    for (int fx = 0; fx < pf_width; fx++) {
        for (int fy = 0; fy < pf_height; fy++) {
            // Index into the display screen to be updated.
            int screen_index = (fy + 2) * scr_width + (fx + 2);

            // What number the field has at this spot,
            // to be used as an index into the literal `" ABCDEFG=#"`
            // when we update the screen buffer.
            auto tile = m_pfield[(fy * pf_width) + fx];

            /**
             * Update the main display output
             * [0]      = L' ':         empty space, 
             * [1...7]: = L"ABCDEFG":   each letter represents a tetris piece.
             * [8]      = L'=':         completed lines/tetris.
             * [9]:     = L'#':         border/wall.
             */
            m_display[screen_index] = L" ABCDEFG=#"[tile];
        }
    }
}

void Tetris::draw_piece() {
    // 
    const int &rotation = m_player.rotation(), &piece_id = m_player.piece_id();

    // Effectively our field indexes, but still need to get the correct offsets.
    const int &fx = m_player.px(), &fy = m_player.py();

    // Screen width as we call it multiple times.
    const int &screen_width = m_display.get_width();

    // 4x4 tetromino grid in question.
    const auto &tetromino = m_tetrominos[piece_id];

    for (int px = 0; px < PIECE_WIDTH; px++) {
        for (int py = 0; py < PIECE_HEIGHT; py++) {
            int cell = rotate(px, py, rotation);
            auto letter = tetromino[cell];

            // Don't write parts of the tetromino reference that aren't occupied
            if (letter != L'X') {
                continue;
            }

            // Offset by current position and 2.
            int scr_index = (fy + py + 2) * screen_width + (fx + px + 2);

            // Update our output console buffer.
            // player_piece + 'A' just gets the capital letter representation,
            // since each tetromino is represented by one particular letter.
            m_display[scr_index] = piece_id + 'A';
        }
    }
}

/*******************************************************************************
********************************* GAME HELPERS *********************************
*******************************************************************************/

bool Tetris::piece_fits(int offset_x, int offset_y, int offset_rotation) {
    const int fx = offset_x + m_player.px(), fy = offset_y + m_player.py();
    const auto &piece = m_tetrominos[m_player.piece_id()];
    const int &rotation = offset_rotation + m_player.rotation();

    int pf_width = m_pfield.get_width();
    for (int px = 0; px < PIECE_WIDTH; px++) {
        for (int py = 0; py < PIECE_HEIGHT; py++) {
            // Get correct index into the reference tetromino
            int piece_index = rotate(px, py, rotation);

            // Spot in playing field where we want to check for collisions.
            int field_index = (fy + py) * pf_width + (fx + px);

            // If out of bounds, ignore. Else, still need to check collisions.
            if (!m_pfield.is_in_bounds(fx + px, fy + py)) {
                continue;
            }

            /** 
             * Accounting for rotation, check if this index is filled in.
             * This represents the part of the Tetromino piece.
             * 
             * @note Reference tetrominos are made of `L'X'` and `L'.'` only.
            */
            bool is_piece = (piece[piece_index] != L'.');

            /** 
             * Checks if this field tile is being occupied.
             * 
             * @note Refer to: `L" ABCDEFG=#"[0] = L' ';`
             * @note Meaning 0 is our empty space for the field.
            */
            bool is_occupied = (m_pfield[field_index] != 0);

            // We're on a part of the piece and it collided with something!
            if (is_piece && is_occupied) {
                return false;
            }
        }
    }
    return true;
}

int Tetris::rotate(int tx, int ty, int rotation) {
    switch (rotation % 4) {
        /** 0 DEGREES:       0  1  2  3
                             4  5  6  7
                             8  9 10 11 
                            12 13 14 15 */
        case 0: return (ty * PIECE_HEIGHT) + tx;

        /** 90 DEGREES:     12  8  4  0
                            13  9  5  1
                            14 10  6  2 
                            15 11  7  3 */
        case 1: return (PIECE_AREA - PIECE_WIDTH) + ty - (tx * PIECE_WIDTH);

        /** 180 DEGREES:    15 14 13 12
                            11 10  9  8
                            7  6  5  4
                            3  2  1  0 */
        case 2: return (PIECE_AREA - 1) - (ty * PIECE_WIDTH) - tx;
        
        /** 270 DEGREES:    3  7 11 15
                            2  6 10 14
                            1  5  9 13
                            0  4  8 12 */
        case 3: return (PIECE_WIDTH - 1) - ty + (tx * PIECE_WIDTH);
    }
    return 0;
}
