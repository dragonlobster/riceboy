#pragma once

#include "MMU.h"
#include <SFML/Graphics.hpp>
#include <array>
#include <vector>

class Fetcher {
  public:
    MMU &gb_mmu;

    const uint16_t LCDC{0xff40};
    const uint16_t LY{0xff44};
    const uint16_t SCY{0xff42};
    const uint16_t SCX{0xff43};
    const uint16_t BGP{0xff47};

    Fetcher(MMU &gb_mmu_) : gb_mmu(gb_mmu_) {};

    // fetcher states
    uint16_t fetcher_ticks{0};
    uint16_t tile_index{0}; // incremented after every push to FIFO
    void tick();
    enum class mode {
        FetchTileNo = 0,
        FetchTileDataLow = 1,
        FetchTileDataHigh = 2,
        PushToFIFO = 3
    };
    uint8_t tile_id{0};   // the current tile id read
    mode current_mode{0}; // start at fetch tile no

    // temporary pixel buffer before merging to fifo
    // stores all 1 bit numbers at first, then 2bpp
    // stored from lsb to msb, need to push out backwards to FIFO (first in
    // first out)
    std::array<uint8_t, 8> pixel_buffer{};

    // background FIFO - stores 8 2-bit (for 8 pixels) (for pixel fetcher)
    std::vector<uint8_t> background_fifo{}; // 2 bits

    // sprite FIFO - stores 8 2-bit (for 8 pixels) (for pixel fetcher)
    std::vector<uint8_t> sprite_fifo{}; // 2 bits

  private:
    // functions
    uint8_t _get(uint16_t address);
    void _set(uint16_t address, uint8_t value);
};

class PPU {
  public:
    MMU &gb_mmu; // the central mmu

    sf::RenderWindow &window;

    PPU(MMU &gb_mmu_, sf::RenderWindow &window_);

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

    uint16_t LCDC{0xff40};
    uint16_t STAT{0xff41};
    uint16_t SCY{0xff42};
    uint16_t SCX{0xff43};
    uint16_t LY{0xff44};
    uint16_t LYC{0xff45};
    uint16_t DMA{0xff46};
    uint16_t BGP{0xff47};
    uint16_t OBP0{0xff48};
    uint16_t OBP1{0xff49};
    uint16_t WX{0xff4b};
    uint16_t WY{0xff4a};

    uint16_t IF{0xff0f};

    // 4 modes
    enum class mode { OAM_Scan = 2, Drawing = 3, HBlank = 0, VBlank = 1 };
    mode last_mode{};

    mode current_mode{2}; // start at OAM Scan

    // tick counter
    uint16_t ppu_ticks{0}; // resets every LY

    // fetcher

    // initialize the fetcher
    Fetcher ppu_fetcher = Fetcher(gb_mmu);

    // # of pixels output to the current scanline
    uint8_t dot_count{0};

    // main tick function
    void tick();

    // background map 0x9800 to 0x9bff, tile data: 0x8000 to 0x8fff

    // OAM sprite metadata 0xfe00 - 0xfe9f
    uint8_t oam_buffer_counter{0};
    const uint16_t OAM_START_ADDRESS{0xfe00};
    const uint16_t OAM_END_ADDRESS{0xfe9f};

    // TODO: maybe use std::array
    // store sprite metadata (4 bytes each), fits up to 10 pixels
    std::vector<std::array<uint8_t, 4>> oam_buffer{};

    // LCD pixels to display
    /* DEBUG ONLY */
    // std::vector<uint8_t> lcd_dots{}; // color only
    std::vector<uint8_t> lcd_dots{}; // color only
    /* DEBUG ONLY */

    sf::Image lcd_dots_image{};
    sf::Texture lcd_dots_texture;
    // sf::Sprite lcd_dots_sprite;

    // palette
    const std::array<uint8_t, 3> color_palette_white{181, 175, 66};
    const std::array<uint8_t, 3> color_palette_light_gray{145, 155, 58};
    const std::array<uint8_t, 3> color_palette_dark_gray{93, 120, 46};
    const std::array<uint8_t, 3> color_palette_black{58, 81, 34};

    // return dot color
    sf::Color get_dot_color(uint8_t dot);

    /* functions */
    void add_to_sprite_buffer(
        std::array<uint8_t, 4> oam_entry); // each sprite is 4 bytes

  private:
    // color id
    std::array<uint8_t, 3> _get_color(uint8_t id);

    // functions
    uint8_t _get(uint16_t address);
    void _set(uint16_t address, uint8_t value);
};
