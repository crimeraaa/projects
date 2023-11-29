#pragma once
#include "include/common.hpp"
#include "include/consolewindow.hpp"
#include "include/playingfield.hpp"
#include "include/player.hpp"
#include "include/gamestate.hpp"

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
        DEFAULT_PFIELD_HEIGHT = 18,
        DEFAULT_STARTING_SPEED = 20
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

// INTERNAL MEMBER VARIABLES
private: 
    ConsoleWindow m_display; // Holds our display output, not playing status/world.
    PlayingField m_pfield; // Holds what pieces populate the game world.
    Player m_player; // Player controls and status.
    GameState m_gamestate; // Game over, speed and force piece down statuses.

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

// CONSTRUCTORS
// If have default values, leave them at declaration.
public: 
    // @warning Be sure you have your dimensions right! Has no error handling.
    Tetris(
        size_t screen_width,
        size_t screen_height,
        size_t pfield_width,
        size_t pfield_height,
        size_t starting_speed
    )
    : m_display{screen_width, screen_height}
    , m_pfield{pfield_width, pfield_height}
    , m_player{pfield_width}
    , m_gamestate{starting_speed} {
        struct timespec sometime;
        clock_gettime(CLOCK_MONOTONIC, &sometime);
        std::srand(sometime.tv_nsec);
    }
    // clang-format off
    /**
     * Constructor delegation is a C++11/C++0x thing. Very nice!
     * This constructor results in:
     * ```cpp
     * this->m_display.width = 80, this->m_display.height = 30;
     * this->m_pfield.width = 12,  this->m_pfield.height = 18;
     * this->m_gamestate.current_speed = 20;
     * ```
     */
    Tetris() : Tetris(
        DEFAULT_SCREEN_WIDTH,
        DEFAULT_SCREEN_HEIGHT,
        DEFAULT_PFIELD_WIDTH,
        DEFAULT_PFIELD_HEIGHT,
        DEFAULT_STARTING_SPEED
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
    const bool &is_gameover() {
        return m_gamestate.game_over;
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

// UPDATE HELPERS
private:
    // Takes the player's current piece and turns it into an obstacle.
    void lock_piece();

    /**
     * Checks for the completion of full horizontal lines.
     * We can afford to check the last tetromino piece, 
     * it's unnecessary to check the whole field due to Tetris's nature!
     */
    void check_for_lines();

    /**
     * Remove completed lines from the playing field and display screen,
     * then move non-completed lines 1 tile down.
     */
    void try_clear_lines();

// RENDER OUTPUT HELPERS
private: 
    // Draw the current state of the playing field.
    void draw_field();

    // Current piece is separate from the field so draw it separately.
    void draw_piece();
};
