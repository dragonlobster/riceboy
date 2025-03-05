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

void Fetcher::reset() {
    // prepare for drawing
    // clear fifo
    this->background_fifo.clear();
    this->sprite_fifo.clear();

    // reset tile index
    this->tile_index = 0;
    // this->ppu_fetcher.tile_index = _get(SCX) / 8;

    // reset lcd x position
    this->lcd_x_position = -1 * (_get(SCX) % 8);
    // this->lcd_x_position = 0;

    // reset window fetch mode
    this->window_fetch = false;

    this->sprite_current_mode = Fetcher::mode::FetchTileNo;
    this->background_current_mode = Fetcher::mode::FetchTileNo;
    this->dummy_fetch = true;
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
    const uint8_t sprite_height = (_get(LCDC) >> 2) & 1 ? 16 : 8;
    // LY + 16 < y pos + sprite_height
    const bool sprite_height_condition =
        (_get(LY) + 16) < (oam_entry[0] + sprite_height);

    if (!x_pos_gt_0 || !LY_plus_16_gt_y_pos || !sprite_height ||
        !sprite_height_condition || ppu_fetcher.sprite_buffer.size() >= 10) {
        // none of the conditions were met
        return;
    }

    // calculate tile for tall mode
    /*
    if ((_get(LCDC) >> 2) & 1) { // tall mode
        if ((_get(LY) + 16) < (oam_entry[0] >= 8 ? (oam_entry[0] - 8) : 0)) {
            oam_entry[2] = oam_entry[2] | 1;
        } else {
            oam_entry[2] = oam_entry[2] & 0xfe; // 1111 1110
        }
    }*/

    // TODO: check suggested change
    if ((_get(LCDC) >> 2) & 1) { // 8×16 mode
        // force bit 0 of tile to 0 for top half, 1 for bottom half
        bool bottom_half = (_get(LY) + 16 - oam_entry[0]) >= 8;
        oam_entry[2] = (oam_entry[2] & 0xfe) | (bottom_half ? 1 : 0);
    }

    ppu_fetcher.sprite_buffer.push_back(oam_entry);
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

sf::Color PPU::get_pixel_color(uint8_t pixel, uint8_t *palette) {
    uint8_t id_0{};
    uint8_t id_1{};
    uint8_t id_2{};
    uint8_t id_3{};

    std::array<uint8_t, 3> color_0{};
    std::array<uint8_t, 3> color_1{};
    std::array<uint8_t, 3> color_2{};
    std::array<uint8_t, 3> color_3{};

    if (!palette) {
        // TODO: maybe i only need to do this once per rom load
        id_0 = _get(BGP) & 0x03;      // id 0 0x03 masks last 2 bits
        id_1 = _get(BGP) >> 2 & 0x03; // id 1
        id_2 = _get(BGP) >> 4 & 0x03; // id 2
        id_3 = _get(BGP) >> 6 & 0x03; // id 3
        // 0 = white, 1 = light gray, 2 = dark gray, 3 = black

        color_0 = _get_color(id_0);
        color_1 = _get_color(id_1);
        color_2 = _get_color(id_2);
        color_3 = _get_color(id_3);
    }

    else {
        assert(*palette == 0 ||
               *palette == 1 && "sprite palette is not 0 or 1!");

        uint8_t obp = *palette ? _get(0xff49) : _get(0xff48);

        id_0 = obp & 0x03; // id 0 0x03 masks last 2 bits (ignored in sprites)
        id_1 = obp >> 2 & 0x03; // id 1
        id_2 = obp >> 4 & 0x03; // id 2
        id_3 = obp >> 6 & 0x03; // id 3
        // 0 = white, 1 = light gray, 2 = dark gray, 3 = black

        color_0 = _get_color(id_0);
        color_1 = _get_color(id_1);
        color_2 = _get_color(id_2);
        color_3 = _get_color(id_3);
    }

    uint8_t r = color_palette_white[0];
    uint8_t g = color_palette_white[1];
    uint8_t b = color_palette_white[2];

    switch (pixel) {
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

void Fetcher::sprite_tick() {

    // TODO: check sprite fetch buffer
    // if there are, pause the background fifo

    this->sprite_fetcher_ticks++;

    switch (sprite_current_mode) {
    case Fetcher::mode::Paused     : this->sprite_fetcher_ticks = 0; break;
    case Fetcher::mode::FetchTileNo: {
        // 1 or 2 ticks?
        // this->sprite_fetcher_ticks = 0;
        if (sprite_fetcher_ticks < 2) {
            return;
        }

        this->sprite_tile = sprite_fetch_buffer.back(); // 2 is tile id
        // TODO: get the sprites from lowest x to highest x

        this->sprite_current_mode = Fetcher::mode::FetchTileDataLow;

        break;
    }

    case Fetcher::mode::FetchTileDataLow: {
        if (sprite_fetcher_ticks < 2) {
            return;
        }

        sprite_fetcher_ticks = 0;
        uint8_t low_byte{};
        uint16_t sprite_address{};
        // 8000 method

        /*
        uint16_t address = 0x8000 + (sprite_tile[2] * 16); // is there offset?
        low_byte = this->gb_mmu.read_memory(address);
        */

        uint16_t tile_offset = 16 * sprite_tile[2];
        // TODO: how to handle sprite_tile[0] < 16 ?
        uint16_t y_pos_diff =
            _get(LY) - (sprite_tile[0] >= 16 ? (sprite_tile[0] - 16) : 0);

        if ((sprite_tile[3] >> 6) & 1) {
            // y flip
            uint8_t line = ((_get(LCDC) >> 2) & 1) ? 30 : 14;
            sprite_address = 0x8000 + (tile_offset + (line - 2 * y_pos_diff));
        } else {
            sprite_address = 0x8000 + (tile_offset + (2 * y_pos_diff));
        }

        low_byte = _get(sprite_address);

        for (unsigned int i = 0; i < 8; ++i) {
            this->temp_sprite_fifo[i] = (low_byte >> i) & 1;
        }

        this->sprite_current_mode = Fetcher::mode::FetchTileDataHigh;
        break;
    }

    case Fetcher::mode::FetchTileDataHigh: {
        if (sprite_fetcher_ticks < 2) {
            return;
        }

        sprite_fetcher_ticks = 0;
        uint8_t high_byte{};
        uint16_t sprite_address{};
        // 8000 method

        /*
        uint16_t address = 0x8000 + (sprite_tile[2] * 16); // is there offset?
        low_byte = this->gb_mmu.read_memory(address);
        */

        uint16_t tile_offset = 16 * sprite_tile[2];
        // TODO: how to handle sprite_tile[0] < 16 ?
        uint16_t y_pos_diff =
            _get(LY) - (sprite_tile[0] >= 16 ? (sprite_tile[0] - 16) : 0);

        if ((sprite_tile[3] >> 6) & 1) { // y flip
            // y flip
            uint8_t line = ((_get(LCDC) >> 2) & 1) ? 30 : 14;
            sprite_address = 0x8000 + (tile_offset + (line - 2 * y_pos_diff));
        } else {
            sprite_address = 0x8000 + (tile_offset + (2 * y_pos_diff));
        }

        high_byte = _get(sprite_address + 1);

        for (unsigned int i = 0; i < 8; ++i) {
            uint8_t high_bit = ((high_byte >> i) << 1) &
                               2; // use second position for the high bits

            this->temp_sprite_fifo[i] =
                this->temp_sprite_fifo[i] | high_bit; // combine 2 bits together
        }

        this->sprite_current_mode = Fetcher::mode::PushToFIFO;
        break;
    }

    case Fetcher::mode::PushToFIFO: {
        if (sprite_fetcher_ticks < 2) {
            return;
        }

        bool hflip = (this->sprite_tile[3] >> 5) & 1;

        for (unsigned int i = 0; i < 8; ++i) {

            // 0 1 2 3 4 5 6 7
            // 7 6 5 4 3 2 1 0
            unsigned int j = hflip ? 7 - i : i;

            this->sprite_fifo.push_back(this->temp_sprite_fifo[j]);
            this->sprite_attr_fifo.push_back(this->sprite_tile[3]);
        }

        // pop the sprite fetch buffer here
        sprite_fetch_buffer.pop_back();

        if (this->sprite_fetch_buffer.empty()) {
            this->background_current_mode = Fetcher::mode::FetchTileNo;
            this->sprite_current_mode = Fetcher::mode::Paused;
            return;
        }

        assert(this->background_current_mode == Fetcher::mode::Paused &&
               "Background Fetcher should be paused right now!!");

        this->sprite_current_mode = Fetcher::mode::FetchTileNo;

        break;
    }
    }
}

void Fetcher::background_tick() {
    // TODO: modularize stuff here
    // fetcher
    this->background_fetcher_ticks++;

    switch (background_current_mode) {
    case Fetcher::mode::Paused     : this->background_fetcher_ticks = 0; break;
    case Fetcher::mode::FetchTileNo: {

        // 2 ticks
        if (background_fetcher_ticks < 2) {
            return;
        }
        background_fetcher_ticks = 0;

        uint16_t address{};

        // check bit 3 of LCDC to see which background map to use
        uint16_t bgmap_start = 0x9800;
        uint16_t bgmap_end = 0x9bff;

        if (!this->window_fetch) {
            if (((_get(LCDC) >> 3) & 1) == 1) {
                bgmap_start = 0x9c00;
                bgmap_end = 0x9fff;
            }

            // fetch tile no.
            // TODO: check scx offset
            uint16_t scy_offset = 32 * (((_get(LY) + _get(SCY)) % 256) / 8);
            uint16_t scx_offset = (_get(SCX) % 256) / 8;
            address =
                bgmap_start + ((scy_offset + scx_offset + tile_index) & 0x3ff);
        }

        else {
            // window fetch
            if (((_get(LCDC) >> 6) & 1) == 1) {
                bgmap_start = 0x9c00;
                bgmap_end = 0x9fff;
            }

            uint16_t wy_offset = 32 * (window_ly / 8);

            address = bgmap_start + ((wy_offset + tile_index) & 0x3ff);
        }

        // TODO: here
        this->tile_id = this->gb_mmu.read_memory(address);
        this->background_current_mode = Fetcher::mode::FetchTileDataLow;
        break;
    }
    case Fetcher::mode::FetchTileDataLow: {
        // 2 ticks
        if (background_fetcher_ticks < 2) {
            return;
        }
        background_fetcher_ticks = 0;

        uint8_t low_byte{};

        uint16_t offset = this->window_fetch ? 2 * (this->window_ly % 8)
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

        for (unsigned int i = 0; i < 8; ++i) {
            this->temp_background_fifo[i] = (low_byte >> i) & 1;
        }

        // bool eightk_method{true};
        this->background_current_mode = Fetcher::mode::FetchTileDataHigh;
        break;
    }

    case Fetcher::mode::FetchTileDataHigh: {
        // 2 ticks
        if (background_fetcher_ticks < 2) {
            return;
        }
        background_fetcher_ticks = 0;

        uint16_t offset = this->window_fetch ? 2 * (this->window_ly % 8)
                                             : 2 * ((_get(LY) + _get(SCY)) % 8);

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
            this->temp_background_fifo[i] = this->temp_background_fifo[i] |
                                            high_bit; // combine 2 bits together
        }

        if (dummy_fetch) {
            this->background_current_mode = Fetcher::mode::FetchTileNo;
            dummy_fetch = false;
            break; // dont set mode to pushtofifo
        }

        /*
        // 6 cycles passed - deal with sprite (sprite fetch tile no)
        // note: DMG LCDC bit 1 disables obj fetcher entirely, CGB doesn't

        // TODO: need to extend by 1 background_tick (oam fetch takes 2 ticks)
        if (_get(LCDC) & 2 &&
            !background_fifo.empty()) { // sprite drawing enabled
            for (int i = 0; i < sprite_buffer.size(); ++i) {
                if (sprite_buffer[i][1] <=
                    tile_index + 8) { // x position <= internal x (dot or
                                      // tile_index?) + 8
                    this->sprite_fetch_buffer.push_back(sprite_buffer[i]);
                }
            }
            // if not empty, fetch sprites
            if (!sprite_fetch_buffer.empty()) {
                this->background_current_mode =
        Fetcher::mode::SpriteFetchTileNo2; break;
            }
        }
        */

        this->background_current_mode = Fetcher::mode::PushToFIFO;
        break;
    }

    case Fetcher::mode::PushToFIFO: {
        // 2 ticks
        if (background_fetcher_ticks < 2) {
            return;
        }
        background_fetcher_ticks = 0;

        // only runs if background fifo is empty, otherwise it repeats
        if (background_fifo.empty()) {
            for (int i = 0; i < 8; ++i) {
                this->background_fifo.push_back(this->temp_background_fifo[i]);
            }

            this->tile_index++; // tile index
            this->background_current_mode = Fetcher::mode::FetchTileNo;
        }

        break;
    }
    }
}

// 1 background_tick = 1 T-Cycle
void PPU::tick() {

    this->ppu_ticks++;

    // set stat to mode
    _set(STAT, (_get(STAT) & 0xfc) | static_cast<uint8_t>(current_mode));
    last_mode = current_mode;

    switch (this->current_mode) {
    case mode::OAM_Scan: {

        if (!this->ppu_fetcher.wy_condition) {
            this->ppu_fetcher.wy_condition = _get(WY) == _get(LY);
        }

        // set IF for interrupt
        if ((this->last_mode != this->current_mode) && (_get(STAT) >> 5 & 1)) {
            _set(IF, _get(IF) | 2);
        }

        if (this->ppu_ticks % 2 == 0 &&
            ppu_fetcher.sprite_buffer_counter < 40) { // 40 oam entries
            // oam scan 1 entry (4 bytes) every 2 ticks
            // bytes from fe00 to fe9f
            // sprite fetcher
            std::array<uint8_t, 4> oam_entry{};
            for (unsigned int i = 0; i < oam_entry.size(); ++i) {
                oam_entry[i] = this->gb_mmu.read_memory(
                    ppu_fetcher.OAM_START_ADDRESS +
                    4 * ppu_fetcher.sprite_buffer_counter + i);
            }
            add_to_sprite_buffer(
                oam_entry); // attempt to add it to the sprite_buffer

            if (this->ppu_fetcher.sprite_buffer.size() == 10) {
                // TODO: sort the sprite buffer by x from low to high
                std::stable_sort(this->ppu_fetcher.sprite_buffer.begin(),
                                 this->ppu_fetcher.sprite_buffer.end(),
                                 [](const std::array<uint8_t, 4> &a,
                                    const std::array<uint8_t, 4> &b) {
                                     return a[1] < b[1];
                                 });
            }

            ppu_fetcher.sprite_buffer_counter++; // each oam entry is 4 bytes
        }

        // oam scan ended
        else if (this->ppu_ticks == 80) {
            // TODO: start the fetcher based on LY
            assert(ppu_fetcher.sprite_buffer_counter == 40 &&
                   "OAM counter is not 40!");

            // reset the ppu fetcher mode
            this->ppu_fetcher.reset();

            this->current_mode = mode::Drawing;
        }
        break;
    }
    case mode::Drawing: {

        // fetcher background_tick
        this->ppu_fetcher.background_tick();

        if (!this->ppu_fetcher.background_fifo.empty()) {

            if (this->ppu_fetcher.lcd_x_position < 0) {
                this->ppu_fetcher.background_fifo.pop_back();
            }

            else {
                assert(this->ppu_fetcher.lcd_x_position <= 160 &&
                       "LCD X Position exceeded the screen width!");

                assert(this->ppu_fetcher.lcd_x_position >= 0 &&
                       "LCD X Position is negative!");

                // sprite fetching: check if mode is in Push to FIFO, LCDC bit 1
                // (sprites enabled)
                bool lcdc_sprites_enabled = (_get(LCDC) >> 1) & 1;

                if (lcdc_sprites_enabled &&
                    !this->ppu_fetcher.sprite_buffer.empty() &&
                    this->ppu_fetcher.sprite_fetch_buffer.empty() &&
                    this->ppu_fetcher.sprite_current_mode !=
                        Fetcher::mode::Paused) { // should only run once per
                                                 // scanline, either sprite
                                                 // fetch buffer is empty or
                                                 // it's paused because it ran
                                                 // this scanline already

                    for (unsigned int i = 0;
                         i < this->ppu_fetcher.sprite_buffer.size(); ++i) {
                        if (this->ppu_fetcher.sprite_buffer[i][1] <=
                            (this->ppu_fetcher.lcd_x_position + 8)) {

                            this->ppu_fetcher.sprite_fetch_buffer.push_back(
                                this->ppu_fetcher.sprite_buffer[i]);
                        }
                    }
                }

                if (!this->ppu_fetcher.sprite_fetch_buffer.empty() &&
                    lcdc_sprites_enabled) {

                    if (this->ppu_fetcher.background_current_mode !=
                            Fetcher::mode::PushToFIFO &&
                        this->ppu_fetcher.background_current_mode !=
                            Fetcher::mode::Paused) {
                        this->ppu_fetcher.background_fifo.clear();
                        return;
                    }

                    this->ppu_fetcher.background_current_mode =
                        Fetcher::mode::Paused; // pause the background
                                               // fetcher
                    this->ppu_fetcher.sprite_tick();
                    return;
                }

                uint8_t position_x = ppu_fetcher.lcd_x_position;
                sf::Vector2u position = {position_x, _get(LY)};

                uint8_t bg_pixel = this->ppu_fetcher.background_fifo.back();
                this->ppu_fetcher.background_fifo.pop_back();

                sf::Color result_pixel = get_pixel_color(bg_pixel);

                if (!this->ppu_fetcher.sprite_fifo.empty()) {
                    assert(
                        this->ppu_fetcher.sprite_fifo.size() ==
                            this->ppu_fetcher.sprite_attr_fifo.size() &&
                        "sprite fifo and sprite attr fifo not the same size!");

                    uint8_t sprite_pixel = this->ppu_fetcher.sprite_fifo.back();
                    this->ppu_fetcher.sprite_fifo.pop_back();

                    uint8_t sprite_attr =
                        this->ppu_fetcher.sprite_attr_fifo.back();
                    this->ppu_fetcher.sprite_attr_fifo.pop_back();

                    if (sprite_pixel != 0) {

                        uint8_t sprite_palette =
                            (sprite_attr >> 4) & 1; // palette is bit 4

                        if (((sprite_attr >> 7) & 1) && bg_pixel != 0) {
                            // sprite not always on top of bg, result pixel
                            // stays the same
                        } else {
                            result_pixel =
                                get_pixel_color(sprite_pixel, &sprite_palette);
                        }
                    }
                }

                // TODO: add sprite pixel and rgb

                // this->lcd_dots.push_back(bg_pixel);

                // TODO: is there a better way to do this?
                // fill up entire screen
                uint16_t position_y_offset = 0;
                for (uint16_t i = 0; i < DrawUtils::SCALE * DrawUtils::SCALE;
                     ++i) {
                    uint16_t position_x = (i % DrawUtils::SCALE) +
                                          (position.x * DrawUtils::SCALE);
                    if (i % DrawUtils::SCALE == 0 && i != 0) {
                        position_y_offset++;
                    }
                    uint16_t position_y =
                        (position.y * DrawUtils::SCALE) + position_y_offset;

                    lcd_dots_image.setPixel({position_x, position_y},
                                            result_pixel);
                }

                // check for window fetch
                if (((_get(LCDC) >> 5) & 1) &&
                    ppu_fetcher.lcd_x_position >= (_get(WX) - 1) &&
                    this->ppu_fetcher.wy_condition &&
                    !this->ppu_fetcher.window_fetch) {

                    this->ppu_fetcher.background_current_mode =
                        Fetcher::mode::FetchTileNo;
                    this->ppu_fetcher.tile_index = 0;
                    this->ppu_fetcher.background_fifo.clear();
                    this->ppu_fetcher.window_fetch = true;
                }

                // TODO: optimize by only loading the palette when rom loads or
                // something

                // TODO: need to find a way to clear while drawing the
                // original pixels need a vector of dots to draw
            }


            // TODO: should i increment it before the check above
            this->ppu_fetcher.lcd_x_position++;
        }

        if (ppu_fetcher.lcd_x_position == 160) {
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
            _set(LY, _get(LY) + 1); // new scanline reached

            // reset lcd x position
            // this->ppu_fetcher.lcd_x_position = 0;
            // this->ppu_fetcher.lcd_x_position = _get(SCX) % 8;

            if (this->ppu_fetcher.window_fetch) {
                this->ppu_fetcher.window_ly++;
            }

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

        this->ppu_fetcher.wy_condition = false;

        if (this->ppu_ticks == 456) {

            this->ppu_ticks = 0;
            _set(LY, _get(LY) + 1); // new scanline reached

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

            if (_get(LY) == 153) {               // wait 10 more scanlines
                _set(LY, 0);                     // reset LY
                this->ppu_fetcher.window_ly = 0; // reset window_ly
                // this->lcd_dots.clear(); // reset LCD
                this->current_mode = PPU::mode::OAM_Scan;
            }
        }
        break;
    }
    }
}
