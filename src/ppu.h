#pragma once

#include "mmu.h"
#include <array>;
#include <vector>
#include <SFML/Graphics.hpp>

class fetcher {
  public:
    mmu &gb_mmu;

    const uint8_t *LY{};
    const uint8_t *SCX{};
    const uint8_t *SCY{};
    const uint8_t *LCDC{};
    const uint8_t *BGP{};

    fetcher(mmu &gb_mmu_) : gb_mmu(gb_mmu_) {
        this->LCDC = this->gb_mmu.get_pointer_from_address(0xff40);
        this->LY = this->gb_mmu.get_pointer_from_address(0xff44);
        this->SCY = this->gb_mmu.get_pointer_from_address(0xff42);
        this->SCX = this->gb_mmu.get_pointer_from_address(0xff43);
        this->BGP = this->gb_mmu.get_pointer_from_address(0xff47);
    };

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
    std::vector<uint8_t> fifo{}; // 2 bits

};

class ppu {
  public:
    mmu &gb_mmu; // the central mmu

    sf::RenderWindow &window;

    ppu(mmu &gb_mmu_, sf::RenderWindow &window_)
        : gb_mmu(gb_mmu_), window(window_) {
        this->LCDC = this->gb_mmu.get_pointer_from_address(0xff40);
        this->STAT = this->gb_mmu.get_pointer_from_address(0xff41);
        this->SCY = this->gb_mmu.get_pointer_from_address(0xff42);
        this->SCX = this->gb_mmu.get_pointer_from_address(0xff43);
        this->LY = this->gb_mmu.get_pointer_from_address(0xff44);
        this->LYC = this->gb_mmu.get_pointer_from_address(0xff45);
        this->DMA = this->gb_mmu.get_pointer_from_address(0xff46);
        this->BGP = this->gb_mmu.get_pointer_from_address(0xff47);
        this->OBP0 = this->gb_mmu.get_pointer_from_address(0xff48);
        this->OBP1 = this->gb_mmu.get_pointer_from_address(0xff49);
        this->WX = this->gb_mmu.get_pointer_from_address(0xff4b);
        this->WY = this->gb_mmu.get_pointer_from_address(0xff4a);

        sf::Image image({window.getSize().x, window.getSize().y},
                        sf::Color::White);
        this->lcd_dots_image = image;
    }; // pass mmu by reference

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
    enum class mode { OAM_Scan = 2, Drawing = 3, HBlank = 0, VBlank = 1 };

    mode current_mode{2}; // start at OAM Scan

    // tick counter
    uint16_t ppu_ticks{0}; // resets every LY

    // fetcher

    // initialize the fetcher
    fetcher ppu_fetcher = fetcher(gb_mmu);

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
    std::vector<uint8_t> lcd_dots{}; // color only
    /* DEBUG ONLY */

    sf::Image lcd_dots_image{};
    sf::Texture lcd_dots_texture;
    //sf::Sprite lcd_dots_sprite;

    // palette
    //std::array<std::array<uint16_t, 3>, 4> bg_lcd_palette 
    uint8_t bg_lcd_palette[4][3] = {{58, 81, 34}, {93, 120, 46}, {145, 155, 58}, {181, 175, 66}};

    /* functions */
    void add_to_sprite_buffer(
        std::array<uint8_t, 4> oam_entry); // each sprite is 4 bytes
};
