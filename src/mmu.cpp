#include "MMU.h"
#include <array>
#include <cassert>
#include <iostream>

void MMU::set_load_rom_complete() {

    if (this->_cartridge_type == MMU::cartridge_type::mbc1) {
        assert(this->cartridge.get() != nullptr &&
               "Cartridge is NULL! Can't complete load_rom_complete");
        this->cartridge->set_load_rom_complete();
    }

    this->load_rom_complete = true;
}

void MMU::set_cartridge_type(uint8_t type) {

    this->_cartridge_type = static_cast<MMU::cartridge_type>(type);

    if (_cartridge_type == MMU::cartridge_type::mbc1) {
        this->cartridge = std::make_unique<MBC1>();
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

    if (this->_cartridge_type == MMU::cartridge_type::mbc1) {
        assert(this->cartridge.get() != nullptr &&
               "Cartridge is NULL! Can't read!");

        return this->cartridge->read_memory(address);
    }

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
        return this->hardware_registers[address - base_address];

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

    if (this->_cartridge_type ==
        MMU::cartridge_type::mbc1) { // TODO: not rom_only

        // cartridge type should be identified and instantiated when writing to
        // memory
        assert(this->cartridge.get() != nullptr &&
               "Cartridge was not initiated! Call set_cartridge_type first!");

        this->cartridge->write_memory(address, value);

        return;
    }

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
            this->hardware_registers[address - 0xff00] = 0; // trap div
        } else {
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
