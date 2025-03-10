#include "ppu2.h"

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
    this->lcd_dots_image = image; // image with pixels to display on the screen
};

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

        else if (ticks > 80) {
            assert(oam_search_counter == 40 && "oam search counter is not 40!");
            oam_search_counter = 0; // reset oam search counter
        }

        current_mode = ppu_mode::Drawing;
        break;
    }

    case ppu_mode::Drawing: {

        // assert that ticks this step takes is between 172 and 289
        // drawing mode 3
        // fetch tile no

        // check for sprites right away
        if (!sprite_buffer.empty() && !fetch_sprite &&
            (_get(LCDC) & 0x02) && sprites_to_fetch.empty()) { // check bit 1 for enable sprites
            for (unsigned int i = 0; i < sprite_buffer.size(); ++i) {
                if (sprite_buffer[i].x <= lcd_x + 8) {
                    sprites_to_fetch.push_back(sprite_buffer[i]);
                    //sprite_to_fetch = sprite_buffer[i];
                }
            }
        }

        // TODO: see if sprites are fetched twice
        if (!sprites_to_fetch.empty()) {
            fetch_sprite = true;
            sprite_to_fetch = sprites_to_fetch[0];
            sprites_to_fetch.erase(sprites_to_fetch.begin()); // remove 1st sprite after setting sprite to fetch
        }

        else {
            fetch_sprite = false; // reset fetch sprite of the sprites to fetch is empty now
        }


        switch (current_fetcher_mode) {
        case fetcher_mode::FetchTileNo: {
            if (ticks % 2 != 0)
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
                }

                else {
                    // window fetch
                    if (_get(LCDC) & 0x40) { // 6th bit
                        bgmap_start = 0x9c00;
                    } // bgmap_start is either 0x9c00 or 0x9800

                    uint16_t wy_offset = 32 * (window_ly / 8);
                    address = bgmap_start + ((wy_offset + tile_index) & 0x3ff);
                }

                this->tile_id = this->gb_mmu.read_memory(address);
            }

            current_fetcher_mode = fetcher_mode::FetchTileDataLow;
            break;
        }

        case fetcher_mode::FetchTileDataLow: {
            if (ticks % 2 != 0)
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

                // 8000 method or 8800 method to read
                if (((_get(LCDC) >> 4) & 1) == 0) {
                    // 8800 method
                    uint16_t address =
                        0x9000 + (static_cast<int8_t>(tile_id) * 16) + offset;
                    low_byte = this->gb_mmu.read_memory(address);

                } else {
                    // 8000 method
                    uint16_t address = 0x8000 + (tile_id * 16) + offset;
                    low_byte = this->gb_mmu.read_memory(address);
                }
            }

            current_fetcher_mode == fetcher_mode::FetchTileDataHigh;
            break;
        }

        case fetcher_mode::FetchTileDataHigh: {
            if (ticks % 2 != 0)
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

                sprite_address = 0x8000 + (16 * tile_id) + line_offset + 1; // high byte

                high_byte = _get(sprite_address);
            }

            else { // bg/window
                uint16_t offset = fetch_window
                                      ? 2 * (this->window_ly % 8)
                                      : 2 * ((_get(LY) + _get(SCY)) % 8);

                // 8000 method or 8800 method to read
                if ((_get(LCDC) >> 4 & 1) == 0) {
                    // 8800 method
                    uint16_t address = 0x9000 +
                                       (static_cast<int8_t>(tile_id) * 16) +
                                       offset + 1;
                    high_byte = this->gb_mmu.read_memory(address);
                } else {
                    // 8000 method
                    uint16_t address = 0x8000 + (tile_id * 16) + offset + 1;
                    high_byte = this->gb_mmu.read_memory(address);
                }

				if (dummy_fetch) {
                    current_fetcher_mode = fetcher_mode::FetchTileNo;
					dummy_fetch = false;
                    break;
					//return;
				}
            }

            current_fetcher_mode == fetcher_mode::PushToFIFO;
            break;
        }
        case fetcher_mode::PushToFIFO: {
            if (ticks % 2 != 0)
                return;

            if (fetch_sprite) {
                for (unsigned int i = 0; i < 8; ++i) {

                    uint8_t final_bit = ((low_byte >> i) & 1) |
                                        (((high_byte >> i) << 1) & 0x02);

                    sprite_fifo_pixel pixel = {
                        final_bit,
                        sprite_to_fetch.flags
                    };

                    if (i < sprite_fifo.size()) {
                        if (sprite_fifo[i].color_id == 0) {
                            // replace it
                            sprite_fifo[i] = pixel;
                        }
                    }

                    else {
                        sprite_fifo[i] = pixel;
                    }
                }
            }

            else {
                if (background_fifo.empty()) {
                    // push to background fifo
                    for (unsigned int i = 0; i < 8; ++i) {
                        uint8_t final_bit = ((low_byte >> i) & 1) | (((high_byte >> i) << 1) & 0x02);
                        background_fifo.push_back(final_bit);
                    }
                }
            }

            current_fetcher_mode == fetcher_mode::FetchTileNo;
            break;
        }
        }

        // push pixels to LCD

    }

        ticks = 0;
    }
}
