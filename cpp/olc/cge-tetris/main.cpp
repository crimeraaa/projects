#include "olcConsoleGameEngine.hpp"
#include <array>

// a.k.a. "Playing Field"
class GameMap {
// Willing to expose these, read-only members anwyay
public:
    const int width; // `#columns`
    const int height; // `#rows`
    const int area; // `#cells`
private:
    // mapinfo, numbers which determine which cell to use
    // indexes into the literal L" ABCEDFG=#".
    uint8_t *m_buffer; 
public:

    GameMap(int field_width, int field_height)
        : width{field_width}
        , height{field_height}
        , area{field_width * field_height}
        , m_buffer{new uint8_t[area]} 
    {
        // Start up game map as just walls and blank spaces
        for (int x = 0; x < field_width; x++) {
            for (int y = 0; y < field_height; y++) {
                int index = (y * field_width) + x;
                // If on edge, this is a boundary!
                if ((x == 0 || x == field_width - 1) || 
                    (y == 0 || y == field_height - 1)) {
                    m_buffer[index] = 9; // L" ABCDEFG=#"[9] = L'#'
                } else {
                    m_buffer[index] = 0; // L" ABCDEFG=#"[0] = L' '
                }
            }
        }
    }

    ~GameMap() {
        delete[] m_buffer;
    }
    // Provide read-write access to the buffer's elements
    unsigned char &operator[](int index) {
        return m_buffer[index];
    }
};

// All members public, is meant to be modified by everybody
struct Player {  
    int piece_id; // Index into tetromino array.
    int rotation; // Current rotated value, to be modulo by 4.
    bool hold; // Stagger rotation to slow it down.
    struct Position {
        int x; 
        int y;
    } position; // Indexes into gamemap/playingfield buffer.

    Player(int field_width = 0)
        : piece_id{0}
        , rotation{0}
        , hold{false}
        , position{field_width / 2, 0} {}
};

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

    // '.' = empty space, 'X' = part of piece
    const std::array<std::wstring, PIECE_COUNT> m_tetrominos = {
        L"..X...X...X...X.", // 4x1: I block
        L"..X..XX...X.....", // 3+1: T block
        L".....XX..XX.....", // 2x2: O block
        L"..X..XX..X......", // 2+2: Z block
        L".X...XX...X.....", // 2+2: S block
        L".X...X...XX.....", // 3+1: L block
        L"..X...X..XX....."  // 3+1: J block
    };
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
        , m_player(screen_width)
    {
        ConstructConsole(screen_width, screen_height, font_width, font_height);
    }

    // Default to 80x80 screen with 8x8 "pixels". (width x height form)
    Tetris() : Tetris(40, 30, 12, 18, 10, 20) {}

    bool OnUserCreate() override 
    {
        return true;
    }

    bool OnUserUpdate(float dt) override 
    {
        // Fill all cells (x1--x2 all the way to y1--y2) blank for now
        // Fill(0, 0, m_nScreenWidth, m_nScreenHeight, L' ', 0);

        // Draw Field
        for (int x = 0; x < m_gamemap.width; x++) {
            for (int y = 0; y < m_gamemap.height; y++) {
                // Use `olcConsoleGameEngine::Draw` to draw to a single cell.
                int map_info = m_gamemap[(y * m_gamemap.width) + x];
                wchar_t map_cell = L" ACBDEFG=#"[map_info];
                Draw(x + 2, y + 2, map_cell);
            }
        }
        return true;
    }
};

int main() {
    Tetris tetris;
    tetris.Start();
    return 0;
}
