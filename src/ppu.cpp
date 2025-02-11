#include "PPU.h"
#include "DrawUtils.h"
#include <iostream>

uint8_t PPU::_get(uint16_t address) {
    return this->gb_mmu.read_memory(address);
}
void PPU::_set(uint16_t address, uint8_t value) {
    this->gb_mmu.write_memory(address, value);
}
uint8_t Fetcher::_get(uint16_t address) {
    return this->gb_mmu.read_memory(address);
}
void Fetcher::_set(uint16_t address, uint8_t value) {
    this->gb_mmu.write_memory(address, value);
}

// ppu constructor
PPU::PPU(MMU &gb_mmu_, sf::RenderWindow &window_)
    : gb_mmu(gb_mmu_), window(window_) {

    // TODO check logic for setting STAT to 1000 0000 (resetting STAT)
    _set(STAT, 0x80);

    sf::Image image({window.getSize().x, window.getSize().y}, sf::Color::White);
    this->lcd_dots_image = image;
}; // pass mmu by reference

// TODO: move fetcher to another file

// determine if sprite should be added to sprite buffer
// TODO: need to change this to get all 4 bytes of the OAM sprite metadata
void PPU::add_to_sprite_buffer(std::array<uint8_t, 4> oam_entry) {
    // byte 0 = y position, byte 1 = x position, byte 2 = tile number, byte 3 =
    // sprite flags
    const bool x_pos_gt_0 = oam_entry[1] > 0;
    const bool LY_plus_16_gt_y_pos = _get(LY) + 16 >= oam_entry[0];
    // get the tall mode LCDC bit 2
    const uint8_t sprite_height = _get(LCDC) >> 2 == 1 ? 16 : 8;
    // LY + 16 < y pos + sprite_height
    const bool sprite_height_condition =
        _get(LCDC) + 16 < oam_entry[0] + sprite_height;

    if (!x_pos_gt_0 || !LY_plus_16_gt_y_pos || !sprite_height ||
        !sprite_height_condition || this->oam_buffer.size() >= 10) {
        // none of the conditions were met
        return;
    }
    oam_buffer.push_back(oam_entry);
}

std::array<uint8_t, 3> PPU::_get_color(uint8_t id) {
    switch (id) {
    case 0 : return color_palette_white; break;
    case 1 : return color_palette_light_gray; break;
    case 2 : return color_palette_dark_gray; break;
    case 3 : return color_palette_black; break;
    default: return color_palette_white; break;
    }
}

sf::Color PPU::get_dot_color(uint8_t dot) {
    // TODO: maybe i only need to do this once per rom load
    uint8_t id_0 = _get(BGP) & 0x03;      // id 0 0x03 masks last 2 bits
    uint8_t id_1 = _get(BGP) >> 2 & 0x03; // id 1
    uint8_t id_2 = _get(BGP) >> 4 & 0x03; // id 2
    uint8_t id_3 = _get(BGP) >> 6 & 0x03; // id 3
    // 0 = white, 1 = light gray, 2 = dark gray, 3 = black

    std::array<uint8_t, 3> color_0 = _get_color(id_0);
    std::array<uint8_t, 3> color_1 = _get_color(id_1);
    std::array<uint8_t, 3> color_2 = _get_color(id_2);
    std::array<uint8_t, 3> color_3 = _get_color(id_3);

    uint8_t r = color_palette_white[0];
    uint8_t g = color_palette_white[1];
    uint8_t b = color_palette_white[2];
    switch (dot) {
    case 0:
        r = color_0[0];
        g = color_0[1];
        b = color_0[2];
        break;
    case 1:
        r = color_1[0];
        g = color_1[1];
        b = color_1[2];
        break;
    case 2:
        r = color_2[0];
        g = color_2[1];
        b = color_2[2];
        break;
    case 3:
        r = color_3[0];
        g = color_3[1];
        b = color_3[2];
        break;
    }
    return {r, g, b};
}

