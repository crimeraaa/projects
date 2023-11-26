#include "include/display.hpp"

Display::Display(size_t screen_width, size_t screen_height)
:   width(screen_width),
    height(screen_height),
    area(screen_width * screen_height),
    m_bytes(0),
    m_screen(new wchar_t[area])
{
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

wchar_t &Display::operator[](size_t index) {
    return m_screen[index];
}
