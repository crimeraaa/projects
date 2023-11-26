#pragma once
#include "common.hpp"

/**
 * The full console window and buffer. Relies on Windows API.
 * @note The `m_screen` buffer is different from the `PlayingField` buffer!
 * @note We just display output here. `PlayingField` keeps track of game state.
 */
class Display {
private: // MEMBER VARIABLES

    const int m_width; // Console screen size (x-axis: columns)
    const int m_height; // Console screen size (y-axis: rows)
    const int m_area; // Private member for slight efficieny.
    DWORD m_bytes; // #bytes written, useless but needed for Windows API.
    wchar_t *m_screen; // Console screen buffer.
    HANDLE m_console; // Windows API handle to console buffer.

public: // CONSTRUCTOR AND DESTRUCTOR
    /**
     * Because using `stdout` or `std::cout` is so inefficient, we need to use
     * the operating system's API directly. In this case: Windows.
     * 
     * @warning Please ensure correct dimensions between this and `PlayingField`!
     * Otherwise, who knows what will happen...
     * 
     * @param swidth Desired active console screen buffer's #columns (x-axis).
     * @param sheight Desired active console screen buffer's #rows (y-axis).
     * 
     * @note Mainly meant to be used by a caller class, e.g. `Tetris`.
     */
    Display(int swidth, int sheight);

    /**
     * Frees the `m_screen` buffer and closes the `m_console` handle.
     * 
     * @warning If printing the gameover screen, close the console handle beforehand!
     * Comment out the `CloseHandle` call inside of this else we crash horribly.
     * 
     * @note `m_screen` is dynamically allocated by the constructor.
     * @note `m_console` is from `CreateConsoleScreenBuffer` (Windows API).
     */
    ~Display();

public: // GETTERS
    /**
     * Retrieves the width assigned to the console screen buffer on construction.
     * 
     * @note We use this because `this->m_width` is private.
     * 
     * @return Read-only reference of this value for you to work with.
     */
    const int &get_width();

    /**
     * Retrieves the width assigned to the console screen buffer on construction.
     * 
     * @note We use this because `this->m_height` is private.
     * 
     * @return Read-only reference of this value for you to work with.
     */
    const int &get_height();

public: // METHODS
    /**
     * Draw the buffer via a call to the Windows API.
     * 
     * @note This is just a wrapper around `WriteConsoleOutputCharacterW`.
     * Pretty much all of the parameters passed are `private` members.
     */
    void render();

public: // OVERLOADS
    /**
     * Little overload to avoid the need for a getter.
     * 
     * @param index Number inside of square brackets, e.g. `display[4]`.
     * 
     * @return Readable and writeable reference to this element.
     */
    wchar_t &operator[](int index);
};

/*******************************************************************************
******************************** IMPLEMENTATION ********************************
*******************************************************************************/

Display::Display(int swidth, int sheight)
:   m_width(swidth),
    m_height(sheight),
    m_area(swidth * sheight),
    m_bytes(0),
    m_screen(new wchar_t[m_area])
{
    // Start the display screen buffer off as completely blank.
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
    // Note that when this called, `cout` won't be seen when written to.
    SetConsoleActiveScreenBuffer(m_console);
}

void Display::render() {
    WriteConsoleOutputCharacterW(
        m_console, // console handle, windows API
        m_screen, // wide character buffer
        m_area, // number of elements in buffer
        {0,0}, // coordinates: write to topleft (x=0, y=0).
        &m_bytes // address to store #bytes written in
    );
}

Display::~Display() {
    delete[] m_screen;
    // ? Need to close handle before printing game over message,
    // ? so maybe don't use this destructor for the `this->m_console`.
    CloseHandle(m_console);
}

const int &Display::get_height() {
    return m_height;
}

const int &Display::get_width() {
    return m_width;
}

wchar_t &Display::operator[](int index) {
    return m_screen[index];
}
