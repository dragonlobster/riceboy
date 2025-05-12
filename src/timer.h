#pragma once
#include <cstdint>

class timer {
  public:
    uint16_t ticks{0};
    // initialize sysclock to be 4
    uint16_t sysclock{4};
    // increment the sysclock
    void tick();
    // div, timer related
    // div counter stored in mmu
    uint8_t tima_ff05{0}; // tima_ff05
    uint8_t tma_ff06{0};
    uint8_t tac_ff07{0xf8};
    uint8_t last_div_state{0}; // for falling edge detection
    bool tima_overflow{false};
    bool tima_overflow_standby{false};
    bool lock_tima_write{false};
    // increment div by M-cycle
    void falling_edge();

    void intialize_values();
};