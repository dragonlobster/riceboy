#include "joypad.h"
#include <assert.h>

void joypad::handle_press(buttons button) {

    switch (button) {
    case buttons::Up    : up = true; break;
    case buttons::Down  : down = true; break;
    case buttons::Left  : left = true; break;
    case buttons::Right : right = true; break;
    case buttons::Start : start = true; break;
    case buttons::Select: select = true; break;
    case buttons::A     : a = true; break;
    case buttons::B     : b = true; break;
    }

    if (button_select) {
        // start, select, b, a - 3, 2, 1, 0
        this->ff00_joyp =
            this->ff00_joyp ^ !start << 3 ^ !select << 2 ^ !b << 1 ^ !a;
    }

    if (dpad_select) {
        this->ff00_joyp =
            this->ff00_joyp ^ !down << 3 ^ !up << 2 ^ !left << 1 ^ !right;
    }
}

void joypad::handle_release(buttons button) {

    switch (button) {
    case buttons::Up    : up = false; break;
    case buttons::Down  : down = false; break;
    case buttons::Left  : left = false; break;
    case buttons::Right : right = false; break;
    case buttons::Start : start = false; break;
    case buttons::Select: select = false; break;
    case buttons::A     : a = false; break;
    case buttons::B     : b = false; break;
    }

    if (button_select) {
        // start, select, b, a - 3, 2, 1, 0
        this->ff00_joyp =
            this->ff00_joyp ^ !start << 3 ^ !select << 2 ^ !b << 1 ^ !a;
    }

    if (dpad_select) {
        this->ff00_joyp =
            this->ff00_joyp ^ !down << 3 ^ !up << 2 ^ !left << 1 ^ !right;
    }
}

void joypad::handle_write(uint8_t value) {

    button_select = !(value >> 5 & 1);
    dpad_select = !(value >> 4 & 1);

    if (button_select) {
        // start, select, b, a - 3, 2, 1, 0
        this->ff00_joyp =
            this->ff00_joyp ^ !start << 3 ^ !select << 2 ^ !b << 1 ^ !a;
    }

    if (dpad_select) {
        this->ff00_joyp =
            this->ff00_joyp ^ !down << 3 ^ !up << 2 ^ !left << 1 ^ !right;
    }
}