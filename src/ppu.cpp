#include "ppu.h"
#include "DrawUtils.h"
#include <iostream>

// determine if sprite should be added to sprite buffer
// TODO: need to change this to get all 4 bytes of the OAM sprite metadata
void ppu::add_to_sprite_buffer(std::array<uint8_t, 4> oam_entry) {
    // byte 0 = y position, byte 1 = x position, byte 2 = tile number, byte 3 =
    // sprite flags
    const bool x_pos_gt_0 = oam_entry[1] > 0;
    const bool LY_plus_16_gt_y_pos = *(this->LY) + 16 >= oam_entry[0];
    // get the tall mode LCDC bit 2
    const uint8_t sprite_height = *(this->LCDC) >> 2 == 1 ? 16 : 8;
    // LY + 16 < y pos + sprite_height
    const bool sprite_height_condition =
        *(this->LCDC) + 16 < oam_entry[0] + sprite_height;

    if (!x_pos_gt_0 || !LY_plus_16_gt_y_pos || !sprite_height ||
        !sprite_height_condition || this->oam_buffer.size() >= 10) {
        // none of the conditions were met
        return;
    }
    oam_buffer.push_back(oam_entry);
}

std::array<uint8_t, 3> ppu::_get_color(uint8_t id) {
    switch (id) {
    case 0 : return color_palette_white; break;
    case 1 : return color_palette_light_gray; break;
    case 2 : return color_palette_dark_gray; break;
    case 3 : return color_palette_black; break;
    default: return color_palette_white; break;
    }
}

