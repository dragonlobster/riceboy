#include "MBC1.h"
#include <cassert>

/*
uint8_t MBC1::read_memory(uint16_t address) {
    // ROM Bank 0 (always accessible) (includes cartridge header and
    if (address <= 0x3fff) {
        // TODO: handle zero bank number based on mode flag
        if (!this->banking_mode) {
            return this->rom.at(address);
        } else {
            uint8_t zero_bank_number{0};
            if (this->rom_size < 0x05) {
                // less than 1 MiB
                // offset = 0
            } else if (this->rom_size == 0x05) {
                zero_bank_number = (this->ram_bank_number) ? 0x20 : 0;
            } else if (this->rom_size > 0x05) {
                // rom size > 0x05
                switch (this->ram_bank_number) {
                // case 0: zero_bank_number = 0; break;
                case 1: zero_bank_number = 0x20; break;
                case 2: zero_bank_number = 0x40; break;
                case 3: zero_bank_number = 0x60; break;
                }
            }
            return this->rom[0x4000 * zero_bank_number + address];
        }
    }
    // Switchable ROM Bank - $4000 - $7FFF
    else if (address >= 0x4000 && address <= 0x7fff) {

        uint16_t high_bank_number{0};

        if (this->rom_size < 0x05) {
            high_bank_number = this->rom_bank_number;
        } else if (this->rom_size == 0x05) {
            high_bank_number =
                this->rom_bank_number + ((this->ram_bank_number & 1) << 5);
        } else if (this->rom_size > 0x05) {
            high_bank_number =
                this->rom_bank_number + (this->ram_bank_number << 5);
        }

        uint16_t bank_offset = this->rom_bank_number * 0x4000;
        return this->rom[(bank_offset * high_bank_number) + (address - 0x4000)];
    }

    // Cartridge RAM - $A000 - $BFFF
    else if (address >= 0xa000 && address <= 0xbfff) {
        if (this->ram_enabled && ram_size > 0) {
            // If ROM Banking Mode: use bank 0
            // If RAM Banking Mode: use selected RAM bank

            if (this->ram_size <= 0x02) {
                uint16_t ram = ram_size == 0x01 ? 2048 : 8192;
                return this->rom[(address - 0xa000) % ram];
            } // 2KiB or 8KiB Ram

            else {
                uint16_t final_address =
                    this->banking_mode
                        ? 0x2000 * this->ram_bank_number + (address - 0xa000)
                        : address - 0xa000;

                return this->rom[final_address];
            }
        }

        return 0xff;
    }

    else {
        return this->rom.at(address);
    }
}
*/

uint8_t MBC1::read_memory(uint16_t address) {
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
            uint8_t ram_bank = this->banking_mode ? this->ram_bank_number : 0;
            return this->rom[(address) + (ram_bank * 0x2000)];
        }
        return 0xff; // Return 0xff (undefined) if RAM is disabled
    }

    return this->rom[address];
}

/*
void MBC1::write_memory(uint16_t address, uint8_t value) {

    if (!this->load_rom_complete) {
        // this->rom[address] = value;
        this->rom.push_back(value);
    }

    else {
        // RAM Enable/Disable - $0000 - $1FFF
        if (address <= 0x1fff) {
            // Enable RAM if value is 0x0A, disable otherwise
            this->ram_enabled = (value & 0x0f) == 0x0a; // 0000 1010
        }

        // ROM Bank Number - $2000 - $3FFF
        else if (address >= 0x2000 && address <= 0x3fff) {

            uint8_t mask{0};

            switch (this->rom_size) {
            case 0: this->rom_bank_number = 1; return;
            case 1: mask = 0x03; break;
            case 2: mask = 0x07; break;
            case 3: mask = 0x0f; break;
            case 4: mask = 0x1f; break;
            case 5: mask = 0x1f; break;
            case 6: mask = 0x1f; break;
            }

            assert(mask > 0 && "ROM size was not expected!");

            uint8_t bank_number = value ? value : 1;

            this->rom_bank_number = bank_number & mask;
        }

        // RAM Bank Number or Upper ROM Bank Bits - $4000 - $5FFF
        else if (address >= 0x4000 && address <= 0x5fff) {
            if (!this->banking_mode) {
                // ROM Banking Mode: Set upper 2 bits of ROM bank
                this->rom_bank_number =
                    (this->rom_bank_number & 0x1f) | ((value & 0x03) << 5);
            } else {
                // RAM Banking Mode: Select RAM bank
                this->ram_bank_number = value & 0x03;
            }
        }

        // Banking Mode Select - $6000 - $7FFF
        else if (address >= 0x6000 && address <= 0x7fff) {
            // Switch between ROM and RAM banking modes
            this->banking_mode = (value & 0x01) == 1;
        }

        // Cartridge RAM Write - $A000 - $BFFF
        else if (address >= 0xa000 && address <= 0xbfff) {
            if (this->ram_enabled && ram_size > 0) {
                // If ROM Banking Mode: use bank 0
                // If RAM Banking Mode: use selected RAM bank

                if (this->ram_size <= 0x02) {
                    uint16_t ram = ram_size == 0x01 ? 2048 : 8192;
                    this->rom[(address - 0xa000) % ram] = value;
                } // 2KiB or 8KiB Ram

                else {
                    uint16_t final_address =
                        this->banking_mode ? 0x2000 * this->ram_bank_number +
                                                 (address - 0xa000)
                                           : address - 0xa000;

                    this->rom[final_address] = value;
                }
            }
        }

        else {
            this->rom[address] = value;
        }
    }
}
*/

void MBC1::write_memory(uint16_t address, uint8_t value) {

    if (!this->load_rom_complete) {
        this->rom.push_back(value);
    }

    else {

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
            if (!this->banking_mode) {
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
            this->banking_mode = (value & 0x01) == 1;
            return;
        }

        // Cartridge RAM Write - $A000 - $BFFF
        else if (address >= 0xa000 && address <= 0xbfff) {
            if (this->ram_enabled) {
                // If ROM Banking Mode: use bank 0
                // If RAM Banking Mode: use selected RAM bank
                uint8_t ram_bank =
                    this->banking_mode ? this->ram_bank_number : 0;
                this->rom[(address) + (ram_bank * 0x2000)] = value;
            }
            return;
        } else {
            // TODO: check locks here
            // lock echo ram and unusable ram

            if (address == 0xff04) {
                this->rom[address] = 0; // trap div
            } else {
                this->rom[address] = value;
            }


        }
    }
}

void MBC1::set_load_rom_complete() {
    this->load_rom_complete = true;

    // set rom size
    this->rom_size = this->rom.at(0x148);
    this->ram_size = this->rom.at(0x149);

    assert(
        this->rom_size <= 6 &&
        "ROM size not valid in memory"); // make sure rom size is only 0 to 0x06
                                         // (MBC1 only supports up to 2MiB)
    assert(this->ram_size <= 5 &&
           "RAM size not valid in memory"); // make sure ram size is only 0  to
                                            // 0x05
}
