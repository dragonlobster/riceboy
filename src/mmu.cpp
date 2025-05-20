#include "mmu.h"
#include "ppu.h"
#include <array>
#include <cassert>
#include <iostream>

// TODO: block writes to LY while LCD is off

// TODO: use constexpr function instead?
#define IS_MBC1                                                                \
    (_cartridge_type == mmu::cartridge_type::mbc1 ||                           \
     _cartridge_type == mmu::cartridge_type::mbc1_ram ||                       \
     _cartridge_type == mmu::cartridge_type::mbc1_ram_battery)

mmu::mmu(timer &timer, interrupt &interrupt, ppu &ppu, joypad &joypad) {
    this->gb_timer = &timer;
    this->gb_interrupt = &interrupt;
    this->gb_ppu = &ppu;
    this->gb_joypad = &joypad;
}

void mmu::initialize_skip_bootrom_values() {
    // initialize gb timer values
    this->gb_timer->intialize_values();

    ppu_mode = 0x02;

    hardware_registers[0x50] = 0xff;
    hardware_registers[0x0f] = 0xe1;
    hardware_registers[0x12] = 0xf3;
    hardware_registers[0x13] = 0xc1;
    hardware_registers[0x14] = 0x87;
    hardware_registers[0x24] = 0x77;
    hardware_registers[0x25] = 0xf3;
    hardware_registers[0x26] = 0x80;
}

void mmu::handle_tma_write(uint8_t value) {

    // tma address at the moment is hardware_registers[0x06]
    this->gb_timer->tma_ff06 = value;

    if (this->gb_timer->lock_tima_write) {
        // if TMA was written during lock tima write (which means overflow case
        // was just handled), the new TMA should be written to TIMA
        this->gb_timer->tima_ff05 = value;
    }
}

void mmu::handle_tima_write(uint8_t value) {

    if (this->gb_timer->lock_tima_write) {
        assert(!this->gb_timer->tima_overflow &&
               "tima overflow should be false on lock tima write!");
        // tima write is locked because we just handled an overflow case, it
        // will stay as the value of TMA instead
        return;
    }

    this->gb_timer->tima_ff05 = value;
    this->gb_timer->tima_overflow = false;
}

void mmu::handle_tima_overflow() {

    // during tima overflow, after 1 M-cycle delay (standby), tima because tma
    // and a timer interrupt bit in IF is set.

    assert(this->gb_timer->tima_overflow_standby &&
           "tima overflown was not requested!");

    // uint8_t _if = read_memory(0xff0f);
    // write_memory(0xff0f, _if | 4);

    this->gb_interrupt->interrupt_flags |= 4;

    // set tima to tma
    uint8_t tma = this->gb_timer->tma_ff06;
    // write_memory(0xff05, tma);
    this->gb_timer->tima_ff05 = tma;
    this->gb_timer->tima_overflow = false;

    assert(!this->gb_timer->lock_tima_write &&
           "tima write should not be locked yet!");

    // we need to lock tima write because during this cycle, any writes to TIMA
    // should be ignored
    this->gb_timer->lock_tima_write = true;

    // reset tima overflow standby
    this->gb_timer->tima_overflow_standby = false;
}

void mmu::handle_div_write() {
    // div writes automatically sets DIV to 0
    this->gb_timer->sysclock = 0;
    this->gb_timer->falling_edge();
}

void mmu::handle_tac_write(uint8_t value) {
    // we only care about the last 3 bits of tac at all times
    this->gb_timer->tac_ff07 = value | 0xf8;
    this->gb_timer->falling_edge();
}

