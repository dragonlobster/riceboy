#include "mmu.h"

// TODO: make more elegant by segregating each address space
uint8_t mmu::get_value_from_address(uint16_t address) {
	
	if (address >= 0 && address <= 0x00ff && !this->is_boot_complete) {
		return this->memory_boot_rom[address];
	}

	else if (address >= 0 && address <= 0x7fff && this->is_boot_complete) {
		return this->memory_cartridge_rom[address];

	}

	else if (address >= 0x8000 && address <= 0x9fff) {
		return this->memory_vram[address - 0x8000]; // need to offset the address by the starting point
	}

	else if (address >= 0xbfff && address <= 0xa000) {
		return this->memory_cartridge_ram[address - 0xbfff];
	}

	else {
		return 0;
	}

}

bool mmu::complete_boot() {
	this->is_boot_complete = true;
	return is_boot_complete;
}
