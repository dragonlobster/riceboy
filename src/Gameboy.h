#pragma once

#include "CPU.h"
#include "MMU.h"
#include "PPU.h"
#include "Timer.h"
#include <SFML/Graphics.hpp>

class Gameboy {

  public:

    sf::RenderWindow &window; // the sfml window (need to draw)
    Gameboy(sf::RenderWindow &window_) : window(window_) {
        this->gb_mmu.set_timer(gb_timer);
        this->gb_timer.set_mmu(gb_mmu);
    };

    // dimensions
    static constexpr unsigned int WIDTH{160};
    static constexpr unsigned int HEIGHT{144};

    // clock speed
    static constexpr unsigned int clock_speed = 4194304;
    // 4194304 T-cycles per second (tick cycles)

    // TODO: mmu, cpu, ppu
    MMU gb_mmu{};
    Timer gb_timer{};
    CPU gb_cpu = CPU(gb_mmu);
    PPU gb_ppu = PPU(gb_mmu, window);

    void tick();

  private:
};