void mmu::handle_lcdc_write(uint8_t value) {

    uint8_t lcd_bit = (this->gb_ppu->lcdc_ff40 >> 7) & 1;

    this->gb_ppu->lcdc_ff40 = value;

    // if (load_rom_complete) {
    uint8_t new_lcd_bit = (this->gb_ppu->lcdc_ff40 >> 7) & 1;
    // true if lcd was toggled on or off
    this->gb_ppu->lcd_toggle = lcd_bit != new_lcd_bit;

    this->gb_ppu->lcd_on = new_lcd_bit;

    // lcd toggled off
    if (this->gb_ppu->lcd_toggle && !new_lcd_bit) {
        // reset LY to 0
        this->gb_ppu->ly_ff44 = 0;

        // reset STAT mode bit to 0
        this->gb_ppu->stat_ff41 &= 0xfc;
    }
    //}
}

void mmu::handle_stat_write(uint8_t value) {
    // mask top bit as 1 always, 0 and 1 bit not writable through mmu
    this->gb_ppu->stat_ff41 = (value | 0x80) & 0xfc;
}

void mmu::handle_dma_write(uint8_t value) {
    this->gb_ppu->dma_ff46 = value;

    if (!gb_ppu->dma_mode && !gb_ppu->dma_delay) {
        // uses echo ram if source is 0xff or 0xfe, but i don't have echo ram so
        // i use wram instead (which echo ram is a mirror of)

        this->dma_source_transfer_address =
            value >= 0xe0 ? (value - 0x20) << 8 : value << 8;

        this->dma_bus_source = (locate_section(dma_source_transfer_address) ==
                                    section::character_ram ||
                                locate_section(dma_source_transfer_address) ==
                                    section::bg_map_data_1 ||
                                locate_section(dma_source_transfer_address) ==
                                    section::bg_map_data_2)
                                   ? bus::vram
                                   : bus::main;
        this->gb_ppu->dma_delay = true;
    }
}

void mmu::set_oam_dma() {
    this->gb_ppu->dma_delay = false;
    // this->dma_write = false; // reset dma write, all writes are ignored at
    // this point anyway
    this->gb_ppu->dma_mode = true;
}

void mmu::dma_transfer() {
    assert(this->gb_ppu->dma_mode && "not in dma mode now!");
    uint16_t dest_address = 0xfe00 | (dma_source_transfer_address & 0x00ff);
    this->write_memory(dest_address,
                       this->read_memory(dma_source_transfer_address));

    if ((dma_source_transfer_address & 0xff) == 0x9f) {
        this->gb_ppu->dma_mode = false;
    }

    else {
        dma_source_transfer_address++;
    }

    assert(((dma_source_transfer_address & 0x00ff) <= 0x9f) &&
           "dma transfer passed oam memory!");
}

void mmu::set_load_rom_complete() {

    if (IS_MBC1) {
        assert(this->cartridge.get() != nullptr &&
               "Cartridge is NULL! Can't complete load_rom_complete");
        this->cartridge->set_load_rom_complete();
    }
    this->load_rom_complete = true;
}

void mmu::set_cartridge_type(uint8_t type) {

    this->_cartridge_type = static_cast<mmu::cartridge_type>(type);

    if (IS_MBC1) {
        this->cartridge = std::make_unique<mbc1>();
    }
}

