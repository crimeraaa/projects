#ifndef OLC_TETRIS_REFACTOR_HPP
#define OLC_TETRIS_REFACTOR_HPP

#include <cstdio>
#include <array>
#include <string>
#include <windows.h>

#define OLC_SCREEN_WIDTH 80
#define OLC_SCREEN_HEIGHT 30

#define OLC_FIELD_WIDTH 12
#define OLC_FIELD_HEIGHT 18

#define OLC_TETROMINO_COUNT 7

class Tetris {
public:
    // Handles our output buffers and Windows API calls.
    class Display;
    class PlayingField;
    class Player;
};

/**
 * Get the correct index into a piece based on its rotation.
 * @param piece_x `px`, piece's x-axis cell, from the caller loop.
 * @param piece_y `py`, piece's y-axis cell, from the caller loop.
 * @param rotation Value of rotation. We modulo by 4 internally.
 */
int rotate(int piece_x, int piece_y, int rotation);

/**
 * @param tetromino_id ID to be used as index into global `tetromino` array.
 * @param rotation Value of rotation to be passed to the `rotate` function.
 * @param field_x Piece X-position in playing field, may add/subtract 1.
 * @param field_y Piece Y-position in playing field, may add/subtract 1.
 */
bool piece_fits(int tetromino_id, int rotation, int field_x, int current_y);

class Tetris::Display {
private:
    const int m_width; // Console screen size (x-axis: columns)
    const int m_height; // Console screen size (y-axis: rows)
    const int m_area; // Private member for slight efficieny.
    DWORD m_bytes; // #bytes written, useless but needed for Windows API.
    wchar_t *m_screen; // Console screen buffer.
    HANDLE m_console; // Windows API console handle.
public:
    Display(int screen_width, int screen_height) 
    :   m_width(screen_width),
        m_height(screen_height),
        m_area(screen_width * screen_height),
        m_bytes(0)
    {
        m_screen = new wchar_t[m_area];
        // Start the screen off as completely blank.
        for (int i = 0; i < m_area; i++) {
            m_screen[i] = L' ';
        }
        m_console = CreateConsoleScreenBuffer(
            GENERIC_READ | GENERIC_WRITE, // bitfield permissions
            0, // share mode 0
            NULL, // security attributes
            CONSOLE_TEXTMODE_BUFFER, // flags
            NULL // screen buffer data
        );
        // Need this else we don't see anything! Windows API needs to know.
        SetConsoleActiveScreenBuffer(m_console);
    }
    ~Display() {
        // delete m_bytes;
        delete[] m_screen;
        // ? Need to close handle before printing game over message.
        // CloseHandle(m_console);
    }

    /**
     * @param loop_x Some value, e.g: `x + 2` or `nCurrentX + px + 2`.
     * @param loop_y Some value, e.g: `y + 2` or `nCurrentY + py + 2`.
     * @param value Assignment to be made at the resulting index.
     * @note Index formula is: `(loop_y * this->m_width) + loop_x`
     */
    void update(int loop_x, int loop_y, wchar_t value) {
        int screen_index = (loop_y * m_width) + loop_x;
        m_screen[screen_index] = value;
    }

    /**
     * Print score to specific location on screen, outside of playing field.
     */
    void print_score(int score) {
        int index = (2 * m_width) + OLC_FIELD_WIDTH + 6;
        swprintf_s(&m_screen[index], 16, L"SCORE: %8d", score);
    }

    /**
     * Actually display the frame by calling Windows API write console output.
     * Wrapper around `WriteConsoleOutputCharacterW`.
     */
    void render() {
        WriteConsoleOutputCharacterW(
            m_console, // console handle, windows API
            m_screen, // wide character buffer
            m_area, // number of elements in buffer
            {0,0}, // coordinates: write to topleft (x=0, y=0).
            &m_bytes // address to store #bytes written in
        );
    }

    // Wrapper because `m_console` is private.
    // @warning Do this before printing the gameover, otherwise you'll crash!
    void close_console() {
        CloseHandle(m_console);
    }
};

