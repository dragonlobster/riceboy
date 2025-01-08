#include <cstdint>
class mmu {
public:

	uint8_t get_value_from_address(uint16_t address);

	// set boot completed to true
	bool complete_boot();

private:

	// boot rom: 0x0000 -> 0x00ff
	uint8_t memory_boot_rom[0x00ff+1]{};

	// cartridge rom: 0x0000 -> 0x7fff
	uint8_t memory_cartridge_rom[0x7fff+1]{};

	// ppu vram (8kb)
	uint8_t memory_vram[(0x9fff - 0x8000) + 1]{};

	// cartridge ram, also known as eram: 0xa000 -> 0xbfff (8KB)
	uint8_t memory_cartridge_ram[(0xbfff - 0xa000) + 1]{};

	// indicates of the boot rom has completed
	bool is_boot_complete{ false };

};