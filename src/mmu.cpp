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
		return this->memory_cartridge_ram[address - 0xbfff]; // E-Ram
	}

	else {
		return 0;
	}

}

void mmu::write_value_to_address(uint16_t address, uint8_t value) {

	if (address >= 0 && address <= 0x00ff && !this->is_boot_complete) {
		// only write boot rom
		this->memory_boot_rom[address] = value;
	}

	else if (address >= 0 && address <= 0x7fff && this->is_boot_complete && !is_cartridge_loaded) {
		this->memory_cartridge_rom[address] = value;
		// TODO: can write once boot rom is loaded, but can't write after cartrdige is loaded
	}

	else if (address >= 0x8000 && address <= 0x9fff) {
		this->memory_vram[address - 0x8000] = value; // need to offset the address by the starting point
	}

	else if (address >= 0xbfff && address <= 0xa000) {
		this->memory_cartridge_ram[address - 0xbfff] = value; // E-Ram
	}

	// TODO: rest of memory

}

bool mmu::complete_boot() {
	this->is_boot_complete = true;
	return is_boot_complete;
}

bool mmu::complete_load_cartridge() {
	this->is_cartridge_loaded = true;
	return is_cartridge_loaded;
}
