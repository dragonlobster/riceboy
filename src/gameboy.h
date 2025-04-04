#pragma once

#include "cpu.h"
#include "mmu.h"
#include "ppu.h"
#include <SFML/Graphics.hpp>

class gameboy {

  public:
    sf::RenderWindow &window; // the sfml window (need to draw)
    gameboy(sf::RenderWindow &window_) : window(window_) {};

    // dimensions
    static constexpr unsigned int WIDTH{160};
    static constexpr unsigned int HEIGHT{144};

    // clock speed
    static constexpr unsigned int clock_speed = 4194304;
    // 4194304 T-cycles per second (background_tick cycles)

    // TODO: mmu, cpu, ppu
    mmu gb_mmu{};
    cpu gb_cpu = cpu(gb_mmu);
    ppu gb_ppu = ppu(gb_mmu, window);

    // used for initial values for the first time when the boot rom finished
    bool need_to_initialize_values{false};
    void initialize_values();

    void tick();

  private:
};
