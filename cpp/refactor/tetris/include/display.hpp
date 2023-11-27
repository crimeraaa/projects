#pragma once
#include "common.hpp"

/**
 * The full console window and buffer. Relies on Windows API.
 * @note The `m_screen` buffer is different from the `PlayingField` buffer!
 * @note We just display output here. `PlayingField` keeps track of game state.
 */
class Display {
public: // EXPOSED MEMBER VARIABLES
    const size_t width; // buffer x-axis size, or #columns
    const size_t height; // buffer y-axis size, or #rows
    const size_t area; // `width * height` = total elements in buffer

private: // INTERNAL MEMBER VARIABLES
    DWORD m_bytes; // #bytes written, useless but needed for Windows API.
    wchar_t *m_screen; // Console screen buffer, needed for Windows API.
    HANDLE m_console; // console buffer handle, needed for Windows API.

public: // CONSTRUCTOR AND DESTRUCTOR
    /**
     * Using `stdout` or `std::cout` is inefficient, use OS's API instead.
     *
     * @warning Please ensure correct dimensions between this and
     * `PlayingField`! Otherwise, who knows what will happen...
     *
     * @todo Make cross-compatible? This is currently Windows-specific.
     */
    Display(size_t screen_width, size_t screen_height);

    /**
     * @warning If printing the gameover screen, close the console handle
     * beforehand!
     * @warning Comment out the call to `CloseHandle` else we crash horribly.
     */
    ~Display();

public: // METHODS
    // Draw the buffer via a call to the Windows API.
    void render();

public: // OVERLOADS
    // Read an/write to element from the screen buffer. (`this->m_screen`)
    wchar_t &operator[](size_t index) {
        return this->m_screen[index];
    }
};

/*******************************************************************************
******************************** IMPLEMENTATION ********************************
*******************************************************************************/

inline Display::Display(size_t screen_width, size_t screen_height)
    : width(screen_width)
    , height(screen_height)
    , area(screen_width * screen_height)
    , m_bytes(0)
    , m_screen(new wchar_t[area]) {
    // Start the display screen buffer off as completely blank.
    for (size_t i = 0; i < area; i++) {
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

inline void Display::render() {
    WriteConsoleOutputCharacterW(
        m_console, // console handle, windows API
        m_screen, // wide character buffer
        area, // number of elements in buffer
        {0, 0}, // coordinates: write to topleft (x=0, y=0).
        &m_bytes // address to store #bytes written in
    );
}

inline Display::~Display() {
    delete[] m_screen;
    // ? Need to close handle before printing game over message,
    // ? so maybe don't use this destructor for the `this->m_console`.
    CloseHandle(m_console);
}
