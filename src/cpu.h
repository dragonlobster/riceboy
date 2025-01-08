#include <cstdint>
#include "mmu.h"

class cpu {

public:
	// pointer to mmu
	cpu(mmu&); // pass by reference
	mmu* gb_mmu{}; // the central mmu

	// main 8-bit registers
	uint8_t A{};
	uint8_t B{};
	uint8_t C{};
	uint8_t D{};
	uint8_t E{};
	
	// flags register, 4 MSBs: Z(ero) N(subtract) H(half-carry) C(carry), 4 LSB 0 0 0 0 unused
	uint8_t F{}; 

	uint8_t H{};
	uint8_t L{};

	// 16-bit registers - stack pointer, program counter
	uint16_t SP{}; 
	uint16_t PC{};

	// execute instructions, return how much program_counter should increment
	int execute_opcode(const uint16_t& opcode);

private:
	int add_hl(const uint16_t& opcode);

	uint16_t get_register_pair(const uint8_t& r1, const uint8_t& r2);

};