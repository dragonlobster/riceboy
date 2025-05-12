#include "interrupt.h"
#include <tuple>

void interrupt::check_current_interrupt() {
    uint8_t _ie = this->interrupt_enable_flag;
    uint8_t _if = this->interrupt_flags;

    uint8_t ie_joypad = (_ie >> 4) & 1;
    uint8_t ie_serial = (_ie >> 3) & 1;
    uint8_t ie_timer = (_ie >> 2) & 1;
    uint8_t ie_lcd = (_ie >> 1) & 1;
    uint8_t ie_vblank = _ie & 1;

    uint8_t if_joypad = (_if >> 4) & 1;
    uint8_t if_serial = (_if >> 3) & 1;
    uint8_t if_timer = (_if >> 2) & 1;
    uint8_t if_lcd = (_if >> 1) & 1;
    uint8_t if_vblank = _if & 1;

    interrupts interrupt{interrupts::none};
    if_mask mask{if_mask::none};

    if (ie_vblank && if_vblank) {
        // vblank interrupt
        interrupt = interrupts::vblank;
        mask = if_mask::vblank;
    } else if (ie_lcd && if_lcd) {
        // lcd interrupt
        interrupt = interrupts::lcd;
        mask = if_mask::lcd;
    } else if (ie_timer && if_timer) {
        // timer interrupt
        interrupt = interrupts::timer;
        mask = if_mask::timer;
    } else if (ie_serial && if_serial) {
        // serial interrupt
        interrupt = interrupts::serial;
        mask = if_mask::serial;
    } else if (ie_joypad && if_joypad) {
        // joypad interrupt
        interrupt = interrupts::joypad;
        mask = if_mask::joypad;
    }

    this->current_interrupt = interrupt;
    this->current_if_mask = mask;
}
