#ifndef OLC_CONSOLE_TETRIS_HPP
#define OLC_CONSOLE_TETRIS_HPP
#include "include/common.hpp"

#include <vector>

#include "include/display.hpp"
#include "include/playingfield.hpp"

class Tetris {
private:
    // List of valid tetris pieces. Each tetronimo is a 4x4 block.
    const std::vector<std::wstring> m_tetrominos = {
        // [0] 4x1: vertical line
        L"..X."
         "..X."
         "..X."
         "..X.",
        // [1] 3+1: T shape
        L"..X."
         ".XX."
         "..X."
         "....", 
        // [2] 2x2: square
        L"...."
         ".XX."
         ".XX."
         "....", 
        // [3] 2+2: Z shape
        L"..X."
         ".XX."
         ".X.."
         "....",
        // [4] 2+2: S shape
        L".X.."
         ".XX."
         "..X."
         "....",
        // [5] 3+1: L shape
        L".X.."
         ".X.."
         ".XX."
         "....",
        // [6] 3+1: Mirror L
        L"..X."
         "..X."
         ".XX."
         "...."
    };

    PlayingField *m_pfield; // Can only use a pointer with this declaration.
    Display *m_display; // Can only use a pointer with this declaration.

    /**
     * Get the correct index into a piece based on its rotation.
     * @param piece_x `px`, piece's x-axis cell, from the caller loop.
     * @param piece_y `py`, piece's y-axis cell, from the caller loop.
     * @param rotation Value of rotation. We modulo by 4 internally.
     */
    int rotate(int piece_x, int piece_y, int rotation) {
        int piece_index = 0;
        switch (rotation % 4) {
            /** 0 DEGREES:       0  1  2  3
                                 4  5  6  7
                                 8  9 10 11 
                                12 13 14 15 */
            case 0: piece_index = (piece_y * 4) + piece_x; break;
            
            /** 90 DEGREES:     12  8  4  0
                                13  9  5  1
                                14 10  6  2 
                                15 11  7  3 */
            case 1: piece_index = 12 + piece_y - (piece_x * 4); break;

            /** 180 DEGREES:    15 14 13 12
                                11 10  9  8
                                 7  6  5  4
                                 3  2  1  0 */
            case 2: piece_index = 15 - (piece_y * 4) - piece_x; break;

            /** 270 DEGREES:    3  7 11 15
                                2  6 10 14
                                1  5  9 13
                                0  4  8 12 */
            case 3: piece_index = 3 - piece_y + (piece_x * 4); break;
        }
        return piece_index;
    }

    /**
     * @param tetromino_id ID to be used as index into global `tetromino` array.
     * @param rotation Value of rotation to be passed to the `rotate` function.
     * @param field_x Piece X-position in playing field, may add/subtract 1.
     * @param field_y Piece Y-position in playing field, may add/subtract 1.
     */
    bool piece_fits(int tetromino_id, int rotation, int field_x, int current_y) {
        // Check each cell of the reference tetromino piece
        for (int px = 0; px < 4; px++) {
            for (int py = 0; py < 4; py++) {
                // Get index into the reference piece given the rotation
                int pi = rotate(px, py, rotation);
                // Get index into the field
            }
        }
        return false;
    }
public:
    Tetris(int field_width, int field_height, int screen_width, int screen_height) {
        m_pfield = new PlayingField(field_width, field_height);
        m_display = new Display(screen_width, screen_height);
        std::cout << "m_pfield: " << m_pfield << "\n" << m_pfield->get_data() << "\n";
        std::cout << "m_display: " << m_display << "\n" << m_display->get_data() << "\n";
    }

    // Also calls the `pfield`'s destructor, very neat!
    ~Tetris() {
        std::cout << "deleting (Tetris) this->m_pfield\n";
        delete m_pfield;
        std::cout << "deleting (Tetris) this->m_display\n";
        delete m_display;
    }

    // testing function
    void dump_pieces() {
        std::wcout << m_tetrominos.size() << L" elements.\n";
        for (auto &ws : m_tetrominos) {
            // std::wcout << ws << L"\n";
            for (int i = 0, limit = ws.size(); i < limit; i++) {
                if (i > 0 && ((i % 4) == 0)) {
                    std::cout << "\n";
                }
                std::cout << (char) ws[i];
            }
            std::cout << "\n\n";
        }
    }
};

#endif // OLC_CONSOLE_TETRIS_HPP
