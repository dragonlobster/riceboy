#pragma once

#include "cartridge.h"
#include "mbc1.h"
#include <cstdint>
#include <memory>
#include <vector>

// TODO: restrict access to ROM, VRAM, and OAM

class mmu {
  public:
    virtual uint8_t read_memory(uint16_t address) const;
    virtual uint8_t bus_read_memory(
        uint16_t address); // corruption bug could modify memory (not const)

    virtual void write_memory(uint16_t address, uint8_t value);
    virtual void bus_write_memory(uint16_t address, uint8_t value);

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
    // zero page = high ram

    // vram - 8000 - 9fff
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

    enum class bus : uint8_t {
        // 2 buses for dmg
        main,
        vram
    };

    static mmu::section locate_section(const uint16_t address);

    cartridge_type _cartridge_type{};

    void set_load_rom_complete();
    void set_cartridge_type(uint8_t type);

    // div, timer related
    // div counter stored in mmu
    uint16_t div_ff04{0};
    uint8_t tima_ff05{0}; // tima_ff05
    uint8_t tac_ff07{0xf8};
    uint8_t last_div_state{0}; // for falling edge detection
    bool tima_overflow{false};
    bool tima_overflow_standby{false};
    bool lock_tima_write{false};
    void increment_div(uint16_t value = 1, bool check_falling_edge = true);
    void falling_edge();
    void handle_tima_overflow();
    void handle_div_write();
    void handle_tac_write(uint8_t value);
    void handle_tima_write(uint8_t value);
    void handle_tma_write(uint8_t value);
    void handle_stat_write(uint8_t value);

    // dma
    uint8_t dma_ff46{0}; // dma starts out as 0xff
    bool dma_mode{false};
    bool dma_delay{false}; // delay dma start by one cycle

    uint16_t dma_source_transfer_address{0}; // address for DMA transfer
    bus dma_bus_source{};
    void set_oam_dma();
    void handle_dma_write(uint8_t value);

    void dma_transfer();

    // oam corruption bug related functions
    void oam_bug_read(uint16_t address);
    void oam_bug_write(uint16_t address);
    void oam_bug_read_inc(
        uint16_t address); // when read and increase occur in the same cycle

    // used for oam corruption bug (ppu current oam row accessed in mode 2)
    uint8_t ppu_current_oam_row{0};

    // lcdc register, lcd was reset
    uint8_t lcdc_ff40{};
    void handle_lcdc_write(uint8_t value);
    bool lcd_toggle{false};
    bool lcd_on{true}; // indicates whether lcd is on or off
    // TODO: fix timer and interrupt as individual classes later on

    // ppu mode
    uint8_t ppu_mode{2};

    // oam write and read block
    bool oam_write_block{false};
    bool oam_read_block{false};
    bool vram_write_block{false};
    bool vram_read_block{false};

    // cpu needs to tell us if it halted so the ppu can know
    bool cpu_halted{false};

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

    std::unique_ptr<cartridge> cartridge{};

    bool load_rom_complete{false};
};
