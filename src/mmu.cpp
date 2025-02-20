#include "MMU.h"
#include <array>
#include <cassert>
#include <iostream>

// TODO: use constexpr function instead?
#define IS_MBC1                                                                \
    (_cartridge_type == MMU::cartridge_type::mbc1 ||                           \
     _cartridge_type == MMU::cartridge_type::mbc1_ram ||                       \
     _cartridge_type == MMU::cartridge_type::mbc1_ram_battery)

// TODO: simplify entire MMU by using a single array as the main memory

void MMU::handle_tma_write(uint8_t value) {

    // tma address at the moment
    hardware_registers[0x06] = value;

    if (lock_tima_write) {
        this->tima_ff05 = value;
    }

}

void MMU::handle_tima_write(uint8_t value) {

    if (this->lock_tima_write) {
        assert(!this->tima_overflow &&
               "tima overflow should be false on lock tima write!");
        return;
    }

    this->tima_ff05 = value;
    this->tima_overflow = false;
}

void MMU::handle_tima_overflow() {

    assert(this->tima_overflow_standby && "tima overflown was not requested!");

    uint8_t _if = read_memory(0xff0f);
    write_memory(0xff0f, _if | 4);

    // set tima to tma
    uint8_t tma = read_memory(0xff06);
    //write_memory(0xff05, tma);
    this->tima_ff05 = tma;
    this->tima_overflow = false;

    assert(!lock_tima_write && "tima write should not be locked yet!");

    // reset tima overflow and tima overflow standby
    // this->tima_overflow = false;
    this->tima_overflow_standby = false;
}

void MMU::falling_edge() {
    uint8_t timer_enable_bit = (this->tac_ff07 & 4) >> 2;
    uint8_t tac_freq_bit = tac_ff07 & 3;

    assert(tac_freq_bit <= 3 && "tac freq bit abnormal!");
    assert(timer_enable_bit <= 1 && "tac timer bit abnormal!");

    uint8_t div_bit{9};
    switch (tac_freq_bit) {
    case 1: div_bit = 3; break;
    case 2: div_bit = 5; break;
    case 3: div_bit = 7; break;
    }

    uint8_t div_state = ((this->div_ff04 >> div_bit) & 1) & (timer_enable_bit);

    assert(div_state <= 1 && "div state abnormal!");

    if (div_state == 0 && last_div_state == 1 &&
        !tima_overflow) { // skip incrementing on tima overflow, it's no use
        this->tima_ff05++;

        if (tima_ff05 == 0) {
            this->tima_overflow = true;
        }
    }
    this->last_div_state = div_state;
}

void MMU::handle_div_write() {
    this->div_ff04 = 0;
    falling_edge();
}

void MMU::handle_tac_write(uint8_t value) {
    this->tac_ff07 = value | 0xf8;
    this->falling_edge();
}

void MMU::set_load_rom_complete() {

    if (IS_MBC1) {
        assert(this->cartridge.get() != nullptr &&
               "Cartridge is NULL! Can't complete load_rom_complete");
        this->cartridge->set_load_rom_complete();
    }

    this->load_rom_complete = true;
}

void MMU::set_cartridge_type(uint8_t type) {

    this->_cartridge_type = static_cast<MMU::cartridge_type>(type);

    if (IS_MBC1) {
        this->cartridge = std::make_unique<MBC1>();
    }
}

// direct increment div ff04 to here, read ff04 also to read div
void MMU::increment_div(uint16_t value, bool check_falling_edge) {
    // if (!this->div_write_ran && !this->tac_write_ran) {
    //}
    if (check_falling_edge) {
        for (uint8_t i = 0; i < value; i++) {
            this->div_ff04++;
            falling_edge();
        }
    } else {
        this->div_ff04 += value;
    }
}

MMU::section MMU::locate_section(const uint16_t address) {
    if (address <= 0x00ff) {
        return MMU::section::restart_and_interrupt_vectors;
    } else if (address >= 0x0100 && address <= 0x014f) {
        return MMU::section::cartridge_header_area;
    } else if (address >= 0x0150 && address <= 0x3fff) {
        return MMU::section::cartridge_rom_bank_0;
    } else if (address >= 0x4000 && address <= 0x7fff) {
        return MMU::section::cartridge_rom_switchable_banks;
    } else if (address >= 0x8000 && address <= 0x97ff) {
        return MMU::section::character_ram;
    } else if (address >= 0x9800 && address <= 0x9bff) {
        return MMU::section::bg_map_data_1;
    } else if (address >= 0x9c00 && address <= 0x9fff) {
        return MMU::section::bg_map_data_2;
    } else if (address >= 0xa000 && address <= 0xbfff) {
        return MMU::section::cartridge_ram;
    } else if (address >= 0xc000 && address <= 0xcfff) {
        return MMU::section::internal_ram_bank_0;
    } else if (address >= 0xd000 && address <= 0xdfff) {
        return MMU::section::internal_ram_bank_1_to_7;
    } else if (address >= 0xe000 && address <= 0xfdff) {
        return MMU::section::echo_ram;
    } else if (address >= 0xfe00 && address <= 0xfe9f) {
        return MMU::section::oam_ram;
    } else if (address >= 0xfea0 && address <= 0xfeff) {
        return MMU::section::unusuable_memory;
    } else if (address >= 0xff00 && address <= 0xff7f) {
        return MMU::section::hardware_registers;
    } else if (address >= 0xff80 && address <= 0xfffe) {
        return MMU::section::zero_page;
    } else if (address == 0xffff) {
        return MMU::section::interrupt_enable_flag;
    }
    return MMU::section::unknown;
}

