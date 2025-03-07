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
            if (entry.x >= 0 && _get(LY) + 16 >= entry.y &&
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

        // assert that ticks is between 172 and 289
        // drawing mode 3
        // fetch tile no

        // check for sprites right away
        if (!sprite_buffer.empty() && !fetch_sprite) {
            for (unsigned int i = 0; i < sprite_buffer.size(); ++i) {
                if (sprite_buffer[i].x <= lcd_x + 8) {
                    fetch_sprite = true;
                    sprite_to_fetch = sprite_buffer[i];
                    break;
                }
            }
        }

        switch (current_fetcher_mode) {
        case fetcher_mode::FetchTileNo: {
            if (ticks % 2 != 0)
                return;

            if (fetch_sprite) {
                // do nothing, sprite to fetch was already set
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
                    if (((_get(LCDC) >> 6) & 1) == 1) {
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
                // TODO: fetch sprite
            }

            current_fetcher_mode == fetcher_mode::FetchTileDataHigh;
            break;
        }

        case fetcher_mode::FetchTileDataHigh: {
            if (ticks % 2 != 0)
                return;

            current_fetcher_mode == fetcher_mode::PushToFIFO;
            break;
        }
        case fetcher_mode::PushToFIFO: {

            current_fetcher_mode == fetcher_mode::PushToFIFO;
            break;
        }
        }
    }

        ticks = 0;
    }
}
