#include "Timer.h"
#include "MMU.h"

void Timer::tick(bool inc) {
    if (inc) {
        // runs every M-cycle
        this->ticks++;
        if (ticks < 4) {
            return;
        }
        this->ticks = 0;

        this->div_ff04+=4;

        if (tima_overflow) {
            // TODO: set timer interrupt
            uint8_t _if = this->gb_mmu->read_memory(0xff0f);
            this->gb_mmu->write_memory(0xff0f, _if | 4);

            // reset timer modulo
            this->set_tima(this->tma_ff06);
            //tima_overflow = false;
        }
    }

    uint8_t div_bit = 9;
    switch (tac_ff07 & 3) {
    case 1: div_bit = 3; break;
    case 2: div_bit = 5; break;
    case 3: div_bit = 7; break;
    }

    uint8_t timer_enable = (tac_ff07 >> 2) & 1;
    uint8_t new_div_bit = ((this->div_ff04 >> div_bit) & 1) & timer_enable;

    if (old_div_bit == 1 && new_div_bit == 0) {
        // falling edge
        //uint8_t tima = _get(0xff05);
        if (this->tima_ff05 == 0xff) {
            // just wrap it now and wait 4 t-cycles
            this->tima_ff05 = 0;
            this->tima_overflow = true;
        }
        else {
            this->tima_ff05++;
        }
    }
    old_div_bit = new_div_bit;
}

uint8_t Timer::get_div() const {
    return (this->div_ff04 & 0xff00) >> 8;
}
void Timer::set_div() {
    this->div_ff04 = 0;
    this->tick(false);
}

uint8_t Timer::get_tima() const {
    return this->tima_ff05;
}
void Timer::set_tima(uint8_t value) {
    this->tima_ff05 = value;
    this->tima_overflow = false;
}

uint8_t Timer::get_tac() const {
    return this->tac_ff07;
}
void Timer::set_tac(uint8_t value) {
    this->tac_ff07 = value;
    this->tick(false);
}