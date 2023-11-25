#include <iostream>
#include <thread> // template function `this_thread::sleep_for`
#include <vector> // template class `std::vector`

#include <windows.h> // `HANDLE`, `WriteConsoleOutputCharacter`, etc.
#include "tetris.hpp"

using namespace std;

#define OLC_DISPLAY_WIDTH 80
#define OLC_DISPLAY_HEIGHT 30
#define OLC_PFIELD_WIDTH 12
#define OLC_PFIELD_HEIGHT 18

/*******************************************************************************
******************************** GLOBAL VARIABLES ******************************
*******************************************************************************/ 

// Gameplay map information as dynamically alloc'd array.
// unsigned char *pField = nullptr; 
// constexpr int nFieldWidth = 12; // Playing field X-axis.
// constexpr int nFieldHeight = 18; // Playing field Y-axis.
// constexpr int nFieldArea = nFieldWidth * nFieldHeight; // Playing field total elements.

// Our screen is 80 chars wide (#columns) and 30 chars tall (#rows).
ConsoleScreen display(OLC_DISPLAY_WIDTH, OLC_DISPLAY_HEIGHT);
PlayingField pfield(OLC_PFIELD_WIDTH, OLC_PFIELD_HEIGHT);

// There are 7 tetrominos, each of which is 4x4 and has a unique shape.
wstring tetromino[7];

// Global- is modified or accessed by many functions.
HANDLE hConsole = nullptr;

int main() {
    init_assets(); // modifies `tetromino` array
    DWORD dwBytesWritten = 0;

    // Game Logic Stuff
    bool bGameOver = false;

    // Game Player State stuff
    int nCurrentPiece = 1; // ID of the piece that's currently falling
    int nCurrentRotation = 0; // Rotation value of the current piece
    int nCurrentX = OLC_DISPLAY_WIDTH / 2; // Start off in the middle of the playing field 
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
                            int index = pfield.get_index(nCurrentX, nCurrentY, px, py);
                            // new value is 1-based index into `L" ABCDEFG=#"`
                            pfield.update_field(index, nCurrentPiece + 1);
                        }
                    }
                }

                // Check for full horizontal lines

                // Choose the next piece
                nCurrentX = OLC_DISPLAY_WIDTH / 2;
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

        // Draw the basic, blank field first (the whole board)
        pfield.draw_field(display);

        // Draw Current Piece - iterate over all cells of the tetromino
        for (int px = 0; px < 4; px++) {
            for (int py = 0; py < 4; py++) {
                // Get the correct index into the piece given the rotation value
                // If it's an 'X', we'll want to draw something to the screen
                // else it's empty space
                if (tetromino[nCurrentPiece][rotate(px, py, nCurrentRotation)] == L'X') {
                    // Offset by the current position and 2.
                    int index = (nCurrentY + py + 2) * display.get_width() + (nCurrentX + px + 2);
                    // Add ASCII value 'A' to get the actual char.
                    wchar_t value = nCurrentPiece + 'A';
                    display.update_screen(index, value);
                }
            }
        }

        display.render();
    }

    return 0;
}

/*******************************************************************************
********************************* INITIALIZATION *******************************
*******************************************************************************/ 

void init_assets() {
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
    // All field cells > 0 are occupied.
    for (int px = 0; px < 4; px++) {
        for (int py = 0; py < 4; py++) {
            // Get correct index into piece based on rotation
            int pi = rotate(px, py, nRotation);

            // ! OLD, MAY NEED TO UNCOMMENT IF IT BREAKS!
            // int fi = (nPosY + py) * nFieldWidth + (nPosX + px);

            // Get index into field based on given positions and the loop positions.
            int fi = pfield.get_index(nPosX, nPosY, px, py);

            // ! OLD, MAY NEED TO UNCOMMENT IF IT BREAKS!
            // if (nPosX + px >= 0 && nPosX < nFieldWidth) {
            //     if (nPosY + py >= 0 && nPosY + py < nFieldHeight) {
            //         // Check the rotated index if there's something already in
            //         // the playing field.
            //         if (tetromino[nTetromino][pi] == L'X' && pField[fi] != 0) {
            //             return false; // fail on first hit, piece cannot fit here.
            //         }
            //     }
            // }

            if (pfield.in_bounds(nPosX, nPosY, px, py)) {
                if (pfield.test_index(tetromino[nTetromino][pi], pi, fi)) {
                    return false;
                }
            }
        }
    }
    return true;
}