mmu::section mmu::locate_section(const uint16_t address) {
    if (address <= 0x00ff) {
        return mmu::section::restart_and_interrupt_vectors;
    } else if (address >= 0x0100 && address <= 0x014f) {
        return mmu::section::cartridge_header_area;
    } else if (address >= 0x0150 && address <= 0x3fff) {
        return mmu::section::cartridge_rom_bank_0;
    } else if (address >= 0x4000 && address <= 0x7fff) {
        return mmu::section::cartridge_rom_switchable_banks;
    } else if (address >= 0x8000 && address <= 0x97ff) {
        return mmu::section::character_ram;
    } else if (address >= 0x9800 && address <= 0x9bff) {
        return mmu::section::bg_map_data_1;
    } else if (address >= 0x9c00 && address <= 0x9fff) {
        return mmu::section::bg_map_data_2;
    } else if (address >= 0xa000 && address <= 0xbfff) {
        return mmu::section::cartridge_ram;
    } else if (address >= 0xc000 && address <= 0xcfff) {
        return mmu::section::internal_ram_bank_0;
    } else if (address >= 0xd000 && address <= 0xdfff) {
        return mmu::section::internal_ram_bank_1_to_7;
    } else if (address >= 0xe000 && address <= 0xfdff) {
        return mmu::section::echo_ram;
    } else if (address >= 0xfe00 && address <= 0xfe9f) {
        return mmu::section::oam_ram;
    } else if (address >= 0xfea0 && address <= 0xfeff) {
        return mmu::section::unusuable_memory;
    } else if (address >= 0xff00 && address <= 0xff7f) {
        return mmu::section::hardware_registers;
    } else if (address >= 0xff80 && address <= 0xfffe) {
        return mmu::section::zero_page;
    } else if (address == 0xffff) {
        return mmu::section::interrupt_enable_flag;
    }
    return mmu::section::unknown;
}

void mmu::oam_bug_read(uint16_t address) {

    if (locate_section(address) != section::oam_ram) {
        return;
    }
    // if address is in oam and the sprite is not first or second object the
    // bug is triggered for write oam row == 0 could also mean ppu is not in
    // mode 2
    // write_memory(address, value);

    // range for ppu current oam row is 0x08 -> 0x98 (20 rows inclusive)
    // a word is 2 bytes, so first "word" is the first 2 bytes (16 bit)

    // Apply OAM corruption formula to first word (first 2 bytes)
    for (int i = 0; i < 2; i++) {
        uint8_t a = this->gb_ppu->oam_ram[this->gb_ppu->current_oam_row + i];
        uint8_t b =
            this->gb_ppu->oam_ram[this->gb_ppu->current_oam_row - 8 + i];
        uint8_t c =
            this->gb_ppu->oam_ram[this->gb_ppu->current_oam_row - 4 + i];
        this->gb_ppu->oam_ram[this->gb_ppu->current_oam_row + i] = b | (a & c);
    }

    // Copy last 6 bytes from previous row (last 3 words)
    memcpy(&this->gb_ppu->oam_ram[this->gb_ppu->current_oam_row + 2],
           &this->gb_ppu->oam_ram[this->gb_ppu->current_oam_row - 6], 6);
}

void mmu::oam_bug_read_inc(uint16_t address) {
    if (locate_section(address) != section::oam_ram) {
        return;
    }
    // won't occur if the accessed row is one of the first four, or the last row

    // if it's the first 4 rows (0x0, 0x8, 0x10, 0x18) or the last row (0x98)
    // just return
    if (this->gb_ppu->current_oam_row < 0x20 ||
        this->gb_ppu->current_oam_row == 0x98) {
        return;
    }

    // magic formula (b & (a | c | d)) | (a & c & d)

    uint8_t a1 = this->gb_ppu->oam_ram[this->gb_ppu->current_oam_row - 0x10];
    uint8_t b1 = this->gb_ppu->oam_ram[this->gb_ppu->current_oam_row - 0x08];
    uint8_t c1 = this->gb_ppu->oam_ram[this->gb_ppu->current_oam_row];
    uint8_t d1 = this->gb_ppu->oam_ram[this->gb_ppu->current_oam_row - 0x04];

    uint8_t a2 = this->gb_ppu->oam_ram[this->gb_ppu->current_oam_row - 0x0f];
    uint8_t b2 = this->gb_ppu->oam_ram[this->gb_ppu->current_oam_row - 0x07];
    uint8_t c2 = this->gb_ppu->oam_ram[this->gb_ppu->current_oam_row + 0x01];
    uint8_t d2 = this->gb_ppu->oam_ram[this->gb_ppu->current_oam_row - 0x03];

    // First corruption: apply glitch formula to two bytes
    this->gb_ppu->oam_ram[this->gb_ppu->current_oam_row - 0x8] =
        (b1 & (a1 | c1 | d1)) | (a1 & c1 & d1);

    this->gb_ppu->oam_ram[this->gb_ppu->current_oam_row - 0x7] =
        (b2 & (a2 | c2 | d2)) | (a2 & c2 & d2);

    // Second corruption: cascading copy to multiple places
    for (unsigned i = 0; i < 8; i++) {
        // This chained assignment copies the value to TWO locations
        this->gb_ppu->oam_ram[this->gb_ppu->current_oam_row + i] =
            this->gb_ppu->oam_ram[this->gb_ppu->current_oam_row - 0x10 + i] =
                this->gb_ppu->oam_ram[this->gb_ppu->current_oam_row - 0x08 + i];
    }
}

