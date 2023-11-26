#pragma once
#include "common.hpp"

/**
 * Playing field is an array of indexes into the literal `L" ABCDEFG=#"`.
 * So think in terms of that!
 * 
 * @note `m_field` buffer determines which character to use in `Display` buffer.
 */
class PlayingField {
private: // MEMBER VARIABLES

    const int m_width; // Playing field's width (x-axis: columns).
    const int m_height; // Playing field's height (y-axis: rows).
    const int m_area; // Playing field's total elements in the buffer.
    unsigned char *m_field; // Array of indexes into the literal `L" ABCDEFG=#"`.

public: // CONSTRUCTOR & DESTRUCTOR
    /**
     * @warning Please ensure correct dimensions between this and `Display`!
     * Otherwise, who knows what will happen...
     * 
     * @param fwidth Desired playing playing field's #columns (x-axis).
     * @param fheight Desired active playing field's #rows (y-axis).
     * 
     * @note Mainly meant to be used by a caller class, e.g. `Tetris`.
     */
    PlayingField(int fwidth, int fheight);

    /**
     * Deletes `this->m_field`. That's it.
     * 
     * @note So far, `this->m_field` is the only dynamically allocated member.
     */
    ~PlayingField();

public: // GETTERS
    /**
     * Retrieves the width assigned to the playing field buffer on construction.
     * 
     * @note We use this because `this->m_width` is private.
     * 
     * @return Read-only reference of this value for you to work with.
     */
    const int &get_width();

    /**
     * Retrieves the height assigned to the playing field buffer on construction.
     * 
     * @note We use this because `this->m_height` is private.
     * 
     * @return Read-only reference of this value for you to work with.
     */
    const int &get_height();

public: // METHODS
    /**
     * Tests an x-axis and y-axis coordinate pair. 
     * Tries to see if they represent a valid location.
     * in the playing field screen buffer.
     * 
     * @param tx Some x-axis value to test, or column number, or width.
     * @param ty Some y-axis value to test, or row number, or height.
     * 
     * @note Remember that indexes in C/C++ are 0-based!
     */
    bool is_in_bounds(int tx, int ty);

    /**
     * Overload for a singular value.
     * Try to see if it's a valid index into `this->m_field`.
     * Remember that `this->m_field` is a 1D array of size `this->m_area`, 
     * which in turn is just the value `this->m_width * this->m_height`.
     * 
     * @param index Complete index to check for.
     * 
     * @note Example of a "full" index: `y * width + x`.
     */
    bool is_in_bounds(int index);

public: // OVERLOADS
    /**
     * Little overload to avoid the need for a getter.
     * 
     * @param index Number inside of square brackets, e.g. `display[4]`.
     * 
     * @return Readable and writeable Reference to this element.
     */
    unsigned char &operator[](int index);
};

/*******************************************************************************
******************************** IMPLEMENTATION ********************************
*******************************************************************************/

PlayingField::PlayingField(int fwidth, int fheight)
:   m_width(fwidth), 
    m_height(fheight), 
    m_area(fwidth * fheight),
    m_field(new unsigned char[m_area]) 
{
    // Start the playing field buffer (our board) as blank with walls
    for (int fx = 0; fx < fwidth; fx++) {
        for (int fy = 0; fy < fheight; fy++) {
            // Check if on board boundary (side or bottom of array).
            int index = (fy * fwidth) + fx;
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

const int &PlayingField::get_width() {
    return m_width;
}

const int &PlayingField::get_height() {
    return m_height;
}

bool PlayingField::is_in_bounds(int test_x, int test_y) {
    return (test_x >= 0 && test_x < m_width) 
        && (test_y >= 0 && test_y < m_height);
}

bool PlayingField::is_in_bounds(int index) {
    return (index >= 0) && (index < m_area);
}

unsigned char &PlayingField::operator[](int index) {
    return m_field[index];
}
