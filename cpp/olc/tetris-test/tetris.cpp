#include <iostream>
#include <thread> // template function `this_thread::sleep_for`
#include <vector> // template class `std::vector`

#include <windows.h> // `HANDLE`, `WriteConsoleOutputCharacter`, etc.
#include "tetris.hpp"

using namespace std;

/*******************************************************************************
******************************** GLOBAL VARIABLES ******************************
*******************************************************************************/ 

// Gameplay map information as dynamically alloc'd array.
unsigned char *pField = nullptr; 
constexpr int nFieldWidth = 12; // Playing field X-axis.
constexpr int nFieldHeight = 18; // Playing field Y-axis.
constexpr int nFieldArea = nFieldWidth * nFieldHeight; // Playing field total elements.

// 1D array to be treated as a 2D array.
wchar_t *screen = nullptr;
constexpr int nScreenWidth = 80; // Console screen X-axis (columns).
constexpr int nScreenHeight = 30; // Console screen Y-axis (rows);
constexpr int nScreenArea = nScreenWidth * nScreenHeight; // Console screen total elements.

// There are 7 tetrominos, each of which is 4x4 and has a unique shape.
wstring tetromino[7];

// Global- is modified or accessed by many functions.
HANDLE hConsole = nullptr;

int main() {
    init_assets(); // modifies `tetromino` array
    init_pfield(); // modifies `pField` pointer
    init_consolescreen(); // modifies `screen` array and `hConsole` pointer
    DWORD dwBytesWritten = 0;

    // Game Logic Stuff
    bool bGameOver = false;

    // Game Player State stuff
    int nCurrentPiece = 1; // ID of the piece that's currently falling
    int nCurrentRotation = 0; // Rotation value of the current piece
    int nCurrentX = nFieldWidth / 2; // Start off in the middle of the playing field 
    int nCurrentY = 0; // Start off at the top

    // Game Input Keys state (all 4 arrows)
    bool bKey[4];
    bool bRotateHold = false; // Help stagger the `Z` key for ease of use

    int nSpeed = 20; // Slowly increment the game speed (difficulty) as it progresses
    int nSpeedCounter = 0; // Number of game ticks (if == nSpeed, force piece down)
    bool bForceDown = false;

    // THE 4 STAGES OF A GAME LOOP =============================================
    while (!bGameOver) {
        // GAME TIMING  ========================================================
        // Milliseconds literals are only available with C++14 and above.
        this_thread::sleep_for(chrono::milliseconds(50)); // Game Tick
        nSpeedCounter++;
        bForceDown = (nSpeedCounter == nSpeed);

        // INPUT ===============================================================
        // Detect user pressing down certain keys
        for (int k = 0; k < 4; k++) {
            // We'll check for the Windows virtual key codes
            bKey[k] = (                                 //  R   L   D  Z
                0x8000 & GetAsyncKeyState((unsigned char)("\x27\x25\x28Z"[k]))
            ) != 0;
        }

        // GAME LOGIC ==========================================================
        // Shapes falling, collision detection, scoring

        // Check for the right arrow key
        // true = 1, false = 0 so we can add those to ints
        if (bKey[0]) {
            if (piece_fits(nCurrentPiece, nCurrentRotation, nCurrentX + 1, nCurrentY)) {
                nCurrentX++;
            }
        }
        // Check for the left arrow key
        if (bKey[1]) {
            if (piece_fits(nCurrentPiece, nCurrentRotation, nCurrentX - 1, nCurrentY)) {
                nCurrentX--;
            }
        }
        // Check for down arrow key (remember that topleft is x=0,y=0).
        if (bKey[2]) {
            if (piece_fits(nCurrentPiece, nCurrentRotation, nCurrentX, nCurrentY + 1)) {
                nCurrentY++;
            } 
        }
        // Check for the `Z` key
        if (bKey[3]) {
            nCurrentRotation += bRotateHold && piece_fits(nCurrentPiece, nCurrentRotation + 1, nCurrentX, nCurrentY);
            bRotateHold = false;
        }
        else {
            bRotateHold = true;
        }

        if (bForceDown) {
            // Does the piece fit any further down?
            if (piece_fits(nCurrentPiece, nCurrentRotation, nCurrentX, nCurrentY + 1)) {
                nCurrentY++;
            }
            else {
                // Lock current piece in the field
                for (int px = 0; px < 4; px++) {
                    for (int py = 0; py < 4; py++) {
                        if (tetromino[nCurrentPiece][rotate(px, py, nCurrentRotation)] == L'X') {
                            // 0 is empty space in the `L" ABCDEFG=#"` literal, so add 1
                            pField[(nCurrentY + py) * nFieldWidth + (nCurrentX + px)] = nCurrentPiece + 1;
                        }
                    }
                }

                // Check for full horizontal lines


                // Choose the next piece
                nCurrentX = nFieldWidth / 2;
                nCurrentY = 0;
                nCurrentRotation = 0;
                nCurrentPiece = rand() % 7;

                // If piece does not fit
                if (!piece_fits(nCurrentPiece, nCurrentRotation, nCurrentX, nCurrentY)) {
                    bGameOver = true;
                }
            }
            nSpeedCounter = 0;
        }

        // RENDER OUTPUT =======================================================

        // Draw Field (which represents the whole board!) 
        for (int x = 0; x < nFieldWidth; x++) {
            for (int y = 0; y < nFieldHeight; y++) {
                // Offset by 2 so we don't draw in the toplefthand corner of the screen
                // Get the index into the wchar array to determine which piece to use.
                screen[(y + 2) * nScreenWidth + (x + 2)] = L" ABCDEFG=#"[pField[y*nFieldWidth + x]];
            }
        }

        // Draw Current Piece - iterate over all cells of the tetromino
        for (int px = 0; px < 4; px++) {
            for (int py = 0; py < 4; py++) {
                // Get the correct index into the piece given the rotation value
                // If it's an 'X', we'll want to draw something to the screen
                // else it's empty space
                if (tetromino[nCurrentPiece][rotate(px, py, nCurrentRotation)] == L'X') {
                    // Offset by the current position and 2
                    // Add ASCII value 'A' to get the actual char.
                    screen[(nCurrentY + py + 2)*nScreenWidth + (nCurrentX + px + 2)] = nCurrentPiece + 'A';
                }
            }
        }

        // Display Frame
        WriteConsoleOutputCharacterW(
            hConsole, // console handle
            screen, // the buffer
            nScreenArea, // number of elements in the buffer
            {0,0},  // coordinates - write to the topleft most corner
            &dwBytesWritten // We don't use this, but the API requires we pass one anyway
        );
    }

    return 0;
}