uint8_t mmu::bus_read_memory(uint16_t address) {
    if (this->gb_ppu->dma_mode) {

        if (locate_section(address) == section::oam_ram) {
            // oam is locked
            return read_memory(dma_source_transfer_address);
        }

        else if (dma_bus_source == bus::vram) {
            // vram is locked if the dma source is in vram
            if (locate_section(address) == section::character_ram ||
                locate_section(address) == section::bg_map_data_1 ||
                locate_section(address) == section::bg_map_data_2) {
                return read_memory(dma_source_transfer_address);
            }
        }

        else if (dma_bus_source == bus::main) {
            // rom: 0 - 7fff (restart, cartridge_roms)
            // wram - c000 - dfff (internal ram banks)
            // sram - caertridge ram
            if (locate_section(address) ==
                    section::restart_and_interrupt_vectors ||
                locate_section(address) == section::cartridge_header_area ||
                locate_section(address) == section::cartridge_rom_bank_0 ||
                locate_section(address) ==
                    section::cartridge_rom_switchable_banks ||
                locate_section(address) == section::internal_ram_bank_0 ||
                locate_section(address) == section::internal_ram_bank_1_to_7 ||
                locate_section(address) == section::cartridge_ram) {
                return read_memory(dma_source_transfer_address);
            }
        }
    }

    // lock vram if ppu is in mode 3, check ppu mode
    if (load_rom_complete &&
        (locate_section(address) == section::character_ram ||
         locate_section(address) == section::bg_map_data_1 ||
         locate_section(address) == section::bg_map_data_2) &&
        this->gb_ppu->vram_read_block) {
        return 0xff;
    }

    // oam is locked during mode 2 and mode 3 but the exact timing of stat.mode
    // seems tricky
    // uint8_t stat_mode = hardware_registers[0x41] & 2;

    if (locate_section(address) == section::oam_ram &&
        (this->gb_ppu->oam_read_block)) {

        return 0xff; // bug returns 0xff i think?
    }

    /*
    if (locate_section(address) == section::oam_ram &&
        (ppu_mode == 2 || ppu_mode == 3)) {

        return 0xff; // bug returns 0xff i think?
    }*/

    // OAM BUG read corruption
    /*
    if (locate_section(address) == section::oam_ram &&
        this->ppu_current_oam_row > 0) {

        oam_bug_read(address);

        return 0xff; // bug returns 0xff i think?
    }*/

    return read_memory(address);
}

