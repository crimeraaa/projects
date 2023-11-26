#include "include/player.hpp"

void Player::input() {
    for (int i = 0; i < KEYS_COUNT; i++) {
        this->controls[i].update();
    }
}

bool Player::is_pressing(enum Keys k) {
    return this->controls[k].down;
}