/*******************************************************************************
********************************* INITIALIZATION *******************************
*******************************************************************************/ 

void init_assets() {
    // Yes this is valid :)
    // Straight line block
    tetromino[0].append(L"..X.");
    tetromino[0].append(L"..X.");
    tetromino[0].append(L"..X.");
    tetromino[0].append(L"..X.");
    // Z block
    tetromino[1].append(L"..X.");
    tetromino[1].append(L".XX.");
    tetromino[1].append(L".X..");
    tetromino[1].append(L"....");
    // S block
    tetromino[2].append(L".X..");
    tetromino[2].append(L".XX.");
    tetromino[2].append(L"..X.");
    tetromino[2].append(L"....");
    // Square block
    tetromino[3].append(L"....");
    tetromino[3].append(L".XX.");
    tetromino[3].append(L".XX.");
    tetromino[3].append(L"....");
    // T block
    tetromino[4].append(L"..X.");
    tetromino[4].append(L".XX.");
    tetromino[4].append(L"..X.");
    tetromino[4].append(L"....");
    // Right L block
    tetromino[5].append(L"....");
    tetromino[5].append(L".XX.");
    tetromino[5].append(L"..X.");
    tetromino[5].append(L"..X.");
    // Left L block
    tetromino[6].append(L"....");
    tetromino[6].append(L".XX.");
    tetromino[6].append(L".X..");
    tetromino[6].append(L".X..");
}

void init_pfield() {
    pField = new unsigned char [nFieldArea];
    // Board boundary
    for (int x = 0; x < nFieldWidth; x++) {
        for (int y = 0; y < nFieldHeight; y++) {
            // simple maths to treat a 1D array as a 2D one
            int index = (y * nFieldWidth) + x;
            // Check if we are on a border
            if (x == 0 || x == nFieldWidth - 1 || y == nFieldHeight - 1) {
                pField[index] = 9; // represents the border
            }
            else {
                pField[index] = 0; // empty space
            }
        }
    }
}

void init_consolescreen() {
    screen = new wchar_t[nScreenArea];
    for (int i = 0; i < nScreenArea; i++) {
        screen[i] = L' ';
    }
    hConsole = CreateConsoleScreenBuffer(
        GENERIC_READ | GENERIC_WRITE, // permissions/access modifiers as bitfield
        0, // share mode
        NULL, // security attributes
        CONSOLE_TEXTMODE_BUFFER, // flags
        NULL // screen buffer data
    );
    // Can't use `std::cout` so need the Windows API to draw to the buffer.
    SetConsoleActiveScreenBuffer(hConsole);
}

/*******************************************************************************
*********************************** GAME LOOP **********************************
*******************************************************************************/ 

int rotate(int px, int py, int r) {
    switch (r % 4) {
        case 0: return py * 4 + px; // 0 degrees
        case 1: return 12 + py - (px * 4); // 90 degrees
        case 2: return 15 - (py * 4) - px; // 180 degrees
        case 3: return 3 - py + (px * 4); // 270 degrees
    } 
    return 0;
}

bool piece_fits(int nTetromino, int nRotation, int nPosX, int nPosY) {
    for (int px = 0; px < 4; px++) {
        for (int py = 0; py < 4; py++) {
            // Get index into piece
            int pi = rotate(px, py, nRotation);

            // Get index into field
            int fi = (nPosY + py) * nFieldWidth + (nPosX + px);

            if (nPosX + px >= 0 && nPosX < nFieldWidth) {
                if (nPosY + py >= 0 && nPosY + py < nFieldHeight) {
                    // Check the rotated index if there's something already in
                    // the playing field.
                    if (tetromino[nTetromino][pi] == L'X' && pField[fi] != 0) {
                        return false; // fail on first hit, piece cannot fit here.
                    }
                }
            }
        }
    }
    return true;
}
