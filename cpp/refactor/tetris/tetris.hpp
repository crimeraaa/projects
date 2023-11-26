#pragma once
#include "include/common.hpp"
#include "include/display.hpp"
#include "include/playingfield.hpp"
#include "include/player.hpp"

class Tetris {
public: // COMPILE-TIME CONSTANTS
    // Compile-time information about tetrominos.
    enum PieceInfo {
        PIECE_COUNT = 7,
        PIECE_WIDTH = 4,
        PIECE_HEIGHT = 4,
        PIECE_AREA = PIECE_WIDTH * PIECE_HEIGHT
    };

    // Default settings for constructor.
    enum Defaults {
        DEFAULT_SCREEN_WIDTH = 80,
        DEFAULT_SCREEN_HEIGHT = 30,
        DEFAULT_PFIELD_WIDTH = 12,
        DEFAULT_PFIELD_HEIGHT = 18
    };

    // Offsets for use in the `key_held` functions.
    enum Offsets {
        OFFSET_NONE,
        OFFSET_UP,
        OFFSET_DOWN,
        OFFSET_LEFT,
        OFFSET_RIGHT,
        OFFSET_ROTATE,
        OFFSET_COUNT
    };

private: // MEMBER VARIABLES
    Display m_display; // @note 1st in initializer list, before `this->m_pfield`. 
    PlayingField m_pfield; // @note 2nd in initializer list, after `this->m_display`. 
    Player m_player; // @note 3rd in initializer list, after `this->m_pfield`,

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

// If have default values, leave them at declaration.
public: // CONSTRUCTORS

    // @warning Be sure you have your dimensions right! Hsd no error handling.
    Tetris(
        size_t screen_width, 
        size_t screen_height, 
        size_t pfield_width, 
        size_t pfield_height
    );

    /**
     * Constructor delegation is a C++11/C++0x thing. Very nice!
     * This constructor results in:
     * ```cpp
     * // Console screen buffer dimensions.
     * this->m_display.width = 80; 
     * this->m_display.height = 30;
     * 
     * // Playing Field buffer dimensions.
     * this->m_pfield.width = 12;
     * this->m_pfield.height = 18;
     * ```
     */
    Tetris() : Tetris(
        DEFAULT_SCREEN_WIDTH, 
        DEFAULT_SCREEN_HEIGHT, 
        DEFAULT_PFIELD_WIDTH, 
        DEFAULT_PFIELD_HEIGHT
    ) { /* so far nothing to write here lol */ }

public: // METHODS
    // Just to get a feel for the variables before I refactor things.
    void game_loop();

    // "Listens" for input via the Windows virtual key codes. See MSDN.
    void input();

    // Updates console screen buffer and playing field buffer,
    // then writes to the active console window.
    void render();

private: // INPUT HELPERS
    /**
     * @brief Get the correct index into a piece based on its rotation.
     * 
     * @param tx Target piece's x-axis cell, usually called `px` in caller loop.
     * @param ty Target piece's y-axis cell, usually called `py` in caller loop.
     * 
     * @param rotation Value of rotation. We modulo by 4 internally.
     */
    size_t rotate(size_t tx, size_t ty, int rotation);

    /**
     * "Transform" a tetromino array to be used in the field array.
     * 
     * @note Checks each cell of tetromino piece, based on given id.
     */
    bool piece_fits(size_t id, int rotation, size_t fx, size_t fy);

    /**
     * Wrapper for a very verbose call.
     * @note See `tetris.hpp` and `include/player.hpp` for help.
     */
    bool key_is_held(enum Player::Keys vkey_id, enum Offsets code) {
        // Determine which offset to use based on `code`.
        int offset_x = 0, offset_y = 0, offset_rotation = 0;
        switch (code) {
            // Y-axis grows downwards, to go up we need to offset by -1.
            // case OFFSET_UP:     offset_y = -1; break;
            // Likewise, to go down we need to offset by 1.
            case OFFSET_DOWN:   offset_y = 1; break;
            case OFFSET_LEFT:   offset_x = -1; break;
            case OFFSET_RIGHT:  offset_x = 1; break;
            // In tetris you can only rotate in 1 direction.
            case OFFSET_ROTATE: offset_rotation = 1; break;
            default: break;
        }

        return m_player.is_pressing(vkey_id) && piece_fits(
            m_player.piece_id,
            m_player.rotation + offset_rotation,
            m_player.position.x + offset_x,
            m_player.position.y + offset_y
        );
    }

private: // RENDER OUTPUT HELPERS
    // Draw the current state of the playing field.
    void draw_field();

