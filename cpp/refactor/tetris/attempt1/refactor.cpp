#include <array>
#include <iostream>
#include <thread>
#include <vector>
#include <cstdio>

using namespace std;

#include "refactor.hpp"

Tetris::Display display(OLC_SCREEN_WIDTH, OLC_SCREEN_HEIGHT);
Tetris::PlayingField pfield(OLC_FIELD_WIDTH, OLC_FIELD_HEIGHT);
Tetris::Player player(OLC_FIELD_WIDTH);
array<wstring, OLC_TETROMINO_COUNT> tetrominos;

int main()
{	
	tetrominos[0].append(L"..X...X...X...X."); // Tetronimos 4x4
	tetrominos[1].append(L"..X..XX...X.....");
	tetrominos[2].append(L".....XX..XX.....");
	tetrominos[3].append(L"..X..XX..X......");
	tetrominos[4].append(L".X...XX...X.....");
	tetrominos[5].append(L".X...X...XX.....");
	tetrominos[6].append(L"..X...X..XX.....");

	// Game Logic
	bool bForceDown = false;

	int nSpeed = 20;
	int nSpeedCount = 0;

	int nPieceCount = 0;
	int nScore = 0;

	#if defined(DEBUG_USE_VLINES)
	vector<int> vLines;
	#endif

	bool bGameOver = false;

	while (!bGameOver) // Main Loop
	{
		// Timing =======================
		// this_thread::sleep_for(50ms); // Small Step = 1 Game Tick
		// ! MODIFICATION: USE `chrono::milliseconds` RATHER THAN LITERAL
		this_thread::sleep_for(chrono::milliseconds(50));
		nSpeedCount++;
		bForceDown = (nSpeedCount == nSpeed);

		// Input ========================
		player.input();
		
		// Game Logic ===================

		// Handle player movement
		player.update();

		// Force the piece down the playfield if it's time
		if (bForceDown) {
			// Update difficulty every 50 pieces
			nSpeedCount = 0;
			nPieceCount++;
			if (nPieceCount % 50 == 0) {
				if (nSpeed >= 10) {
					nSpeed--;
				}
			}

			// Game over occurs when we can't move down and can't fit anywhere.
			bGameOver = !player.try_move_down(player.get_piece(tetrominos), pfield) && !player.test_cell();
		}
		
		// Display ======================
		
		// Draw Field
		pfield.draw(display);

		// Draw Current Piece
		player.draw(player.get_piece(tetrominos), display);

		// Draw Score
		display.print_score(nScore);
		
		#if defined(DEBUG_USE_VLINES)
		// Animate Line Completion
		if (!vLines.empty()) {
			// Display Frame (cheekily to draw lines)
			display.render();
			// this_thread::sleep_for(400ms); // Delay a bit
			// ! MODIFICATION: USE CHRONO EXPLICITLY
			this_thread::sleep_for(chrono::milliseconds(400));
			for (auto &v : vLines) {
				for (int px = 1; px < nFieldWidth - 1; px++) {
					for (int py = v; py > 0; py--) {
						pField[py * nFieldWidth + px] = pField[(py - 1) * nFieldWidth + px];
					}
					pField[px] = 0;
				}
			}
			vLines.clear();
		}
		#endif // if defined(DEBUG_USE_VLINES)
		// Display Frame
		display.render();
	}
	// Oh Dear
	display.close_console();
	cout << "Game Over!! Score:" << nScore << endl;
	system("pause");
	return 0;
}

int rotate(int piece_x, int piece_y, int rotation) {
    int piece_index = 0;
    switch (rotation % 4) {
        /** 0 DEGREES:       0  1  2  3
                             4  5  6  7
                             8  9 10 11 
                            12 13 14 15 */
        case 0: 
            piece_index = (piece_y * 4) + piece_x; 
            break;
        /** 90 DEGREES:     12  8  4  0
                            13  9  5  1
                            14 10  6  2 
                            15 11  7  3 */
        case 1: 
            piece_index = 12 + piece_y - (piece_x * 4); 
            break;
        /** 180 DEGREES:    15 14 13 12
                            11 10  9  8
                             7  6  5  4
                             3  2  1  0 */
        case 2: 
            piece_index = 15 - (piece_y * 4) - piece_x; 
            break;
        /** 270 DEGREES:     3  7 11 15
                             2  6 10 14
                             1  5  9 13
                             0  4  8 12 */
        case 3: 
            piece_index = 3 - piece_y + (piece_x * 4); 
            break;
    }
    return piece_index;
}

bool piece_fits(int tetromino_id, int rotation, int field_x, int field_y) {
    // All field cells > 0 are occupied.
    for (int px = 0; px < 4; px++) {
        for (int py = 0; py < 4; py++) {
            // Correct index into the piece.
            int piece_index = rotate(px, py, rotation);
            // Correct index into the playing field.
			int field_index = pfield.get_width_index(field_x + px, field_y + py);
			// The character at this particular cell.
			wchar_t value = tetrominos[tetromino_id][piece_index];
			if (pfield.in_bounds(field_x + px, field_y + py) && 
				pfield.is_occupied(value, field_index)) {
				return false;
			}
		}
    }
	return true;
}
