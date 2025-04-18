#include "mbc1.h"
#include <cassert>
#include <iostream>

// TODO: implement MBCM1 (multicart)

uint16_t mbc1::read_memory(uint16_t address) {
    if (address <= 0x3fff) {
        // mode 0 - only access to normal address range
        // mode 1 - can access banks 1, 20, 40, 60 (stored in *0x4000) depending
        // on size of rom
        if (!this->banking_mode) {
            return this->rom[address];
        } else {
            uint8_t zero_bank_number{0};

            if (this->rom_size < 0x05) {
                // less than 1 MiB
                // offset = 0
            } else if (this->rom_size == 0x05) {
                zero_bank_number = ((this->ram_bank_number & 1) << 5);
            } else if (this->rom_size > 0x05) {
                // rom size > 0x05
                zero_bank_number = ((this->ram_bank_number) << 5);
            }
            return this->rom[0x4000 * zero_bank_number + address];
        }
    }

    // Switchable ROM Bank - $4000 - $7FFF
    else if (address >= 0x4000 && address <= 0x7fff) {

        uint16_t high_bank_number{0};

        if (this->rom_size < 0x05) { // less than 1MiB
            high_bank_number = this->rom_bank_number;

        } else if (this->rom_size == 0x05) { // = 1MiB
            high_bank_number =
                this->rom_bank_number + ((this->ram_bank_number & 1) << 5);
            // lowest bit of ram bank number should take bit 5
            // place of the rom bank number (use | instead?)
        } else if (this->rom_size > 0x05) { // > 1MiB
            high_bank_number =
                this->rom_bank_number +
                (this->ram_bank_number
                 << 5); // both bits of ram bank number takes bit 5 and 6
                        // of the rom bank number (use | instead?)
        }

        // std::cout << final_address << '\n';

        uint32_t final_address = 0x4000 * high_bank_number + (address - 0x4000);
        final_address %= this->rom.size(); // wrap # of banks
        uint8_t result = this->rom.at(final_address);
        return result;

    }

    // Cartridge RAM - $A000 - $BFFF
    else if (address >= 0xa000 && address <= 0xbfff) {
        if (this->ram_enabled && ram_size > 0) {
            // If ROM Banking Mode: use bank 0
            // If RAM Banking Mode: use selected RAM bank

            if (this->ram_size <= 0x02) {
                uint16_t ram = ram_size == 0x01 ? 2048 : 8192;
                try {
                    return this->ram.at((address - 0xa000) % ram);
                } catch (std::out_of_range) { return 0xff; }
            } // 2KiB or 8KiB Ram

            else {
                uint16_t final_address =
                    this->banking_mode
                        ? 0x2000 * this->ram_bank_number + (address - 0xa000)
                        : address - 0xa000;

                return this->ram[final_address];
                try {
                    return this->ram.at(final_address);
                } catch (std::out_of_range) { return 0xff; }
            }
        }
        return 0xff; // TODO: handle 0xff return?
    }

    return 0xfff;
}

void mbc1::write_memory(uint16_t address, uint8_t value) {

    if (!this->load_rom_complete) {
        this->rom.push_back(value);
    }

    else {
        // RAM Enable/Disable - $0000 - $1FFF
        if (address <= 0x1fff) {
            // Enable RAM if value is 0x0A, disable otherwise
            this->ram_enabled = (value & 0x0f) == 0x0a; // 0000 1010
        }

        else if (address >= 0x2000 && address <= 0x3fff) {

            if ((value & 0x1f) == 0) {
                this->rom_bank_number = 1;
            }

            else {
                uint8_t mask{0};

                assert(this->rom_size <= 6 && "ROM size was not expected!");
                // the mask is used to address bank #'s that exceed the # of
                // banks available on the cart. e.g., 256 KiB cart with 16 banks
                // only needs 4 bits (0-15), so we mask it with 0000 1111 (lower
                // 4 bits)
                switch (this->rom_size) {
                case 0: mask = this->rom_bank_number = 1; return;
                case 1: mask = 0x03; break; // 0000 0011
                case 2: mask = 0x07; break; // 0000 0111
                case 3: mask = 0x0f; break; // 0000 1111
                case 4: mask = 0x1f; break; // 0001 1111
                case 5: mask = 0x1f; break; // 0001 1111
                case 6: mask = 0x1f; break; // 0001 1111
                }

                this->rom_bank_number = value & mask;
            }

        }

        // RAM Bank Number or Upper ROM Bank Bits - $4000 - $5FFF
        else if (address >= 0x4000 && address <= 0x5fff) {
            /*
            if (this->ram_size < 3 && this->rom_size < 5) {
                return;
            } else {
                this->ram_bank_number = value & 3;
            }*/
            this->ram_bank_number = value & 3;
        }

        // Banking Mode Select - $6000 - $7FFF
        else if (address >= 0x6000 && address <= 0x7fff) {
            // Switch between ROM and RAM banking modes
            if (this->ram_size <= 2 && this->rom_size <= 4) {
                // ram <= 8Kib && rom <= 512 KiB - no observable effect
                return;
            }
            this->banking_mode = (value & 0x01) != 0;
        }

        // Cartridge RAM Write - $A000 - $BFFF
        else if (address >= 0xa000 && address <= 0xbfff) {
            if (this->ram_enabled && ram_size > 0) {
                // If ROM Banking Mode: use bank 0
                // If RAM Banking Mode: use selected RAM bank

                if (this->ram_size <= 0x02) {
                    // uint16_t ram_size_bytes = ram_size == 0x01 ? 2048 : 8192;
                    this->ram[(address - 0xa000) % this->ram.size()] = value;
                } // 2KiB or 8KiB Ram

                else if (this->ram_size == 0x03) {
                    uint16_t final_address =
                        this->banking_mode ? 0x2000 * this->ram_bank_number +
                                                 (address - 0xa000)
                                           : address - 0xa000;

                    assert(final_address <= this->ram.size() &&
                           "final address is out of bounds of the RAM!");

                    this->ram[final_address] = value;
                }
            }
        }
    }
}

void mbc1::set_load_rom_complete() {
    this->load_rom_complete = true;

    // set rom size
    this->rom_size = this->rom.at(0x148);
    this->ram_size = this->rom.at(0x149);

    switch (ram_size) {
    case 1: this->ram.resize(2048); break;
    case 2: this->ram.resize(8192); break;
    case 3: this->ram.resize(32768); break;
    }

    assert(
        this->rom_size <= 6 &&
        "ROM size not valid in memory"); // make sure rom size is only 0 to 0x06
                                         // (MBC1 only supports up to 2MiB)
    assert(this->ram_size <= 5 &&
           "RAM size not valid in memory"); // make sure ram size is only 0  to
                                            // 0x05
}
