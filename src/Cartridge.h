#pragma once

#include <cstdint>

class Cartridge {
  public:
    virtual ~Cartridge() {}
    virtual uint8_t read_memory(uint16_t address) = 0;
    virtual void write_memory(uint16_t address, uint8_t value) = 0;
    virtual void set_load_rom_complete() = 0;
};