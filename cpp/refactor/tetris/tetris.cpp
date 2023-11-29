#include "include/common.hpp"
#include "tetris.hpp"

void Tetris::input() {
    m_speedcount++;
    m_forcedown = (m_speedcount == m_speed);

    // Asynchronously updates the list of pressed keys.
    for (size_t i = 0; i < Player::KEYS_COUNT; i++) {
        m_player.controls[i].update();
    }

    // ? Use booleans to our advantage: true = 1, false = 0, so we can use that!
    m_player.position.x += validate_key(Player::KEY_RIGHT, OFFSET_RIGHT);
    m_player.position.x -= validate_key(Player::KEY_LEFT, OFFSET_LEFT);
    m_player.position.y += validate_key(Player::KEY_DOWN, OFFSET_DOWN);

    // If held consecutively between calls, `m_player.hold == false` still.
    // This enforces the "staggering" behaviour in the compound assignment.
    if (validate_key(Player::KEY_Z, OFFSET_ROTATE)) {
        m_player.rotation += m_player.hold;
        m_player.hold = false;
    } else {
        // Reset the moment we detect 'Player::KEY_Z' has been let go.
        m_player.hold = true;
    }
}

bool Tetris::validate_key(enum Player::Keys vkey_id, enum Offsets code) {
    return m_player.controls[vkey_id].down && piece_fits(code);
}

bool Tetris::piece_fits(enum Offsets code) {
    // Determine which offset to use based on offset `code`.
    int offset_x = 0, offset_y = 0, offset_rotation = 0;
    switch (code) {
        // Y-axis grows downwards, to go up we need to offset by -1.
        // case OFFSET_UP:     offset_y = -1; break;
        // Likewise, to go down we need to offset by 1.
        case OFFSET_DOWN: offset_y = 1; break;
        case OFFSET_LEFT: offset_x = -1; break;
        case OFFSET_RIGHT: offset_x = 1; break;
        // In tetris you can only rotate in 1 direction: clockwise.
        case OFFSET_ROTATE: offset_rotation = 1; break;
        default: break; // No offset given, test current location as is.
    }
    // Call the overloaded one because that has the actual implementation
    return piece_fits(
        m_player.piece_id,
        m_player.rotation + offset_rotation,
        m_player.position.x + offset_x,
        m_player.position.y + offset_y
    );
}

bool Tetris::piece_fits(size_t id, int rotation, size_t fx, size_t fy) {
    // reference tetromino
    const auto &piece = m_tetrominos[id];
    for (size_t px = 0; px < PIECE_WIDTH; px++) {
        for (size_t py = 0; py < PIECE_HEIGHT; py++) {
            // Correct index into the reference tetromino
            size_t piece_index = rotate(px, py, rotation);

            // Spot in playing field where we want to check for collisions.
            size_t field_index = (fy + py) * m_pfield.width + (fx + px);

            // Remember the screen buffer is much larger than the playing field.
            if (!m_pfield.is_in_bounds(fx + px, fy + py)) {
                continue;
            }
            // Consider 'X' as part of the piece in the tetromino grid.
            bool is_piece = (piece[piece_index] != L'.');

            // Playing field: Value 0 represents whitespace character `' '`.
            bool is_occupied = (m_pfield[field_index] != 0);

            // We're on a part of the piece and it collided with something!
            if (is_piece && is_occupied) {
                return false;
            }
        }
    }
    return true;
}

size_t Tetris::rotate(size_t tx, size_t ty, int rotation) {
    switch (rotation % 4) {
        /** 0 DEGREES:       0  1  2  3
                             4  5  6  7
                             8  9 10 11
                            12 13 14 15 */
        case 0: return (ty * PIECE_WIDTH) + tx;

        /** 90 DEGREES:     12  8  4  0
                            13  9  5  1
                            14 10  6  2
                            15 11  7  3 */
        case 1: return (PIECE_AREA - PIECE_WIDTH) + ty - (tx * PIECE_WIDTH);

        /** 180 DEGREES:    15 14 13 12
                            11 10  9  8
                            7  6  5  4
                            3  2  1  0 */
        case 2: return (PIECE_AREA - 1) - (ty * PIECE_WIDTH) - tx;

        /** 270 DEGREES:    3  7 11 15
                            2  6 10 14
                            1  5  9 13
                            0  4  8 12 */
        case 3: return (PIECE_WIDTH - 1) - ty + (tx * PIECE_WIDTH);
    }
    return 0;
}

