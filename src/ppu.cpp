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

// update the ppu mode
void ppu::update_ppu_mode(ppu_mode mode) {
    last_mode = this->current_mode;
    this->current_mode = mode;

    if (current_mode == ppu_mode::VBlank) {
        // first time we entered vblank
        vblank_start = true;
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

    bool oam_scan =
        (stat_mode == ppu_mode::OAM_Scan) &&
        (_get(STAT) &
         0x20); // only trigger the delay the tick after the mode starts
    // 0010 0000

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
    this->dummy_ticks = 0;
    this->mode3_ticks = 0;
    this->mode0_ticks = 0;
}

void ppu::sprite_fetch_tile_data_low() {

    assert((*sprite_to_fetch).y >= 16 &&
           "sprite to fetch y position is abnormal!");

    uint16_t sprite_address{};
    uint16_t line_offset{};

    bool y_flip = (*sprite_to_fetch).flags & 0x40; // bit 6 y-flip

    if (_get(LCDC) & 0x04) {
        // tall sprite mode
        if (_get(LY) <= ((*sprite_to_fetch).y - 16) + 7) {
            // top half
            if (!y_flip) {
                tile_id &= 0xfe; // set lsb to 0
                line_offset = (_get(LY) - ((*sprite_to_fetch).y - 16)) * 2;
            } else {
                tile_id |= 0x01; // set lsb to 1
                line_offset =
                    (8 - 1 - (_get(LY) - ((*sprite_to_fetch).y - 16))) * 2;
            }
        }

        else {
            // bottom half
            if (!y_flip) {
                tile_id |= 0x01;
                line_offset = (_get(LY) - ((*sprite_to_fetch).y - 16)) * 2;
            } else {
                tile_id &= 0xfe;
                line_offset =
                    (8 - 1 - (_get(LY) - (((*sprite_to_fetch).y - 16) + 7))) *
                    2;
            }
        }

    }

    else {
        if (!y_flip) {
            line_offset = (_get(LY) - ((*sprite_to_fetch).y - 16)) * 2;
        }

        else {
            // get the flipped sprite data
            line_offset =
                (8 - 1 - (_get(LY) - ((*sprite_to_fetch).y - 16))) * 2;
        }
    }

    sprite_address = 0x8000 + (16 * tile_id) + line_offset;

    low_byte = _get(sprite_address);
    high_byte_address = sprite_address + 1;
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
        this->gb_mmu.lcd_toggle = false; // reset the lcd toggle
        //  lcd enabled again, LCD reset should be true
        //_set(LY, 0); // reset LY incase there was any write to it (handled by
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

            update_ppu_mode(ppu_mode::Drawing);
        }

        break;
    }

    case ppu_mode::Drawing: {
        mode3_ticks++;
        // assert that ticks this step takes is between 172+80 and 289+80
        // drawing mode 3
        // fetch tile no

        // currently we have 174 cycles for minimum case (8 dummy fetch (6
        // fetch, 2 discard?) + 6 initial fetch + 160 tiles inital fetch
        // ),

        // this->gb_mmu.vram_write_block = true;
        // this->gb_mmu.oam_write_block = true;

        if (dummy_fetch) {
            // wait 6-8 ticks (172 or 174 for background tiles)
            dummy_ticks++;

            if (dummy_ticks < 7) {
                return;
            }
            current_fetcher_mode =
                fetcher_mode::PushToFIFO; // set to fetch tile no in
                                          // beginning
            // dummy_fetch = false;           // reset dummy fetch, dummy ticks
            dummy_ticks = 0;
            // return;
        }

        /*
        // check for sprites right away
        if (!sprite_buffer.empty() && !fetch_sprite && (_get(LCDC) & 0x02) &&
            sprites_to_fetch.empty()) { // check bit 1 for enable sprites

            for (unsigned int i = 0; i < sprite_buffer.size();) {
                if (sprite_buffer[i].x <= lcd_x) {
                    sprites_to_fetch.push_back(sprite_buffer[i]);
                    sprite_buffer.erase(sprite_buffer.begin() + i);
                } else {
                    ++i;
                }
            }
        }

        if (!sprites_to_fetch.empty() &&
            !sprite_to_fetch) { // check if we are already fetching a sprite
            fetch_sprite = true;

            // NOTE: this fixed a bug where my first column of tiles was missing
            current_fetcher_mode =
                fetcher_mode::FetchTileNo; // set to fetch tile no for sprites

            sprite_to_fetch = &sprites_to_fetch[0];
        }*/

        fetcher_ticks++;
        switch (current_fetcher_mode) {

        case fetcher_mode::FetchTileNo: {
            if (fetcher_ticks < 2) {
                break;
            }

            if (fetch_sprite) {
                // set tile id to sprite to fetch tile id
                this->tile_id = (*sprite_to_fetch).tile_id;
            }

            else {
                uint16_t address{};
                // check to see which map to use
                uint16_t bgmap_start = 0x9800;

                if (!fetch_window) {
                    if (((_get(LCDC) >> 3) & 1) == 1) {
                        bgmap_start = 0x9c00;
                    }

                    uint16_t scy_offset =
                        32 * (((_get(LY) + _get(SCY)) % 256) / 8);

                    uint16_t scx_offset = (tile_index + _get(SCX) / 8) % 32;

                    address = bgmap_start + ((scy_offset + scx_offset) & 0x3ff);

                    tile_id = _get(address);

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

                this->tile_id = _get(address);
            }

            current_fetcher_mode = fetcher_mode::FetchTileDataLow;
            break;
        }

        case fetcher_mode::FetchTileDataLow: {
            if (fetcher_ticks < 4) {
                break;
            }

            if (fetch_sprite) {
                sprite_fetch_tile_data_low();
            }

            else { // bg/window
                uint16_t offset = fetch_window
                                      ? 2 * (this->window_ly % 8)
                                      : 2 * ((_get(LY) + _get(SCY)) % 8);

                uint16_t address{};

                // 8000 method or 8800 method to read
                if (((_get(LCDC) >> 4) & 1) == 0) {
                    // 8800 method
                    address =
                        0x9000 + (static_cast<int8_t>(tile_id) * 16) + offset;

                } else {
                    // 8000 method
                    address = 0x8000 + (tile_id * 16) + offset;
                }

                low_byte = _get(address);
                high_byte_address = address + 1;
            }

            current_fetcher_mode = fetcher_mode::FetchTileDataHigh;
            break;
        }

        case fetcher_mode::FetchTileDataHigh: {
            if (fetcher_ticks < 6) {
                break;
            }

            high_byte = _get(high_byte_address);

            current_fetcher_mode = fetcher_mode::PushToFIFO;
            break;
            //[[fallthrough]];
        }

        case fetcher_mode::PushToFIFO: {
            if (fetch_sprite) {

                assert((*sprite_to_fetch).x <= lcd_x + 8 &&
                       "sprite x position is not <= lcd_x + 8!");

                bool x_flip = (*sprite_to_fetch).flags & 0x20; // x flip

                while (sprite_fifo.size() < 8) {
                    sprite_fifo_pixel filler = {0, 0, 0};
                    sprite_fifo.emplace(sprite_fifo.begin(), filler);
                }

                for (unsigned int i = 0, fifo_index = 0; i < 8;
                     ++i, ++fifo_index) {
                    // check sprite x position to see if pixels should be loaded
                    // into fifo

                    if ((*sprite_to_fetch).x < 8) {
                        if (i < 8 - (*sprite_to_fetch).x) {
                            --fifo_index;
                            continue;
                        }
                    }

                    unsigned int j = x_flip ? 7 - i : i;

                    uint8_t final_bit = ((low_byte >> j) & 1) |
                                        (((high_byte >> j) << 1) & 0x02);

                    sprite_fifo_pixel pixel = {(*sprite_to_fetch).x, final_bit,
                                               (*sprite_to_fetch).flags};

                    if (sprite_fifo[fifo_index].color_id == 0) {
                        sprite_fifo[fifo_index] = pixel;
                    }
                }

                // handle sprites to fetch array
                sprite_to_fetch = nullptr;
                sprites_to_fetch.erase(sprites_to_fetch.begin());
                if (sprites_to_fetch.empty()) {
                    fetch_sprite = false;
                } else {
                    sprite_to_fetch = &sprites_to_fetch[0];
                }

                fetcher_ticks = 1;
                // takes up 1 cycle of FetchTileNo
                current_fetcher_mode = fetcher_mode::FetchTileNo;
            }

            else {
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
                    // push to background fifo
                    for (unsigned int i = 0; i < 8; ++i) {
                        uint8_t final_bit = ((low_byte >> i) & 1) |
                                            (((high_byte >> i) << 1) & 0x02);
                        background_fifo.push_back(final_bit);
                    }
                    tile_index++;

                    current_fetcher_mode = fetcher_mode::FetchTileNo;
                    // takes up 1 cycle of FetchTileNo
                    fetcher_ticks = 1;
                }

                // if sprites are not empty
                if (!sprites_to_fetch.empty()) {
                    fetch_sprite = true;
                    sprite_to_fetch = &sprites_to_fetch[0];

                    // fetch tile no step should be completed already! resync
                    // timing
                    this->tile_id = (*sprite_to_fetch).tile_id;

                    // see if you want to fetch data low as well fetcher_ticks = 4-5
                    sprite_fetch_tile_data_low();

                    // try fetch tile data high? fetcher ticks = 6
                    //high_byte = _get(high_byte_address);
                    //current_fetcher_mode = fetcher_mode::PushToFIFO;

                    current_fetcher_mode = fetcher_mode::FetchTileDataHigh;
                    fetcher_ticks = 5; // beginning of fetch tile data low
                }
            }
            break;
        }
        }

        // check for sprites every dot, wait for background fifo to be empty
        if (!sprite_buffer.empty() && !fetch_sprite && (_get(LCDC) & 0x02) &&
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

        // push pixels to LCD, 1 pixel per dot
        if (!background_fifo.empty() && !fetch_sprite &&
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
            if (!fetch_window && (_get(LCDC) & 0x20) &&
                (lcd_x >= _get(WX) + 1) && wy_condition) {
                tile_index = 0;
                background_fifo.clear();
                fetch_window = true;
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
            // std::cout << ticks << '\n';

            update_ppu_mode(ppu_mode::HBlank);
        }

        break;
    }

    case ppu_mode::HBlank: {
        mode0_ticks++;

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

            if (fetch_window) {
                window_ly++;
            } // add window ly before resetting fetch window

            reset_ticks(); // resets ticks, fetcher_ticks, dummy_ticks,
                           // mode3_ticks, mode0_ticks

            // resets lcd_x to 0, and stat irq
            reset_scanline();

            // dummy fetch too?
            dummy_fetch = true;
            fetch_window = false;

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
            reset_ticks(); // resets ticks, fetcher_ticks, dummy_ticks,
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