// TODO: make more elegant by segregating each address space
uint8_t mmu::read_memory(uint16_t address) const {
    uint16_t base_address = static_cast<uint16_t>(locate_section(address));

    if (IS_MBC1) {
        assert(this->cartridge.get() != nullptr &&
               "Cartridge is NULL! Can't read!");

        switch (address) {
        // joypad
        case 0xff00: return this->gb_joypad->ff00_joyp;

        // timer registers
        case 0xff04: return (this->gb_timer->sysclock & 0x0ff00) >> 8;
        case 0xff05: return this->gb_timer->tima_ff05;
        case 0xff06: return this->gb_timer->tma_ff06;
        case 0xff07: return this->gb_timer->tac_ff07;

        // interrupt
        case 0xff0f:
            return this->gb_interrupt->interrupt_flags |
                   0xe0; // mask top 3 bits for IF to 1 (unused)
        case 0xffff:
            return this->gb_interrupt->interrupt_enable_flag &
                   0x1f; // mask top 3 bits for IE to 0 (unused)

        // ppu
        case 0xff40: return this->gb_ppu->lcdc_ff40;
        case 0xff41: return this->gb_ppu->stat_ff41;
        case 0xff42: return this->gb_ppu->scy_ff42;
        case 0xff43: return this->gb_ppu->scx_ff43;
        case 0xff44: return this->gb_ppu->ly_ff44;
        case 0xff45: return this->gb_ppu->lyc_ff45;
        case 0xff46: return this->gb_ppu->dma_ff46;
        case 0xff47: return this->gb_ppu->bgp_ff47;
        case 0xff48: return this->gb_ppu->obp0_ff48;
        case 0xff49: return this->gb_ppu->obp1_ff49;
        case 0xff4a: return this->gb_ppu->wy_ff4a;
        case 0xff4b: return this->gb_ppu->wx_ff4b;

        default    : {
            // Attempt to read from cartridge
            uint16_t result = this->cartridge->read_memory(address);

            // Make sure result fits in 8 bits
            if (result <= 0xff) {
                return result;
            }
        }
        }
    }

    // read from mmu's base arrays if the read function in mbc1 resulted in
    // 0

    switch (locate_section(address)) {
    case mmu::section::restart_and_interrupt_vectors:
        return this->restart_and_interrupt_vectors[address - base_address];

    case mmu::section::cartridge_header_area:
        return this->cartridge_header_area[address - base_address];

    case mmu::section::cartridge_rom_bank_0:
        return this->cartridge_rom_bank_0[address - base_address];

    case mmu::section::cartridge_rom_switchable_banks:
        return this->cartridge_rom_switchable_banks[address - base_address];

    case mmu::section::character_ram:
        return this->gb_ppu->character_ram[address - base_address];

    case mmu::section::bg_map_data_1:
        return this->gb_ppu->bg_map_data_1[address - base_address];

    case mmu::section::bg_map_data_2:
        return this->gb_ppu->bg_map_data_2[address - base_address];

    case mmu::section::cartridge_ram:
        return this->cartridge_ram[address - base_address];

    case mmu::section::internal_ram_bank_0:
        return this->internal_ram_bank_0[address - base_address];

    case mmu::section::internal_ram_bank_1_to_7:
        return this->internal_ram_bank_1_to_7[address - base_address];

    case mmu::section::echo_ram: return this->echo_ram[address - base_address];

    case mmu::section::oam_ram:
        return this->gb_ppu->oam_ram[address - base_address];

    case mmu::section::unusuable_memory: return 0; // DMG returns 0x00;

    case mmu::section::hardware_registers:
        switch (address) {
        // joypad
        case 0xff00: return this->gb_joypad->ff00_joyp;

        // timer
        case 0xff04: return (this->gb_timer->sysclock & 0xff00) >> 8;
        case 0xff05: return this->gb_timer->tima_ff05;
        case 0xff06: return this->gb_timer->tma_ff06;
        case 0xff07: return this->gb_timer->tac_ff07;

        // interrupt
        case 0xff0f: return this->gb_interrupt->interrupt_flags | 0xe0;

        // ppu
        case 0xff40: return this->gb_ppu->lcdc_ff40;
        case 0xff41: return this->gb_ppu->stat_ff41;
        case 0xff42: return this->gb_ppu->scy_ff42;
        case 0xff43: return this->gb_ppu->scx_ff43;
        case 0xff44: return this->gb_ppu->ly_ff44;
        case 0xff45: return this->gb_ppu->lyc_ff45;
        case 0xff46: return this->gb_ppu->dma_ff46;
        case 0xff47: return this->gb_ppu->bgp_ff47;
        case 0xff48: return this->gb_ppu->obp0_ff48;
        case 0xff49: return this->gb_ppu->obp1_ff49;
        case 0xff4a: return this->gb_ppu->wy_ff4a;
        case 0xff4b: return this->gb_ppu->wx_ff4b;

        // unused
        case 0xff03:
        case 0xff15:
        case 0xff1f:
        case 0xff4d:
        case 0xff4e:
        case 0xff4f: return 0xff;

        default:
            // Check ranges for other unused registers
            if ((address > 0xff07 && address < 0xff0f) ||
                (address > 0xff26 && address < 0xff30) || (address >= 0xff51)) {
                return 0xff;
            }
            return this->hardware_registers[address - base_address];
        }

    case mmu::section::zero_page:
        return this->zero_page[address - base_address];

    case mmu::section::interrupt_enable_flag:
        return this->gb_interrupt->interrupt_enable_flag & 0x1f;
        // mask top 3 bits with 1s for IE (they are unused)

    default:
        std::cout << "(read) memory not implemented: "
                  << "hex: 0x" << std::hex << static_cast<unsigned int>(address)
                  << std::endl;
        return 0;
    }
}

