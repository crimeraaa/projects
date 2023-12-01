#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <string.h>

#define eprintf(log) \
fprintf(stderr, __FILE__":%i: Got error %ld, " log, __LINE__, GetLastError());

const int SCREEN_WIDTH = 80, SCREEN_HEIGHT = 30;
const wchar_t *appname = L"Thing";
// Need separate handles for input and output streams.
HANDLE h_console_original, h_console_stdin, h_console;
CONSOLE_SCREEN_BUFFER_INFO og_consoleinfo = {0};

SMALL_RECT rect_window = {0};
CHAR_INFO *screen_buffer = NULL;

// Adapted from the olcConsoleGameEngine tutorial by javidx9:
//      https://www.youtube.com/watch?v=u5BhrA8ED0o
int main(int argc, char *argv[]) {
    h_console_stdin = GetStdHandle(STD_INPUT_HANDLE);
    h_console = GetStdHandle(STD_OUTPUT_HANDLE);
    if (h_console_stdin == INVALID_HANDLE_VALUE || h_console == INVALID_HANDLE_VALUE) {
        eprintf("Bad Handle!");
        return 1;
    }

    // Change console visual size to a minimum so screen buffer can shrink below the
    // actual visual size.
    rect_window.Top = 0; 
    rect_window.Left = 0;
    rect_window.Right = 1; 
    rect_window.Bottom = 1;
    SetConsoleWindowInfo(h_console, TRUE, &rect_window);

    // Sets the size of the screen buffer. This is not the dimensions of the
    // console window itself! This just affects the text buffer.
    // See `SetConsoleWindowInfo`.
    // @note memory must be set up BEFORE adjusting the window size.
    COORD coords = { (short)SCREEN_WIDTH, (short)SCREEN_HEIGHT};
    if (!SetConsoleScreenBufferSize(h_console, coords)) {
        eprintf("SetConsoleScreenBufferSize failed.");
        return 2;
    }
    // Assign screen buffer to the console.
    if (!SetConsoleActiveScreenBuffer(h_console)) {
        eprintf("SetConsoleActiveScreenBuffer failed.");
        return 3;
    }
    // Set the font size now that the screen buffer was assigned to console.
    CONSOLE_FONT_INFOEX cfi;
    cfi.cbSize = sizeof(cfi);
    cfi.nFont = 0;
    cfi.dwFontSize.X = 8;
    cfi.dwFontSize.Y = 8;
    cfi.FontFamily = FF_DONTCARE;
    cfi.FontWeight = FW_NORMAL;
    wcscpy(cfi.FaceName, L"Consolas");
    if (!SetCurrentConsoleFontEx(h_console, FALSE, &cfi)) {
        eprintf("SetCurrentConsoleFontEx failed.");
        return 4;
    }

    // Get screen buffer info and check the max allowed window size.
    // Return error if exceeded so user knows their dimensions/fontsize are too large!
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (!GetConsoleScreenBufferInfo(h_console, &csbi)) {
        eprintf("GetConsoleScreenBufferInfo failed.");
        return 5;
    }
    if (SCREEN_HEIGHT > csbi.dwMaximumWindowSize.Y) {
        eprintf("Screen Height / Font Height too big!");
        return 6;
    }
    if (SCREEN_WIDTH > csbi.dwMaximumWindowSize.X) {
        eprintf("Screen Width / Font Width too big!");
        return 7;
    }

    // Set Physical Console Window size
    rect_window.Left = 0;
    rect_window.Top = 0;
    rect_window.Right = SCREEN_WIDTH - 1;
    rect_window.Bottom = SCREEN_HEIGHT - 1;
    if (!SetConsoleWindowInfo(h_console, TRUE, &rect_window)) {
        eprintf("SetConsoleWindowInfo failed.");
        return 8;
    }

    // Set flags to allow mouse input		
    if (!SetConsoleMode(h_console_stdin, ENABLE_EXTENDED_FLAGS | ENABLE_WINDOW_INPUT | ENABLE_MOUSE_INPUT)) {
        eprintf("SetConsoleMode failed.");
        return 9;
    }

    // Allocate memory for screen buffer
    // screen_buffer = new CHAR_INFO[m_nScreenWidth*m_nScreenHeight];
    screen_buffer = calloc(SCREEN_WIDTH * SCREEN_HEIGHT, sizeof(CHAR_INFO));
    if (!screen_buffer) {
        eprintf("screen_buffer calloc failed.");
        return 10;
    }
    // Zeroing out memory stuff, in C can do this with calloc
    // memset(screen_buffer, 0, sizeof(CHAR_INFO) * SCREEN_WIDTH * SCREEN_HEIGHT);
    // Threading stuff
    // SetConsoleCtrlHandler((PHANDLER_ROUTINE)CloseHandler, TRUE);
    DWORD bytes = 0;
    WriteConsoleOutputCharacter(h_console, L"Hi mom!", 8, (COORD){0,0}, &bytes);
    CloseHandle(h_console);
    return 0;
}
