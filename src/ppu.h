#pragma once

#include "draw.h"
#include "mmu.h"
#include "interrupt.h"
#include <SFML/Graphics.hpp>

class ppu {
  public:
    interrupt &gb_interrupt;
    sf::RenderWindow &window;

    ppu(interrupt &gb_interrupt, sf::RenderWindow &window_);

    void initialize_skip_bootrom_values();

    uint8_t _get(uint16_t address); // get vram or oam ram;

    // dot = tick = T-cycle
    void tick();
    uint16_t ticks{0}; // tick counter
    uint16_t fetcher_ticks{
        0}; // pixel fetcher ticking for accurate 2 tick counts
    uint16_t sprite_ticks{0}; // sprite fetcher ticks
    uint16_t mode3_ticks{0};  // how many ticks mode 3 drawing takes
    uint16_t mode0_ticks{0};  // how many ticks mode 0 hblank takes
    void reset_ticks();

    // separate from the other ticks, aligns interrupt with T-cycles
    uint16_t interrupt_ticks{0};

    // 2 fifos
    std::vector<uint8_t> background_fifo{}; // 2 bits

    uint8_t oam_search_counter{0}; // count oam searched

    uint8_t window_ly{0}; // window internal counter
    bool wy_condition{
        false}; // triggers window if wy == ly at some point in this frame

    uint8_t tile_index{0};       // saved tile index for fetcher
    bool fetch_window_ip{false}; // fetch window in progress
    bool fetch_sprite_ip{false}; // fetch sprite in progress

    uint8_t bg_tile_id{0}; // saved tile_id for grabbing
    // saved low byte and high byte for processing
    uint8_t bg_low_byte{};
    uint8_t bg_high_byte{};
    // save the high byte address from fetch low byte step
    uint16_t bg_high_byte_address{};

    uint8_t sprite_tile_id{0}; // saved tile_id for grabbing
    // saved low byte and high byte for processing
    uint8_t sprite_low_byte{};
    uint8_t sprite_high_byte{};
    // save the high byte address from fetch low byte step
    uint16_t sprite_high_byte_address{};

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

    void increment_ly();

    // calculate t and m cycles of current tick
    // std::tuple<uint8_t, uint8_t> get_current_cycle();

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

    // sprite fetch tile data low function
    void sprite_fetch_tile_data_low(oam_entry sprite);
    // sprite push to fifo function
    void sprite_push_to_fifo(oam_entry sprite);

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
    // oam_entry *sprite_to_fetch{nullptr};

    // fetch sprite method so i can control the precise timing, returns total
    // stall
    void fetch_sprites();
    // uint8_t sprite_fetch_stall_cycles{0};
    uint8_t sprite_fetch_stall_cycles{0};
    // sprite_accumulated_offset;
    uint8_t sprite_accumulated_offset{0};
    // how many cycles i subtracted from sprite fetching to reach the % 4 == 2
    // part
    uint8_t sprite_compensation_offset{0};

    // update ppu mode
    void update_ppu_mode(ppu_mode mode);
    uint16_t mode_change_ticks{0};
    uint16_t ppu_total_ticks{0};

    // lcd x position
    uint8_t lcd_x{0};

    // keep track of each scanline
    void reset_scanline();

    // lcd was reset
    bool lcd_reset{false};

    // ppu registers
    uint8_t lcdc_ff40{0};
    uint8_t stat_ff41{0};
    uint8_t scy_ff42{0};
    uint8_t scx_ff43{0};
    uint8_t ly_ff44{0};
    uint8_t lyc_ff45{0};
    uint8_t dma_ff46{0};
    uint8_t bgp_ff47{0};
    uint8_t obp0_ff48{0};
    uint8_t obp1_ff49{0};
    uint8_t wx_ff4b{0};
    uint8_t wy_ff4a{0};

    // vram
    // bg_map_data_2 - 0x9C00 - 0x9FFF
    uint8_t bg_map_data_2[(0x9fff - 0x9c00) + 1]{};
    // bg_map_data_1 - 0x9800 - 0x9bff
    uint8_t bg_map_data_1[(0x9bff - 0x9800) + 1]{};
    // character ram - 0x8000 - 0x97ff
    uint8_t character_ram[(0x97ff - 0x8000) + 1]{};
    // oam ram - 0xfe00 - 0xfe9f
    uint8_t oam_ram[(0xfe9f - 0xfe00) + 1]{};

    // oam and vram blocking
    bool oam_read_block{false};
    bool vram_read_block{false};
    bool oam_write_block{false};
    bool vram_write_block{false};

    // lcd on, lcd toggle
    bool lcd_on{false};
    bool lcd_toggle{false};

    // used for oam corruption bug (ppu current oam row accessed in mode 2)
    uint8_t current_oam_row{0};

    // dma related stuff
    // dma
    bool dma_mode{false};
    bool dma_delay{false}; // delay dma start by one cycle

    // pixel drawing
    sf::Image lcd_frame_image{};
    sf::Texture lcd_frame{};

    sf::Color get_pixel_color(
        uint8_t pixel,
        uint8_t palette = 2); // 2 means get BGP (non sprite palette)

  private:
    // palette
    const std::array<uint8_t, 3> color_palette_white{181, 175, 66};
    const std::array<uint8_t, 3> color_palette_light_gray{145, 155, 58};
    const std::array<uint8_t, 3> color_palette_dark_gray{93, 120, 46};
    const std::array<uint8_t, 3> color_palette_black{58, 81, 34};
};
