#pragma once

// TODO: restrict access to ROM, VRAM, and OAM

#include <cstdint>
class mmu {
  public:
    virtual uint8_t get_value_from_address(uint16_t address) const;

    uint8_t* get_pointer_from_address(uint16_t address);

    virtual void write_value_to_address(uint16_t address, uint8_t value);

    enum class section {
        restart_and_interrupt_vectors,
        cartridge_header_area,
        cartridge_rom_bank_0,
        cartridge_rom_switchable_banks,
        character_ram,
        bg_map_data_1,
        bg_map_data_2,
        cartridge_ram,
        internal_ram_bank_0,
        internal_ram_bank_1_to_7,
        echo_ram,
        oam_ram,
        unusuable_memory,
        hardware_registers,
        zero_page,
        interrupt_enable_flag,
        unknown
    };

    static section locate_section(uint16_t address);

    //section locate_section(uint16_t address);

  private:
    // Interrupt enable flag - 0xFFFF
    uint8_t interrupt_enable_flag{};

    // zero page - ff80 - fffe, High RAM (127 bytes)
    uint8_t zero_page[(0xfffe - 0xff80) + 1]{};

    // hardware registers - ff00 - ff7f
    uint8_t hardware_registers[(0xff7f - 0xff00) + 1]{};

    // ununsable memory - 0xfea0 - 0xfeff
    uint8_t unusable_memory[(0xfeff - 0xfea0) + 1]{};

    // oam ram - 0xfe00 - 0xfe9f
    uint8_t oam_ram[(0xfe9f - 0xfe00) + 1]{};

    // echo ram - reserved, do not use 0xe000 - 0xfdff
    uint8_t echo_ram[(0xfdff - 0xe000) + 1]{};

    // internal ram bank 1 - 7 (switchable CGB only) - 0xd000 - 0xdfff
    uint8_t internal_ram_bank_1_to_7[(0xdfff - 0xd000) + 1]{};

    // internal ram bank 0 - 0xc000 - 0xcfff 
    uint8_t internal_ram_bank_0[(0xcfff - 0xc000) + 1]{};

    // cartridge ram - 0xa000 - 0xbfff
    uint8_t cartridge_ram[(0xbfff - 0xa000) + 1]{}; // e-ram

    // bg_map_data_2 - 0x9C00 - 0x9FFF
    uint8_t bg_map_data_2[(0x9fff - 0x9c00) + 1]{};

    // bg_map_data_1 - 0x9800 - 0x9bff
    uint8_t bg_map_data_1[(0x9bff - 0x9800) + 1]{};

    // character ram - 0x8000 - 0x97ff
    uint8_t character_ram[(0x97ff - 0x8000) + 1]{};

    // cartridge rom - switchable banks 1-xx - 0x4000 - 0x7FFF
    uint8_t cartridge_rom_switchable_banks[(0x7fff - 0x4000) + 1]{};

    // cartridge rom - bank 0 (fixed) - 0x0150 - 0x3FFF
    uint8_t cartridge_rom_bank_0[(0x3fff - 0x0150) + 1]{};

    // cartridge header area - 0x0100 - 0x014F
    uint8_t cartridge_header_area[(0x014f - 0x0100) + 1]{};

    // restart and interrupt vectors - 0x0000 - 0x00FF
    uint8_t restart_and_interrupt_vectors[0x00ff + 1]{};


};