#include "cpu.h"
#include <iostream>
#include <fstream>

// TODO: abstract utility later
std::tuple<uint8_t, bool, bool, bool, bool> cpu::_addition_8bit(uint8_t x, uint8_t y) {
	// return result, zero, sub, half_carry, full_carry
	uint8_t result = x + y;
	bool zero = result == 0;
	bool half_carry = (x & 0xf) + (y & 0xf) > 0xf; // see if 4 bit sum fits in 4 bits
	bool full_carry = static_cast<uint16_t>(x) + static_cast<uint16_t>(y) > 0xff; // see if added numbers > 8 bits
	return { result, zero, false, half_carry, full_carry };
}

std::tuple<uint8_t, bool, bool, bool, bool> cpu::_subtraction_8bit(uint8_t x, uint8_t y) {
	// return result, zero, sub, half_carry, full_carry
	uint8_t result = x - y;
	bool zero = result == 0;
	bool half_carry = static_cast<int8_t>(x & 0xf) - static_cast<int8_t>(y & 0xf) < 0;
	bool full_carry = x < y;
	return { result, zero, true, half_carry, full_carry };
}

std::tuple<uint16_t, bool, bool, bool, bool> cpu::_addition_16bit(uint16_t x, uint16_t y) {
	uint8_t result = x + y;
	bool zero = result == 0;
	bool half_carry = (x & 0xfff) + (y & 0xfff) > 0xfff; // see if 4 bit sum fits in 4 bits
	bool full_carry = static_cast<uint32_t>(x) + static_cast<uint32_t>(y) > 0xffff;
	return { result, zero, false, half_carry, full_carry };
}

uint8_t cpu::_flags_to_byte() const {
	uint8_t Z = this->Zf ? 1 : 0;
	uint8_t N = this->Nf ? 1 : 0;
	uint8_t H = this->Hf ? 1 : 0;
	uint8_t C = this->Cf ? 1 : 0;
	return (Z << 7) | (N << 6) | (H << 5) | (C << 4);
}

// TODO: all pairs can be converted to const expr inside constructor ? maybe not
uint16_t cpu::_combine_2_8bits(const uint8_t& msb, const uint8_t& lsb) {
	return (msb << 8) | lsb;
}

std::tuple<uint8_t, uint8_t> cpu::_split_16bit(const uint16_t& r16) {

	uint8_t msb = (r16 >> 8) & 0x00ff; // msb
	uint8_t lsb = r16 & 0x00ff; // lsb

	return { msb, lsb };
	//return (r1 << 8) | r2;
}

uint8_t* cpu::_get_register(const char& r8) {
	uint8_t* register_value{};
	switch (r8) {
	case 'A':
	{
		register_value = &this->A;
		break;
	}
	case 'B': {
		register_value = &this->B;
		break;
	}
	case 'C': {
		register_value = &this->C;
		break;
	}
	case 'D': {
		register_value = &this->D;
		break;
	}
	case 'E': {
		register_value = &this->E;
		break;
	}
	}

	return register_value;
}

uint8_t cpu::_read_memory(const uint16_t& address) {
	return this->gb_mmu->get_value_from_address(address);
}

void cpu::_write_memory(const uint16_t& address, const uint8_t& value) {
	this->gb_mmu->write_value_to_address(address, value);
}

// abstracted M-operations

// TOOD: combine with add_a_r8 later on
void cpu::add_hl() {

	// TODO: if capture (this) goes out of scope it could cause crashes
	auto _add_hl = [&]() {
		uint16_t hl = this->_combine_2_8bits(this->H, this->L);
		this->W = this->_read_memory(hl); // assign value of HL to temp register W
		this->PC++; // increment PC
		// 1 M-cycle - add HL value to register A
		auto [result, z, n, h, c] = _addition_8bit(this->A, this->W);

		// optional: reset W after
		this->W = 0;

		// set a register to the result
		this->A = result;

		// flags
		this->Zf = z; this->Nf = n; this->Hf = h; this->Cf = c;

		this->PC++; // increment PC
		};


	// pushed backwards due to FIFO
	this->M_operations.push_back(_add_hl);
}

