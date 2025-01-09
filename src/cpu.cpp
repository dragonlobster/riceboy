#include "cpu.h"

// TODO: abstract utility later
std::tuple<uint8_t, bool, bool> cpu::_addition_8bit(uint8_t x, uint8_t y) {
	// return result, half_carry, full_carry
	uint8_t result = x + y;
	bool half_carry = (x & 0xf) + (y & 0xf) > 0xf; // see if 4 bit sum fits in 4 bits
	bool full_carry = static_cast<uint16_t>(x) + static_cast<uint16_t>(y) > 0xff; // see if added numbers > 8 bits
	return {result, half_carry, full_carry};
}

std::tuple<uint8_t, bool, bool> cpu::_subtraction_8bit(uint8_t x, uint8_t y) {
	// return result, half_carry, full_carry
	uint8_t result = x - y;
	bool half_carry = static_cast<int8_t>(x & 0xf) - static_cast<int8_t>(y & 0xf) < 0;
	bool full_carry = x < y;
	return { result, half_carry, full_carry };
}

std::tuple<uint16_t, bool, bool> cpu::_addition_16bit(uint16_t x, uint16_t y) {
	uint8_t result = x + y;
	bool half_carry = (x & 0xfff) + (y & 0xfff) > 0xfff; // see if 4 bit sum fits in 4 bits
	bool full_carry = static_cast<uint32_t>(x) + static_cast<uint32_t>(y) > 0xffff;
	return { result, half_carry, full_carry };
}

uint8_t cpu::_flags_to_byte() const {
	uint8_t Z = this->Zf ? 1 : 0;
	uint8_t N = this->Nf ? 1 : 0;
	uint8_t H = this->Hf ? 1 : 0;
	uint8_t C = this->Cf ? 1 : 0;
	return (Z << 7) | (N << 6) | (H << 5) | (C << 4);
}

// TODO: all pairs can be converted to const expr inside constructor ? maybe not
uint16_t cpu::_get_register_pair(const uint8_t &r1, const uint8_t &r2) {
	return (r1 << 8) | r2;
}

uint8_t cpu::_get_register_value(const char& r8) const {
	uint8_t register_value = 0;
	switch (r8) {
	case 'A':
		register_value = this->A;
		break;
	case 'B':
		register_value = this->B;
		break;
	case 'C':
		register_value = this->C;
		break;
	case 'D':
		register_value = this->D;
		break;
	case 'E':
		register_value = this->E;
		break;
	}
	return register_value;
}


// abstracted micro operations
uint8_t cpu::_read_memory(const uint16_t& address) {
	// increment prgram counter once
	this->PC++;

	// 1 M-cycle - get value from address
	return this->gb_mmu->get_value_from_address(address);

}

// TOOD: combine with add_a_r8 later on
void cpu::add_hl() {

	// lambda function
	auto get_value_from_hl = [&]() {
		uint16_t hl = this->_get_register_pair(this->H, this->L);
		this->W = this->_read_memory(hl); // assign value of HL to temp register W
		this->PC++; // increment PC
	};

	auto _add_hl = [&]() {
		// 1 M-cycle - add HL value to register A
		auto [result, half_carry, carry] = _addition_8bit(this->A, this->W);

		// optional: reset W after
		this->W = 0;

		// set a register to the result
		this->A = result;

		// flags
		if (result == 0) {
			this->Zf = true;
		}
		else {
			this->Zf = false;
		}
		this->Nf = false;
		this->Hf = half_carry;
		this->Cf = carry;

		this->PC++; // increment PC
	};


	// pushed backwards due to FIFO
	this->M_operations.push_back(_add_hl);
	this->M_operations.push_back(get_value_from_hl);
}

// TODO: use enum instead of char
void cpu::add_a_r8(const char& r8) {

	// TODO: if capture (r8) goes out of scope it could cause crashes
	auto _add_a_r8 = [&]() {
		uint8_t register_value = this->_get_register_value(r8);
		// 1 M-cycle - add value in register_value to a
		auto [result, half_carry, carry] = _addition_8bit(this->A, register_value);
		// set a register to the result
		this->A = result;
		// flags
		if (result == 0) {
			this->Zf = true;
		}
		else {
			this->Zf = false;
		}
		this->Nf = false;
		this->Hf = half_carry;
		this->Cf = carry;
		// increment program counter once
		this->PC++;
		};

	this->M_operations.push_back(_add_a_r8);
}


void cpu::bit_b_r8(const char& r8) {

	// NOTE: this is a pointer to your class, so it gets passed by value, but that doesn't matter;
	auto get_bit0_of_r8 = [&]() {
		const uint8_t register_value = _get_register_value(r8);
		this->W = register_value & 1;
		this->PC++; // increment program_counter
		};

	auto set_z_flag = [&]() {
		this->Z = ~this->W;
		this->W = 0;
		this->PC++; // increment program_counter
		};


	// pushed backwards due to FIFO
	this->M_operations.push_back(set_z_flag);
	this->M_operations.push_back(get_bit0_of_r8);
	//this->M_operations->push_back();
}

void cpu::call() {
	// get least and most significant byte via program_counter next
	// M2
	auto get_lsb = [&]() {
		this->W = this->_read_memory(this->PC);
		this->PC++;
		};

	// M3
	auto get_msb = [&]() {
		this->Z = this->_read_memory(this->PC);
		this->PC++;
		};

	// M4
	auto decrement_sp = [&]() {
		this->SP = SP - 1; // decrement stack pointer
		};

	// M5
	auto store_pc_msb = [&]() {
		// store the ms byte and ls byte of current pointer counter to stack pointer
		uint8_t pc_msb = (this->PC >> 8) & 0x00ff;

		this->gb_mmu->write_value_to_address(SP, pc_msb);
		this->SP--;
		};

	// M6
	auto store_pc_lsb = [&]() {
		uint8_t pc_lsb = this->PC & 0x00ff;
		this->gb_mmu->write_value_to_address(SP, pc_lsb);
		// set program counter to function
		this->PC = (this->Z << 8) | this->W; // the function 
		};

	this->M_operations.push_back(store_pc_lsb);
	this->M_operations.push_back(store_pc_msb);
	this->M_operations.push_back(decrement_sp);
	this->M_operations.push_back(get_msb);
	this->M_operations.push_back(get_lsb);
}

// cpu constructor
cpu::cpu(mmu& mmu) { // pass by reference
	this->gb_mmu = &mmu; // & refers to actual address to assign to the pointer
}

void cpu::execute_opcode(const uint16_t& opcode) {

	// TODO: pc += 1 after reading opcode (1 byte)

	// big switch case for differnet instructions
	switch (opcode) {
		// TODO: implement
	case 0x31:
		
	}

}