void Fetcher::tick() {
    // TODO: modularize stuff here
    // fetcher
    this->fetcher_ticks++;
    if (fetcher_ticks < 2) {
        return;
    }
    fetcher_ticks = 0;

    switch (current_mode) {
    case Fetcher::mode::FetchTileNo: {
        // check bit 3 of LCDC to see which background map to use
        uint16_t bgmap_start = 0x9800;
        uint16_t bgmap_end = 0x9bff;
        if ((_get(LCDC) >> 3 & 1) == 1) {
            bgmap_start = 0x9c00;
            bgmap_end = 0x9fff;
        }

        // fetch tile no.
        // TODO: check scx offset
        uint16_t scy_offset = 32 * (((_get(LY) + _get(SCY)) % 256) / 8);
        uint16_t scx_offset = (_get(SCX) % 256) / 8;
        uint16_t address =
            bgmap_start + ((scy_offset + scx_offset + tile_index) & 0x3ff);

        // TODO: here
        this->tile_id = this->gb_mmu.read_memory(address);
        this->current_mode = Fetcher::mode::FetchTileDataLow;
        break;
    }
    case Fetcher::mode::FetchTileDataLow: {
        uint16_t offset = 2 * ((_get(LY) + _get(SCY)) % 8);
        uint8_t low_byte{};

        // 8000 method or 8800 method to read
        if (((_get(LCDC) >> 4) & 1) != 1) {
            // 8800 method
            uint16_t address =
                0x9000 + (static_cast<int8_t>(tile_id) * 16) + offset;
            low_byte = this->gb_mmu.read_memory(address);
        } else {
            // 8000 method
            uint16_t address = 0x8000 + (tile_id * 16) + offset;
            low_byte = this->gb_mmu.read_memory(address);
        }
        for (unsigned int i = 0; i < 8; ++i) {
            this->pixel_buffer[i] = (low_byte >> i) & 1;
        }

        // bool eightk_method{true};
        this->current_mode = Fetcher::mode::FetchTileDataHigh;
        break;
    }

    case Fetcher::mode::FetchTileDataHigh: {
        uint16_t offset = 2 * ((_get(LY) + _get(SCY)) % 8);
        uint8_t high_byte{};

        // 8000 method or 8800 method to read
        if ((_get(LCDC) >> 4 & 1) != 1) {
            // 8800 method
            uint16_t address =
                0x9000 + (static_cast<int8_t>(tile_id) * 16) + offset + 1;
            high_byte = this->gb_mmu.read_memory(address);
        } else {
            // 8000 method
            uint16_t address = 0x8000 + (tile_id * 16) + offset + 1;
            high_byte = this->gb_mmu.read_memory(address);
        }
        for (unsigned int i = 0; i < 8; ++i) {
            uint8_t high_bit = ((high_byte >> i) << 1) &
                               2; // use second position for the high bits
            this->pixel_buffer[i] =
                this->pixel_buffer[i] | high_bit; // combine 2 bits together
        }

        this->current_mode = Fetcher::mode::PushToFIFO;
        break;
    }
    case Fetcher::mode::PushToFIFO: {
        if (fifo.size() <= 8) {
            // for (int i = 7; i >= 0; --i) {
            for (int i = 0; i < 8; ++i) {
                this->fifo.push_back(this->pixel_buffer[i]);
            }
        }
        this->tile_index++;
        this->current_mode = Fetcher::mode::FetchTileNo;
        break;
    }
    }
}

