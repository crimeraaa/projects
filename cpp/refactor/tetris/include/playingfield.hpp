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
    unsigned char *m_field; // Array of indexes into the literal `L" ABCDEFG=#"`.
    const int m_width; // Playing field's width (x-axis: columns).
    const int m_height; // Playing field's height (y-axis: rows).
    const int m_area; // Playing field's total elements in the buffer.
public:
    PlayingField(int field_width, int field_height) 
    :   m_width(field_width), 
        m_height(field_height), 
        m_area(field_width * field_height)
    {
        m_field = new unsigned char[m_area];
        // Start the board as blank with walls
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

    // Deletes `m_field`.
    ~PlayingField() {
        delete[] m_field;
    }
};
