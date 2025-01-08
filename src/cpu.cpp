#include "cpu.h"

// abstracted operations
int cpu::add_hl(const uint16_t& opcode) {
	
	uint16_t hl = get_register_pair(this->H, this->L);
	this->gb_mmu->get_value_from_address(hl);
}

// TODO: all pairs can be converted to const expr inside constructor
uint16_t cpu::get_register_pair(const uint8_t &r1, const uint8_t &r2) {
	return (r1 << 8) | r2;
}

// cpu constructor
cpu::cpu(mmu& mmu) { // pass by reference
	this->gb_mmu = &mmu; // & refers to actual address to assign to the pointer
}

int cpu::execute_opcode(const uint16_t& opcode) {

	// big switch case for differnet instructions
	switch (opcode) {
		// TODO: implement
	case 0x31:
		return 1;
		
	}

	return 0;
}