void mmu::oam_bug_write(uint16_t address) {

    if (locate_section(address) != section::oam_ram &&
        this->gb_ppu->current_oam_row == 0) {
        return;
    }
    // if address is in oam and the sprite is not first or second object the
    // bug is triggered for write oam row == 0 could also mean ppu is not in
    // mode 2
    // write_memory(address, value);

    // range for ppu current oam row is 0x08 -> 0x98 (20 rows inclusive)
    // a word is 2 bytes, so first "word" is the first 2 bytes (16 bit)

    // Apply OAM corruption formula to first word (first 2 bytes)
    for (int i = 0; i < 2; i++) {
        uint8_t a = this->gb_ppu->oam_ram[this->gb_ppu->current_oam_row + i];
        uint8_t b =
            this->gb_ppu->oam_ram[this->gb_ppu->current_oam_row - 8 + i];
        uint8_t c =
            this->gb_ppu->oam_ram[this->gb_ppu->current_oam_row - 4 + i];
        this->gb_ppu->oam_ram[this->gb_ppu->current_oam_row + i] =
            ((a ^ c) & (b ^ c)) ^ c;
    }

    // Copy last 6 bytes from previous row (last 3 words)
    memcpy(&this->gb_ppu->oam_ram[this->gb_ppu->current_oam_row + 2],
           &this->gb_ppu->oam_ram[this->gb_ppu->current_oam_row - 6], 6);
}