/*******************************************************************************
******************************** GAME UPDATE ***********************************
*******************************************************************************/

void Tetris::update() {
    if (m_forcedown) {
        if (piece_fits(OFFSET_DOWN)) {
            m_player.position.y++;
        } else {
            // Lock the current piece into the field, becoming an obstacle.
            lock_piece();

            // Check for the completion of full horizontal lines.

            // Choose next piece.
            m_player.position.x = m_pfield.width / 2;
            m_player.position.y = 0;
            m_player.rotation = 0;
            m_player.piece_id = std::rand() % 7;

            // If piece doesn't fit in the current location, you lose!
            m_gameover = !piece_fits(OFFSET_NONE);
        }
        // reset so we can increment and change against `m_speed` again
        m_speedcount = 0;
    }
}

void Tetris::lock_piece() {
    // Reference tetromino piece to get grid information from.
    const auto &piece = m_tetrominos[m_player.piece_id];
    // Shorthands because these are so long!
    const size_t fx = m_player.position.x, fy = m_player.position.y;
    for (size_t px = 0; px < PIECE_WIDTH; px++) {
        for (size_t py = 0; py < PIECE_HEIGHT; py++) {
            const size_t piece_index = rotate(px, py, m_player.rotation);
            if (piece[piece_index] != L'X') {
                continue;
            }
            size_t field_index = ((fy + py) * m_pfield.width) + (fx + px);
            // 0 is empty space so add 1 to get a character
            // refer to the string literal `L" ABCDEFG=#"`.
            m_pfield[field_index] = m_player.piece_id + 1;
        }
    }
}

/*******************************************************************************
******************************** RENDER OUTPUT *********************************
*******************************************************************************/

void Tetris::render() {
    // Update the `m_display` buffer. based on our `m_pfield` buffer,
    draw_field();

    // Update the `m_pfield` buffer with the player's current tetromino.
    draw_piece();

    // Actually write the display frame to the console output.
    m_display.render();
}

void Tetris::draw_field() {
    // Loop through each cell in our playing field.
    for (size_t fx = 0; fx < m_pfield.width; fx++) {
        for (size_t fy = 0; fy < m_pfield.height; fy++) {
            // Index into the display screen to be updated.
            size_t screen_index = ((fy + 2) * m_display.width) + (fx + 2);

            // Value received is an index into `" ABCDEFG=#"`, which is used
            // when we update the screen buffer.
            auto tile = m_pfield[(fy * m_pfield.width) + fx];

            /**
             * Update the main display output
             * [0]      = L' ':         empty space,
             * [1...7]: = L"ABCDEFG":   each letter represents a tetris piece.
             * [8]      = L'=':         completed lines/tetris.
             * [9]:     = L'#':         border/wall.
             */
            m_display[screen_index] = L" ABCDEFG=#"[tile];
        }
    }
}

void Tetris::draw_piece() {
    // Should we use locals to access member variables when in a `for` loop?
    // See: https://stackoverflow.com/a/238568

    // Local so I can work with shorter names.
    // Effectively our field indexes. Still need to get appropriate offsets.
    const size_t fx = m_player.position.x, fy = m_player.position.y;

    const auto &tetromino = m_tetrominos[m_player.piece_id];

    for (size_t px = 0; px < PIECE_WIDTH; px++) {
        for (size_t py = 0; py < PIECE_HEIGHT; py++) {
            // Don't update the display screen with `'.'` characters.
            size_t tetromino_index = rotate(px, py, m_player.rotation);
            if (tetromino[tetromino_index] != L'X') {
                continue;
            }
            // offset by 2 so we don't end up writing to the corners
            size_t screen_index = (fy + py + 2) * m_display.width + (fx + px + 2);
            // piece_id + 'A' gets a capital letter, should be from 'A' to 'G'.
            m_display[screen_index] = m_player.piece_id + 'A';
        }
    }
}
