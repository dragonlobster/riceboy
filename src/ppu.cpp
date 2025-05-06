#include "ppu.h"
#include <algorithm>
#include <iostream>

uint8_t ppu::_get(uint16_t address) {
    return this->gb_mmu.read_memory(address);
}

void ppu::_set(uint16_t address, uint8_t value) {
    this->gb_mmu.write_memory(address, value);
}

void ppu::increment_ly() {
    _set(LY, _get(LY) + 1); // new scanline reached
}

// ppu constructor
ppu::ppu(mmu &gb_mmu_, sf::RenderWindow &window_)
    : gb_mmu(gb_mmu_), window(window_) {

    // TODO check logic for setting STAT to 1000 0000 (resetting STAT)
    _set(STAT, 0x80);

    sf::Image image({window.getSize().x, window.getSize().y}, sf::Color::White);
    this->lcd_frame_image = image; // image with pixels to display on the screen
};

void ppu::initialize_skip_bootrom_values() {
    ticks = 0x019f;
    fetcher_ticks = 0;
    mode3_ticks = 0;
    mode0_ticks = 0;
    interrupt_ticks = 0;
    oam_search_counter = 0;
    window_ly = 0;
    wy_condition = true;
    tile_index = 0x14;
    bg_tile_id = 0;
    fetch_window_ip = false;
    fetch_sprite_ip = false;
    bg_low_byte = 0;
    bg_high_byte = 0;
    bg_high_byte_address = 0x800f;
    dummy_fetch = true;
    scx_discard_count = 0;
    scx_discard = true;
    end_frame = true;
    current_interrupt_line = false;
    interrupt_t_cycle = 0;
    interrupt_m_cycle = 0;
    vblank_start = false;
    current_mode = ppu_mode::VBlank;
    last_mode = ppu_mode::HBlank;
    mode_t_cycle = 0;
    mode_m_cycle = 0;
    current_fetcher_mode = fetcher_mode::PushToFIFO;
    mode_change_ticks = 0;
    lcd_x = 0;
    lcd_reset = false;
}

// update the ppu mode
void ppu::update_ppu_mode(ppu_mode mode) {
    last_mode = this->current_mode;
    this->current_mode = mode;

    if (current_mode == ppu_mode::VBlank) {
        // first time we entered vblank
        vblank_start = true;
    }

    if (current_mode == ppu_mode::Drawing) {
        // CRITICAL: remember to reset pixel fetcehr mode!
        this->current_fetcher_mode = fetcher_mode::FetchTileNo;
    }

    this->gb_mmu.oam_read_block =
        current_mode == ppu_mode::OAM_Scan || current_mode == ppu_mode::Drawing;

    this->gb_mmu.vram_read_block = current_mode == ppu_mode::Drawing;

    this->gb_mmu.oam_write_block =
        current_mode == ppu_mode::OAM_Scan || current_mode == ppu_mode::Drawing;

    this->gb_mmu.vram_write_block = current_mode == ppu_mode::Drawing;
}

void ppu::reset_scanline() {
    lcd_x = 0;
    scx_discard = true;
}

void ppu::interrupt_line_check() {
    assert(this->gb_mmu.lcd_on &&
           "lcd is not on! no interrupt check should occur!");

    bool prev_interrupt_line = current_interrupt_line;

    ppu_mode stat_mode = static_cast<ppu_mode>(_get(STAT) & 3);

    bool oam_scan = (stat_mode == ppu_mode::OAM_Scan) &&
                    (_get(STAT) & 0x20); // only trigger the delay the tick
                                         // after the mode starts 0010 0000

    bool hblank = (stat_mode == ppu_mode::HBlank) && (_get(STAT) & 0x08);
    // 0000 1000

    // bit 5 (& 0x20) applies to both VBlank and OAM_Scan
    bool vblank =
        (stat_mode == ppu_mode::VBlank) &&
        ((_get(STAT) & 0x10) || ((_get(STAT) & 0x20) && (_get(LY) == 144)));
    // 0001 0000, 0010 0000

    // get the old LY to compare
    bool ly_lyc = (_get(LY) == _get(LYC)) && (_get(STAT) & 0x40);
    // 0100 0000

    current_interrupt_line = oam_scan || hblank || vblank || ly_lyc;

    //    // ly == lyc comparison delayed for 1 cycle
    if (ticks >= 452) {
        _set(STAT,
             _get(STAT) & ~4); // explicitely sets coincidence flag to 0
    }

    else {
        if (_get(LY) == _get(LYC)) {
            _set(STAT, _get(STAT) | 4); // sets coincidence flag to 1
        }

        else {
            _set(STAT,
                 _get(STAT) & ~4); // explicitely sets coincidence flag to 0
        }
    }

    if (!prev_interrupt_line && current_interrupt_line) {
        // rising edge occured, set the IF bit
        _set(IF, _get(IF) | 2);
    }
}

