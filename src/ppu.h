#pragma once

#include "mmu.h"
#include <array>;

class ppu {
  public:
    ppu(mmu &mmu); // pass mmu by reference
    mmu *gb_mmu{}; // the central mmu

    // WY register: in memory 0xff4a
    // WX register: in memory 0xff4b (7 means left of screen)
    // LY = current scanline #

    // LCDC (LCD control): ff40
    // STAT (LCD Status): ff41
    // SCY (scroll y): ff42
    // SCX (scroll x):: ff43
    // LY (Scanline #): ff44
    // LYC (ly compare): ff45
    // DMA (dma transfer and start): ff46
    // BGP (bg palette): ff47
    // OBP0 (object palette 0): ff48
    // OBP1 (object palette 0): ff49
    // WY (window Y pos): ff4a
    // WX (window X pos): ff4b

    uint8_t *LY{};
    uint8_t *LCDC{};
    uint8_t *STAT{};
    uint8_t *SCY{};
    uint8_t *SCX{};
    uint8_t *LYC{};
    uint8_t *DMA{};
    uint8_t *BGP{};
    uint8_t *OBP0{};
    uint8_t *OBP1{};
    uint8_t *WX{};
    uint8_t *WY{};

    // 4 modes
    enum class mode {
        OAM_Scan = 2,
        Drawing = 3,
        HBlank = 0,
        VBlank = 1
    };

    mode current_mode{};

    // tick counter
    uint8_t ticks{0};

    // main tick function
    void tick();

    // pixel fetcher every 2 T-cycles
    void pixel_fetcher_tick();

    // background map 0x9800 to 0x9bff, tile data: 0x8000 to 0x8fff


    // TODO: maybe use std::array
    // sprite buffer - stores 10 pixels (10 2-bits)
    std::array<uint8_t, 10> sprite_buffer; // 2 bits

    // background FIFO, sprite FIFO - stores 8 2-bit (for 8 pixels) (for pixel fetcher)
    uint8_t background_fifo[8]{}; // 2 bits
    uint8_t sprite_fifo[8]{}; // 2 bits

    /* functions */
    void add_to_sprite_buffer();

};
