#pragma once

#include "draw.h"
#include "mmu.h"
#include <SFML/Graphics.hpp>

class ppu {
  public:
    mmu &gb_mmu; // the central mmu

    sf::RenderWindow &window;

    ppu(mmu &gb_mmu_, sf::RenderWindow &window_);

    // dot = tick = T-cycle
    void tick();
    uint16_t ticks{0}; // tick counter
    uint16_t fetcher_ticks{
        0}; // pixel fetcher ticking for accurate 2 tick counts
    uint8_t dummy_ticks{
        0}; // 8 dummy ticks for dummy fetching (6 to fetch 2 to discard)
    uint16_t mode3_ticks{0}; // how many ticks mode 3 drawing takes
    uint16_t mode0_ticks{0}; // how many ticks mode 0 hblank takes
    void reset_ticks();

    // separate from the other ticks, aligns interrupt with T-cycles
    uint16_t interrupt_ticks{0};

    // 2 fifos
    std::vector<uint8_t> background_fifo{}; // 2 bits
    uint8_t oam_search_counter{0};          // count oam searched

    uint8_t window_ly{0}; // window internal counter
    bool wy_condition{
        false}; // triggers window if wy == ly at some point in this frame

    uint8_t tile_index{0}; // saved tile index for fetcher
    uint8_t tile_id{0};    // saved tile_id for grabbing
    bool fetch_window{false};
    bool fetch_sprite{false};

    // saved low byte and high byte for processing
    uint8_t low_byte{};
    uint8_t high_byte{};
    // save the high byte address from fetch low byte step
    uint16_t high_byte_address{};

    // dummy fetch once per scanline
    bool dummy_fetch{true};

    // scx mod 8 bg pixels discard
    uint8_t scx_discard_count{0};
    bool scx_discard{true}; // keep track of if we did the discard already

    // boolean value which VBlank checks to end the frame
    bool end_frame{false};

    struct oam_entry {
        uint8_t x{};       // x position
        uint8_t y{};       // y position
        uint8_t tile_id{}; // tile #
        uint8_t flags{};   // sprite flags
    };

    struct sprite_fifo_pixel {
        uint8_t x{};
        uint8_t color_id{};
        uint8_t flags{};
    };

    std::vector<sprite_fifo_pixel> sprite_fifo{}; // the whole pixel

    // interrupts, stat handling
    bool current_interrupt_line{false}; // 0x48 interrupt (LCD)

    uint8_t interrupt_t_cycle{0};
    // the exact T cycle (T1, T2, T3, T4) that the rising edge occured
    uint8_t interrupt_m_cycle{0};
    // the exact M-cycle (1-114) that the rising edge occured
    // uint8_t interrupt_delay{0}; // set the interrupt IF later on
    // 255 means off

    void increment_ly();

    // calculate t and m cycles of current tick
    //std::tuple<uint8_t, uint8_t> get_current_cycle();

    bool vblank_start{false};
    void interrupt_line_check();

    enum class ppu_mode {
        OAM_Scan = 2,
        Drawing = 3,
        HBlank = 0,
        VBlank = 1,
        LCDToggledOn = 4
    };

    enum class fetcher_mode {
        FetchTileNo,
        FetchTileDataLow,
        FetchTileDataHigh,
        PushToFIFO
    };

    // setting stat.mode might be delayed
    ppu_mode current_mode{ppu_mode::OAM_Scan};
    // setting last_mode
    ppu_mode last_mode{ppu_mode::OAM_Scan};

    bool extend_oam_write_block{false};

    // the exact T cycle mode changes (1, 2, 3, 4)
    uint8_t mode_t_cycle{0};
    // the M-cycle group (1-114) that mode changed
    uint8_t mode_m_cycle{0};

    fetcher_mode current_fetcher_mode{fetcher_mode::FetchTileNo};

    std::vector<oam_entry> sprite_buffer{}; // sprite buffer

    std::vector<oam_entry> sprites_to_fetch{};
    oam_entry *sprite_to_fetch{nullptr};

    // update ppu mode
    void update_ppu_mode(ppu_mode mode);
    uint16_t mode_change_ticks{0};

    // lcd x position
    uint8_t lcd_x{0};

    // keep track of each scanline
    void reset_scanline();

    // lcd was reset
    bool lcd_reset{false};

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

    // internal LY, (real LY) different from LY register value
    uint8_t internal_ly{0};

    // pixel drawing
    sf::Image lcd_frame_image{};
    sf::Texture lcd_frame{};

    sf::Color get_pixel_color(
        uint8_t pixel,
        uint8_t palette = 2); // 2 means get BGP (non sprite palette)

  private:
    // functions
    uint8_t _get(uint16_t address);
    void _set(uint16_t address, uint8_t value);

    // palette
    const std::array<uint8_t, 3> color_palette_white{181, 175, 66};
    const std::array<uint8_t, 3> color_palette_light_gray{145, 155, 58};
    const std::array<uint8_t, 3> color_palette_dark_gray{93, 120, 46};
    const std::array<uint8_t, 3> color_palette_black{58, 81, 34};
};
