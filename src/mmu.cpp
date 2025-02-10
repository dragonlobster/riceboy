#include "mmu.h"
#include <array>
#include <iostream>

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

uint8_t *
mmu::get_pointer_from_address(const uint16_t address) { // for PPU, will modify
    if (locate_section(address) == mmu::section::character_ram) {
        return &(this->character_ram[address - 0x8000]);
    } else if (locate_section(address) == mmu::section::bg_map_data_1) {
        return &(this->bg_map_data_1[address - 0x9800]);
    } else if (locate_section(address) == mmu::section::bg_map_data_2) {
        return &(this->bg_map_data_2[address - 0x9c00]);
    } else if (locate_section(address) == mmu::section::hardware_registers) {
        return &(this->hardware_registers[address - 0xff00]);
    } else if (locate_section(address) == mmu::section::oam_ram) {
        return &(this->oam_ram[address - 0xfe00]);
    }
    std::cout << "(read) memory not implemented: " //<< static_cast<unsigned
                                                   // int>(opcode)
              << "hex: 0x" << std::hex << static_cast<unsigned int>(address)
              << std::endl;
    return nullptr;
}

// TODO: make more elegant by segregating each address space
uint8_t mmu::get_value_from_address(uint16_t address) const {
    uint16_t base_address = static_cast<uint16_t>(locate_section(address));

    if (this->_cartridge_type == mmu::cartridge_type::mbc1) {
        // ROM Bank 0 (always accessible) (includes cartridge header and
        if (address <= 0x3fff) {
            // TODO: handle zero bank number based on mode flag
            return this->rom[address];
        }
        // Switchable ROM Bank - $4000 - $7FFF
        else if (address >= 0x4000 && address <= 0x7fff) {
            uint16_t bank_offset = this->rom_bank_number * 0x4000;
            return this->rom[bank_offset + (address - 0x4000)];
        }

        // Cartridge RAM - $A000 - $BFFF
        else if (address >= 0xa000 && address <= 0xbfff) {
            if (this->ram_enabled) {
                // If ROM Banking Mode: use bank 0
                // If RAM Banking Mode: use selected RAM bank
                uint8_t ram_bank =
                    this->rom_banking_mode ? 0 : this->ram_bank_number;
                return this->rom[(address) + (ram_bank * 0x2000)];
            }
            return 0xff; // Return 0xff (undefined) if RAM is disabled
        }
    }

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
        return this->character_ram[address - base_address];

    case mmu::section::bg_map_data_1:
        return this->bg_map_data_1[address - base_address];

    case mmu::section::bg_map_data_2:
        return this->bg_map_data_2[address - base_address];

    case mmu::section::cartridge_ram:
        return this->cartridge_ram[address - base_address];

    case mmu::section::internal_ram_bank_0:
        return this->internal_ram_bank_0[address - base_address];

    case mmu::section::internal_ram_bank_1_to_7:
        return this->internal_ram_bank_1_to_7[address - base_address];

    case mmu::section::echo_ram: return this->echo_ram[address - base_address];

    case mmu::section::oam_ram : return this->oam_ram[address - base_address];

    case mmu::section::hardware_registers:
        return this->hardware_registers[address - base_address];

    case mmu::section::zero_page:
        return this->zero_page[address - base_address];

    case mmu::section::interrupt_enable_flag:
        return this->interrupt_enable_flag;

    default:
        std::cout << "(read) memory not implemented: "
                  << "hex: 0x" << std::hex << static_cast<unsigned int>(address)
                  << std::endl;
        return 0;
    }
}

void mmu::write_value_to_address(uint16_t address, uint8_t value) {

    if (!this->load_rom_complete &&
        (this->_cartridge_type == mmu::cartridge_type::mbc1)) { // TODO: not rom_only
        this->rom.push_back(value);
        return;
    }

    uint16_t base_address = static_cast<uint16_t>(locate_section(address));

    if (this->_cartridge_type == mmu::cartridge_type::mbc1) {
        // RAM Enable/Disable - $0000 - $1FFF
        if (address <= 0x1fff) {
            // Enable RAM if value is 0x0A, disable otherwise
            this->ram_enabled = (value & 0x0f) == 0x0a; // 0000 1010
            return;
        }

        // ROM Bank Number - $2000 - $3FFF
        else if (address >= 0x2000 && address <= 0x3fff) {
            // Get the lower 5 bits
            uint8_t bank_number = value;

            // Handle special cases
            if (bank_number == 0) {
                bank_number = 1;
            }

            // Update ROM bank number
            // Clear lower 5 bits and set new value
            this->rom_bank_number =
                (this->rom_bank_number & 0xe0) | (bank_number & 0x1f);
            return;
        }

        // RAM Bank Number or Upper ROM Bank Bits - $4000 - $5FFF
        else if (address >= 0x4000 && address <= 0x5fff) {
            if (this->rom_banking_mode) {
                // ROM Banking Mode: Set upper 2 bits of ROM bank
                this->rom_bank_number =
                    (this->rom_bank_number & 0x1f) | ((value & 0x03) << 5);
            } else {
                // RAM Banking Mode: Select RAM bank
                this->ram_bank_number = value & 0x03;
            }
            return;
        }

        // Banking Mode Select - $6000 - $7FFF
        else if (address >= 0x6000 && address <= 0x7fff) {
            // Switch between ROM and RAM banking modes
            this->rom_banking_mode = (value & 0x01) == 0;
            return;
        }

        // Cartridge RAM Write - $A000 - $BFFF
        else if (address >= 0xa000 && address <= 0xbfff) {
            if (this->ram_enabled) {
                // If ROM Banking Mode: use bank 0
                // If RAM Banking Mode: use selected RAM bank
                uint8_t ram_bank =
                    this->rom_banking_mode ? 0 : this->ram_bank_number;
                this->rom[(address) + (ram_bank * 0x2000)] = value;
            }
            return;
        }
    }

    switch (locate_section(address)) {
    case mmu::section::restart_and_interrupt_vectors:
        this->restart_and_interrupt_vectors[address] = value;
        break;

    case mmu::section::cartridge_header_area:
        this->cartridge_header_area[address - base_address] = value;
        break;

    case mmu::section::cartridge_rom_bank_0:
        this->cartridge_rom_bank_0[address - base_address] = value;
        break;

    case mmu::section::cartridge_rom_switchable_banks:
        this->cartridge_rom_switchable_banks[address - base_address] = value;
        break;

    case mmu::section::character_ram:
        this->character_ram[address - base_address] = value;
        break;

    case mmu::section::bg_map_data_1:
        this->bg_map_data_1[address - base_address] = value;
        break;

    case mmu::section::bg_map_data_2:
        this->bg_map_data_2[address - base_address] = value;
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
        this->oam_ram[address - base_address] = value;
        break;

    case mmu::section::hardware_registers:
        if (address == 0xff04) {
            this->hardware_registers[address - 0xff00] = 0; // trap div
        } else {
            this->hardware_registers[address - base_address] = value;
        }
        break;

    case mmu::section::zero_page:
        this->zero_page[address - base_address] = value;
        break;

    case mmu::section::interrupt_enable_flag:
        this->interrupt_enable_flag = value;
        break;

    default:
        std::cout << "(write) memory not implemented: "
                  << "hex: 0x" << std::hex << static_cast<unsigned int>(address)
                  << std::endl;
        break;
    }
}
