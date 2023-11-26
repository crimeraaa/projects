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
     * @warning Please ensure correct dimensions between this and `PlayingField`!
     * Otherwise, who knows what will happen...
     * 
     * @todo Make cross-compatible? This is currently Windows-specific.
     */
    Display(size_t screen_width, size_t screen_height);

    /**
     * @warning If printing the gameover screen, close the console handle beforehand!
     * @warning Comment out the call to `CloseHandle` else we crash horribly.
     */
    ~Display();

public: // METHODS
    // Draw the buffer via a call to the Windows API.
    void render();

public: // OVERLOADS
    /**
     * Read an/write to element from the screen buffer.
     * ```cpp
     * return this->m_screen[index];
     * ```
     */
    wchar_t &operator[](size_t index);
};
