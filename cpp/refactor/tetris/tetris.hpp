#pragma once
#include "include/common.hpp"
#include "include/display.hpp"
#include "include/playingfield.hpp"
#include "include/player.hpp"


class Tetris {
private: // COMPILE-TIME CONSTANTS
    // Compile-time information about tetrominos
    enum TetrominoInformatino {
        PIECE_COUNT = 7,
        PIECE_WIDTH = 4,
        PIECE_HEIGHT = 4,
        PIECE_AREA = PIECE_WIDTH * PIECE_HEIGHT
    };

private: // MEMBER VARIABLES
    Display m_display; // @note 1st in initializer list, before `this->m_pfield`. 
    PlayingField m_pfield; // @note 2nd in initializer list, after `this->m_display`. 
    TetrisPlayer m_player; // @note 3rd in initializer list, after `this->m_pfield`,

    // List of valid tetris pieces, from indexes 0-6. 
    // Each tetronimo is a 4x4 block but stored as a 1D array of `wchar_t`.
    const std::array<std::wstring, PIECE_COUNT> m_tetrominos = {
        L"..X...X...X...X.", // 4x1: vertical line
        L"..X..XX...X.....", // 3+1: T shape
        L".....XX..XX.....", // 2x2: square
        L"..X..XX..X......", // 2+2: Z shape
        L".X...XX...X.....", // 2+2: S shape
        L".X...X...XX.....", // 3+1: L shape
        L"..X...X..XX....."  // 3+1: Mirror L
    };

public: // CONSTRUCTOR
    /**
     * @warning Please pass 0 arguments or all 4, nothing in between,
     * unless you know what you are doing.
     * Dire things may happen if you pass some but others use the defaults.
     * 
     * @note Default values should be left at declaration.
     */
    Tetris(int scr_width = 80, int scr_height = 30, int pf_width = 12, int pf_height = 18);

public: // METHODS
    // Just to get a feel for the variables before I refactor things.
    void game_loop();

    // "Listens" for input via the Windows virtual key codes.
    void input();

    // Updates console screen buffer and playing field buffer then writes
    // to the active console window.
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
     * We check each cell of the reference tetromino piece (based on given ID).
     * 
     * @param offset_x Offset into the player's target x-axis index (column#)
     * @param offset_y Offset into the player's target y-axis index (row#)
     * @param or Offset into the player's rotation value.
     * 
     * @note You can call with no arguments to just check the current position.
     */
    bool piece_fits(int offset_x = 0, int offset_y = 0, int offset_rotation = 0);

private: // RENDER OUTPUT HELPER METHODS
    // Draw the current state of the playing field.
    void draw_field();

    // Current piece is separate from the field so draw it separately. 
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
            m_player.rotation += hold_rotate && piece_fits(0, 0, 1);
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
    for (int i = 0; i < m_player.KEYS_COUNT; i++) {
        m_player.controls[i].update();
    }

    // ? Use booleans to our advantage: true = 1, false = 0, so we can use that!
    // `offset_x = 1`: Only check the cell to our right.
    if (m_player.is_pressing(m_player.KEY_RIGHT) && piece_fits(1)) {
        m_player.position.x++;
    }

    // `offset_x = -1`: Only check the cell to our left.
    if (m_player.is_pressing(m_player.KEY_LEFT) && piece_fits(-1)) {
        m_player.position.x--;
    }

    // `offset_x = 0, offset_y = 1`: Only check the cells below us.
    if (m_player.is_pressing(m_player.KEY_DOWN) && piece_fits(0, 1)) {
        // Use use positive 1 since topleft is 0,0, meaning y-axis grows down.
        m_player.position.y++;
    }
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
    // Playing field dimensions.
    const int pf_width = m_pfield.get_width(), pf_height = m_pfield.get_height();

    // Loop through each cell in our playing field.
    for (int fx = 0; fx < pf_width; fx++) {
        for (int fy = 0; fy < pf_height; fy++) {
            // Index into the display screen to be updated.
            int screen_index = (fy + 2) * m_display.width + (fx + 2);

            // Value received is an index into `" ABCDEFG=#"`, which is used
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
    // Effectively our field indexes. Still need to get the correct offsets.
    const int fx = m_player.position.x, fy = m_player.position.y;

    const int piece_id = m_player.piece_id; // index into tetromino array

    const auto &tetromino = m_tetrominos[piece_id];

    for (int px = 0; px < PIECE_WIDTH; px++) {
        for (int py = 0; py < PIECE_HEIGHT; py++) {
            int cell = rotate(px, py, m_player.rotation);
            // We only wanna update the display screen with tetris pieces
            if (tetromino[cell] != L'X') {
                continue;
            }
            // offset by 2 so we don't end up writing to the corners
            int scr_index = (fy + py + 2) * m_display.width + (fx + px + 2);
            // player_piece + 'A' just gets the capital letter representation.
            m_display[scr_index] = piece_id + 'A';
        }
    }
}

/*******************************************************************************
********************************* GAME HELPERS *********************************
*******************************************************************************/

bool Tetris::piece_fits(int offset_x, int offset_y, int offset_rotation) {
    // playing field indexes
    const int fx = offset_x + m_player.position.x, fy = offset_y + m_player.position.y;

    // reference tetromino
    const auto &piece = m_tetrominos[m_player.piece_id];
    const int &rotation = offset_rotation + m_player.rotation;

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
            // we consider 'X' as part of the piece in the tetromino grid
            bool is_piece = (piece[piece_index] != L'.');
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