sf::Color ppu::get_pixel_color(uint8_t pixel, uint8_t palette) {
    // Get the appropriate palette register
    uint8_t palette_value{};

    if (palette == 2) {
        palette_value = _get(BGP); // Background palette
    } else {
        // Assert valid sprite palette before using it
        assert(palette == 0 || palette == 1 && "sprite palette is not 0 or 1!");
        palette_value = palette == 0 ? _get(0xff48) : _get(0xff49);
        palette_value &= 0xfc; // 0 out the last 2 bits
    }

    // Extract the color ID for the pixel (each pixel uses 2 bits in the
    // palette)
    uint8_t color_id = (palette_value >> (2 * (pixel & 0x03))) & 0x03;

    // Map the color ID directly to the appropriate color palette
    const std::array<std::array<uint8_t, 3>, 4> color_map = {
        color_palette_white, color_palette_light_gray, color_palette_dark_gray,
        color_palette_black};

    // Get the RGB values and return as sf::Color
    const auto &color = color_map[color_id];
    return {color[0], color[1], color[2]};
}

void ppu::reset_ticks() {
    this->ticks = 0;
    this->fetcher_ticks = 0;
    this->mode3_ticks = 0;
    this->mode0_ticks = 0;
}

void ppu::sprite_fetch_tile_data_low(oam_entry sprite) {

    assert((sprite).y >= 16 && "sprite to fetch y position is abnormal!");

    uint16_t sprite_address{};
    uint16_t line_offset{};

    bool y_flip = (sprite).flags & 0x40; // bit 6 y-flip

    if (_get(LCDC) & 0x04) {
        // tall sprite mode
        if (_get(LY) <= ((sprite).y - 16) + 7) {
            // top half
            if (!y_flip) {
                sprite_tile_id &= 0xfe; // set lsb to 0
                line_offset = (_get(LY) - ((sprite).y - 16)) * 2;
            } else {
                sprite_tile_id |= 0x01; // set lsb to 1
                line_offset = (8 - 1 - (_get(LY) - ((sprite).y - 16))) * 2;
            }
        }

        else {
            // bottom half
            if (!y_flip) {
                sprite_tile_id |= 0x01;
                line_offset = (_get(LY) - ((sprite).y - 16)) * 2;
            } else {
                sprite_tile_id &= 0xfe;
                line_offset =
                    (8 - 1 - (_get(LY) - (((sprite).y - 16) + 7))) * 2;
            }
        }

    }

    else {
        if (!y_flip) {
            line_offset = (_get(LY) - ((sprite).y - 16)) * 2;
        }

        else {
            // get the flipped sprite data
            line_offset = (8 - 1 - (_get(LY) - ((sprite).y - 16))) * 2;
        }
    }

    sprite_address = 0x8000 + (16 * sprite_tile_id) + line_offset;

    sprite_low_byte = _get(sprite_address);
    sprite_high_byte_address = sprite_address + 1;
}

void ppu::sprite_push_to_fifo(oam_entry sprite) {

    assert((sprite).x <= lcd_x && "sprite x position is not <= lcd_x!");

    bool x_flip = (sprite).flags & 0x20; // x flip

    while (sprite_fifo.size() < 8) {
        sprite_fifo_pixel filler = {0, 0, 0};
        sprite_fifo.emplace(sprite_fifo.begin(), filler);
    }

    for (unsigned int i = 0, fifo_index = 0; i < 8; ++i, ++fifo_index) {
        // check sprite x position to see if pixels should be loaded
        // into fifo

        if ((sprite).x < 8) {
            if (i < 8 - (sprite).x) {
                --fifo_index;
                continue;
            }
        }

        unsigned int j = x_flip ? 7 - i : i;

        uint8_t final_bit = ((sprite_low_byte >> j) & 1) |
                            (((sprite_high_byte >> j) << 1) & 0x02);

        sprite_fifo_pixel pixel = {(sprite).x, final_bit, (sprite).flags};

        if (sprite_fifo[fifo_index].color_id == 0) {
            sprite_fifo[fifo_index] = pixel;
        }
    }

    // handle sprites to fetch array
    /*
       sprite_to_fetch = nullptr;
       sprites_to_fetch.erase(sprites_to_fetch.begin());
       if (sprites_to_fetch.empty()) {
       fetch_sprite_ip = false;
       } else {
       sprite_to_fetch = &sprites_to_fetch[0];
       }*/
}

