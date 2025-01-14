#include "mmu.h"
#include <iostream>

mmu::section mmu::locate_section(uint16_t address) {

    if (address <= 0x00ff) {
        return mmu::section::restart_and_interrupt_vectors;
    }

    else if (address >= 0x0100 && address <= 0x014f) {
        return mmu::section::cartridge_header_area;
    }

    else if (address >= 0x0150 && address <= 0x3fff) {
        return mmu::section::cartridge_rom_bank_0;
    }

    else if (address >= 0x4000 && address <= 0x7fff) {
        return mmu::section::cartridge_rom_switchable_banks;
    }

    else if (address >= 0x8000 && address <= 0x97ff) {
        return mmu::section::character_ram;
    }

    else if (address >= 0x9800 && address <= 0x9bff) {
        return mmu::section::bg_map_data_1;
    }

    else if (address >= 0x9c00 && address <= 0x9fff) {
        return mmu::section::bg_map_data_2;
    }

    else if (address >= 0xa000 && address <= 0xbfff) {
        return mmu::section::cartridge_ram;
    }

    else if (address >= 0xc000 && address <= 0xcfff) {
        return mmu::section::internal_ram_bank_0;
    }

    else if (address >= 0xd000 && address <= 0xdfff) {
        return mmu::section::internal_ram_bank_1_to_7;
    }

    else if (address >= 0xe000 && address <= 0xfdff) {
        return mmu::section::echo_ram;
    }

    else if (address >= 0xfe00 && address <= 0xfe9f) {
        return mmu::section::oam_ram;
    }

    else if (address >= 0xfea0 && address <= 0xfeff) {
        return mmu::section::unusuable_memory;
    }

    else if (address >= 0xff00 && address <= 0xff7f) {
        return mmu::section::hardware_registers;
    }

    else if (address >= 0xff80 && address <= 0xfffe) {
        return mmu::section::zero_page;
    }

    else if (address == 0xffff) {
        return mmu::section::interrupt_enable_flag;
    }

    return mmu::section::unknown;
}

// TODO: make more elegant by segregating each address space
uint8_t mmu::get_value_from_address(uint16_t address) const {

    if (locate_section(address) ==
        mmu::section::restart_and_interrupt_vectors) {
        return this->restart_and_interrupt_vectors[address];
    }

    else if (locate_section(address) ==
        mmu::section::cartridge_header_area) {
        return this->cartridge_header_area[address - 0x0100];
    }

    else if (locate_section(address) ==
        mmu::section::cartridge_rom_bank_0) {
        return this->cartridge_rom_bank_0[address - 0x0150];
    }

    else if (locate_section(address) ==
        mmu::section::cartridge_rom_switchable_banks) {
        return this->cartridge_rom_switchable_banks[address - 0x4000];
    }

    else if (locate_section(address) ==
        mmu::section::character_ram) {
        return this->character_ram[address - 0x8000];
    }

    else if (locate_section(address) ==
        mmu::section::bg_map_data_1) {
        return this->bg_map_data_1[address - 0x9800];
    }

    else if (locate_section(address) ==
        mmu::section::bg_map_data_2) {
        return this->bg_map_data_2[address - 0x9c00];
    }

    else if (locate_section(address) ==
        mmu::section::cartridge_ram) {
        return this->cartridge_ram[address - 0xa000];
    }

    else if (locate_section(address) ==
        mmu::section::internal_ram_bank_0) {
        return this->internal_ram_bank_0[address - 0xc000];
    }

    else if (locate_section(address) ==
        mmu::section::internal_ram_bank_1_to_7) {
        return this->internal_ram_bank_1_to_7[address - 0xd000];
    }

    else if (locate_section(address) ==
        mmu::section::echo_ram) {
        return this->echo_ram[address - 0xe000];
    }

    else if (locate_section(address) ==
        mmu::section::oam_ram) {
        return this->oam_ram[address - 0xfe00];
    }

    else if (locate_section(address) ==
        mmu::section::hardware_registers) {
        return this->hardware_registers[address - 0xff00];
    }

    else if (locate_section(address) ==
        mmu::section::zero_page) {
        return this->zero_page[address - 0xff80];
    }

    else if (locate_section(address) ==
        mmu::section::interrupt_enable_flag) {
        return this->interrupt_enable_flag;
    }


    else {
        std::cout << "(read) memory not implemented: " //<< static_cast<unsigned int>(opcode)
                  << "hex: 0x" << std::hex << static_cast<unsigned int>(address)
                  << std::endl;
        return 0;
    }
}

void mmu::write_value_to_address(uint16_t address, uint8_t value) {

    // TODO: restrict access
    if (locate_section(address) ==
        mmu::section::restart_and_interrupt_vectors) {
        this->restart_and_interrupt_vectors[address] = value;
    }

    else if (locate_section(address) ==
        mmu::section::cartridge_header_area) {
        this->cartridge_header_area[address - 0x0100] = value;
    }

    else if (locate_section(address) ==
        mmu::section::cartridge_rom_bank_0) {
        this->cartridge_rom_bank_0[address - 0x0150] = value;
    }

    else if (locate_section(address) ==
        mmu::section::cartridge_rom_switchable_banks) {
        this->cartridge_rom_switchable_banks[address - 0x4000] = value;
    }

    else if (locate_section(address) ==
        mmu::section::character_ram) {
        this->character_ram[address - 0x8000] = value;
    }

    else if (locate_section(address) ==
        mmu::section::bg_map_data_1) {
        this->bg_map_data_1[address - 0x9800] = value;
    }

    else if (locate_section(address) ==
        mmu::section::bg_map_data_2) {
        this->bg_map_data_2[address - 0x9c00] = value;
    }

    else if (locate_section(address) ==
        mmu::section::cartridge_ram) {
        this->cartridge_ram[address - 0xa000] = value;
    }

    else if (locate_section(address) ==
        mmu::section::internal_ram_bank_0) {
        this->internal_ram_bank_0[address - 0xc000] = value;
    }

    else if (locate_section(address) ==
        mmu::section::internal_ram_bank_1_to_7) {
        this->internal_ram_bank_1_to_7[address - 0xd000] = value;
    }

    /*
    else if (locate_section(address) ==
        mmu::section::echo_ram) {
        this->echo_ram[address - 0xe000] = value;
    }*/

    else if (locate_section(address) ==
        mmu::section::oam_ram) {
        this->oam_ram[address - 0xfe00] = value;
    }

    else if (locate_section(address) ==
        mmu::section::hardware_registers) {
        this->hardware_registers[address - 0xff00] = value;
    }

    else if (locate_section(address) ==
        mmu::section::zero_page) {
        this->zero_page[address - 0xff80] = value;
    }

    else if (locate_section(address) ==
        mmu::section::interrupt_enable_flag) {
        this->interrupt_enable_flag = value;
    }


    else {
        std::cout << "(write) memory not implemented: " //<< static_cast<unsigned int>(opcode)
                  << "hex: 0x" << std::hex << static_cast<unsigned int>(address)
                  << std::endl;
    }

    // TODO: rest of memory
}
