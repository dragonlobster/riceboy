#pragma once

// TODO: restrict access to ROM, VRAM, and OAM

#include <cstdint>
class mmu {
  public:
    virtual uint8_t get_value_from_address(uint16_t address) const;

    uint8_t *get_pointer_from_address(uint16_t address);

    virtual void write_value_to_address(uint16_t address, uint8_t value);

    enum class section : uint16_t {
        restart_and_interrupt_vectors = 0,       // 0x00ff
        cartridge_header_area = 0x0100,          // 0x14f
        cartridge_rom_bank_0 = 0x0150,           // 0x3fff
        cartridge_rom_switchable_banks = 0x4000, // 0x7fff
        character_ram = 0x8000,                  // 0x97ff
        bg_map_data_1 = 0x9800,                  // 0x9bff
        bg_map_data_2 = 0x9c00,                  // 0x9fff
        cartridge_ram = 0xa000,                  // 0xbfff
        internal_ram_bank_0 = 0xc000,            // 0xcfff
        internal_ram_bank_1_to_7 = 0xd000,       // 0xdfff
        echo_ram = 0xe000,                       // 0xfdff
        oam_ram = 0xfe00,                        // 0xfe9f
        unusuable_memory = 0xfea0,               // 0xfeff
        hardware_registers = 0xff00,             // 0xff7f
        zero_page = 0xff80,                      // 0xfffe
        interrupt_enable_flag = 0xffff,
        unknown = 1
    };

    enum class cartridge_type : uint8_t {
        rom_only = 0,
        mbc1 = 1,
        mbc1_ram = 2,
        mbc1_ram_battery = 3,
        mbc2 = 5,
        mbc2_battery = 6,
        rom_ram = 8,
        rom_ram_battery = 9,
        mmm01 = 0xb,
        mmm01_ram = 0xc,
        mmm01_ram_battery = 0xd,
        mbc3_timer_battery = 0xf,
        mbc3_timer_ram_battery = 0x10,
        mbc3 = 0x11,
        mbc3_ram = 0x12,
        mbc3_ram_battery = 0x13,
        mbc5 = 0x19,
        mbc5_ram = 0x1a,
        mbc5_ram_battery = 0x1b,
        mbc5_rumble = 0x1c,
        mbc5_rumble_ram = 0x1d,
        mbc5_rumble_ram_battery = 0x1e,
        mbc6 = 0x20,
        mbc7_sensor_rumble_ram_battery = 0x22,
        pocket_camera = 0xfc,
        bandai_tama5 = 0xfd,
        huc3 = 0xfe,
        huc1_ram_battery = 0xff
    };

    static mmu::section locate_section(const uint16_t address);

    void load_rom_complete();

  private:
    // Interrupt enable flag - 0xFFFF
    uint8_t interrupt_enable_flag{};

    // zero page - ff80 - fffe, High RAM (127 bytes)
    uint8_t zero_page[(0xfffe - 0xff80) + 1]{};

    // hardware registers - ff00 - ff7f
    uint8_t hardware_registers[(0xff7f - 0xff00) + 1]{};

    // fea0

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

    // load rom complete
    bool _load_rom_complete{false};

    cartridge_type _cartridge_type{};

    uint8_t rom_bank_number{1}; // Current ROM bank (1-based)
    uint8_t ram_bank_number{0}; // Current RAM bank
    bool ram_enabled{false};
    bool rom_banking_mode{true}; // True = ROM banking, False = RAM banking
};