void ppu::fetch_sprites() {
    // fetch a sprite, remember to stall the appropriate amount of cycles based
    // on sprite x mod 8

    uint8_t first_sprite_x{0};

    // sprites greater than or equal to 168
    uint8_t sprites_ge_168{0};

    for (uint8_t sprite_index = 0; sprite_index < sprites_to_fetch.size();
         sprite_index++) {

        oam_entry sprite = sprites_to_fetch[sprite_index];

        // skip sprites ge 168
        if (sprite.x >= 168) {
            sprites_ge_168++;
            continue;
        }

        // save first sprite's x position to calculate stall for 1st sprite
        if (sprite_index == 0) {
            first_sprite_x = sprite.x;
        }

        // step 1: get tile id
        this->sprite_tile_id = (sprite).tile_id;

        // step 2: fetch tile data low
        sprite_fetch_tile_data_low(sprite);

        // step 3: fetch tile data high:
        sprite_high_byte = _get(sprite_high_byte_address);

        // step 4: push to sprite fifo
        sprite_push_to_fifo(sprite);
    }

    // do not set to false yet, we need to stall for stall cycles
    // fetch_sprite_ip = true;

    // default cycles for sprites that are not the first sprite and not >= 168
    uint8_t default_cycles =
        (6 * (sprites_to_fetch.size() - 1 - sprites_ge_168));

    // mod 8 of first sprie x
    uint8_t mod8 = first_sprite_x % 8;

    // initialize first sprite cycles, assign it differently depending on mod8
    uint8_t first_sprite_cycles{0};

    // default cycle per mod 8
    uint8_t default_cycle{0};

    // first sprite x can only be 0-7 (because we used %8)
    switch (mod8) {
    case 0: default_cycle = 5; break;
    case 1: default_cycle = 4; break;
    case 2: default_cycle = 3; break;
    case 3: default_cycle = 2; break;
    case 4: default_cycle = 1; break;
    case 5: default_cycle = 0; break;
    case 6: default_cycle = 0; break;
    case 7: default_cycle = 0; break;
    }

    sprite_fetch_stall_cycles =
        (6 * (sprites_to_fetch.size() - sprites_ge_168));

    sprite_fetch_stall_cycles-=3;

    // try to move sprite fetch stall cycles down 1 M-cycle ?
    /*
    while ((sprite_fetch_stall_cycles + 174 + default_cycle +
            sprite_accumulated_offset) %
               4 !=
           2) {
        sprite_fetch_stall_cycles--;
    }*/

    // off screen penalties (> 160)
    switch (first_sprite_x) {
    case 160: sprite_fetch_stall_cycles += 0; break;
    case 161: sprite_fetch_stall_cycles += 0; break;
    case 162: sprite_fetch_stall_cycles += 0; break;
    case 163: sprite_fetch_stall_cycles += 0; break;
    case 164: sprite_fetch_stall_cycles += 0; break;
    case 165: sprite_fetch_stall_cycles += 0; break;
    case 166: sprite_fetch_stall_cycles += 0; break;
    case 167: sprite_fetch_stall_cycles += 0; break;
    }

    //sprite_accumulated_offset += (sprite_fetch_stall_cycles + default_cycle);

    if (first_sprite_x == 160 && sprites_to_fetch.size() == 5) {
        sprite_fetch_stall_cycles += 0;
    }

    sprites_to_fetch.clear();

    return;
}

