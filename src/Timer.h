#pragma once

#include <cstdint>
class MMU; // dependency injection (avoids circular depdencies)

class Timer {

  public:
    // pointer to mmu
    MMU *gb_mmu{};
    void set_mmu(MMU &mmu) {
        this->gb_mmu = &mmu;
    }
    //Timer(MMU* mmu_ptr) : gb_mmu(mmu_ptr) {}

    uint16_t ticks{0};
    uint16_t old_div_bit{0};
    bool tima_overflow{false};

    uint16_t div_ff04{0};
    uint8_t tima_ff05{0};
    uint8_t tma_ff06{0};
    uint8_t tac_ff07{0};

    uint8_t get_div() const; // get high 8 bits
    void set_div();    // sets to 0

    uint8_t get_tima() const;
    void set_tima(uint8_t);

    uint8_t get_tac() const;
    void set_tac(uint8_t);

    void tick(bool inc = true);
};