// 1 tick = 1 T-Cycle
void PPU::tick() {
    this->ppu_ticks++;

    // set stat to mode
    _set(STAT, (_get(STAT) & 0xfc) | static_cast<uint8_t>(current_mode));
    last_mode = current_mode;

    switch (this->current_mode) {
    case mode::OAM_Scan: {

        // set IF for interrupt
        if ((this->last_mode != this->current_mode) && (_get(STAT) >> 5 & 1)) {
            _set(IF, _get(IF) | 2);
        }

        if (this->ppu_ticks % 2 == 0 &&
            oam_buffer_counter < 40) { // 40 oam entries
            // oam scan 1 entry (4 bytes) every 2 ticks
            // bytes from fe00 to fe9f
            // sprite fetcher
            std::array<uint8_t, 4> oam_entry{};
            for (unsigned int i = 0; i < oam_entry.size(); ++i) {
                oam_entry[i] = this->gb_mmu.read_memory(
                    OAM_START_ADDRESS + 4 * oam_buffer_counter + i);
            }
            oam_buffer_counter++; // each oam entry is 4 bytes
        } else if (this->ppu_ticks == 80) {
            // TODO: start the fetcher based on LY
            assert(oam_buffer_counter == 40 && "OAM counter is not 40!");

            // prepare for drawing
            // clear fifo
            this->ppu_fetcher.fifo.clear();

            // reset tile index
            // this->ppu_fetcher.tile_index = 0;
            this->ppu_fetcher.tile_index = _get(SCX) / 8;

            // reset the ppu fetcher mode
            this->ppu_fetcher.current_mode = Fetcher::mode::FetchTileNo;

            this->current_mode = mode::Drawing;
        }
        break;
    }
    case mode::Drawing: {

        // fetcher tick
        this->ppu_fetcher.tick();

        if (!this->ppu_fetcher.fifo.empty()) {
            // draw
            /*
            float f_dot_count = dot_count;
            float f_ly = *(this->LY);
            sf::Vector2f position = {f_dot_count, f_ly};
            */

            sf::Vector2u position = {dot_count, _get(LY)};

            uint8_t dot = this->ppu_fetcher.fifo.back();
            this->ppu_fetcher.fifo.pop_back();

            sf::Color rgb = get_dot_color(dot);

            // sf::Vertex lcd_dot =
            //     DrawUtils::add_vertex(position, r, g, b);

            // sf::RectangleShape lcd_dot =
            //     DrawUtils::add_pixel({f_dot_count, f_ly}, rgb.r, rgb.g,
            //     rgb.b);

            this->lcd_dots.push_back(dot);

            // TODO: is there a better way to do this?
            // fill up entire screen
            uint16_t position_y_offset = 0;
            for (uint16_t i = 0; i < DrawUtils::SCALE * DrawUtils::SCALE; ++i) {
                uint16_t position_x =
                    (i % DrawUtils::SCALE) + (position.x * DrawUtils::SCALE);
                if (i % DrawUtils::SCALE == 0 && i != 0) {
                    position_y_offset++;
                }
                uint16_t position_y =
                    (position.y * DrawUtils::SCALE) + position_y_offset;

                lcd_dots_image.setPixel({position_x, position_y}, rgb);
            }

            // TODO: optimize by only loading the palette when rom loads or
            // something

            // TODO: need to find a way to clear while drawing the
            // original pixels need a vector of dots to draw

            this->dot_count++;
        }

        // TODO: draw without fifo first, remove after

        if (dot_count == 160) {

            // finished
            this->current_mode = mode::HBlank;
        }
        break;
    }

    case mode::HBlank: {

        // set IF for interrupt
        if ((this->last_mode != this->current_mode) && (_get(STAT) >> 3 & 1)) {
            _set(IF, _get(IF) | 2);
        }

        // wait 456 t-cycles
        if (this->ppu_ticks == 456) {
            this->ppu_ticks = 0;
            _set(LY, _get(LY)+1); // new scanline reached

            // reset dot
            this->dot_count = 0;

            if (_get(LY) == 144) {

                // draw?
                bool success = lcd_dots_texture.loadFromImage(lcd_dots_image);
                assert(success && "LCD dots texture error loading from image");

                sf::Sprite lcd_dots_sprite(lcd_dots_texture);

                window.clear(sf::Color::White);
                window.draw(lcd_dots_sprite);
                window.display();

                // v-blank IF interrupt
                _set(IF, _get(IF) | 1);
                // end v-blank if interrupt

                this->current_mode = mode::VBlank;
            } else {
                this->current_mode = mode::OAM_Scan;
            }
        }

        break;
    }

    case mode::VBlank: {

        // set IF for interrupt
        if ((this->last_mode != this->current_mode) && (_get(STAT) >> 4 & 1)) {
            _set(IF, _get(IF) | 2);
        }

        if (this->ppu_ticks == 456) {

            this->ppu_ticks = 0;
            _set(LY, _get(LY)+1); // new scanline reached

            // stat interrupt check
            // TODO: check stat interrupt
            if (_get(LY) == _get(LYC)) {
                if ((_get(STAT) >> 6) & 1) {
                    if (((_get(STAT) >> 2) & 1) == 0) {
                        _set(IF, _get(IF) | 2);
                        _set(STAT, _get(STAT) | 4);
                    }
                }
            } else {
                _set(STAT, _get(STAT) & ~4);
            }

            if (_get(LY) == 153) {   // wait 10 more scanlines
                _set(LY, 0);      // reset LY
                this->lcd_dots.clear(); // reset LCD
                this->current_mode = PPU::mode::OAM_Scan;
            }
        }
        break;
    }
    }
}