// TODO: use enum instead of char
void cpu::add_a_r8(const char& r8) {

	uint8_t register_value = *(this->_get_register(r8));
	// 1 M-cycle - add value in register_value to a
	auto [result, z, n, h, c] = _addition_8bit(this->A, register_value);
	// set a register to the result
	this->A = result;
	// flags
	this->Zf = z;
	this->Nf = n;
	this->Hf = h;
	this->Cf = c;
	// increment program counter once
	this->PC++;
}

void cpu::bit_b_r8(const char& r8, uint8_t b) { // b is 0 or 1
	// NOTE: this is a pointer to your class, so it gets passed by value, but that doesn't matter;
	auto get_bit0_of_r8 = [&]() {
		const uint8_t register_value = *_get_register(r8);
		if (b == 0) {
			this->W = register_value & 1; // get 0 bit
		}
		else if (b == 1) {
			this->W = (register_value >> 1) & 1; // get 1 bit
		}
		this->PC++; // increment program_counter

		// set flags
		this->Zf = (this->W == 0) ? true : false;
		this->Hf = 1;
		this->W = 0; // optional (reset Zf)
		this->PC++; // increment program_counter
		};

	this->M_operations.push_back(get_bit0_of_r8);
}

void cpu::call(bool check_z_flag) {
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

		this->_write_memory(SP, pc_msb);
		this->SP--;
		};

	// M6
	auto store_pc_lsb = [&]() {
		uint8_t pc_lsb = this->PC & 0x00ff;
		this->_write_memory(SP, pc_lsb);
		// set program counter to function
		this->PC = (this->Z << 8) | this->W; // the function 
		};

	// push backwards because of FIFO
	if (!check_z_flag || !this->Zf) {
		this->M_operations.push_back(store_pc_lsb);
		this->M_operations.push_back(store_pc_msb);
		// if we don't need to check z flag, keep going; else check that Z flag is false
		this->M_operations.push_back(decrement_sp);
	}
	this->M_operations.push_back(get_msb);
	this->M_operations.push_back(get_lsb);

}

void cpu::cp_a_imm8() {

	auto read_imm8 = [&]() {
		this->W = this->_read_memory(this->PC); // imm8 stored in W
		this->PC++;

		// sub_and_set_flags
		auto [result, z, n, h, c] = this->_subtraction_8bit(this->A, this->W);
		this->W = 0; // reset W (maybe not necessary)

		// TODO: M3/M1 - overlaps with the fetch of the next operation, should occur 4 ticks after the last op, how to handle?
		// set flags
		this->Zf = z;
		this->Nf = n;
		this->Hf = h;
		this->Cf = c;
		};

	this->M_operations.push_back(read_imm8);
}

void cpu::cp_a_hl() {
	// TODO: check
	auto read_hl = [&]() {
		uint16_t hl = _combine_2_8bits(this->H, this->L);
		this->W = this->_read_memory(hl); // memory[hl] stored in W
		this->PC++;

		// sub an set flags 
		auto [result, z, n, h, c] = this->_subtraction_8bit(this->A, this->W);
		this->W = 0; // reset W (maybe not necessary)

		// set flags
		this->Zf = z;
		this->Nf = n;
		this->Hf = h;
		this->Cf = c;
		};

	// push backwards because of FIFO
	this->M_operations.push_back(read_hl);
}

void cpu::inc_or_dec_r8(const char& r8, bool inc) {
	uint8_t* register_pointer = this->_get_register(r8);

	if (inc) {
		auto [result, z, n, h, c] = this->_addition_8bit(*register_pointer, 1);
		*register_pointer = result; // modifies the appropriate register pointer
		this->Zf = z;
		this->Nf = n;
		this->Hf = h;
	}
	else if (!inc) {
		auto [result, z, n, h, c] = this->_subtraction_8bit(*register_pointer, 1);
		*register_pointer = result; // modifies the appropriate register pointer
		this->Zf = z;
		this->Nf = n;
		this->Hf = h;
	}
}