void mmu::bus_write_memory(uint16_t address, uint8_t value) {
    // TODO: stopping LCD operation (LCDC bit 7 1->0) happens in vblank only,
    // otherwise crash
    if (this->gb_ppu->dma_mode) {
        if (locate_section(address) == section::oam_ram) {
            // oam is locked
            write_memory(dma_source_transfer_address, value);
            return;
        }

        else if (dma_bus_source == bus::vram) {
            // vram is locked if the dma source is in vram
            if (locate_section(address) == section::character_ram ||
                locate_section(address) == section::bg_map_data_1 ||
                locate_section(address) == section::bg_map_data_2) {
                write_memory(dma_source_transfer_address, value);
                return;
            }
        }

        else if (dma_bus_source == bus::main) {
            // rom: 0 - 7fff (restart, cartridge_roms)
            // wram - c000 - dfff (internal ram banks)
            // sram - caertridge ram
            if (locate_section(address) ==
                    section::restart_and_interrupt_vectors ||
                locate_section(address) == section::cartridge_header_area ||
                locate_section(address) == section::cartridge_rom_bank_0 ||
                locate_section(address) ==
                    section::cartridge_rom_switchable_banks ||
                locate_section(address) == section::internal_ram_bank_0 ||
                locate_section(address) == section::internal_ram_bank_1_to_7 ||
                locate_section(address) == section::cartridge_ram) {
                write_memory(dma_source_transfer_address, value);
                return;
            }
        }
    }

    // lock vram if ppu is in mode 3, check ppu mode
    if (load_rom_complete &&
        (locate_section(address) == section::character_ram ||
         locate_section(address) == section::bg_map_data_1 ||
         locate_section(address) == section::bg_map_data_2) &&
        this->gb_ppu->vram_write_block) {
        return;
    }

    // OAM BUG write corruption
    /*
    else if (locate_section(address) == section::oam_ram &&
             this->ppu_current_oam_row > 0) {
        oam_bug_write(address);
    }*/

    /*
    else if (locate_section(address) == section::oam_ram &&
             (ppu_mode == 2 || ppu_mode == 3) && load_rom_complete) {
        return; // block oam access during mode 2 and 3
    }*/

    else if (locate_section(address) == section::oam_ram && load_rom_complete &&
             this->gb_ppu->oam_write_block) {
        return;
        // return; // block oam access during mode 2 and 3
    }

    else {
        write_memory(address, value);
    }
}

