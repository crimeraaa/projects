#pragma once
#include <array>
#include <string>

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

// HELPER FUNCTIONS
private:
    // Determine if given coordinates are on the side or bottom of the array.
    // @note We don't consider the upper "lid" part of the array.
    bool is_edge(int x, int y) 
    {
        return (x == 0 || x == width - 1) || (y == height - 1);
    }
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
                if (is_edge(x, y)) {
                    m_buffer[index] = 9; // L" ABCDEFG=#"[9] = L'#'
                } else {
                    m_buffer[index] = 0; // L" ABCDEFG=#"[0] = L' '
                }
            }
        }
    }

    ~GameMap() 
    {
        delete[] m_buffer;
    }

    // Provide read-write access to the buffer's elements
    unsigned char &operator[](int index) 
    {
        return m_buffer[index];
    }

    // Check if the coordinates are in bounds of the buffer.
    bool is_in_bounds(int x, int y) 
    {
        return (x >= 0 && x < width) && (y >= 0 && y < height);
    }

    // Overload: check if a precomputed index is in bounds of the buffer.
    bool is_in_bounds(int index)
    {
        return index < area && index >= 0;
    }
};

// All members public, is meant to be modified by everybody
struct Player {  
    int piece_id; // Index into tetromino array.
    int rotation; // Current rotated value, to be modulo by 4.
    bool hold_rotate; // Stagger rotation to slow it down.
    int x; // `column#` index into game map/playing field
    int y; // `row#` index into game map/playing field

    Player(int field_width = 0)
        : piece_id{0}
        , rotation{0}
        , hold_rotate{false}
        , x{field_width / 2}
        , y{0} 
    {}
};
