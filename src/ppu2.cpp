#include "ppu2.h"
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

sf::Color PPU2::get_pixel_color(uint8_t pixel, uint8_t *palette) {
    // Get the appropriate palette register
    uint8_t palette_value;

    if (!palette) {
        palette_value = _get(BGP); // Background palette
    } else {
        // Assert valid sprite palette before using it
        assert(*palette == 0 ||
               *palette == 1 && "sprite palette is not 0 or 1!");
        palette_value = *palette == 0 ? _get(0xFF48) : _get(0xFF49);
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

    // oam search mode 2
    switch (current_mode) {
    case ppu_mode::OAM_Scan: {

        assert(ticks <= 80 && "ticks must be <= 80 during OAM Scan");
        if ((ticks % 2 == 0)) { // 40 objects

            std::array<uint8_t, 4> oam_entry_array{};
            for (unsigned int i = 0; i < 4; ++i) {
                // 4 bytes per entry
                oam_entry_array[i] =
                    _get(0xfe00 + (oam_search_counter * 4) + i);
            }

            oam_entry entry = {
                oam_entry_array[1], // x position
                oam_entry_array[0], // y position
                oam_entry_array[2], // tile #
                oam_entry_array[3]  // flags
            };

            // add to sprite buffer on specific conditions
            if (entry.x > 0 && _get(LY) + 16 >= entry.y &&
                _get(LY) + 16 < entry.y + (_get(LCDC) & 0x04 ? 16 : 8) &&
                sprite_buffer.size() < 10) {

                sprite_buffer.push_back(entry);
            }

            // TODO: is this sorting necessary? do i need to sort reverse
            if (sprite_buffer.size() > 1) {
                std::sort(
                    sprite_buffer.begin(), sprite_buffer.end(),
                    [](const oam_entry &a, oam_entry &b) { return a.x < b.x; });
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
            for (unsigned int i = 0; i < sprite_buffer.size(); ++i) {
                if (sprite_buffer[i].x <= lcd_x + 8) {
                    sprites_to_fetch.push_back(sprite_buffer[i]);
                    // sprite_to_fetch = sprite_buffer[i];
                }
            }
        }

        // TODO: see if sprites are fetched twice
        if (!sprites_to_fetch.empty()) {
            fetch_sprite = true;
            sprite_to_fetch = sprites_to_fetch[0];
            sprites_to_fetch.erase(
                sprites_to_fetch.begin()); // remove 1st sprite after setting
                                           // sprite to fetch
        }

        else {
            fetch_sprite = false; // reset fetch sprite of the sprites to fetch
                                  // is empty now
        }

        switch (current_fetcher_mode) {
        case fetcher_mode::FetchTileNo: {
            if (ticks % 2 == 0)
                return;

            if (fetch_sprite) {
                // set tile id to sprite to fetch tile id
                this->tile_id = sprite_to_fetch.tile_id;
            }

            else {
                uint16_t address{};
                // check to see which map to use
                uint16_t bgmap_start = 0x9800;

                if (!fetch_window) {
                    if (((_get(LCDC) >> 3) & 1) == 1) {
                        bgmap_start = 0x9c00;
                    }
                    // fetch tile no.
                    uint16_t scy_offset =
                        32 * (((_get(LY) + _get(SCY)) % 256) / 8);
                    uint16_t scx_offset = (_get(SCX) % 256) / 8;
                    address = bgmap_start +
                              ((scy_offset + scx_offset + tile_index) & 0x3ff);

					tile_id = _get(address);
                }

                else {
                    // window fetch
                    if (_get(LCDC) & 0x40) { // 6th bit
                        bgmap_start = 0x9c00;
                    } // bgmap_start is either 0x9c00 or 0x9800

                    uint16_t wy_offset = 32 * (window_ly / 8);
                    address = bgmap_start + ((wy_offset + tile_index) & 0x3ff);
                }

                this->tile_id = _get(address);
            }

            current_fetcher_mode = fetcher_mode::FetchTileDataLow;
            break;
        }

        case fetcher_mode::FetchTileDataLow: {
            if (ticks % 2 == 0)
                return;

            if (fetch_sprite) {
                assert(sprite_to_fetch.y >= 16 &&
                       "sprite to fetch y position is abnormal!");

                // TODO: fetch sprite
                uint16_t sprite_address{};

                bool y_flip =
                    _get(sprite_to_fetch.flags) & 0x40; // bit 6 y-flip

                if (_get(LCDC) & 0x04) {
                    // tall sprite mode
                    if (_get(LY) <= (sprite_to_fetch.y - 16) + 7) {
                        // top half
                        if (!y_flip) {
                            tile_id &= 0xfe; // set lsb to 0
                        } else {
                            tile_id |= 0x01; // set lsb to 1
                        }
                    }

                    else {
                        // bottom half
                        if (!y_flip) {
                            tile_id |= 0x01;
                        } else {
                            tile_id &= 0xfe;
                        }
                    }
                }

                uint16_t line_offset{};
                if (!y_flip) {
                    line_offset = (_get(LY) - (sprite_to_fetch.y - 16)) * 2;
                }

                else {
                    // get the flipped sprite data
                    line_offset =
                        (8 - 1 - (_get(LY) - (sprite_to_fetch.y - 16))) * 2;
                }

                sprite_address = 0x8000 + (16 * tile_id) + line_offset;

                low_byte = _get(sprite_address);
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
            }

            current_fetcher_mode = fetcher_mode::FetchTileDataHigh;
            break;
        }

        case fetcher_mode::FetchTileDataHigh: {
            if (ticks % 2 == 0)
                return;

            if (fetch_sprite) {
                assert(sprite_to_fetch.y >= 16 &&
                       "sprite to fetch y position is abnormal!");

                // TODO: fetch sprite
                uint16_t sprite_address{};

                bool y_flip =
                    _get(sprite_to_fetch.flags) & 0x40; // bit 6 y-flip

                if (_get(LCDC) & 0x04) {
                    // tall sprite mode
                    if (_get(LY) <= (sprite_to_fetch.y - 16) + 7) {
                        // top half
                        if (!y_flip) {
                            tile_id &= 0xfe; // set lsb to 0
                        } else {
                            tile_id |= 0x01; // set lsb to 1
                        }
                    }

                    else {
                        // bottom half
                        if (!y_flip) {
                            tile_id |= 0x01;
                        } else {
                            tile_id &= 0xfe;
                        }
                    }
                }

                uint16_t line_offset{};
                if (!y_flip) {
                    line_offset = (_get(LY) - (sprite_to_fetch.y - 16)) * 2;
                }

                else {
                    // get the flipped sprite data
                    line_offset =
                        (8 - 1 - (_get(LY) - (sprite_to_fetch.y - 16))) * 2;
                }

                sprite_address =
                    0x8000 + (16 * tile_id) + line_offset + 1; // high byte

                high_byte = _get(sprite_address);
            }

            else { // bg/window
                uint16_t offset = fetch_window
                                      ? 2 * (this->window_ly % 8)
                                      : 2 * ((_get(LY) + _get(SCY)) % 8);

                uint16_t address{};

                // 8000 method or 8800 method to read
                if ((_get(LCDC) >> 4 & 1) == 0) {
                    // 8800 method
                    address =
                        0x9000 + (static_cast<int8_t>(tile_id) * 16) + offset;
                } else {
                    // 8000 method
                    address = 0x8000 + (tile_id * 16) + offset;
                }
                high_byte = _get(address + 1);
            }

            current_fetcher_mode = fetcher_mode::PushToFIFO;
            break;
        }
        case fetcher_mode::PushToFIFO: {
            if (fetch_sprite) {

                assert(sprite_to_fetch.x <= lcd_x + 8 &&
                       "sprite x position is not <= lcd_x + 8!");

                bool x_flip = sprite_to_fetch.flags & 0x20; // x flip

                for (unsigned int i = 0; i < 8; ++i) {

                    unsigned int j = x_flip ? 7 - i : i;

                    uint8_t final_bit = ((low_byte >> j) & 1) |
                                        (((high_byte >> j) << 1) & 0x02);

                    // check sprite x position to see if pixels should be loaded
                    // into fifo
                    if (sprite_to_fetch.x < 8) {
                        if (i < 8 - sprite_to_fetch.x) {
                            continue;
                        }
                    }

                    sprite_fifo_pixel pixel = {final_bit,
                                               sprite_to_fetch.flags};

                    uint8_t y =
                        sprite_to_fetch.x < 8 ? i - (8 - sprite_to_fetch.x) : i;

                    if (y < sprite_fifo.size()) {
                        if (sprite_fifo[y].color_id == 0) {
                            // replace it
                            sprite_fifo[y] = pixel;
                        }
                    }

                    else {
                        sprite_fifo[y] = pixel;
                    }
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
        if (!background_fifo.empty()) {

            // fine x (scx)
            if (lcd_x == 0) {
                for (unsigned int i = 0; i < _get(SCX) % 8; ++i) {
                    background_fifo.pop_back();
                }
            }

            // get the background pixel
            uint8_t bg_pixel = background_fifo.back();
            background_fifo.pop_back();

            sf::Color final_pixel_color = get_pixel_color(bg_pixel);

            if (!sprite_fifo.empty()) {
                sprite_fifo_pixel sprite_pixel = sprite_fifo.back();
                sprite_fifo.pop_back();

                uint8_t palette = (sprite_pixel.flags >> 4) & 1;

                sf::Color sprite_pixel_color =
                    get_pixel_color(sprite_pixel.color_id, &palette);

                bool sprite_priority =
                    !((sprite_pixel.flags >> 7) & 1) || !bg_pixel;

                if (sprite_pixel.color_id && sprite_priority) {
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
        }

        if (lcd_x == 160) {
            assert(ticks >= 172+80 && ticks <= 289+80 &&
                   "ticks in drawing mode should be between 172 and 289!!");

            current_mode = ppu_mode::HBlank;
        }

        break;
    }
    case ppu_mode::HBlank: {

        // pause until 456 T-cycles have finished


        // wait 456 T-cycles
        if (ticks == 456) {
            ticks = 0; // reset ticks

            // i think we can reset LCD x in HBlank
            lcd_x = 0;
            // dummy fetch too?
            dummy_fetch = true;
            fetch_window = false;


            _set(LY, _get(LY) + 1); // new scanline reached
            if (fetch_window) {
                window_ly++;
            }

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

        if (ticks == 456) {
            ticks = 0; // wait 456 T-cycles for the whole scanline, reset ticks
            _set(LY, _get(LY) + 1); // new scanline reached

            if (_get(LY) == 154) {
                // reset LY, window LY
                _set(LY, 0);
                window_ly = 0;

                current_mode = ppu_mode::OAM_Scan;

            } // after 10 scanlines
        }

        break;
    }

    }
}
