#pragma once
#include "common.hpp"

// Some 2D grid implemented as a giant 1D char array.
template<typename T> class FieldBuffer {
public:
    const size_t width; // x-axis size (`#columns`)
    const size_t height; // y-axis size (`#rows`)
    const size_t area; // `#elements` in buffer (`width * height`)
protected:
    T *m_buffer; // `new` allocated, be careful!
public:
    FieldBuffer(size_t target_width, size_t target_height)
    : width(target_width)
    , height(target_height)
    , area(target_width * target_height)
    , m_buffer(new T[area]) {}

    ~FieldBuffer() {
        delete[] m_buffer;
    }
};
