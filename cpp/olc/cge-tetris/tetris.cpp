#include "olcConsoleGameEngine.hpp"
#include "tetris.hpp"
#include <array>
#include <functional>
#include <string>
#include <thread>

class Tetris : public olcConsoleGameEngine {
// COMPILE-TIME CONSTANTS
private:
    enum PieceInfo {PIECE_WIDTH = 4, 
                    PIECE_HEIGHT = 4, 
                    PIECE_AREA = PIECE_WIDTH * PIECE_HEIGHT,
                    PIECE_COUNT = 7};    

// INTERNAL MEMBERS
private: 
    GameMap m_gamemap;
    Player m_player;
    TetrisGameState m_state;

    // Assets representing tetris pieces. '.' = empty space, 'X' = part of piece
    const std::array<std::wstring, PIECE_COUNT> m_tetrominos = {
        L"..X...X...X...X.", // 4x1: I block
        L"..X..XX...X.....", // 3+1: T block
        L".....XX..XX.....", // 2x2: O block
        L"..X..XX..X......", // 2+2: Z block
        L".X...XX...X.....", // 2+2: S block
        L".X...X...XX.....", // 3+1: L block
        L"..X...X..XX.....", // 3+1: J block
    };
// CONSTRUCTOR AND DESTRUCTION
public:
    // screen is the screen buffer, font is more like our "pixel".
    // Calls `ConstructConsole` so you don't have to as well
    Tetris(int screen_width, 
           int screen_height, 
           int gamemap_width,
           int gamemap_height,
           int font_width, 
           int font_height) 
        : m_gamemap(gamemap_width, gamemap_height)
        , m_player(gamemap_width)
        , m_state(20)
    {
        m_sAppName = L"Tetris";
        ConstructConsole(screen_width, screen_height, font_width, font_height);
    }

    // Default to 80x30 screen, 12x18 Tetris game, with 8x18 "pixels". 
    // @note Using `<width> x <height>`, like screen ratios `4:3` and `16:9`.
    Tetris() : Tetris(80, 30, 12, 18, 8, 18) {}

// GAME PROPER
public:
    // Game initialization, do nothing as most of it is done in constructors
    bool OnUserCreate() override 
    {
        return true;
    }

    // Game loop: the 4 stages of every computer game ever
    // @note Don't include `override` keyword when defining outside!
    bool OnUserUpdate(float dt) override;

    // Loop through `m_gamemap.width`, then `m_gamemap.height`.
    // @param callback Loop body, use coords to accesse game map cell.
    void loop_map(std::function<void(int mx, int my)> callback) 
    {
        for (int x = 0; x < m_gamemap.width; x++) {
            for (int y = 0; y < m_gamemap.height; y++) {
                callback(x, y);
            }
        }
    }

    // Loop through `PIECE_WIDTH`, then through `PIECE_HEIGHT`.
    // @param callback Loop body, use coords to access reference piece cell.
    void loop_piece(std::function<void(int px, int py)> callback) 
    {
        for (int x = 0; x < PIECE_WIDTH; x++) {
            for (int y = 0; y < PIECE_HEIGHT; y++) {
                callback(x, y);
            }
        }
    }

    // Symbolic constants to be passed into `is_valid_move`.
    enum class Offset {NONE, LEFT, RIGHT, DOWN, ROTATE};

    /**
     * @brief Get the correct index into a piece based on its rotation.
     *
     * @param tx Target piece's x-axis cell, usually called `px` in caller loop.
     * @param ty Target piece's y-axis cell, usually called `py` in caller loop.
     * @param rotation Value of rotation. We modulo by 4 internally.
     */
    int rotate(int tx, int ty, int rotation);

    // "Transforms" a tetromino array to be used in the field array.
    // @note This is javidx9's original implementation.
    bool piece_fits(int id, int rotation, int fx, int fy);

    // Overload to be called to just check current piece based on given offset.
    // @note After offsets are determined, this calls the above `piece_fits`.
    bool piece_fits(Offset code);

    // Check if the given key is held and piece fits with the given offset.
    // @note You can opt to use other virtual key codes, 
    // e.g. instead of `VK_LEFT` you use the char `'W'` for left movement.
    bool is_valid_move(unsigned char vkey, Offset offset);

    // Tries to move player's current piece downards, may try to lock the piece.
    // @return If locked the piece, does the player's new piece fit at the top?
    bool can_forcedown(void);
};

