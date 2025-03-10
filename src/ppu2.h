#pragma once

#include "MMU.h"
#include <SFML/Graphics.hpp>

class PPU2 {
  public:
    MMU &gb_mmu; // the central mmu

    sf::RenderWindow &window;

    PPU2(MMU &gb_mmu_, sf::RenderWindow &window_);

    // a "dot"
    void tick();
    uint16_t ticks{0}; // tick counter

    // 2 fifos
    std::vector<uint8_t> background_fifo{}; // 2 bits
    uint8_t oam_search_counter{0};          // count oam searched

    uint8_t window_ly{0};  // window internal counter
    uint8_t tile_index{0}; // saved tile index for fetcher
    uint8_t tile_id{0};    // saved tile_id for grabbing
    bool fetch_window{false};
    bool fetch_sprite{false};

    // saved low byte and high byte for processing
    uint8_t low_byte{};
    uint8_t high_byte{};

    // dummy fetch once per scanline
    bool dummy_fetch{true};

    struct oam_entry {
        uint8_t x{};           // x position
        uint8_t y{};           // y position
        uint8_t tile_id{}; // tile #
        uint8_t flags{};       // sprite flags
    };

    struct sprite_fifo_pixel {
        uint8_t color_id{};
        uint8_t flags{};
    };

    std::vector<sprite_fifo_pixel> sprite_fifo{};     // the whole pixel

    enum class ppu_mode { OAM_Scan, Drawing, HBlank, VBlank };

    enum class fetcher_mode {
        FetchTileNo,
        FetchTileDataLow,
        FetchTileDataHigh,
        PushToFIFO
    };

    ppu_mode current_mode{ppu_mode::OAM_Scan};

    fetcher_mode current_fetcher_mode{fetcher_mode::FetchTileNo};

    std::vector<oam_entry> sprite_buffer{}; // sprite buffer

    std::vector<oam_entry> sprites_to_fetch{};
    oam_entry sprite_to_fetch{};

    // lcd x position
    uint8_t lcd_x{0};

    // registers
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

    // pixel drawing
    sf::Image lcd_dots_image{};
    sf::Texture lcd_dots_texture;

  private:
    // functions
    uint8_t _get(uint16_t address);
    void _set(uint16_t address, uint8_t value);
};
