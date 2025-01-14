#pragma once

#include "cpu.h"
#include "mmu.h"

class Gameboy {

  public:
    // dimensions
    static const unsigned int WIDTH{160};
    static const unsigned int HEIGHT{144};

    // clock speed
    static const unsigned int clock_speed = 4194304;
    // 4194304 T-cycles per second (tick cycles)

    // TODO: mmu, cpu, ppu
    mmu gb_mmu{};
    cpu gb_cpu = cpu(gb_mmu);

  private:
};