sf::Color ppu::get_dot_color(uint8_t dot) {
    // TODO: maybe i only need to do this once per rom load
    uint8_t id_0 = *BGP & 0x03;      // id 0 0x03 masks last 2 bits
    uint8_t id_1 = *BGP >> 2 & 0x03; // id 1
    uint8_t id_2 = *BGP >> 4 & 0x03; // id 2
    uint8_t id_3 = *BGP >> 6 & 0x03; // id 3
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

void fetcher::tick() {
    // TODO: modularize stuff here
    // fetcher
    this->fetcher_ticks++;
    if (fetcher_ticks < 2) {
        return;
    }
    fetcher_ticks = 0;

    switch (current_mode) {
    case fetcher::mode::FetchTileNo: {
        // check bit 3 of LCDC to see which background map to use
        uint16_t bgmap_start = 0x9800;
        uint16_t bgmap_end = 0x9bff;
        if ((*LCDC >> 3 & 1) == 1) {
            bgmap_start = 0x9c00;
            bgmap_end = 0x9fff;
        }

        // fetch tile no.
        // TODO: check scx offset
        uint16_t scy_offset = 32 * (((*LY + *SCY) % 256) / 8);
        uint16_t scx_offset = (*SCX % 256) / 8;
        uint16_t address =
            bgmap_start + ((scy_offset + scx_offset + tile_index) & 0x3ff);

        // TODO: here
        this->tile_id = this->gb_mmu.get_value_from_address(address);
        this->current_mode = fetcher::mode::FetchTileDataLow;
        break;
    }
    case fetcher::mode::FetchTileDataLow: {
        uint16_t offset = 2 * ((*LY + *SCY) % 8);
        uint8_t low_byte{};

        // 8000 method or 8800 method to read
        if (((*LCDC >> 4) & 1) != 1) {
            // 8800 method
            uint16_t address =
                0x9000 + (static_cast<int8_t>(tile_id) * 16) + offset;
            low_byte = this->gb_mmu.get_value_from_address(address);
        } else {
            // 8000 method
            uint16_t address = 0x8000 + (tile_id * 16) + offset;
            low_byte = this->gb_mmu.get_value_from_address(address);
        }
        for (unsigned int i = 0; i < 8; ++i) {
            this->pixel_buffer[i] = (low_byte >> i) & 1;
        }

        // bool eightk_method{true};
        this->current_mode = fetcher::mode::FetchTileDataHigh;
        break;
    }

    case fetcher::mode::FetchTileDataHigh: {
        uint16_t offset = 2 * ((*LY + *SCY) % 8);
        uint8_t high_byte{};

        // 8000 method or 8800 method to read
        if ((*LCDC >> 4 & 1) != 1) {
            // 8800 method
            uint16_t address =
                0x9000 + (static_cast<int8_t>(tile_id) * 16) + offset + 1;
            high_byte = this->gb_mmu.get_value_from_address(address);
        } else {
            // 8000 method
            uint16_t address = 0x8000 + (tile_id * 16) + offset + 1;
            high_byte = this->gb_mmu.get_value_from_address(address);
        }
        for (unsigned int i = 0; i < 8; ++i) {
            uint8_t high_bit = ((high_byte >> i) << 1) &
                               2; // use second position for the high bits
            this->pixel_buffer[i] =
                this->pixel_buffer[i] | high_bit; // combine 2 bits together
        }

        this->current_mode = fetcher::mode::PushToFIFO;
        break;
    }
    case fetcher::mode::PushToFIFO: {
        if (fifo.size() <= 8) {
            // for (int i = 7; i >= 0; --i) {
            for (int i = 0; i < 8; ++i) {
                // loop backwards 8 times (7-0) to push into fifo backwards
                // later you can loop fifo forwards to draw the pixels to LCD
                this->fifo.push_back(this->pixel_buffer[i]);
            }
        }
        this->tile_index++;
        this->current_mode = fetcher::mode::FetchTileNo;
        break;
    }
    }
}

// 1 tick = 1 T-Cycle
void ppu::tick() {
    this->ppu_ticks++;
    
    // set stat to mode
    *STAT = (*STAT & 0xfc) | static_cast<uint8_t>(current_mode);
    last_mode = current_mode;

    switch (this->current_mode) {
    case mode::OAM_Scan: {

        // set IF for interrupt
        if ((this->last_mode != this->current_mode) && (*STAT >> 5 & 1)) {
            *IF = *IF | 2;
        }

        if (this->ppu_ticks % 2 == 0 &&
            oam_buffer_counter < 40) { // 40 oam entries
            // oam scan 1 entry (4 bytes) every 2 ticks
            // bytes from fe00 to fe9f
            // sprite fetcher
            std::array<uint8_t, 4> oam_entry{};
            for (unsigned int i = 0; i < oam_entry.size(); ++i) {
                oam_entry[i] = this->gb_mmu.get_value_from_address(
                    OAM_START_ADDRESS + 4 * oam_buffer_counter + i);
            }
            oam_buffer_counter++; // each oam entry is 4 bytes
        } else if (this->ppu_ticks == 80 && oam_buffer_counter == 40) {
            // TODO: start the fetcher based on LY


            // prepare for drawing
            // clear fifo
            this->ppu_fetcher.fifo.clear();

            // reset tile index
            this->ppu_fetcher.tile_index = 0;

            // set the ppu fetcher mode
            // this->ppu_fetcher.current_mode = fetcher::mode::FetchTileNo;

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
            uint16_t f_dot_count = dot_count;
            uint16_t f_ly = *(this->LY);
            sf::Vector2u position = {f_dot_count, f_ly};

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
        if ((this->last_mode != this->current_mode) && (*STAT >> 3 & 1)) {
            *IF = *IF | 2;
        }

        // wait 456 t-cycles
        if (this->ppu_ticks == 456) {
            this->ppu_ticks = 0;
            ++*(this->LY); // new scanline reached

            // reset dot
            this->dot_count = 0;

            if (*(this->LY) == 144) {

                // draw?
                lcd_dots_texture.loadFromImage(lcd_dots_image);
                sf::Sprite lcd_dots_sprite(lcd_dots_texture);

                window.clear(sf::Color::White);
                window.draw(lcd_dots_sprite);
                window.display();

                // v-blank IF interrupt
                *IF = *IF | 1;
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
        if ((this->last_mode != this->current_mode) && (*STAT >> 4 & 1)) {
            *IF = *IF | 2;
        }

        if (this->ppu_ticks == 456) {

            this->ppu_ticks = 0;
            ++*(this->LY); // new blank scanline reached

            // stat interrupt check
            // TODO: check stat interrupt
            if (*LY == *LYC) {
                if ((*STAT >> 6) & 1) {
                    if (((*STAT >> 2) & 1) == 0) {
                        *IF = *IF | 2;
                        *STAT = *STAT | 4;
                    }
                }
            } else {
                *STAT = *STAT & ~4;
            }

            if (*(this->LY) == 153) {   // wait 10 more scanlines
                (*(this->LY)) = 0;      // reset LY
                this->lcd_dots.clear(); // reset LCD
                this->current_mode = ppu::mode::OAM_Scan;
            }
        }
        break;
    }
    }
}
