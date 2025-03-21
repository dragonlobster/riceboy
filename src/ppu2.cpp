#include "ppu2.h"
#include <algorithm>
#include <iostream>

uint8_t PPU2::_get(uint16_t address) {
    return this->gb_mmu.read_memory(address);
}
void PPU2::_set(uint16_t address, uint8_t value) {
    this->gb_mmu.write_memory(address, value);
}

// ppu constructor
PPU2::PPU2(MMU &gb_mmu_, sf::RenderWindow &window_)
    : gb_mmu(gb_mmu_), window(window_) {

    // TODO check logic for setting STAT to 1000 0000 (resetting STAT)
    _set(STAT, 0x80);

    sf::Image image({window.getSize().x, window.getSize().y}, sf::Color::White);
    this->lcd_frame_image = image; // image with pixels to display on the screen
};

sf::Color PPU2::get_pixel_color(uint8_t pixel, uint8_t palette) {
    // Get the appropriate palette register
    uint8_t palette_value{};

    if (palette == 2) {
        palette_value = _get(BGP); // Background palette
    } else {
        // Assert valid sprite palette before using it
        assert(palette == 0 ||
               palette == 1 && "sprite palette is not 0 or 1!");
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

void PPU2::tick() {
    ticks++;
    // 1 T-cycle

    // wy == ly every tick
    if (!wy_condition) {
        wy_condition = _get(WY) == _get(LY);
    }

    // set stat to mode
    _set(STAT, (_get(STAT) & 0xfc) | static_cast<uint8_t>(current_mode));
    last_mode = current_mode;

    // oam search mode 2
    switch (current_mode) {
    case ppu_mode::OAM_Scan: {

        assert(ticks <= 80 && "ticks must be <= 80 during OAM Scan");

        // set IF for interrupt
        if ((this->last_mode != this->current_mode) && (_get(STAT) >> 5 & 1)) {
            _set(IF, _get(IF) | 2);
        }

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
        }

        if (ticks == 80) {
            assert(oam_search_counter == 40 && "oam search counter is not 40!");
            oam_search_counter = 0; // reset oam search counter

            // reset fifos and tile index
            background_fifo.clear();
            sprite_fifo.clear();
            tile_index = 0;

            // stable sort sprite buffer
            if (sprite_buffer.size() > 1) {
                std::stable_sort(
                    sprite_buffer.begin(), sprite_buffer.end(),
                    [](const oam_entry &a, const oam_entry &b) -> bool {
                        return a.x < b.x; // lower x has priority
                    });
            }

            current_mode = ppu_mode::Drawing;
        }

        break;
    }

    case ppu_mode::Drawing: {

        // assert that ticks this step takes is between 172+80 and 289+80
        // drawing mode 3
        // fetch tile no

        // dummy fetch - after 6 cycles
        if (dummy_fetch) {
            // current_fetcher_mode = fetcher_mode::FetchTileNo;
            assert(ticks >= 81 && ticks <= 87 &&
                   "ticks not correct for dummy fetch!");
            if (ticks == 87) { // 12 ticks for dummy fetching
                current_fetcher_mode = fetcher_mode::FetchTileNo;
                dummy_fetch = false;
            }
            return;
        }

        // check for sprites right away
        if (!sprite_buffer.empty() && !fetch_sprite && (_get(LCDC) & 0x02) &&
            sprites_to_fetch.empty()) { // check bit 1 for enable sprites

            for (unsigned int i = 0; i < sprite_buffer.size();) {
                if (sprite_buffer[i].x <= lcd_x + 8) {
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
            current_fetcher_mode =
                fetcher_mode::FetchTileNo; // set to fetch tile no for sprites

            sprite_to_fetch = &sprites_to_fetch[0];
        }

        switch (current_fetcher_mode) {
        case fetcher_mode::FetchTileNo: {
            if (ticks % 2 == 0)
                break;

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
            if (ticks % 2 == 0)
                break;

            if (fetch_sprite) {
                assert((*sprite_to_fetch).y >= 16 &&
                       "sprite to fetch y position is abnormal!");

                // TODO: fetch sprite
                uint16_t sprite_address{};
                uint16_t line_offset{};

                bool y_flip = (*sprite_to_fetch).flags & 0x40; // bit 6 y-flip

                if (_get(LCDC) & 0x04) {
                    // tall sprite mode
                    if (_get(LY) <= ((*sprite_to_fetch).y - 16) + 7) {
                        // top half
                        if (!y_flip) {
                            tile_id &= 0xfe; // set lsb to 0
                            line_offset =
                                (_get(LY) - ((*sprite_to_fetch).y - 16)) * 2;
                        } else {
                            tile_id |= 0x01; // set lsb to 1
                            line_offset =
                                (8 - 1 -
                                 (_get(LY) - ((*sprite_to_fetch).y - 16))) *
                                2;
                        }
                    }

                    else {
                        // bottom half
                        if (!y_flip) {
                            tile_id |= 0x01;
                            line_offset =
                                (_get(LY) - ((*sprite_to_fetch).y - 16)) * 2;
                        } else {
                            tile_id &= 0xfe;
                            line_offset =
                                (8 - 1 -
                                 (_get(LY) -
                                  (((*sprite_to_fetch).y - 16) + 7))) *
                                2;
                        }
                    }

                }

                else {
                    if (!y_flip) {
                        line_offset =
                            (_get(LY) - ((*sprite_to_fetch).y - 16)) * 2;
                    }

                    else {
                        // get the flipped sprite data
                        line_offset =
                            (8 - 1 - (_get(LY) - ((*sprite_to_fetch).y - 16))) *
                            2;
                    }
                }

                sprite_address = 0x8000 + (16 * tile_id) + line_offset;

                low_byte = _get(sprite_address);
                high_byte_address = sprite_address + 1;
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
            if (ticks % 2 == 0)
                break;

            high_byte = _get(high_byte_address);

            current_fetcher_mode = fetcher_mode::PushToFIFO;
            break;
        }

        case fetcher_mode::PushToFIFO: {
            if (fetch_sprite) {

                assert((*sprite_to_fetch).x <= lcd_x + 8 &&
                       "sprite x position is not <= lcd_x + 8!");

                bool x_flip = (*sprite_to_fetch).flags & 0x20; // x flip

                std::vector<sprite_fifo_pixel> temp_sprite_fifo{};

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

                    if (fifo_index < sprite_fifo.size()) {
                        if (sprite_fifo[fifo_index].color_id == 0) {
                            sprite_fifo[fifo_index] = pixel;
                        }
                    }

                    // TODO: shift pixels down first before pushing new pixel
                    // CRITICAL BUG
                    else {
                        temp_sprite_fifo.push_back(pixel);
                    }
                }

                sprite_fifo.insert(sprite_fifo.begin(),
                                   temp_sprite_fifo.begin(),
                                   temp_sprite_fifo.end());

                // handle sprites to fetch array
                sprite_to_fetch = nullptr;
                sprites_to_fetch.erase(sprites_to_fetch.begin());
                if (sprites_to_fetch.empty()) {
                    fetch_sprite = false;
                }

                current_fetcher_mode = fetcher_mode::FetchTileNo;
            }

            else {
                if (background_fifo.empty()) {
                    // push to background fifo
                    for (unsigned int i = 0; i < 8; ++i) {
                        uint8_t final_bit = ((low_byte >> i) & 1) |
                                            (((high_byte >> i) << 1) & 0x02);
                        background_fifo.push_back(final_bit);
                    }
                    tile_index++;
                    current_fetcher_mode = fetcher_mode::FetchTileNo;
                }
            }

            break;
        }
        }

        // push pixels to LCD, 1 pixel per dot
        if (!background_fifo.empty() && !fetch_sprite) {

            // fine x (scx)
            if (lcd_x == 0) {
                for (unsigned int i = 0; i < _get(SCX) % 8; ++i) {
                    background_fifo.pop_back();
                }
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

            assert(lcd_x < 160 && "LCD X Position exceeded the screen width!");

            for (uint16_t y = 0; y < DrawUtils::SCALE; ++y) {
                for (uint16_t x = 0; x < DrawUtils::SCALE; ++x) {
                    uint16_t position_x = (lcd_x * DrawUtils::SCALE) + x;
                    uint16_t position_y = (_get(LY) * DrawUtils::SCALE) + y;

                    lcd_frame_image.setPixel({position_x, position_y},
                                             final_pixel_color);
                }
            }

            lcd_x++; // increment the lcd x position

            // check for window fetching after pixel was shifted out to LCD
            if (!fetch_window && (_get(LCDC) & 0x20) &&
                (lcd_x >= _get(WX) - 7) && wy_condition) {
                tile_index = 0;
                background_fifo.clear();
                fetch_window = true;
                current_fetcher_mode = fetcher_mode::FetchTileNo;
            }
        }

        if (lcd_x == 160) {
            /*
            assert(ticks >= 173+80 && ticks <= 293+80 &&
                   "ticks in drawing mode should be between 172 and 289!!");
                   */

            // TODO: ticks are around 389 - 413, figure out why

            if (ticks > 373) {
                std::cout << ticks << '\n';
            }
            // std::cout << ticks << '\n';

            current_mode = ppu_mode::HBlank;
        }

        break;
    }

    case ppu_mode::HBlank: {

        // set IF for interrupt
        if ((this->last_mode != this->current_mode) && (_get(STAT) >> 3 & 1)) {
            _set(IF, _get(IF) | 2);
        }

        // pause until 456 T-cycles have finished

        // wait 456 T-cycles
        if (ticks == 456) {

            if (fetch_window) {
                window_ly++;
            } // add window ly before resetting fetch window

            ticks = 0; // reset ticks

            // i think we can reset LCD x in HBlank
            lcd_x = 0;
            // dummy fetch too?
            dummy_fetch = true;
            fetch_window = false;

            // clear sprite buffer
            sprite_buffer.clear();

            _set(LY, _get(LY) + 1); // new scanline reached

            if (_get(LY) == 144) {
                // hit VBlank for the first time

                bool success = lcd_frame.loadFromImage(lcd_frame_image);
                assert(success && "LCD dots texture error loading from image");

                sf::Sprite frame(lcd_frame);

                window.clear(sf::Color::White);
                window.draw(frame);
                window.display();

                // v-blank IF interrupt
                _set(IF, _get(IF) | 1);
                // end v-blank if interrupt

                current_mode = ppu_mode::VBlank;
            }

            else {
                // reset LCD x
                current_mode = ppu_mode::OAM_Scan;
            }
        }

        break;
    }

    case ppu_mode::VBlank: {

        // set IF for interrupt
        if ((this->last_mode != this->current_mode) && (_get(STAT) >> 4 & 1)) {
            _set(IF, _get(IF) | 2);
        }

        if (ticks == 456) {
            ticks = 0; // wait 456 T-cycles for the whole scanline, reset ticks
            _set(LY, _get(LY) + 1); // new scanline reached

            if (_get(LY) == 154) {
                // reset LY, window LY
                _set(LY, 0);
                window_ly = 0;

                // set wy_condition = false since we are on the next frame
                wy_condition = false;

                current_mode = ppu_mode::OAM_Scan;

            } // after 10 scanlines
        }

        break;
    }
    }

    // stat interrupt every tick
    if (_get(LY) == _get(LYC)) {
        if ((_get(STAT) >> 6) & 1) {
            if (((_get(STAT) >> 2) & 1) == 0) {
                _set(IF, _get(IF) | 2);
                _set(STAT, _get(STAT) | 4);
            }
        }
    } else {
        _set(STAT, _get(STAT) & ~4); // sets coincidence bit to 0 explicitly
    }
}
