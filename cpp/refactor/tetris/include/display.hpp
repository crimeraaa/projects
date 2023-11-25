#pragma once
#include "common.hpp"

// Windows API 
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/**
 * The full console window and buffer. Relies on Windows API.
 * @note The `m_screen` buffer is different from the `PlayingField` buffer!
 * @note We just display output here. `PlayingField` keeps track of game state.
 */
class Display {
private:
    const int m_width; // Console screen size (x-axis: columns)
    const int m_height; // Console screen size (y-axis: rows)
    const int m_area; // Private member for slight efficieny.
    DWORD m_bytes; // #bytes written, useless but needed for Windows API.
    wchar_t *m_screen; // Console screen buffer.
    HANDLE m_console; // Windows API handle to console buffer.
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
        delete[] m_screen;
        // ? Need to close handle before printing game over message,
        // so maybe don't use this destructor.
        CloseHandle(m_console);
    }

    /**
     * Draw the buffer via a call to the Windows API.
     * This is wrapper around `WriteConsoleOutputCharacterW`.
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
};