// TODO: make more elegant by segregating each address space
uint8_t MMU::read_memory(uint16_t address) const {
    uint16_t base_address = static_cast<uint16_t>(locate_section(address));

    if (IS_MBC1) {
        assert(this->cartridge.get() != nullptr &&
               "Cartridge is NULL! Can't read!");

        if (address == 0xff04) {
            return (this->div_ff04 & 0x0ff00) >> 8;
        }

        if (address == 0xff05) {
            return this->tima_ff05;
        }

        if (address == 0xff07) {
            return this->tac_ff07;
        }

        uint16_t result = this->cartridge->read_memory(address);

        if (result <= 0xff) { // make sure result fits in 8 bits
            return result;
        }
    }

    // read from mmu's base arrays if the read function in mbc1 resulted in 0

    switch (locate_section(address)) {
    case MMU::section::restart_and_interrupt_vectors:
        return this->restart_and_interrupt_vectors[address - base_address];

    case MMU::section::cartridge_header_area:
        return this->cartridge_header_area[address - base_address];

    case MMU::section::cartridge_rom_bank_0:
        return this->cartridge_rom_bank_0[address - base_address];

    case MMU::section::cartridge_rom_switchable_banks:
        return this->cartridge_rom_switchable_banks[address - base_address];

    case MMU::section::character_ram:
        return this->character_ram[address - base_address];

    case MMU::section::bg_map_data_1:
        return this->bg_map_data_1[address - base_address];

    case MMU::section::bg_map_data_2:
        return this->bg_map_data_2[address - base_address];

    case MMU::section::cartridge_ram:
        return this->cartridge_ram[address - base_address];

    case MMU::section::internal_ram_bank_0:
        return this->internal_ram_bank_0[address - base_address];

    case MMU::section::internal_ram_bank_1_to_7:
        return this->internal_ram_bank_1_to_7[address - base_address];

    case MMU::section::echo_ram: return this->echo_ram[address - base_address];

    case MMU::section::oam_ram : return this->oam_ram[address - base_address];

    case MMU::section::hardware_registers:
        if (address == 0xff04) {
            return (this->div_ff04 & 0xff00) >> 8;
        } else if (address == 0xff05) {
            return this->tima_ff05;
        } else if (address == 0xff07) {
            return this->tac_ff07;
        } else {
            return this->hardware_registers[address - base_address];
        }

    case MMU::section::zero_page:
        return this->zero_page[address - base_address];

    case MMU::section::interrupt_enable_flag:
        return this->interrupt_enable_flag;

    default:
        std::cout << "(read) memory not implemented: "
                  << "hex: 0x" << std::hex << static_cast<unsigned int>(address)
                  << std::endl;
        return 0;
    }
}

void MMU::write_memory(uint16_t address, uint8_t value) {

    if (!this->load_rom_complete && (IS_MBC1)) { // TODO: not rom_only

        assert(this->cartridge.get() != nullptr &&
               "Cartridge is NULL! Can't write!");

        this->cartridge->write_memory(address, value);
        return;
    }

    else if (IS_MBC1) {
        assert(this->cartridge.get() != nullptr &&
               "Cartridge is NULL! Can't write!");

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

        this->cartridge->write_memory(address, value);
    }

    // write to base memory if writing memory to mbc1 didn't occur

    uint16_t base_address = static_cast<uint16_t>(locate_section(address));

    switch (locate_section(address)) {
    case MMU::section::restart_and_interrupt_vectors:
        this->restart_and_interrupt_vectors[address] = value;
        break;

    case MMU::section::cartridge_header_area:
        this->cartridge_header_area[address - base_address] = value;
        break;

    case MMU::section::cartridge_rom_bank_0:
        this->cartridge_rom_bank_0[address - base_address] = value;
        break;

    case MMU::section::cartridge_rom_switchable_banks:
        this->cartridge_rom_switchable_banks[address - base_address] = value;
        break;

    case MMU::section::character_ram:
        this->character_ram[address - base_address] = value;
        break;

    case MMU::section::bg_map_data_1:
        this->bg_map_data_1[address - base_address] = value;
        break;

    case MMU::section::bg_map_data_2:
        this->bg_map_data_2[address - base_address] = value;
        break;

    case MMU::section::cartridge_ram:
        this->cartridge_ram[address - base_address] = value;
        break;

    case MMU::section::internal_ram_bank_0:
        this->internal_ram_bank_0[address - base_address] = value;
        break;

    case MMU::section::internal_ram_bank_1_to_7:
        this->internal_ram_bank_1_to_7[address - base_address] = value;
        break;

    case MMU::section::echo_ram:
        if (!load_rom_complete) {
            this->echo_ram[address - base_address] = value;
        }
        break;

    case MMU::section::unusuable_memory:
        if (!load_rom_complete) {
            this->unusable_memory[address - base_address] = value;
        }
        break;

    case MMU::section::oam_ram:
        this->oam_ram[address - base_address] = value;
        break;

    case MMU::section::hardware_registers:
        if (address == 0xff04) {
            handle_div_write();
        }

        else if (address == 0xff05) { // tima_ff05
            handle_tima_write(value);
        }

        if (address == 0xff06) {
            handle_tma_write(value);
            return;
        }

        else if (address == 0xff07) {
            handle_tac_write(value);
        }

        else {
            this->hardware_registers[address - base_address] = value;
        }
        break;

    case MMU::section::zero_page:
        this->zero_page[address - base_address] = value;
        break;

    case MMU::section::interrupt_enable_flag:
        this->interrupt_enable_flag = value;
        break;

    default:
        std::cout << "(write) memory not implemented: "
                  << "hex: 0x" << std::hex << static_cast<unsigned int>(address)
                  << std::endl;
        break;
    }
}
