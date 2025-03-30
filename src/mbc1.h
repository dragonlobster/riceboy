#pragma once

#include "cartridge.h"
#include <array>
#include <vector>

class mbc1 : public cartridge {
  public:
    virtual uint16_t read_memory(uint16_t address); // 16 bit to return > 8 bit for unprocessed ranges
    virtual void write_memory(uint16_t address, uint8_t value);
    virtual void set_load_rom_complete();

  private:
    //std::array<uint8_t, 2097152> rom{};
    std::vector<uint8_t> rom{};
    std::vector<uint8_t> ram{}; // ram 

    uint8_t rom_bank_number{1};  // (1-based) 5 bit register (bank1)
    uint8_t ram_bank_number{0};  // 2 bit register (bank2)
    bool ram_enabled{false};     // ram enabled or not
    bool banking_mode{false};    // 0 - rom banking mode, 1 - ram banking mode
    bool load_rom_complete{false};

    uint8_t rom_size{0}; //0x00 - 0x08, 32KiB, 64KiB, 128KiB, 256KiB, 512KiB, 1MiB, 2MiB, 4MiB, 8MiB

    uint8_t ram_size{0}; //0x00 - 0x05, 0, 2KiB, 8KiB, 32KiB, 128KiB, 64KiB

    // DEBUG ONLY
    uint8_t old_bank_number{0};
    uint8_t old_value{0};
};