void cpu::inc_or_dec_r16(const char& r1, const char& r2, bool inc, bool sp) {
	auto _inc_or_dec_r16 = [&]() {
		if (sp && inc) {
			this->SP++;
		}
		else if (sp && !inc) {
			this->SP--;
		}

		else {
			uint8_t* r1_pointer = this->_get_register(r1);
			uint8_t* r2_pointer = this->_get_register(r2);

			uint16_t current_value = _combine_2_8bits(r1, r2);

			if (inc) {
				current_value++;
			}
			else if (!inc) {
				current_value--;
			}

			auto [r1, r2] = _split_16bit(current_value);
			*r1_pointer = r1;
			*r2_pointer = r2;
		}
		};

	this->M_operations.push_back(_inc_or_dec_r16);
}

void cpu::jp_imm16(bool check_z_flag) {
	// get least and most significant byte via program_counter next
	auto get_lsb = [&]() {
		this->W = this->_read_memory(this->PC);
		this->PC++;
		};

	auto get_msb = [&]() {
		this->Z = this->_read_memory(this->PC);
		this->PC++;
		};

	auto set_pc = [&]() {
		this->PC = (this->Z << 8) | this->W; // the function 
		};

	if (!check_z_flag || !this->Zf) {
		this->M_operations.push_back(set_pc);
	}
	this->M_operations.push_back(get_msb);
	this->M_operations.push_back(get_lsb);
}

void cpu::jr_imm16(bool check_z_flag) {
	// TODO: check
	// get least and most significant byte via program_counter next
	auto get_value = [&]() {
		this->Z = this->_read_memory(this->PC);
		this->PC++;
		};

	auto set_pc = [&]() {
		uint8_t Z_sign = (this->Z >> 7) & 1;
		auto [msb, lsb] = _split_16bit(this->PC);

		auto [result, z, n, h, c] = _addition_8bit(this->Z, lsb);
		this->Z = result;
		uint8_t adj{ 0 };
		if (c && Z_sign == 0) {
			adj = 1;
		}
		else if (!c && Z_sign == 1) {
			adj = -1;
		}
		W = msb + adj;
		this->PC = _combine_2_8bits(W, Z);
		};

	if (!check_z_flag || !this->Zf) {
		this->M_operations.push_back(set_pc);
	}
	this->M_operations.push_back(get_value);
}

void cpu::ld_imm16_a(bool to_a) {
	auto m1 = [&]() {
		auto [msb, lsb] = this->_split_16bit(this->PC);
		this->W = msb;
		this->Z = lsb;
		PC++;
		};

	auto m2 = [&]() {
		PC++;
		};

	auto m3 = [&]() {
		uint16_t address = this->_combine_2_8bits(W, Z);
		if (to_a) {
			this->_write_memory(address, this->A); // write A to address
		}
		else {
			this->A = this->_read_memory(address); // write value from address to A
		}
		};

	this->M_operations.push_back(m3);
	this->M_operations.push_back(m2);
	this->M_operations.push_back(m1);
}

void cpu::ld_r_r(const char& r_to, const char& r_from) {
	uint8_t* register_pointer_to = _get_register(r_to);
	uint8_t* register_pointer_from = _get_register(r_from);
	*register_pointer_to = *register_pointer_from;
}

void cpu::ld_r_imm8(const char& r) {

	auto m1 = [&]() {
		uint8_t* register_pointer = this->_get_register(r);
		uint8_t value = this->_read_memory(this->PC);
		*register_pointer = value;
		this->PC++;
		};

	this->M_operations.push_back(m1);
}

