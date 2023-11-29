// Could be a precompiled header (PCH)
// compile: g++ -o common.hpp.gch common.hpp
// check: g++ -H -o tetris tetris.cpp
// -H flag dumps all the included files and paths.

// Don't use pragma for PCH
// #pragma once
#ifndef OLC_REFACTOR_TETRIS_COMMON_PCH
#define OLC_REFACTOR_TETRIS_COMMON_PCH

#include <array>
#include <iostream>
#include <random>
#include <string>
#include <thread>
#include <vector>

// Windows API 
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

// Hate this so use it already
using std::size_t;

#endif // OLC_REFACTOR_TETRIS_COMMON_PCH
