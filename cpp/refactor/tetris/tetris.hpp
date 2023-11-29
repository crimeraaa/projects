#pragma once
#include "include/common.hpp"
#include "include/consolewindow.hpp"
#include "include/playingfield.hpp"
#include "include/player.hpp"

class Tetris {
// COMPILE-TIME CONSTANTS
public: 
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

// MEMBER VARIABLES
private: 
    ConsoleWindow m_display; // @note 1st in initializer list, before `this->m_pfield`.
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
        L"..X...X..XX....." // 3+1: Mirror L
    };

    bool m_gameover;
    int m_speed; // slowly decreases; interval between ticks gets faster!
    int m_speedcount;
    bool m_forcedown; // when `m_speedcount == m_speed`, put pressure on the player.

// CONSTRUCTORS
// If have default values, leave them at declaration.
public: 
    // @warning Be sure you have your dimensions right! Has no error handling.
    Tetris(
        size_t screen_width,
        size_t screen_height,
        size_t pfield_width,
        size_t pfield_height
    )
        : m_display(screen_width, screen_height)
        , m_pfield(pfield_width, pfield_height)
        , m_player(pfield_width)
        , m_gameover(false)
        , m_speed(20)
        , m_speedcount(0)
        , m_forcedown(false) {}
    // clang-format off
    /**
     * Constructor delegation is a C++11/C++0x thing. Very nice!
     * This constructor results in:
     * ```cpp
     * this->m_display.width = 80, this->m_display.height = 30;
     * this->m_pfield.width = 12,  this->m_pfield.height = 18;
     * ```
     */
    Tetris() : Tetris(
        DEFAULT_SCREEN_WIDTH,
        DEFAULT_SCREEN_HEIGHT,
        DEFAULT_PFIELD_WIDTH,
        DEFAULT_PFIELD_HEIGHT
    ) {}
    // clang-format on

// METHODS
public: 
    // "Listens" for input via the Windows virtual key codes. See MSDN.
    void input();

    // Forces your piece down and makes the game faster over time.
    void update();

    // Updates console screen buffer and playing field buffer,
    // then writes to the active console window.
    void render();

    // Getter of `this->m_gameover` so user can't mess around with it.
    bool is_gameover() {
        return m_gameover;
    }

// INPUT HELPERS
private: 
    /**
     * @brief Get the correct index into a piece based on its rotation.
     *
     * @param tx Target piece's x-axis cell, usually called `px` in caller loop.
     * @param ty Target piece's y-axis cell, usually called `py` in caller loop.
     * @param rotation Value of rotation. We modulo by 4 internally.
     */
    size_t rotate(size_t tx, size_t ty, int rotation);

    // javidx9's original implementation.
    // "Transforms" a tetromino array to be used in the field array.
    bool piece_fits(size_t id, int rotation, size_t fx, size_t fy);

    // Overload to be called to just check the current piece based on a given offset.
    // @note After offsets are determined, just calls the above `piece_fits`.
    bool piece_fits(enum Offsets code);

    // Tests given key is currently held down and current piece fits
    // in the given offset area.
    bool validate_key(enum Player::Keys vkey_id, enum Offsets code);

    // Takes the player's current piece and turns it into an obstacle.
    void lock_piece();

// RENDER OUTPUT HELPERS
private: 
    // Draw the current state of the playing field.
    void draw_field();

    // Current piece is separate from the field so draw it separately.
    void draw_piece();
};
