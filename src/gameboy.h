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
    timer gb_timer{};
    interrupt gb_interrupt{};
    ppu gb_ppu = ppu(gb_interrupt, window);
    joypad gb_joypad{};
    mmu gb_mmu{gb_timer, gb_interrupt, gb_ppu, gb_joypad};
    cpu gb_cpu = cpu(gb_mmu, gb_timer, gb_interrupt);

    void tick();

    // skip the boot rom?
    void skip_bootrom();

  private:
};