    // Current piece is separate from the field so draw it separately. 
    void draw_piece();

};

/*******************************************************************************
******************************** IMPLEMENTATION ********************************
*******************************************************************************/

Tetris::Tetris(
    size_t screen_width, 
    size_t screen_height, 
    size_t pfield_width, 
    size_t pfield_height
) 
:   m_display(screen_width, screen_height),
    m_pfield(pfield_width, pfield_height),
    m_player(pfield_width)
{
    // no code here, unless you want to do something important
}

void Tetris::game_loop() {
    bool is_gameover = false;

    // The 4 stages of most game loops
    while (!is_gameover) {
        /*************************** GAME TIMING ******************************/
        // milliseconds suffix for literals only available with C++14 and above.
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    
        /*************************** GAME INPUT ******************************/
        input();

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
    for (size_t i = 0; i < Player::KEYS_COUNT; i++) {
        m_player.controls[i].update();
    }

    // ? Use booleans to our advantage: true = 1, false = 0, so we can use that!
    m_player.position.x += key_is_held(Player::KEY_RIGHT, OFFSET_RIGHT);
    m_player.position.x -= key_is_held(Player::KEY_LEFT, OFFSET_LEFT);
    m_player.position.y += key_is_held(Player::KEY_DOWN, OFFSET_DOWN);

    // If held consecutively between calls, `m_player.hold == false` still.
    // This enforces the "staggering" behaviour in the compound assignment.
    if (key_is_held(Player::KEY_Z, OFFSET_ROTATE)) {
        m_player.rotation += m_player.hold;
        m_player.hold = false;
    }
    else {
        // Reset the moment we detect 'Player::KEY_Z' has been let go.
        m_player.hold = true;
    }
}

bool Tetris::piece_fits(size_t id, int rotation, size_t fx, size_t fy) {
    // reference tetromino
    const auto &piece = m_tetrominos[id];

    for (size_t px = 0; px < PIECE_WIDTH; px++) {
        for (size_t py = 0; py < PIECE_HEIGHT; py++) {
            // Correct index into the reference tetromino
            size_t piece_index = rotate(px, py, rotation);

            // Spot in playing field where we want to check for collisions.
            size_t field_index = (fy + py) * m_pfield.width + (fx + px);

            // If test is out of bounds, ignore. Else, still need to check collisions.
            if (!m_pfield.is_in_bounds(fx + px, fy + py)) {
                continue;
            }
            // Consider 'X' as part of the piece in the tetromino grid.
            bool is_piece = (piece[piece_index] != L'.');

            // Playing field: Value 0 represents whitespace character `' '`.
            bool is_occupied = (m_pfield[field_index] != 0);

            // We're on a part of the piece and it collided with something!
            if (is_piece && is_occupied) {
                return false;
            }
        }
    }
    return true;
}

size_t Tetris::rotate(size_t tx, size_t ty, int rotation) {
    switch (rotation % 4) {
        /** 0 DEGREES:       0  1  2  3
                             4  5  6  7
                             8  9 10 11 
                            12 13 14 15 */
        case 0: return (ty * PIECE_WIDTH) + tx;

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
    // Loop through each cell in our playing field.
    for (size_t fx = 0; fx < m_pfield.width; fx++) {
        for (size_t fy = 0; fy < m_pfield.height; fy++) {
            // Index into the display screen to be updated.
            size_t screen_index = (fy + 2) * m_display.width + (fx + 2);

            // Value received is an index into `" ABCDEFG=#"`, which is used
            // when we update the screen buffer.
            auto tile = m_pfield[(fy * m_pfield.width) + fx];

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
    // Should we use locals to access member variables when in a `for` loop? 
    // See: https://stackoverflow.com/a/238568

    // Local so I can work with shorter names.
    // Effectively our field indexes. Still need to get appropriate offsets.
    const size_t fx = m_player.position.x, fy = m_player.position.y;

    const auto &tetromino = m_tetrominos[m_player.piece_id];

    for (size_t px = 0; px < PIECE_WIDTH; px++) {
        for (size_t py = 0; py < PIECE_HEIGHT; py++) {
            // Don't update the display screen with `'.'` characters.
            size_t tetromino_index = rotate(px, py, m_player.rotation);
            if (tetromino[tetromino_index] != L'X') {
                continue;
            }
            // offset by 2 so we don't end up writing to the corners
            size_t screen_index = (fy + py + 2) * m_display.width + (fx + px + 2);

            // piece_id + 'A' gets a capital letter, should be from 'A' to 'G'.
            m_display[screen_index] = m_player.piece_id + 'A';
        }
    }
}
