#pragma once

#include "Cartridge.h"
#include <array>
#include <vector>

class MBC1 : public Cartridge {
  public:
    virtual uint8_t read_memory(uint16_t address);
    virtual void write_memory(uint16_t address, uint8_t value);
    virtual void set_load_rom_complete();

  private:
    //std::array<uint8_t, 2097152> rom{};
    std::vector<uint8_t> rom{};

    uint8_t rom_bank_number{1};  // Current ROM bank (1-based)
    uint8_t ram_bank_number{0};  // Current RAM bank
    bool ram_enabled{false};     // ram enabled or not
    bool banking_mode{false}; // False = ROM banking, True = RAM banking

    bool load_rom_complete{false};
    uint8_t rom_size{0}; //0x00 - 0x08, 32KiB, 64KiB, 128KiB, 256KiB, 512KiB, 1MiB, 2MiB, 4MiB, 8MiB

    uint8_t ram_size{0}; //0x00 - 0x05, 0, 2KiB, 8KiB, 32KiB, 128KiB, 64KiB
};