bool Tetris::OnUserUpdate(float dt) 
{
    using std::chrono_literals::operator""ms; // I hate it here (use C++14/17)
    m_state.counter++;
    m_state.forcedown = (m_state.counter == m_state.speed);

    /******************************* GAME TIMING ******************************/
    // 50ms results in roughly 14-20 FPS, since 1000ms/50ms = 20
    std::this_thread::sleep_for(50ms);

    /******************************** USER INPUT ******************************/
    // Actual input listening is done via `olcConsoleGameEngine::GameThread`
    // So really we're just updating player position!

    // ? Booleans evaluate to true = 1 and false = 0, so we can do arithmetic!
    m_player.x -= is_valid_move(VK_LEFT, Offset::LEFT);
    m_player.x += is_valid_move(VK_RIGHT, Offset::RIGHT);
    // {x=0, y=0} = topleft, so add to go downwards the y-axis
    m_player.y += is_valid_move(VK_DOWN, Offset::DOWN);

    // Prevent rotation entire time rotation key is held (after first rotate)
    if (is_valid_move('Z', Offset::ROTATE)) {
        // Yes, the boolean NOT is intentional for this: think about it!
        m_player.rotation += !m_player.hold_rotate;
        // On next call if this branch is reached, !true = 0 so we'll not rotate.
        m_player.hold_rotate = true; 
    } else {
        // Stop holding the moment rotation key is let go!
        m_player.hold_rotate = false; 
    }

    if (m_state.forcedown) {
        m_state.gameover = !can_forcedown();
        m_state.counter = 0; // reset so we can do the == comparison again
    }

    /******************************** GAME LOGIC ******************************/
    // Shapes falling, collision detection, scoring

    /****************************** RENDER OUTPUT *****************************/

    // Draw Field (a.k.a. update map data)
    loop_map([this](int x, int y)
    {
        int map_info = m_gamemap[(y * m_gamemap.width) + x];
        wchar_t map_cell = L" ABCDEFG=#"[map_info];
        // Use `olcConsoleGameEngine::Draw` to draw to a single cell.
        Draw(x + 2, y + 2, map_cell);
    });

    // Draw Current Piece
    const std::wstring &piece = m_tetrominos[m_player.piece_id]; 
    loop_piece([this, piece](int px, int py)
    {
        // Reference piece grid
        int index = rotate(px, py, m_player.rotation);
        // Skip parts of the grid that aren't part of the piece itself
        // Else: is part of the piece so we wanna draw something to the screen
        if (piece[index] == L'X') {
            // Add 'A' to piece ID to get actual char representation to draw.
            Draw(m_player.x + px + 2, 
                 m_player.y + py + 2, 
                 m_player.piece_id + 'A');
        }
    });
    return !m_state.gameover; // This (should) trigger the exit if false!
}

bool Tetris::is_valid_move(unsigned char vkey, Offset offset)
{
    // Call the overloaded piece_fits cause cause can be used w/o a vkey
    return m_keys[vkey].bHeld && piece_fits(offset);
}

bool Tetris::can_forcedown(void) 
{
    // Can fit in row below us so we are good
    if (piece_fits(Offset::DOWN)) {
        m_player.y++; 
        return true;
    }
    // Implied else: can't go further so lock piece at the current location!
    // Assign a variable to avoid constantly indexing inside the loop.
    const std::wstring &piece = m_tetrominos[m_player.piece_id]; 
    loop_piece([this, piece](int px, int py)
    {
        int rotation = rotate(px, py, m_player.rotation);
        if (piece[rotation] == L'X') {
            int i = ((m_player.y + py) * m_gamemap.width) + (m_player.x + px);
            // Add one so that we skip the first char, ' ' in literal.
            m_gamemap[i] = m_player.piece_id + 1;
        }
    });

    // Check for full completed horizontal line

    // Choose next piece
    m_player.x = m_gamemap.width / 2;
    m_player.y = 0;
    m_player.rotation = 0;
    m_player.piece_id = std::rand() % 7; // TODO: Use different RNG

    // Newly chosen piece doesn't fit at starting location = game over!
    return piece_fits(Offset::NONE);
}

bool Tetris::piece_fits(Offset offset)
{
    int x = 0, y = 0, r = 0; // our values to offset by, default is 0 all.
    switch (offset) {
        case Offset::NONE: break;
        case Offset::LEFT: x = -1; break;
        case Offset::RIGHT: x = 1; break;
        case Offset::DOWN: y = 1; break;
        case Offset::ROTATE: r = 1; break; // Only have clockwise rotation :(
    }
    // Call original implementation to do the actual calculations.
    return piece_fits(m_player.piece_id, 
                      m_player.rotation + r,
                      m_player.x + x,
                      m_player.y + y);
}

bool Tetris::piece_fits(int id, int rotation, int fx, int fy) 
{
    // reference tetromino
    const std::wstring &piece = m_tetrominos[id];
    // TODO: Use lambda for this? But have early return, maybe not
    for (int px = 0; px < PIECE_WIDTH; px++) {
        for (int py = 0; py < PIECE_HEIGHT; py++) {
            int piece_index = rotate(px, py, rotation);
            int field_index = (fy + py) * m_gamemap.width + (fx + px);
            if (!m_gamemap.is_in_bounds(fx + px, fy + py)) {
                continue;
            }
            // Is cell part of tetris piece/block itself?
            bool is_piece = (piece[piece_index] == L'X');

            // Game Map cell w/ value 0 represents ' ' (space) character 
            bool is_occupied = (m_gamemap[field_index] != 0);
            if (is_piece && is_occupied) {
                return false; // got collision
            }
        }
    }
    return true;
}

int Tetris::rotate(int tx, int ty, int rotation)
{
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

int main() {
    // system("pause");
    Tetris tetris;
    tetris.Start();
    return 0;
}
