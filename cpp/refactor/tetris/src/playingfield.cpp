#include "include/playingfield.hpp"

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
    return (test_x < this->width) && (test_y < this->height);
}

bool PlayingField::is_in_bounds(size_t index) {
    return index < this->area;
}

unsigned char &PlayingField::operator[](size_t index) {
    return m_field[index];
}
