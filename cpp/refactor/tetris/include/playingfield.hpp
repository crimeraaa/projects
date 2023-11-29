#pragma once
#include "common.hpp"
#include "fieldbuffer.hpp"

/**
 * Playing field is an array of indexes into the literal `L" ABCDEFG=#"`.
 * So think in terms of that!
 *
 * @note `m_buffer` buffer determines which character to write to `ConsoleWindow`.
 * It's based on the literal `L" ABCDEFG=#"`.
 */
class PlayingField : public FieldBuffer<unsigned char> {
// CONSTRUCTOR & DESTRUCTOR
public: 
    /**
     * @warning Please ensure correct dimensions between this and `ConsoleWindow`!
     * Otherwise, who knows what will happen...
     */
    PlayingField(size_t fwidth, size_t field_height);

// METHODS
public: 
    // Test an x-y pair (2D coordinates) to turn them into a 1D index.
    bool is_in_bounds(size_t tx, size_t ty) {
        return (tx < width) && (ty < height);
    }

// OVERLOADS
public: 
    // Overload for a lone 1D index.
    bool is_in_bounds(size_t index) {
        return index < area;
    }

    // Read an/write to an element from the buffer. No bounds checking!
    unsigned char &operator[](size_t index) {
        return m_buffer[index];
    }
};

/*******************************************************************************
******************************** IMPLEMENTATION ********************************
*******************************************************************************/

inline PlayingField::PlayingField(size_t field_width, size_t field_height)
    : FieldBuffer(field_width, field_height) {
    // Start the playing field buffer (our board) as blank with walls
    for (size_t fx = 0; fx < field_width; fx++) {
        for (size_t fy = 0; fy < field_height; fy++) {
            // Check if on board boundary (side or bottom of array).
            size_t index = (fy * field_width) + fx;
            if (fx == 0 || fx == field_width - 1 || fy == field_height - 1) {
                m_buffer[index] = 9; // wall: hash, (L" ACBDEFG=#"[9] = L'#')
            } else {
                m_buffer[index] = 0; // tile space, (L" ACBDEFG=#"[0] = L' ')
            }
        }
    }
}
