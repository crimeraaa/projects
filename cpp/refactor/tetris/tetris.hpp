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
        L"..X...X..XX....." // 3+1: Mirror L
    };

    // If have default values, leave them at declaration.
    public: // CONSTRUCTORS
    // @warning Be sure you have your dimensions right! Has no error handling.
    Tetris(
        size_t screen_width,
        size_t screen_height,
        size_t pfield_width,
        size_t pfield_height
    )
        : m_display(screen_width, screen_height)
        , m_pfield(pfield_width, pfield_height)
        , m_player(pfield_width) {}
    /**
     * Constructor delegation is a C++11/C++0x thing. Very nice!
     * This constructor results in:
     * ```cpp
     * this->m_display.width = 80, this->m_display.height = 30;
     * this->m_pfield.width = 12,  this->m_pfield.height = 18;
     * ```
     */
    Tetris()
        : Tetris(
              DEFAULT_SCREEN_WIDTH,
              DEFAULT_SCREEN_HEIGHT,
              DEFAULT_PFIELD_WIDTH,
              DEFAULT_PFIELD_HEIGHT
          ) {}

    public: // METHODS
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

    // Tests given key is currently held down and fits in the given offset area.
    bool key_is_held(enum Player::Keys vkey_id, enum Offsets code);

    private: // RENDER OUTPUT HELPERS
    // Draw the current state of the playing field.
    void draw_field();

    // Current piece is separate from the field so draw it separately.
    void draw_piece();
};
