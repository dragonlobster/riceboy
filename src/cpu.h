#include <cstdint>
#include "mmu.h"
#include <tuple>
#include <vector>
#include <functional>

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
	//uint8_t F{}; 
	bool Zf{}; // zero flag
	bool Nf{}; // subtract flag
	bool Hf{}; // half-carry flag
	bool Cf{}; // carry-flag

	uint8_t H{};
	uint8_t L{};

	// 16-bit registers - stack pointer, program counter
	uint16_t SP{}; 
	uint16_t PC{};

	// temporary registers for complex operations
	uint8_t W{};
	uint8_t Z{};

	std::vector<std::function<void()>> M_operations;

	// execute instructions, return how much program_counter should increment
	void execute_opcode(const uint16_t& opcode);

private:

	// micro operations
	uint8_t _read_memory(const uint16_t& address);


	void add_hl(); // add content from address HL to A

	void add_a_r8(const char& r8); // add content from register r8 to A

	void bit_b_r8(const char& r8);

	void call();

	// utility //
	uint16_t _get_register_pair(const uint8_t& r1, const uint8_t& r2);

	// all return result, half-carry, carry
	std::tuple<uint8_t, bool, bool> _addition_8bit(uint8_t x, uint8_t y);
	std::tuple<uint8_t, bool, bool> _subtraction_8bit(uint8_t x, uint8_t y);
	std::tuple<uint16_t, bool, bool> _addition_16bit(uint16_t x, uint16_t y);

	uint8_t _flags_to_byte() const; // const after function declaration makes it a compiler error to edit class members inside the function

	// TODO: make enum
	uint8_t _get_register_value(const char& r8) const; // get the register value based on char r8

};
