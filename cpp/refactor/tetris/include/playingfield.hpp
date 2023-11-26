#pragma once
#include "common.hpp"

/**
 * Playing field is an array of indexes into the literal `L" ABCDEFG=#"`.
 * So think in terms of that!
 * 
 * @note `m_field` buffer determines which character to use in `Display` buffer.
 */
class PlayingField {
public: // EXPOSED MEMBER VARIABLES

    const size_t width; // buffer x-axis size, or #columns
    const size_t height; // buffer y-axis size, or #rows
    const size_t area; // `width * height` = total elements in buffer

private: // INTERNAL MEMBER VARIABLES

    unsigned char *m_field; // Array of indexes into the literal `L" ABCDEFG=#"`.

public: // CONSTRUCTOR & DESTRUCTOR
    /**
     * @warning Please ensure correct dimensions between this and `Display`!
     * Otherwise, who knows what will happen...
     */
    PlayingField(size_t fwidth, size_t fheight);

    // Deletes `this->m_field`. That's it.
    ~PlayingField();

public: // METHODS
    /**
     * Test an x-y coordinate pair.
     * ```cpp
     * return (tx < this->m_width) && (ty < this->m_height);
     * ```
     */
    bool is_in_bounds(size_t tx, size_t ty);

    /**
     * ```cpp
     * return index < this->m_area;
     * ```
     */
    bool is_in_bounds(size_t index);

public: // OVERLOADS
    /**
     * Read an/write to an element from the buffer. No bounds checking.
     * ```cpp
     * return this->m_field[index];
     * ```
     */
    unsigned char &operator[](size_t index);
};

/*******************************************************************************
******************************** IMPLEMENTATION ********************************
*******************************************************************************/

PlayingField::PlayingField(size_t fwidth, size_t fheight)
:   width(fwidth), 
    height(fheight), 
    area(fwidth * fheight),
    m_field(new unsigned char[area]) 
{
    // Start the playing field buffer (our board) as blank with walls
    for (size_t fx = 0; fx < fwidth; fx++) {
        for (size_t fy = 0; fy < fheight; fy++) {
            // Check if on board boundary (side or bottom of array).
            size_t index = (fy * fwidth) + fx;
            if (fx == 0 || fx == fwidth - 1 || fy == fheight - 1) {
                m_field[index] = 9; // wall: hash, from L" ACBDEFG=#"[9] = L'#'
            }
            else {
                m_field[index] = 0; // tile space, from L" ACBDEFG=#"[0] = L' '
            }
        }
    }   
}

PlayingField::~PlayingField() {
    delete[] m_field;
}

bool PlayingField::is_in_bounds(size_t test_x, size_t test_y) {
    return (test_x < width) && (test_y < height);
}

bool PlayingField::is_in_bounds(size_t index) {
    return index < area;
}

unsigned char &PlayingField::operator[](size_t index) {
    return m_field[index];
}