class Tetris::PlayingField {
private:
    unsigned char *m_field; // Array of indexes into the literal `L" ABCDEFG=#"`.
    int m_width; // Playing field's width (x-axis: columns).
    int m_height; // Playing field's height (y-axis: rows).
    int m_area; // Playing field's total elements in the buffer.
public:
    PlayingField(int width, int height) 
    :   m_width(width), 
        m_height(height), 
        m_area(width * height)
    {
        m_field = new unsigned char[m_area];
        for (int x = 0; x < width; x++) {
            for (int y = 0; y < height; y++) {
                int index = (y * width) + x;
                if (x == 0 || x == width - 1 || y == height - 1) {
                    m_field[index] = 9; // wall: hash, from L" ACBDEFG=#"[9] = L'#'
                }
                else {
                    m_field[index] = 0; // tile space, from L" ACBDEFG=#"[0] = L' '
                }
            }
        }
    }

    ~PlayingField() {
        delete[] m_field;
    }

    /**
     * Wrapper for the width and index formula due to how common it is,
     * and also because `m_width` is private.
     * @note Formula: `(test_y * this->m_width) + test_x`
     */
    int get_width_index(int test_x, int test_y) {
        return (test_y * m_width) + test_x;
    }

    /**
     * Check if the cell here is occupied, in which case it's a colllision.
     * That means our piece can't fit there.
     * @param value Pass `tetromino[tetromino_id][piece_index]`.
     * @param field_index Index into `m_field` array.
     * @note Formula: `value != L'.' && m_field[field_index] != 0`
     */
    bool is_occupied(wchar_t value, int field_index) {
        return value != L'.' && m_field[field_index] != 0;
    }

    /**
     * @param loop_x Some value, e.g: `nCurrentX + px` or just `px`.
     * @param loop_y Some value, e.g: `nCurrentY + py`.
     * @param value Assignment to be made at the resulting index.
     * @note Internal index formula is: `(loop_y * this->m_width) + loop_x`
     */
    void update(int loop_x, int loop_y, unsigned char value) {
        m_field[get_width_index(loop_x, loop_y)] = value;
    }

    /**
     * Checks if test values are in bounds. To be used in the `piece_fits` function.
     * @note Out of bounds does not necessarily mean failure.
     * As long as vertical piece has cells that lie outside the boundary,
     * we will ignore them and move on.
     */
    bool in_bounds(int test_x, int test_y) {
	return (test_x >= 0 && test_x < m_width)
		&& (test_y >= 0 && test_y < m_height);
    }

    /**
     * @param piece Pass in `player.get_piece(tetromino)`.
     * @param rotation Player's current piece rotation.
     * @param field_x Player's current piece x-axis coordinate on the field.
     * @param field_y Player's current piece y-axis coordinate on the field.
     */
    bool piece_fits(const std::wstring &piece, int rotation, int field_x, int field_y) {
        // All field cells > 0 are occupied.
        for (int px = 0; px < 4; px++) {
            for (int py = 0; py < 4; py++) {
                // Correct index into the piece.
                int piece_index = rotate(px, py, rotation);
                // Correct index into the playing field.
                int field_index = get_width_index(field_x + px, field_y + py);
                // The character at this particular cell.
                wchar_t value = piece[piece_index];
                if (in_bounds(field_x + px, field_y + py) && 
                    is_occupied(value, field_index)) {
                    return false;
                }
            }
        }
        return true;
    }

    /**
     * Updates `display.screen` based on each of `this->m_field`'s cells.
     * @param display Pass in the `Display` instance to be updated.
     * @note We add 2 to all x and y values as an offset to avoid drawing
     * at the very corners.
     */
    void draw(Display &display) {
        const wchar_t *charset = L" ABCDEFG=#";
        for (int x = 0; x < m_width; x++) {
            for (int y = 0; y < m_height; y++) {
                wchar_t value = charset[m_field[(y * m_width) + x]];
                display.update(x + 2, y + 2, value);
            }
        }
    }
};

class Tetris::Player {
private:
    int m_piece_id; // Current piece. Index into the `tetromino` array.
    int m_rotation; // Current rotation value.
    int m_coord_x; // Current X-axis coordinate (playing field).
    int m_coord_y; // Current Y-axis coordinate (playing field).
    bool m_rotatehold; // Current rotation value, we get modulo 4 internally.
    std::array<bool, 4> m_controls; // Keys are: left, right, down and 'Z'.
public:
    Player(int width)
    :   m_piece_id(0),
        m_rotation(0),
        m_coord_x(width / 2), // start in the center
        m_coord_y(0),
        m_rotatehold(false)
    {
        for (int k = 0; k < 4; k++) {
            m_controls[k] = false;
        }
    }

