#pragma once
#include "common.hpp"
#include "fieldbuffer.hpp"

/**
 * The full console window and buffer. Relies on Windows API.
 * @note The `m_screen` buffer is different from the `PlayingField` buffer!
 * @note We just display output here. `PlayingField` keeps track of game state.
 */
class ConsoleWindow : public FieldBuffer<wchar_t> {
// INTERNAL MEMBER VARIABLES
private: 
    DWORD m_bytes; // `#bytes` written, useless but needed for Windows API.
    HANDLE m_handle; // Console buffer handle, needed for Windows API.

// CONSTRUCTOR AND DESTRUCTOR
public:
    /**
     * Using `stdout` or `std::cout` is inefficient, use OS's API instead.
     *
     * @warning Please ensure correct dimensions between this and
     * `PlayingField`! Otherwise, who knows what will happen...
     *
     * @todo Make cross-compatible? This is currently Windows-specific.
     */
    ConsoleWindow(size_t screen_width, size_t screen_height);

    /**
     * @warning If printing the gameover screen, close the console handle
     * beforehand!
     * @warning Comment out the call to `CloseHandle` else we crash horribly.
     */
    ~ConsoleWindow();

// METHODS
public: 
    // Draw the buffer via a call to the Windows API.
    void render();

// OVERLOADS
public: 
    // Read an/write to element from the screen buffer. (`this->m_screen`)
    wchar_t &operator[](size_t index) {
        return m_buffer[index];
    }
};

/*******************************************************************************
******************************** IMPLEMENTATION ********************************
*******************************************************************************/

inline ConsoleWindow::ConsoleWindow(size_t screen_width, size_t screen_height)
: FieldBuffer(screen_width, screen_height)
, m_bytes(0) {
    // Start the display screen buffer off as completely blank.
    for (size_t i = 0; i < area; i++) {
        m_buffer[i] = L' ';
    }
    m_handle = CreateConsoleScreenBuffer(
        GENERIC_READ | GENERIC_WRITE, // bitfield permissions
        0, // share mode 0
        NULL, // security attributes
        CONSOLE_TEXTMODE_BUFFER, // flags
        NULL // screen buffer data
    );
    // Need this else we don't see anything! Windows API needs to know.
    // Note that when this called, `cout` won't be seen when written to.
    SetConsoleActiveScreenBuffer(m_handle);
}

inline void ConsoleWindow::render() {
    WriteConsoleOutputCharacterW(
        m_handle, // console handle, windows API
        m_buffer, // wide character buffer
        area, // number of elements in buffer
        {0, 0}, // coordinates: write to topleft (x=0, y=0).
        &m_bytes // address to store `#bytes` written in
    );
}

inline ConsoleWindow::~ConsoleWindow() {
    // delete[] m_screen; // Is now inherited from `Field`, which deletes it!
    // ? Need to close handle before printing game over message,
    // ? so maybe don't use this destructor for the `this->m_console`.
    CloseHandle(m_handle);
}