void ppu::tick() {
    // if lcd got toggled off
    if (this->gb_mmu.lcd_toggle && !((_get(LCDC) >> 7) & 1)) {
        this->gb_mmu.lcd_toggle = false; // reset the lcd toggle
        reset_ticks();
        //_set(LY, 0);                     // reset LY (handled by mmu already)
        assert(oam_search_counter == 0 &&
               this->gb_mmu.ppu_current_oam_row == 0 && dummy_fetch &&
               "lcd toggled off outside of vblank!");

        // keep current interrupt line as the value before lcd turned off

        update_ppu_mode(ppu_mode::LCDToggledOn);

        return;
    }

    // if lcd is off
    else if (!this->gb_mmu.lcd_on) {
        assert(!this->gb_mmu.oam_write_block &&
               "no write blocks while lcd is off");
        return;
    }

    // if lcd got toggled on
    if (this->gb_mmu.lcd_toggle && ((_get(LCDC) >> 7) & 1)) {
        this->gb_mmu.lcd_toggle =
            false; // reset the lcd toggle
                   //  lcd enabled again, LCD reset should be true
                   //_set(LY, 0); // reset LY incase there was any write to it
                   //(handled by
                   // mmu already)
        this->lcd_reset = true;
        // update_ppu_mode(ppu_mode::OAM_Scan);

        // TODO: check if i need to run all the functions and just align ticks
        // instead of returning here (does LCD turn on the same cycle as LCDC
        // write?)
        return;
    }

    ticks++;
    // 1 T-cycle

    // wy == ly every tick
    if (!wy_condition) {
        wy_condition = _get(WY) == _get(LY);
    }

    // for LCDToggledOn (4), the STAT mode should read 0 (HBlank)
    _set(STAT, (_get(STAT) & 0xfc) | (static_cast<uint8_t>(current_mode) % 4));

    // oam search mode 2
    switch (current_mode) {
    case ppu_mode::LCDToggledOn: {

        assert(ticks <= 76 && "ticks must be <= 76 when lcd is toggled on ");

        assert(!this->gb_mmu.oam_write_block && "oam write blocked!");

        if (ticks == 76) {
            // LCD On for the first time started 4 T-cycles shorter, so we
            // artifically align the tick numbers for this frame
            ticks += 4;
            // this->current_fetcher_mode = fetcher_mode::FetchTileNo;
            update_ppu_mode(ppu_mode::Drawing);
        }

        break;
    }

    case ppu_mode::OAM_Scan: {
        // this->gb_mmu.oam_write_block = true;

        assert(ticks <= 80 && "ticks must be <= 80 during OAM Scan");

        if ((ticks % 2 == 0)) { // 40 objects

            std::array<uint8_t, 4> oam_entry_array{};
            for (unsigned int i = 0; i < 4; ++i) {
                // 4 bytes per entry

                uint16_t address = 0xfe00 + (oam_search_counter * 4) + i;

                oam_entry_array[i] = _get(address);
            }

            oam_entry entry = {
                oam_entry_array[1], // x position
                oam_entry_array[0], // y position
                oam_entry_array[2], // tile #
                oam_entry_array[3]  // flags
            };

            // add to sprite buffer on specific conditions
            if ((_get(LY) + 16) >= entry.y &&
                (_get(LY) + 16) < entry.y + (_get(LCDC) & 0x04 ? 16 : 8) &&
                sprite_buffer.size() < 10) {

                sprite_buffer.push_back(entry);
            }

            oam_search_counter++;
            this->gb_mmu.ppu_current_oam_row =
                (oam_search_counter & ~1) *
                4; // function calculation for the start row OAM address of the
                   // current row, each row has 2 sprites (4 bytes each so 8
                   // bytes total for 1 row) and a total of 20 rows (0-19)

            // i can increment here because the cpu will tick first (thus
            // current oam row will point to the current tick's oam access row
            // by the ppu)
        }

        if (ticks == 76) {
            this->gb_mmu.vram_read_block = true;
            this->gb_mmu.oam_write_block = false;
        }

        if (ticks == 80) {
            assert(oam_search_counter == 40 && "oam search counter is not 40!");
            assert(this->gb_mmu.ppu_current_oam_row == 0xa0 &&
                   "ppu current oam row is not 20! ending at 0xa0 (row 152+1 "
                   "start!)");

            oam_search_counter = 0;               // reset oam search counter
            this->gb_mmu.ppu_current_oam_row = 0; // reset current oam row scan

            // reset fifos and tile index
            background_fifo.clear();
            sprite_fifo.clear();
            tile_index = 0;

            // TODO: do we really clear the sprite buffer if DMA is active right
            // now? or if it was ever active during mode 2?:
            if (this->gb_mmu.dma_mode) {
                // all sprites on the line are hidden if DMA is active during
                // mode 2
                sprite_buffer.clear();
            }

            // stable sort sprite buffer
            if (sprite_buffer.size() > 1) {
                std::stable_sort(
                    sprite_buffer.begin(), sprite_buffer.end(),
                    [](const oam_entry &a, const oam_entry &b) -> bool {
                        return a.x < b.x; // lower x has priority
                    });
            }

            // NOTE: this fixed a bug where my first column of tiles was
            // missing; reset the current fetcher mode
            // current_fetcher_mode =
            //    fetcher_mode::FetchTileNo; // set to fetch tile no for sprites

            update_ppu_mode(ppu_mode::Drawing);
        }

        break;
    }

    case ppu_mode::Drawing: {
        mode3_ticks++;

        fetcher_ticks++;

        switch (current_fetcher_mode) {

        case fetcher_mode::FetchTileNo: {

            if (fetcher_ticks < 2) {
                break;
            }

            if (dummy_fetch) {
                current_fetcher_mode = fetcher_mode::FetchTileDataLow;
                break;
            }

            uint16_t address{};
            // check to see which map to use
            uint16_t bgmap_start = 0x9800;

            if (!fetch_window_ip) {
                if (((_get(LCDC) >> 3) & 1) == 1) {
                    bgmap_start = 0x9c00;
                }

                uint16_t scy_offset = 32 * (((_get(LY) + _get(SCY)) % 256) / 8);

                uint16_t scx_offset = (tile_index + _get(SCX) / 8) % 32;

                address = bgmap_start + ((scy_offset + scx_offset) & 0x3ff);

                bg_tile_id = _get(address);

            }

            else {
                // window fetch
                if (_get(LCDC) & 0x40) { // 6th bit
                    bgmap_start = 0x9c00;
                } // bgmap_start is either 0x9c00 or 0x9800

                uint16_t wy_offset = 32 * (window_ly / 8);
                uint16_t wx_offset = tile_index;

                address = bgmap_start + ((wy_offset + wx_offset) & 0x3ff);
            }

            this->bg_tile_id = _get(address);

            current_fetcher_mode = fetcher_mode::FetchTileDataLow;
            break;
        }

        case fetcher_mode::FetchTileDataLow: {
            if (fetcher_ticks < 4) {
                break;
            }

            if (dummy_fetch) {
                current_fetcher_mode = fetcher_mode::FetchTileDataHigh;
                break;
            }

            uint16_t offset = fetch_window_ip
                                  ? 2 * (this->window_ly % 8)
                                  : 2 * ((_get(LY) + _get(SCY)) % 8);

            uint16_t address{};

            // 8000 method or 8800 method to read
            if (((_get(LCDC) >> 4) & 1) == 0) {
                // 8800 method
                address =
                    0x9000 + (static_cast<int8_t>(bg_tile_id) * 16) + offset;

            } else {
                // 8000 method
                address = 0x8000 + (bg_tile_id * 16) + offset;
            }

            bg_low_byte = _get(address);
            bg_high_byte_address = address + 1;

            current_fetcher_mode = fetcher_mode::FetchTileDataHigh;
            break;
        }

        case fetcher_mode::FetchTileDataHigh: {
            if (fetcher_ticks < 6) {
                break;
            }

            if (dummy_fetch) {
                current_fetcher_mode = fetcher_mode::PushToFIFO;
                break;
            }

            bg_high_byte = _get(bg_high_byte_address);

            current_fetcher_mode = fetcher_mode::PushToFIFO;
            break;
            //[[fallthrough]];
        }

        case fetcher_mode::PushToFIFO: {
            // PushToFIFO still happens on fetch_sprite_ip because background
            // fifo won't be empty anyway and if it was it needs to be filled in
            // on fetcher tick 7

            if (dummy_fetch) {
                for (unsigned int i = 0; i < 8; ++i) {
                    background_fifo.push_back(0);
                }
                current_fetcher_mode = fetcher_mode::FetchTileNo;
                // takes up 1 cycle of FetchTileNo
                fetcher_ticks = 1;
                dummy_fetch = false;
            }

            if (background_fifo.empty()) {
                assert(!dummy_fetch && "dummy fetch should be false here!");
                // push to background fifo
                for (unsigned int i = 0; i < 8; ++i) {
                    uint8_t final_bit = ((bg_low_byte >> i) & 1) |
                                        (((bg_high_byte >> i) << 1) & 0x02);
                    background_fifo.push_back(final_bit);
                }
                tile_index++;

                current_fetcher_mode = fetcher_mode::FetchTileNo;
                // takes up 1 cycle of FetchTileNo
                fetcher_ticks = 1;
            }

        } break;
        }

        // check for sprites every dot, wait for background fifo to be empty
        if (!sprite_buffer.empty() && !fetch_sprite_ip && (_get(LCDC) & 0x02) &&
            sprites_to_fetch.empty() &&
            !background_fifo.empty()) { // check bit 1 for enable sprites

            for (unsigned int i = 0; i < sprite_buffer.size();) {
                if (sprite_buffer[i].x <= lcd_x) {
                    sprites_to_fetch.push_back(sprite_buffer[i]);
                    sprite_buffer.erase(sprite_buffer.begin() + i);
                } else {
                    ++i;
                }
            }
        }

        if (!sprites_to_fetch.empty() && !fetch_sprite_ip) {
            fetch_sprite_ip = true;
        }

        // wait for bg fetcher to finish?
        if (fetch_sprite_ip &&
            current_fetcher_mode == fetcher_mode::PushToFIFO) {
            if (!sprites_to_fetch.empty()) {
                fetch_sprites();
                // first cycle of sprite fetch shares the last cycle of bg fetch
                // after waiting
                fetcher_ticks = 1;
            }

            while ((fetcher_ticks) < sprite_fetch_stall_cycles) {
                return;
            }

            sprite_fetch_stall_cycles = 0;
            fetch_sprite_ip = false;
            sprite_ticks = 0;

            // do not reset fetcher, we are stuck in PushToFIFO mode since we
            // already fetched the tile needed

            return;
        }

        // push pixels to LCD, 1 pixel per dot
        if (!background_fifo.empty() && !fetch_sprite_ip &&
            sprites_to_fetch.empty()) {

            // discard scx (one per dot)
            if (lcd_x == 8 && (_get(SCX) % 8) && scx_discard) {

                if (!scx_discard_count) {
                    scx_discard_count = _get(SCX) % 8;
                }

                background_fifo.pop_back();
                scx_discard_count--;

                if (scx_discard_count == 0) {
                    scx_discard = false;
                }

                return;
            }

            // get the background pixel
            uint8_t bg_pixel = background_fifo.back();
            background_fifo.pop_back();

            sf::Color final_pixel_color =
                _get(LCDC) & 1 ? get_pixel_color(bg_pixel) : get_pixel_color(0);

            if (!sprite_fifo.empty()) {
                sprite_fifo_pixel sprite_pixel = sprite_fifo.back();
                sprite_fifo.pop_back();

                uint8_t palette = (sprite_pixel.flags >> 4) & 1;

                sf::Color sprite_pixel_color =
                    get_pixel_color(sprite_pixel.color_id, palette);

                bool sprite_priority =
                    sprite_pixel.color_id && _get(LCDC) & 0x02 &&
                    (!((sprite_pixel.flags >> 7) & 1) || !bg_pixel);

                if (sprite_priority) {
                    final_pixel_color = sprite_pixel_color;
                }
            }

            assert(lcd_x < 168 && "LCD X Position exceeded the screen width!");

            if (!lcd_reset && lcd_x >= 8) {
                uint16_t position_x = (lcd_x - 8);
                uint16_t position_y = (_get(LY));

                lcd_frame_image.setPixel({position_x, position_y},
                                         final_pixel_color);
            }

            lcd_x++; // increment the lcd x position

            // check for window fetching after pixel was shifted out to LCD
            if (!fetch_window_ip && (_get(LCDC) & 0x20) &&
                (lcd_x >= _get(WX) + 1) && wy_condition) {
                tile_index = 0;
                background_fifo.clear();
                fetch_window_ip = true;
                current_fetcher_mode = fetcher_mode::FetchTileNo;
            }
        }

        if (lcd_x == 168) {
            /*
               assert(ticks >= 172+80 && ticks <= 293+80 &&
               "ticks in drawing mode should be between 172 and 289!!");
               */

            if (ticks > 400) {
                std::cout << ticks << '\n';
            }

            update_ppu_mode(ppu_mode::HBlank);
        }

        break;
    }

    case ppu_mode::HBlank: {
        mode0_ticks++;

        // extend oam write block by 1 M (on lcd reset only? or not?)
        // this->gb_mmu.oam_write_block = mode0_ticks <= 4;

        // this->gb_mmu.oam_write_block = false;
        // this->gb_mmu.vram_write_block = false;

        // test increment LY 6 T-cycles earlier (past line 0)
        if (ticks == 452) {
            // reading LY at this exact dot returns a bitwise AND between prev
            // LY and current LY
            //_set(LY, _get(LY) + 1);
            increment_ly(); // new scanline reached

            this->gb_mmu.oam_read_block = true;
        }
        /*
           if (ticks == 456 && _get(LY) == 0) {
        // reading LY at this exact dot returns a bitwise AND between prev
        // LY and current LY
        _set(LY, _get(LY) + 1); // new scanline reached
        }*/

        // wait 456 T-cycles (scanline ends there)
        if (ticks == 456) {

            assert(mode0_ticks + mode3_ticks + 80 == 456 &&
                   "timing for ticks in the scnaline is not correct!");

            //_set(LY, _get(LY) + 1); // new scanline reached

            if (fetch_window_ip) {
                window_ly++;
            } // add window ly before resetting fetch window

            reset_ticks(); // resets ticks, fetcher_ticks, ,
                           // mode3_ticks, mode0_ticks

            // resets lcd_x to 0, and stat irq
            reset_scanline();

            // dummy fetch too?
            dummy_fetch = true;
            fetch_window_ip = false;

            // reset sprite_accumualted_offset
            sprite_accumulated_offset = 0;

            // clear sprite buffer
            sprite_buffer.clear();

            if (_get(LY) == 144) {
                // hit VBlank for the first time
                // TODO: interrupts during the first frame after lcd enabled
                // again?

                if (lcd_reset) {
                    lcd_reset = false;
                }

                else {
                    bool success = lcd_frame.loadFromImage(lcd_frame_image);
                    assert(success &&
                           "LCD dots texture error loading from image");

                    sf::Sprite frame(lcd_frame);

                    frame.setScale({draw::SCALE, draw::SCALE});

                    window.clear(sf::Color::White);
                    window.draw(frame);
                    window.display();
                }

                update_ppu_mode(ppu_mode::VBlank);
            }

            else {
                // reset LCD x
                update_ppu_mode(ppu_mode::OAM_Scan);
            }
        }

        break;
    }

    case ppu_mode::VBlank: {
        // vblank interrupt when we hit it the first time
        if (vblank_start) {
            assert(_get(LY) == 144 &&
                   "VBlank should only be entered when LY=144!");

            _set(IF, _get(IF) | 1);
            vblank_start = false;

            assert(this->gb_mmu.oam_write_block == false &&
                   "oam write block should be false here!");
            // this->gb_mmu.oam_write_block = false;
        }

        // At line 153 LY=153 only lasts 4 dots before snapping to 0
        // for the rest of the line
        if (_get(LY) == 153 && ticks == 4) {
            _set(LY, 0);
            end_frame = true;
        }

        // test increment LY 6 T-cycles earlier (past line 0)
        /*
           if (ticks == 452 && _get(LY) > 0 && !end_frame) {
        // reading LY at this exact dot returns a bitwise AND between prev
        // LY and current LY
        _set(LY, _get(LY) + 1); // new scanline reached
        }*/

        if (ticks == 452 && !end_frame) {
            // reading LY at this exact dot returns a bitwise AND between prev
            // LY and current LY
            //_set(LY, _get(LY) + 1); // new scanline reached
            increment_ly(); // new scanline reached
        }

        // is oam read blocked early in vblank?

        if (ticks == 456) {
            // ticks = 0; // wait 456 T-cycles for the whole scanline, reset
            reset_ticks(); // resets ticks, fetcher_ticks,
                           // mode3_ticks, mode0_ticks

            // resets lcd_x to 0, and stat irq
            reset_scanline();

            if (end_frame) {
                // reset LY, window LY
                //_set(LY, 0);
                window_ly = 0;

                end_frame = false;
                // set wy_condition = false since we are on the next frame
                wy_condition = false;

                update_ppu_mode(ppu_mode::OAM_Scan);

            } // after 10 scanlines
        }

        break;
    }
    }

    interrupt_line_check();
}
