#include "timer.h"
#include <assert.h>

void timer::intialize_values() {
    this->sysclock = 0xabc8;
    this->tac_ff07 = 0xf8;
}

void timer::falling_edge() {
    // falling edge should be run on: write to div, write to tac, and increment
    // div TIMA increments in the case of a falling edge occuring
    uint8_t timer_enable_bit = (this->tac_ff07 & 4) >> 2;
    uint8_t tac_freq_bit = tac_ff07 & 3;

    assert(tac_freq_bit <= 3 && "tac freq bit abnormal!");
    assert(timer_enable_bit <= 1 && "tac timer bit abnormal!");

    uint8_t div_bit{9};
    switch (tac_freq_bit) {
    case 1: div_bit = 3; break;
    case 2: div_bit = 5; break;
    case 3: div_bit = 7; break;
    }

    uint8_t div_state = ((this->sysclock >> div_bit) & 1) & (timer_enable_bit);

    assert(div_state <= 1 && "div state abnormal!");

    if (div_state == 0 && last_div_state == 1 &&
        !tima_overflow) { // skip incrementing on tima overflow, it's no use
        this->tima_ff05++;

        if (tima_ff05 == 0) {
            // tima overflowed (pass 255 wrapped to 0)
            this->tima_overflow = true;
        }
    }
    this->last_div_state = div_state;
}

void timer::increment_sysclock() {
    this->sysclock += 4;
    falling_edge();
}
