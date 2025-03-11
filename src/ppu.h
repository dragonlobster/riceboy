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
    uint16_t background_fetcher_ticks{0};
    uint16_t sprite_fetcher_ticks{0};
    uint16_t tile_index{0}; // incremented after every push to FIFO
    bool window_fetch {false};

    void background_tick();
    void sprite_tick();

    enum class mode {
        FetchTileNo = 0,
        FetchTileDataLow = 1,
        FetchTileDataHigh = 2,
        PushToFIFO = 3,
        Paused = 4
    };

    uint8_t tile_id{0};                   // the current tile id read
    std::array<uint8_t, 4> sprite_tile{}; // the current sprite tile read
    mode background_current_mode{0};      // start at fetch tile no
    mode sprite_current_mode{0};          // start at fetch tile no

    // need to run dummy fetch? (once per scanline)
    bool dummy_fetch{true};
    void reset(); // reset on every scanline

    // temporary pixel buffer before merging to fifo
    // stores all 1 bit numbers at first, then 2bpp
    // stored from lsb to msb, need to push out backwards to FIFO (first in
    // first out)
    std::array<uint8_t, 8> temp_background_fifo{};
    std::array<uint8_t, 8> temp_sprite_fifo{};

    // background FIFO - stores 8 2-bit (for 8 pixels) (for pixel fetcher)
    std::vector<uint8_t> background_fifo{}; // 2 bits

    // OAM sprite metadata 0xfe00 - 0xfe9f
    uint8_t sprite_buffer_counter{0};
    const uint16_t OAM_START_ADDRESS{0xfe00};
    const uint16_t OAM_END_ADDRESS{0xfe9f};

    // # of pixels output to the current scanline
    int16_t lcd_x_position{0};

    // TODO: maybe use std::array
    // store sprite metadata (4 bytes each), fits up to 10 pixels
    std::vector<std::array<uint8_t, 4>> sprite_buffer{};
    // sprite FIFO - stores 8 2-bit (for 8 pixels) (for pixel fetcher)
    std::vector<uint8_t> sprite_fifo{}; // 2 bits

    // sprite attribute fifo - stores the sprite fifo attributes for palette and
    // other flags
    std::vector<uint8_t> sprite_attr_fifo{}; // 2 bits

    // sprites to fetch based on X + 8 from OAM buffer
    std::vector<std::array<uint8_t, 4>> sprite_fetch_buffer{};

    // window fetching condition
    bool wy_condition{false};

    // window ly position
    uint8_t window_ly{0};

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

    // background_tick counter
    uint16_t ppu_ticks{0}; // resets every LY

    // fetcher

    // initialize the fetcher
    Fetcher ppu_fetcher = Fetcher(gb_mmu);

    // main background_tick function
    void tick();

    // background map 0x9800 to 0x9bff, tile data: 0x8000 to 0x8fff

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

    // return pixel color
    sf::Color get_pixel_color(uint8_t pixel, uint8_t *palette = nullptr); // palette 0 OBP0, palette 1 OBP1

    /* functions */

    // byte 0 = y position, byte 1 = x position, byte 2 = tile number, byte 3 =
    void add_to_sprite_buffer(
        std::array<uint8_t, 4> oam_entry); // each sprite is 4 bytes


  private:
    // color id
    std::array<uint8_t, 3> _get_color(uint8_t id);

    // functions
    uint8_t _get(uint16_t address);
    void _set(uint16_t address, uint8_t value);
};
