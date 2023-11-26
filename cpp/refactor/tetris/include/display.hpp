#pragma once
#include "common.hpp"

/**
 * The full console window and buffer. Relies on Windows API.
 * @note The `m_screen` buffer is different from the `PlayingField` buffer!
 * @note We just display output here. `PlayingField` keeps track of game state.
 */
class Display {
public: // EXPOSED MEMBER VARIABLES

    const int width; // #columns
    const int height; // #rows
    const int area; // width * height, e.g. 1920x1080

private: // INTERNAL MEMBER VARIABLES

    DWORD m_bytes; // #bytes written, useless but needed for Windows API.
    wchar_t *m_screen; // Console screen buffer, needed for Windows API.
    HANDLE m_console; // console buffer handle, needed for Windows API.

public: // CONSTRUCTOR AND DESTRUCTOR
    /**
     * Using `stdout` or `std::cout` is so inefficient, we use the OS's API.
     * 
     * @warning Please ensure correct dimensions between this and `PlayingField`!
     * Otherwise, who knows what will happen...
     */
    Display(int screen_width, int screen_height);

    /**
     * @warning If printing the gameover screen, close the console handle beforehand!
     * Comment out the `CloseHandle` call inside of this else we crash horribly.
     */
    ~Display();

public: // METHODS
    // Draw the buffer via a call to the Windows API.
    void render();

public: // OVERLOADS
    // Access an element from the screen buffer (`this->m_screen`).
    wchar_t &operator[](int index);
};

/*******************************************************************************
******************************** IMPLEMENTATION ********************************
*******************************************************************************/

Display::Display(int screen_width, int screen_height)
:   width(screen_width),
    height(screen_height),
    area(screen_width * screen_height),
    m_bytes(0),
    m_screen(new wchar_t[area])
{
    // Start the display screen buffer off as completely blank.
    for (int i = 0; i < area; i++) {
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
        area, // number of elements in buffer
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

wchar_t &Display::operator[](int index) {
    return m_screen[index];
}
