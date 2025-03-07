#pragma once

#include "CPU.h"
#include "MMU.h"
#include "PPU2.h"
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
    // 4194304 T-cycles per second (background_tick cycles)

    // TODO: mmu, cpu, ppu
    MMU gb_mmu{};
    CPU gb_cpu = CPU(gb_mmu);
    PPU2 gb_ppu = PPU2(gb_mmu, window);

    void tick();

  private:
};