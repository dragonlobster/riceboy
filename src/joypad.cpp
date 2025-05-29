#include "joypad.h"
#include "joypad.h"
#include <assert.h>

void joypad::handle_button(buttons button, bool pressed) {
    // Set button state
    switch (button) {
    case buttons::Up:    up = pressed; break;
    case buttons::Down:  down = pressed; break;
    case buttons::Left:  left = pressed; break;
    case buttons::Right: right = pressed; break;
    case buttons::Start: start = pressed; break;
    case buttons::Select: select = pressed; break;
    case buttons::A:     a = pressed; break;
    case buttons::B:     b = pressed; break;
    }
    
    // Update register based on which button matrix is selected
    update_joyp_register();
}

void joypad::handle_write(uint8_t value) {
    // Set selection bits (bits 4-5)
    button_select = !(value & 0x20);  // Bit 5
    dpad_select = !(value & 0x10);    // Bit 4
    
    // Always preserve bits 6-7 as 1
    ff00_joyp = (value & 0xf0) | 0xc0;
    
    // Update register with button states
    update_joyp_register();
}

void joypad::update_joyp_register() {
    // Always set upper two bits (unused), clear lower nibble
    ff00_joyp = 0xc0 | (ff00_joyp & 0xf0);
    
    // Set lower nibble based on selection
    if (button_select) {
        // Buttons: 0=pressed, 1=released
        // Bit 3=Start, 2=Select, 1=B, 0=A
        if (!start)  ff00_joyp |= 0x08;
        if (!select) ff00_joyp |= 0x04;
        if (!b)      ff00_joyp |= 0x02;
        if (!a)      ff00_joyp |= 0x01;
    }
    
    if (dpad_select) {
        // D-pad: 0=pressed, 1=released
        // Bit 3=Down, 2=Up, 1=Left, 0=Right
        if (!down)  ff00_joyp |= 0x08;
        if (!up)    ff00_joyp |= 0x04;
        if (!left)  ff00_joyp |= 0x02;
        if (!right) ff00_joyp |= 0x01;
    }
}