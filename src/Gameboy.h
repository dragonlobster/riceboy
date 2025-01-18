#pragma once

#include "cpu.h"
#include "mmu.h"
#include "ppu.h"
#include <SFML/Graphics.hpp>

class Gameboy {

  public:

    sf::RenderWindow &window; // the sfml window (need to draw)
    Gameboy(sf::RenderWindow &window_) : window(window_) {};

    // dimensions
    static constexpr unsigned int WIDTH{160};
    static constexpr unsigned int HEIGHT{144};

    // clock speed
    static constexpr unsigned int clock_speed = 4194304;
    // 4194304 T-cycles per second (tick cycles)

    // TODO: mmu, cpu, ppu
    mmu gb_mmu{};
    cpu gb_cpu = cpu(gb_mmu);
    ppu gb_ppu = ppu(gb_mmu);

  private:
};