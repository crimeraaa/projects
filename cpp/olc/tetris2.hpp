#ifndef OLC_CONSOLE_TETRIS_HPP
#define OLC_CONSOLE_TETRIS_HPP

#include <windows.h>

/**
 * Abstraction over the calls to the Windows API cause it's really ugly.
 */
class ConsoleScreen {
    HANDLE m_console; // Windows API console handle.
    wchar_t *m_screen; // Screen's character buffer- 1D array treated as 2D array.
    const int m_width; // Console screen x axis (columns).
    const int m_height; // Console screen y axis (rows).
    const int m_area; // Console screen total elements.
    DWORD m_bytes; // Bytes written---only needed for calls to `WriteConsoleOutputCharacter`.
public:

    ConsoleScreen(int width, int height)
    :   m_width(width),
        m_height(height),
        m_area(width * height),
        m_bytes(0)
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

    // Display Frame
    void render() {
        WriteConsoleOutputCharacterW(
            m_console, // console handle
            m_screen, // the buffer
            m_area, // number of elements in the buffer
            {0,0},  // coordinates - write to the topleft most corner
            &m_bytes // We don't use this, but the API requires we pass one anyway
        );
    }
};

/**
 * Represents the ingame map or world where the elements reside.
 * @todo Make the member variables private. 
 */
class PlayingField {
    unsigned char *m_buffer; // Gameplay map/world. 1D array treated as 2D.
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
         // Board boundary
        for (int x = 0; x < width; x++) {
            for (int y = 0; y < height; y++) {
                // simple maths to treat a 1D array as a 2D one
                int index = (y * width) + x;
                // Check if we are on a border
                if (x == 0 || x == width - 1 || y == height - 1) {
                    m_buffer[index] = 9; // represents the border
                }
                else {
                    m_buffer[index] = 0; // empty space
                }
            }
        }
    }
    ~PlayingField() {
        delete[] m_buffer;
    }
};

class Position2D {
public:
    int x;
    int y;
};

/**
 * Keeps track of the currently falling piece, its roation and position.
 * Also has functions to update this the data of this.
 */
class PieceState {
public:
    int m_piece = 1; // ID of the piece that's currently falling.
    int m_rotation = 0; // Rotation value of the current piece.
    Position2D m_position; // 2D: has x and y values.

    // Pass in the `m_width` field of a `PlayingField` instance.
    PieceState(int width) {
        reset(width);
    }

    void reset(int width) {
        m_position.x = width / 2;
        m_position.y = 0;
        m_rotation = 0;
        m_piece = rand() % 7;
    }
};

class InputState {
public:

};

#endif // OLC_CONSOLE_TETRIS_HPP
