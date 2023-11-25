#ifndef OLC_CONSOLE_TETRIS_HPP
#define OLC_CONSOLE_TETRIS_HPP

/**
 * Sets up each element in the global `wstring` array `tetromino`.
 * Periods represent empty space, capital X represents part of the tetromino.
 * @note Each tetromino is 4x4 wide characters (type `wchar_t`).
 */
void init_assets();

/**
 * Sets up the playing field `pField`.
 * Given the globals `nFieldWidth` and `nFieldHeight`, set up the `pField` array.
 */
void init_pfield();

/**
 * Sets up the globals `screen` and `hConsole`.
 * Relies on the existence of `nScreenWidth` and `nScreenHeight`.
 */
void init_consolescreen();

/**
 * @param px X coordinate of the piece.
 * @param py Y coordinate of the piece.
 * @param r Rotation value, we get its modulo by 4 internally.
 * @return Index into the tetromino piece.
 */
int rotate(int px, int py, int r);

/**
 * Translate 4x4 tetromino array into a field array.
 * If any check fails, we can assume the piece doesn't fit.
 * @param nTetromino ID of the tetromino in the `tetromino` array.
 * @param nRotation Current rotation for this piece.
 * @param nPosX X-axis location of the piece in the array.
 * @param nPosY Y-axis location of the piece in the array.
 */
bool piece_fits(int nTetromino, int nRotation, int nPosX, int nPosY);

#endif /* OLC_CONSOLE_TETRIS_HPP */
