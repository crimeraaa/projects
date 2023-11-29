#pragma once
#include "common.hpp"

class GameState {
public:
    bool game_over; // Can also use in debugging to determine memory cleanup.
    size_t current_speed; // `#milliseconds` between ticks. Decrease to speed up.
    size_t speed_counter; // If `== current_speed`, speed up the game by 1 ms!
    bool force_down; // Stagger rotation so it's easier to use.

    GameState(size_t starting_speed)
    : game_over{false}
    , current_speed{starting_speed}
    , speed_counter{0}
    , force_down{false} {}

    // Just increments `speed_counter` and checks `force_down`.
    // If `current_speed == speed_counter`, set `force_down = true`.
    void update() {
        speed_counter++;
        force_down = (current_speed == speed_counter);
    }
};