void mmu::write_memory(uint16_t address, uint8_t value) {

    if (!this->load_rom_complete && (IS_MBC1)) { // TODO: not rom_only

        assert(this->cartridge.get() != nullptr &&
               "Cartridge is NULL! Can't write!");

        this->cartridge->write_memory(address, value);
        return;
    }

    else if (IS_MBC1) {
        assert(this->cartridge.get() != nullptr &&
               "Cartridge is NULL! Can't write!");

        if (address == 0xff00) {
            this->gb_joypad->handle_write(value);
        }

        if (address == 0xff04) {
            handle_div_write();
            return;
        }

        if (address == 0xff05) {
            handle_tima_write(value);
            return;
        }

        if (address == 0xff06) {
            handle_tma_write(value);
            return;
        }

        if (address == 0xff07) {
            handle_tac_write(value);
            return;
        }

        // lcdc
        if (address == 0xff40) {
            handle_lcdc_write(value);
            return;
        }

        // stat (mask top bit)
        if (address == 0xff41) {
            handle_stat_write(value);
            return;
        }

        if (address == 0xff42) {
            this->gb_ppu->scy_ff42 = value;
            return;
        }

        if (address == 0xff43) {
            this->gb_ppu->scx_ff43 = value;
            return;
        }

        if (address == 0xff44) {
            // ly not writeable
            return;
        }

        if (address == 0xff45) {
            this->gb_ppu->lyc_ff45 = value;
            return;
        }

        // dma
        if (address == 0xff46) {
            handle_dma_write(value);
            return;
        }

        if (address == 0xff47) {
            this->gb_ppu->bgp_ff47 = value;
            return;
        }

        if (address == 0xff48) {
            this->gb_ppu->obp0_ff48 = value;
            return;
        }

        if (address == 0xff49) {
            this->gb_ppu->obp1_ff49 = value;
            return;
        }

        if (address == 0xff4a) {
            this->gb_ppu->wy_ff4a = value;
            return;
        }

        if (address == 0xff4b) {
            this->gb_ppu->wx_ff4b = value;
            return;
        }

        if (address == 0xff0f) {
            this->gb_interrupt->interrupt_flags = value;
            return;
        }

        if (address == 0xff50) {
            hardware_registers[0x50] = 0xff;
            return;
        }

        this->cartridge->write_memory(address, value);
    }

    uint16_t base_address = static_cast<uint16_t>(locate_section(address));

    switch (locate_section(address)) {
    case mmu::section::restart_and_interrupt_vectors:
        if (!this->load_rom_complete) {
            this->restart_and_interrupt_vectors[address] = value;
        }
        break;

    case mmu::section::cartridge_header_area:
        if (!this->load_rom_complete) {
            this->cartridge_header_area[address - base_address] = value;
        }
        break;

    case mmu::section::cartridge_rom_bank_0:
        if (!this->load_rom_complete) {
            this->cartridge_rom_bank_0[address - base_address] = value;
        }
        break;

    case mmu::section::cartridge_rom_switchable_banks:
        if (!this->load_rom_complete) {
            this->cartridge_rom_switchable_banks[address - base_address] =
                value;
        }
        break;

    case mmu::section::character_ram:
        this->gb_ppu->character_ram[address - base_address] = value;
        break;

    case mmu::section::bg_map_data_1:
        this->gb_ppu->bg_map_data_1[address - base_address] = value;
        break;

    case mmu::section::bg_map_data_2:
        this->gb_ppu->bg_map_data_2[address - base_address] = value;
        break;

    case mmu::section::cartridge_ram:
        this->cartridge_ram[address - base_address] = value;
        break;

    case mmu::section::internal_ram_bank_0:
        this->internal_ram_bank_0[address - base_address] = value;
        break;

    case mmu::section::internal_ram_bank_1_to_7:
        this->internal_ram_bank_1_to_7[address - base_address] = value;
        break;

    case mmu::section::echo_ram:
        if (!load_rom_complete) {
            this->echo_ram[address - base_address] = value;
        }
        break;

    case mmu::section::unusuable_memory:
        if (!load_rom_complete) {
            this->unusable_memory[address - base_address] = value;
        }
        break;

    case mmu::section::oam_ram:
        this->gb_ppu->oam_ram[address - base_address] = value;
        break;

    case mmu::section::hardware_registers:
        if (address == 0xff00) {
            this->gb_joypad->handle_write(value);
        }

        else if (address == 0xff04) {
            handle_div_write();
        }

        else if (address == 0xff05) { // tima_ff05
            handle_tima_write(value);
        }

        else if (address == 0xff06) {
            handle_tma_write(value);
        }

        else if (address == 0xff07) {
            handle_tac_write(value);
        }

        // lcdc
        else if (address == 0xff40) {
            handle_lcdc_write(value);
        }

        // stat (mask top bit)
        else if (address == 0xff41) {
            handle_stat_write(value);
        }

        else if (address == 0xff42) {
            this->gb_ppu->scy_ff42 = value;
        }

        else if (address == 0xff43) {
            this->gb_ppu->scx_ff43 = value;
        }

        else if (address == 0xff44) {
            // ly not writeable
        }

        else if (address == 0xff45) {
            this->gb_ppu->lyc_ff45 = value;
        }

        // dma
        else if (address == 0xff46) {
            handle_dma_write(value);
        }

        else if (address == 0xff47) {
            this->gb_ppu->bgp_ff47 = value;
        }

        else if (address == 0xff48) {
            this->gb_ppu->obp0_ff48 = value;
        }

        else if (address == 0xff49) {
            this->gb_ppu->obp1_ff49 = value;
        }

        else if (address == 0xff4a) {
            this->gb_ppu->wy_ff4a = value;
        }

        else if (address == 0xff4b) {
            this->gb_ppu->wx_ff4b = value;
        }

        else if (address == 0xff50) {
            hardware_registers[0x50] = 0xff;
        }

        else if (address == 0xff0f) {
            this->gb_interrupt->interrupt_flags = value;
        }

        else {
            this->hardware_registers[address - base_address] = value;
            std::cout << "";
        }
        break;

    case mmu::section::zero_page:
        this->zero_page[address - base_address] = value;
        break;

    case mmu::section::interrupt_enable_flag:
        this->gb_interrupt->interrupt_enable_flag = value;
        break;

    default:
        std::cout << "(write) memory not implemented: "
                  << "hex: 0x" << std::hex << static_cast<unsigned int>(address)
                  << std::endl;
        break;
    }
}