void cpu::ld_rr_address(const char& r1, const char& r2, bool sp) {

	auto m1 = [&]() {
		uint8_t msb = this->_read_memory(PC);
		this->W = msb;
		this->PC++;
		uint8_t lsb = this->_read_memory(PC);
		this->Z = lsb;
		};

	auto m2 = [&]() {
		this->PC++;
		if (sp) {
			this->SP = this->_combine_2_8bits(W, Z);
		}
		else {
			uint8_t* register_pointer_1 = this->_get_register(r1);
			uint8_t* register_pointer_2 = this->_get_register(r2);
			*register_pointer_1 = this->W; // msb
			*register_pointer_2 = this->Z; // lsb
		}
		};

	this->M_operations.push_back(m2);
	this->M_operations.push_back(m1);
}

void cpu::ld_hl_a(bool increment) {

	auto m1 = [&]() {
		uint8_t address = this->_combine_2_8bits(this->H, this->L);
		this->_write_memory(address, this->A);
		if (increment) {
			address++;
		}
		else {
			address--;
		}
		auto [h, l] = this->_split_16bit(address);
		this->H = h;
		this->L = l;
		};

	this->M_operations.push_back(m1);
} // covers hl+ and hl-

void cpu::pop_rr(const char& r1, const char& r2) {
	auto m1 = [&]() {
		this->Z = _read_memory(this->SP); // lsb
		this->SP++;
		};

	auto m2 = [&]() {
		this->W = _read_memory(this->SP); // msb
		this->SP++;

		uint8_t* r_pointer1 = _get_register(r1);
		uint8_t* r_pointer2 = _get_register(r2);

		*r_pointer1 = W;
		*r_pointer2 = Z;
		};

	this->M_operations.push_back(m2);
	this->M_operations.push_back(m1);
}

void cpu::push_rr(const char& r1, const char& r2) {
	auto m1 = [&]() {
		this->SP--;
		};

	auto m2 = [&]() {
		auto r1_p = this->_get_register(r1);
		this->_write_memory(this->SP, *r1_p);
		this->SP--;
		};

	auto m3 = [&]() {
		auto r2_p = this->_get_register(r2);
		this->_write_memory(this->SP, *r2_p);
		};

	this->M_operations.push_back(m3);
	this->M_operations.push_back(m2);
	this->M_operations.push_back(m1);
}

void cpu::ret() {
	auto m1 = [&]() {
		this->Z = _read_memory(this->SP);
		this->SP++;
		};

	auto m2 = [&]() {
		this->W = _read_memory(this->SP);
		this->SP++;
		};

	auto m3 = [&]() {
		this->PC = this->_combine_2_8bits(W, Z);
		};

	this->M_operations.push_back(m3);
	this->M_operations.push_back(m2);
	this->M_operations.push_back(m1);
}

// TODO: combine rla with sla
void cpu::rl_a() {
	auto m1 = [&]() {
		uint8_t msbit = (this->A >> 7) & 1; // save the "carry" bit
		uint8_t carry_flag = this->Cf ? 1 : 0; // take current carry flag
		this->A <<= 1; // left shift A by 1 bit
		this->A = this->A | carry_flag; // put carry_flag into bit 0

		// set carry flag to msbit
		if (msbit == 0) {
			this->Cf = false;
		}

		else if (msbit == 1) {
			this->Cf = true;
		}
		};

	this->M_operations.push_back(m1);
}

void cpu::sla_r(const char& r) {

	auto m1 = [&]() {
		uint8_t* rp = this->_get_register(r); // r falls out of scope for some reason
		//uint8_t* rp = &this->B;

		uint8_t msbit = (*rp >> 7) & 1; // save the "carry" bit
		uint8_t carry_flag = this->Cf ? 1 : 0; // take current carry flag
		*rp <<= 1; // left shift A by 1 bit
		// bit 0 is reset to 0

		// set carry flag to msbit
		if (msbit == 0) {
			this->Cf = false;
		}

		else if (msbit == 1) {
			this->Cf = true;
		}
		};

	this->M_operations.push_back(m1);
}

void cpu::rl_r(const char& r) {

	auto m1 = [&]() {
		auto rp = this->_get_register(r);
		uint8_t msbit = (*rp >> 7) & 1;
		uint8_t carry_flag = this->Cf ? 1 : 0;
		*rp <<= 1;
		*rp = *rp | carry_flag;
		// set carry flag
		if (msbit == 0) {
			this->Cf = false;
		}
		else if (msbit == 1) {
			this->Cf = true;
		}
		};

	this->M_operations.push_back(m1);
}

