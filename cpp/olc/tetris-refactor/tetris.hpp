#ifndef OLC_CONSOLE_TETRIS_HPP
#define OLC_CONSOLE_TETRIS_HPP

#include <windows.h>

class ConsoleScreen {
private:
    wchar_t *m_screen; // 1D array, this is all the stuff shown onscreen.
    HANDLE m_console; // Windows API console handle pointer.
    const int m_width; // Playing field/screen X-axis number of columns.
    const int m_height; // Playing field/screen Y-axis number of rows.
    const int m_area; // Playing field total number of cells.
    DWORD bytes_written; // Not used by us, but needed for the Windows API functions.
public:
    // Note that initializer lists must be in order of appearance.
    ConsoleScreen(int screen_width, int screen_height) 
    :   m_width(screen_width),
        m_height(screen_height),
        m_area(screen_width * screen_height),
        bytes_written(0)
    {
        m_screen = new wchar_t[m_area];
        for (int i = 0; i < m_area; i++) {
            m_screen[i] = L' ';
        }
        m_console = CreateConsoleScreenBuffer(
            GENERIC_READ | GENERIC_WRITE, // permissions/access modifiers as bitfield
            0, // share mode
            NULL, // security attributes
            CONSOLE_TEXTMODE_BUFFER, // flags
            NULL // screen buffer data
        );
        // Can't use `std::cout` so need the Windows API to draw to the buffer.
        SetConsoleActiveScreenBuffer(m_console);
    }

    ~ConsoleScreen() {
        delete[] m_screen;
    }

    // Getter so that `m_width` can be `private`.
    int get_width() {
        return m_width;
    }

    /**
     * Wrapper so that `m_screen` can be private.
     * @param index Index used to access a value within`m_screen`.
     * @param new_value New value to be assigned to this index.
     */
    void update_screen(int index, wchar_t new_value) {
        m_screen[index] = new_value;
    }

    // Actually write our buffer to the screen.
    void render() {
        WriteConsoleOutputCharacterW(
            m_console, // console handle
            m_screen, // screen buffer
            m_area, // number of elements in screen buffer
            {0,0},  // coordinates - write to the topleft most corner
            &bytes_written // We don't use this, but the API requires we pass one anyway
        );
    }
};

class PlayingField {
private:
    /**
     * A faux 2D array of indexes into the `wchar_t` literal `L" ABCDEFG=#"`.
     * We use it to update the values of the global `ConsoleScreen` instance.
     * 
     * @note It should be smaller than the console screen's buffer.
     */
    unsigned char *m_buffer; 
    const int m_width; // Playing field X-axis.
    const int m_height; // Playing field Y-axis.
    const int m_area; // Playing field total elements.
public:
    PlayingField(int width, int height) 
    :   m_width(width),
        m_height(height),
        m_area(width * height)
    {
        m_buffer = new unsigned char[m_area];
        // Set up the playing field board to be blank, with borders.
        for (int x = 0; x < width; x++) {
            for (int y = 0; y < height; y++) {
                // simple maths to treat a 1D array as a 2D one
                int index = (y * width) + x;
                // Check if we are on a border
                if (x == 0 || x == width - 1 || y == height - 1) {
                    m_buffer[index] = 9; // L" ACBDEFG=#"[9] = L'#', our border 
                }
                else {
                    m_buffer[index] = 0; // L" ACBDEFG=#"[0] = L' ', empty space
                }
            }
        }
    }

    ~PlayingField() {
        delete[] m_buffer;
    }

    // Index is based on piece's current position and some field position.
    int get_index(int current_x, int current_y, int test_x, int test_y) {
        return (current_y + test_y) * m_width + (current_x + test_x);
    }

    /**
     * Draws the basic blank playing area, the borders and empty spaces.
     * Wide characters are one of: `L" ABCDEFG=#"`.
     * @note Everything outside the playing area is just whitespaces.
     */
    void draw_field(ConsoleScreen &display) {
        for (int x = 0; x < m_width; x++) {
            for (int y = 0; y < m_height; y++) {
                // Offset by 2, don't draw at topleft corner of the screen
                int index = (y + 2) * m_width + (x + 2);
                // Get the character to use for this cell.
                wchar_t value = L" ABCDEFG=#"[m_buffer[(y * m_width) + x]];
                // m_screen[index] = value;
                display.update_screen(index, value);
            }
        }
    }

    /**
     * Check that test is in bounds. 
     * Note that out of bounds does not necessarily mean failure. 
     * As long as the long vertical piece can have cells that lie outside the 
     * boundery, we'll ignore them.
     */
    bool in_bounds(int current_x, int current_y, int test_x, int test_y) {
        return (current_x + test_x >= 0 && current_x < m_width)
            && (current_y + test_y >= 0 && current_y + test_y < m_height);
    }

    /**
     * Check the rotated index if there's something already in the playing field.
     */
    bool test_index(wchar_t c, int piece_index, int field_index) {
        return c == L'X' && m_buffer[field_index] != 0;
    }

    // Wrapper so that `m_buffer` can be `private`.
    void update_field(int index, unsigned char new_value) {
        m_buffer[index] = new_value;
    }
};

class CurrentPiece {
private:
    int m_id; // ID (index into `tetromino` array) of currently falling piece.
    int m_rotation; // Rotation value to be used in `piece_fits`.
    int m_position_x; // X-axis position. Starts off in the middle.
    int m_position_y; // Y-axis position. Start off at the very top.
public:
    CurrentPiece()
    :   m_id(0),
        m_rotation(0),
        m_position_x(0),
        m_position_y(0)
    {

    }
};

/**
 * Sets up each element in the global `wstring` array `tetromino`.
 * Periods represent empty space, capital X represents part of the tetromino.
 * @note Each tetromino is 4x4 wide characters (type `wchar_t`).
 */
void init_assets();

/**
 * @param px X coordinate of the piece.
 * @param py Y coordinate of the piece.
 * @param r Rotation value, we get its modulo by 4 internally.
 * @return Index into the tetromino piece.
 */
int rotate(int px, int py, int r);

/**
 * Translate 4x4 tetromino array into a field array.
 * If any check fails, we can assume the piece doesn't fit.
 * @param nTetromino ID of the tetromino in the `tetromino` array.
 * @param nRotation Current rotation for this piece.
 * @param nPosX X-axis location, to test, of piece in playing field.
 * @param nPosY Y-axis location, to test, of piece in playing field.
 */
bool piece_fits(int nTetromino, int nRotation, int nPosX, int nPosY);

#endif /* OLC_CONSOLE_TETRIS_HPP */
