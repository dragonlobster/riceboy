#include "joypad.h"
#include <assert.h>

uint8_t joypad::handle_read() {
    // Start with upper bits 7-6 always set to 1
    uint8_t value = 0xc0;

    // Add selection bits 5-4 (0=selected, 1=not selected)
    value |= ((!dpad) << 4) | ((!buttons) << 5);

    // Default all button states to "not pressed" (1)
    uint8_t button_states = 0xf;

    if (buttons) {
        button_states &= ~(start << 3);
        button_states &= ~(select << 2);
        button_states &= ~(b << 1);
        button_states &= ~(a);
    }

    else if (dpad) {
        button_states &= ~(down << 3);
        button_states &= ~(up << 2);
        button_states &= ~(left << 1);
        button_states &= ~(right);
    }

    return value | button_states;
}

void joypad::handle_write(uint8_t value) {

    uint8_t buttons_bit = value >> 5 & 1;
    uint8_t dpad_bit = value >> 4 & 1;

    this->buttons = !buttons_bit;
    this->dpad = !dpad_bit;
}