void cpu::sub_r(const char& r) {
	auto p = this->_get_register(r);
	auto [result, z, n, h, c] = this->_subtraction_8bit(this->A, *p);
	this->A = result;

	// set flags
	this->Zf = z;
	this->Nf = n;
	this->Hf = h;
	this->Cf = c;
}

void cpu::xor_r(const char& r) {
	auto p = this->_get_register(r);
	uint8_t result = this->A ^ *p;
	this->A = result;

	// set flags
	this->Zf = result == 0 ? 1 : 0;
	this->Nf = 0;
	this->Hf = 0;
	this->Cf = 0;
}

void cpu::load_boot_rom() {
	// TODO: change path
	std::ifstream file(
		"BOOT/dmg_boot.bin", std::ios::binary |
		std::ios::ate); // read file from end, in binary format

	if (file.is_open()) {
		int size = file.tellg(); // check position of cursor (file size)

		std::vector<char> buffer(
			size); // prepare buffer with size = size of file

		file.seekg(0, std::ios::beg); // move cursor to beginning of file
		file.read(buffer.data(), size);

		for (long i = 0; i < size; ++i) {
			// TODO: make sure rom doesnt take up more space than it should
			this->_write_memory(i, buffer[i]);
		}
		//this->PC = 0; // initialize program counter
	}
	//this->gb_mmu->complete_boot();

}

// cpu constructor
cpu::cpu(mmu& mmu) { // pass by reference
	this->gb_mmu = &mmu; // & refers to actual address to assign to the pointer
}

int cpu::handle_opcode(const uint8_t& opcode) {

	// big switch case for differnet instructions
	switch (opcode) {
		// TODO: implement
	case 0x31:
		ld_rr_address(NULL, NULL, true); // LD SP, r16
		break;

	case 0xaf:
		xor_r('A'); // xor a
		break;

	case 0x21:
		ld_rr_address(this->H, this->L, false);
		break;

	case 0x32:
		ld_hl_a(false); // decrement
		break;

	case 0xcb: {
		// extended set of instructions
		this->PC++;
		uint8_t cb_opcode = this->_read_memory(this->PC);
		this->handle_cb_opcode(cb_opcode);
		break;
	}

	default:
		// print out the opcodes we didnt' implement
		std::cout << "not implemented: " << std::hex << static_cast<unsigned int>(opcode) << std::endl;
		system("pause");
		return 0;
		break;
	}

	return 1;

}

int cpu::handle_cb_opcode(const uint8_t& cb_opcode) {

	switch (cb_opcode) {

	case 0x20:
		sla_r('B'); //sla b, TODO: B not working
		break;

	default:
		// print out the opcodes we didnt' implement
		std::cout << "not implemented extended (cb): " << std::hex << static_cast<unsigned int>(cb_opcode) << std::endl;
		system("pause");
		return 0;
		break;
	}

	return 1;

}



void cpu::tick() {
	this->ticks++;
	if (ticks < 4) {
		return;
	}
	else {
		this->ticks = 0; // reset ticks

		switch (this->fetch_opcode) {
		case true: { // fetch opcode, then execute what you can this M-cycle
			uint8_t opcode = this->_read_memory(this->PC); // get current opcode
			this->PC++; // increment opcode
			handle_opcode(opcode); // handle the opcode

			if (this->M_operations.size() > 0) {
				this->fetch_opcode = false; // going past fetch opcode, executing more instructions
			}
			break;
		}

		case false: { // execute any further instructions 
			if (this->M_operations.size() > 0) {
				std::function<void()> operation = this->M_operations.back();
				this->M_operations.pop_back();
				// TODO: check if this reference is valid
				operation();
			}

			if (this->M_operations.size() == 0) {
				this->fetch_opcode = true;
			}
			break;
		}

		}

	}
}
