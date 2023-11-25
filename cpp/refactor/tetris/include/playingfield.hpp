#pragma once
#include "common.hpp"

/**
 * Playing field is an array of indexes into the literal `L" ABCDEFG=#"`.
 * So think in terms of that!
 * 
 * @note `m_field` buffer determines which character to use in `Display` buffer.
 */
class PlayingField {
private:
    const int m_width; // Playing field's width (x-axis: columns).
    const int m_height; // Playing field's height (y-axis: rows).
    const int m_area; // Playing field's total elements in the buffer.
    unsigned char *m_field; // Array of indexes into the literal `L" ABCDEFG=#"`.
public:

    /**
     * @warning Please ensure correct dimensions between this and `Display`!
     * Otherwise, who knows what will happen...
     * @param field_width Desired playing playing field's #columns (x-axis).
     * @param field_height Desired active playing field's #rows (y-axis).
     * @note Mainly meant to be used by a caller class, e.g. `Tetris`.
     */
    PlayingField(int field_width, int field_height);

    /**
     * Wrapper because `m_field` is private.
     * @param index Desired index into `m_field`.
     * @note Remember this is an index into the wchar literal `L" ABCDEFG=#"`.
     * @warning Has no error handling whatsoever!
     */
    wchar_t get_field_at(int index);

    bool is_in_bounds(int test_x, int test_y) {
        return (test_x >= 0 && test_x < m_width) 
            && (test_y >= 0 && test_y < m_height);
    }

    /**
     * Deletes `m_field`.
     * @note So far, `m_field` is the only dynamically allocated member.
     * @warning This messes up the VSCode C/C++ Extension doxygen thingy!
     */
    ~PlayingField();

};

/*******************************************************************************
******************************** IMPLEMENTATION ********************************
*******************************************************************************/

PlayingField::PlayingField(int field_width, int field_height)
:   m_width(field_width), 
    m_height(field_height), 
    m_area(field_width * field_height),
    m_field(new unsigned char[m_area]) 
{
    // Start the playing field buffer (our board) as blank with walls
    for (int x = 0; x < field_width; x++) {
        for (int y = 0; y < field_height; y++) {
            // Check if on board boundary (side or bottom of array).
            int index = (y * field_width) + x;
            if (x == 0 || x == field_width - 1 || y == field_height - 1) {
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

wchar_t PlayingField::get_field_at(int index) {
    return m_field[index];
}