    /**
     * @param tetromino Pass in the array to index into.
     * @note Uses internal `m_piece_id`, which is private.
     */
    const std::wstring &get_piece(std::array<std::wstring, OLC_TETROMINO_COUNT> &tetromino) {
        return tetromino[m_piece_id];
    }

    /**
     * Test if the player's current piece can be moved down.
     * @param piece Index into the `tetromino` array beforehand.
     * @param pfield Pass in the `PlayingField` instance to update.
     * @note Should be called in the `if (bForceDown)` scope. 
     */
    bool try_move_down(const std::wstring &piece, PlayingField &pfield) {
        if (test_cell(0, 0, 1)) {
            m_coord_y++;
            return true;
        }
        // Implicit else: can't move down, try to lock the piece in place.
        for (int px = 0; px < 4; px++) {
            for (int py = 0; py < 4; py++) {
                if (piece[rotate(px, py, m_rotation)] != L'.') {
                    int test_x = m_coord_x + px;
                    int test_y = m_coord_y + py;
                    unsigned char value = m_piece_id + 1;
                    pfield.update(test_x, test_y, value);
                }
            }
        }

        // ! Problem: uses Player, PlayingField, vLines and nScore.
        #if defined(DEBUG_USE_VLINES)
        // Check for lines
        for (int py = 0; py < 4; py++) {
            if(nCurrentY + py < nFieldHeight - 1) {
                bool bLine = true;
                for (int px = 1; px < nFieldWidth - 1; px++) {
                    bLine &= (pField[(nCurrentY + py) * nFieldWidth + px]) != 0;
                }
                if (bLine) {
                    // Remove Line, set to =
                    for (int px = 1; px < nFieldWidth - 1; px++) {
                        pField[(nCurrentY + py) * nFieldWidth + px] = 8;
                    }
                    vLines.push_back(nCurrentY + py);
                }						
            }
        }
        nScore += 25;
        if (!vLines.empty()) {
            nScore += (1 << vLines.size()) * 100;
        }
        #endif // if defined(DEBUG_USE_VLINES)

        // Pick new piece
        m_coord_x = OLC_FIELD_WIDTH / 2;
        m_coord_y = 0;
        m_rotation = 0;
        m_piece_id = rand() % 7;
        return false;
    }

    void input() {
        /**
         * Windows Virtual Key Codes:
         * `\x27` = Right
         * `\x25` = Left
         * `\x28` = Up 
         */
        const char *controls = "\x27\x25\x28Z";
        for (int k = 0; k < 4; k++) {
            m_controls[k] = (GetAsyncKeyState(controls[k]) & 0x8000) != 0;
        }
    }

    // Not self-explanatory, I'm using a wrapper because this gets so long.
    // Pass nothing to test if the current piece's coordinates fit.
    bool test_cell(int offset_rotation = 0, int offset_x = 0, int offset_y = 0) {
        return piece_fits(
            m_piece_id,
            m_rotation + offset_rotation,
            m_coord_x + offset_x,
            m_coord_y + offset_y
        );
    }

    /**
     * Handles updating the player's coordinates based on each key pressed.
     * @note `true = 1`, `false = 0`, so we can use them as integrals.
     */
    void update() {
        m_coord_x += m_controls[0] && test_cell(0, 1);
        m_coord_x -= m_controls[1] && test_cell(0, -1);
        m_coord_y += m_controls[2] && test_cell(0, 0, 1);
        // Rotate but latch to stop wild spinning.
        if (m_controls[3]) {
            m_rotation += m_rotatehold && test_cell(1);
            m_rotatehold = false;
        }
        else {
            m_rotatehold = true;
        }
    }
    
    /**
     * Draws the given piece.
     * @param piece Index into the `tetromino` array beforehand.
     * @param display Pass in the `Display` instance to update.
     */
    void draw(const std::wstring &piece, Display &display) {
        for (int px = 0; px < 4; px++) {
            for (int py = 0; py < 4; py++) {
                if (piece[rotate(px, py, m_rotation)] != L'.') {
                    int test_x = m_coord_x + px + 2;
                    int test_y = m_coord_y + py + 2;
                    wchar_t value = m_piece_id + 'A';
                    display.update(test_x, test_y, value);
                }
            }
        }
    }
};

#endif // OLC_TETRIS_REFACTOR_HPP
