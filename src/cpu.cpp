#include "cpu.h"
#include <fstream>
#include <iostream>

// TODO: abstract utility later
std::tuple<uint8_t, bool, bool, bool, bool> cpu::_addition_8bit(uint8_t x,
                                                                uint8_t y) {
    // return result, zero, sub, half_carry, full_carry
    uint8_t result = x + y;
    bool zero = result == 0;
    bool half_carry =
        (x & 0xf) + (y & 0xf) > 0xf; // see if 4 bit sum fits in 4 bits
    bool full_carry = static_cast<uint16_t>(x) + static_cast<uint16_t>(y) >
                      0xff; // see if added numbers > 8 bits
    return {result, zero, false, half_carry, full_carry};
}

std::tuple<uint8_t, bool, bool, bool, bool>
cpu::_subtraction_8bit(const uint8_t x, const uint8_t y) {
    // return result, zero, sub, half_carry, full_carry
    uint8_t result = x - y;
    bool zero = result == 0;
    bool half_carry =
        static_cast<int8_t>(x & 0xf) - static_cast<int8_t>(y & 0xf) < 0;
    bool full_carry = x < y;
    return {result, zero, true, half_carry, full_carry};
}

std::tuple<uint16_t, bool, bool, bool, bool> cpu::_addition_16bit(uint16_t x,
                                                                  uint16_t y) {
    uint8_t result = x + y;
    bool zero = result == 0;
    bool half_carry =
        (x & 0xfff) + (y & 0xfff) > 0xfff; // see if 4 bit sum fits in 4 bits
    bool full_carry =
        static_cast<uint32_t>(x) + static_cast<uint32_t>(y) > 0xffff;
    return {result, zero, false, half_carry, full_carry};
}

uint8_t cpu::_flags_to_byte() const {
    uint8_t Z = this->Zf ? 1 : 0;
    uint8_t N = this->Nf ? 1 : 0;
    uint8_t H = this->Hf ? 1 : 0;
    uint8_t C = this->Cf ? 1 : 0;
    return (Z << 7) | (N << 6) | (H << 5) | (C << 4);
}

// TODO: all pairs can be converted to const expr inside constructor ? maybe not
uint16_t cpu::_combine_2_8bits(const uint8_t &msb, const uint8_t &lsb) {
    return (msb << 8) | lsb;
}

std::tuple<uint8_t, uint8_t> cpu::_split_16bit(const uint16_t &r16) {
    uint8_t msb = (r16 >> 8) & 0x00ff; // msb
    uint8_t lsb = r16 & 0x00ff;        // lsb

    return {msb, lsb};
    // return (r1 << 8) | r2;
}

uint8_t *cpu::_get_register(const registers &r8) {
    uint8_t *register_value{};
    switch (r8) {
    case registers::A: {
        register_value = &this->A;
        break;
    }
    case registers::B: {
        register_value = &this->B;
        break;
    }
    case registers::C: {
        register_value = &this->C;
        break;
    }
    case registers::D: {
        register_value = &this->D;
        break;
    }
    case registers::E: {
        register_value = &this->E;
        break;
    }
    case registers::H: {
        register_value = &this->H;
        break;
    }
    case registers::L: {
        register_value = &this->L;
        break;
    }
    default:
        break;
    }

    return register_value;
}

uint8_t cpu::_read_memory(const uint16_t &address) {
    return this->gb_mmu->get_value_from_address(address);
}

void cpu::_write_memory(const uint16_t &address, const uint8_t &value) {
    this->gb_mmu->write_value_to_address(address, value);
}

// abstracted M-operations

// TODO: combine with add_a_r8 later on
void cpu::add_hl() {
    // TODO: if capture (this) goes out of scope it could cause crashes
    auto _add_hl = [=]() {
        const uint16_t hl = this->_combine_2_8bits(this->H, this->L);
        this->W =
            this->_read_memory(hl); // assign value of HL to temp register W
        // 1 M-cycle - add HL value to register A
        auto [result, z, n, h, c] = _addition_8bit(this->A, this->W);

        // optional: reset W after
        this->W = 0;

        // set a register to the result
        this->A = result;

        // flags
        this->Zf = z;
        this->Nf = n;
        this->Hf = h;
        this->Cf = c;
    };

    // pushed backwards due to FIFO
    this->M_operations.push_back(_add_hl);
}

// TODO: use enum instead of char
void cpu::add_a_r8(const registers &r8) {
    const uint8_t register_value = *(this->_get_register(r8));
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

void cpu::bit_b_r8(const registers &r8, uint8_t b) {
    // bit b
    // NOTE: this is a pointer to your class, so it gets passed by value, but
    // that doesn't matter;
    auto get_bit_of_r8 = [=]() {
        const uint8_t *register_value = _get_register(r8);

        // set flags
        this->Zf = ((*register_value >> b) & 1) == 0;
        this->Nf = false;
        this->Hf = true;
    };

    this->M_operations.push_back(get_bit_of_r8);
}

void cpu::call(const bool &check_z_flag) {
    // get least and most significant byte via program_counter next
    // M2
    auto get_lsb = [=]() {
        this->W = this->_read_memory(this->PC);
        this->PC++;
    };

    // M3
    auto get_msb = [=]() {
        this->Z = this->_read_memory(this->PC);
        this->PC++;
    };

    // M4
    auto decrement_sp = [=]() {
        this->SP = SP - 1; // decrement stack pointer
    };

    // M5
    auto store_pc_msb = [=]() {
        // store the ms byte and ls byte of current pointer counter to stack
        // pointer
        uint8_t pc_msb = (this->PC >> 8) & 0x00ff;

        this->_write_memory(SP, pc_msb);
        this->SP--;
    };

    // M6
    auto store_pc_lsb = [=]() {
        uint8_t pc_lsb = this->PC & 0x00ff;
        this->_write_memory(SP, pc_lsb);
        // set program counter to function
        this->PC = (this->Z << 8) | this->W; // the function
    };

    // push backwards because of FIFO
    if (!check_z_flag || !this->Zf) {
        this->M_operations.push_back(store_pc_lsb);
        this->M_operations.push_back(store_pc_msb);
        // if we don't need to check z flag, keep going; else check that Z flag
        // is false
        this->M_operations.push_back(decrement_sp);
    }
    this->M_operations.push_back(get_msb);
    this->M_operations.push_back(get_lsb);
}

void cpu::cp_a_imm8() {
    auto read_imm8 = [=]() {
        this->W = this->_read_memory(this->PC); // imm8 stored in W
        this->PC++;

        // sub_and_set_flags
        auto [result, z, n, h, c] = this->_subtraction_8bit(this->A, this->W);
        // this->W = 0; // reset W (maybe not necessary)

        // TODO: M3/M1 - overlaps with the fetch of the next operation, should
        // occur 4 ticks after the last op, how to handle? set flags
        this->Zf = z;
        this->Nf = n;
        this->Hf = h;
        this->Cf = c;
    };

    this->M_operations.push_back(read_imm8);
}

void cpu::cp_a_hl() {
    // TODO: check
    auto read_hl = [=]() {
        uint16_t hl = _combine_2_8bits(this->H, this->L);
        this->W = this->_read_memory(hl); // memory[hl] stored in W

        // sub an set flags
        auto [result, z, n, h, c] = this->_subtraction_8bit(this->A, this->W);

        // set flags
        this->Zf = z;
        this->Nf = n;
        this->Hf = h;
        this->Cf = c;
    };

    // push backwards because of FIFO
    this->M_operations.push_back(read_hl);
}

void cpu::inc_or_dec_r8(const registers &r8, const bool &inc) {
    uint8_t *register_pointer = this->_get_register(r8);

    if (inc) {
        auto [result, z, n, h, c] = this->_addition_8bit(*register_pointer, 1);
        *register_pointer = result; // modifies the appropriate register pointer
        this->Zf = z;
        this->Nf = n;
        this->Hf = h;
    } else if (!inc) {
        auto [result, z, n, h, c] =
            this->_subtraction_8bit(*register_pointer, 1);
        *register_pointer = result; // modifies the appropriate register pointer
        this->Zf = z;
        this->Nf = n;
        this->Hf = h;
    }
}

void cpu::inc_or_dec_r16(const registers &r1, const registers &r2,
                         const bool &inc, const bool &sp) {
    auto _inc_or_dec_r16 = [=]() {
        if (sp && inc) {
            this->SP++;
        } else if (sp && !inc) {
            this->SP--;
        } else {
            uint8_t *r1_pointer = this->_get_register(r1);
            uint8_t *r2_pointer = this->_get_register(r2);

            uint16_t current_value = _combine_2_8bits(*r1_pointer, *r2_pointer);

            if (inc) {
                current_value++;
            } else if (!inc) {
                current_value--;
            }

            auto [r1, r2] = _split_16bit(current_value);
            *r1_pointer = r1;
            *r2_pointer = r2;
        }
    };

    this->M_operations.push_back(_inc_or_dec_r16);
}

void cpu::jp_imm16(const bool &check_z_flag) {
    // get least and most significant byte via program_counter next
    auto get_lsb = [=]() {
        this->W = this->_read_memory(this->PC);
        this->PC++;
    };

    auto get_msb = [=]() {
        this->Z = this->_read_memory(this->PC);
        this->PC++;
    };

    auto set_pc = [=]() {
        this->PC = (this->Z << 8) | this->W; // the function
    };

    if (!check_z_flag || !this->Zf) {
        this->M_operations.push_back(set_pc);
    }
    this->M_operations.push_back(get_msb);
    this->M_operations.push_back(get_lsb);
}

void cpu::jr_s8(const bool &check_z_flag, const bool &nz) {
    // jump relative to signed 8 bit next in memory
    auto get_value = [=]() {
        this->Z = this->_read_memory(this->PC);
        this->PC++;
    };

    auto set_pc = [=]() {
        // 1001 0010
        int8_t value = this->Z;
        this->PC += value;
    };

    if (!check_z_flag || ((nz && !this->Zf) || (!nz && this->Zf))) {
        // nz jump if flag is 0, !nz jump if Z flag is 1
        this->M_operations.push_back(set_pc);
    }

    this->M_operations.push_back(get_value);
}

void cpu::ld_imm16_a(const bool &to_a) {
    auto m1 = [=]() {
        this->Z = this->_read_memory(PC); // lsb
        PC++;
    };

    auto m2 = [=]() {
        this->W = this->_read_memory(PC); // msb
        PC++;
    };

    auto m3 = [=]() {
        uint16_t address = this->_combine_2_8bits(W, Z);
        if (!to_a) {
            this->_write_memory(address, this->A); // write A to address
        } else {
            this->A =
                this->_read_memory(address); // write value from address to A
        }
    };

    this->M_operations.push_back(m3);
    this->M_operations.push_back(m2);
    this->M_operations.push_back(m1);
}

void cpu::ld_r_r(const registers &r_to, const registers &r_from) {
    uint8_t *register_pointer_to = _get_register(r_to);
    uint8_t *register_pointer_from = _get_register(r_from);
    *register_pointer_to = *register_pointer_from;
}

void cpu::ld_r_imm8(const registers &r) {
    auto m1 = [=]() {
        uint8_t *register_pointer = this->_get_register(r);
        uint8_t value = this->_read_memory(this->PC);
        *register_pointer = value;
        this->PC++;
    };

    this->M_operations.push_back(m1);
}

void cpu::ld_rr_address(const registers &r1, const registers &r2,
                        const bool &sp) {
    auto m1 = [=]() {
        this->Z = this->_read_memory(PC); // lsb
        this->PC++;
        this->W = this->_read_memory(PC); // msb
        this->PC++;
    };

    auto f_sp = [=]() { this->SP = this->_combine_2_8bits(W, Z); };

    auto non_sp = [=]() {
        uint8_t *register_pointer_1 = this->_get_register(r1);
        uint8_t *register_pointer_2 = this->_get_register(r2);

        *register_pointer_1 = this->W; // r1 - msb
        *register_pointer_2 = this->Z; // r2 - lsb
    };

    if (sp) {
        this->M_operations.push_back(f_sp);
    } else {
        this->M_operations.push_back(non_sp);
    }
    this->M_operations.push_back(m1);
}

void cpu::ld_hl_a(const bool &increment) {
    auto m1 = [=]() {
        uint16_t address = this->_combine_2_8bits(this->H, this->L);

        this->_write_memory(address, this->A);

        if (increment) {
            address++;
        } else {
            address--;
        }
        auto [h, l] = this->_split_16bit(address);
        this->H = h;
        this->L = l;
    };

    this->M_operations.push_back(m1);
} // covers hl+ and hl-

void cpu::ld_hl_r8(const registers &r) {
    auto m1 = [=]() {
        uint16_t address = this->_combine_2_8bits(this->H, this->L);
        uint8_t *rp = this->_get_register(r);
        this->_write_memory(address, *rp);
    };

    this->M_operations.push_back(m1);
}

void cpu::pop_rr(const registers &r1, const registers &r2) {
    auto m1 = [=]() {
        this->Z = _read_memory(this->SP); // lsb
        this->SP++;
    };

    auto m2 = [=]() {
        this->W = _read_memory(this->SP); // msb
        this->SP++;

        uint8_t *r_pointer1 = _get_register(r1);
        uint8_t *r_pointer2 = _get_register(r2);

        *r_pointer1 = W;
        *r_pointer2 = Z;
    };

    this->M_operations.push_back(m2);
    this->M_operations.push_back(m1);
}

void cpu::push_rr(const registers &r1, const registers &r2) {
    auto m1 = [=]() { this->SP--; };

    auto m2 = [=]() {
        auto r1_p = this->_get_register(r1);
        this->_write_memory(this->SP, *r1_p);
        this->SP--;
    };

    auto m3 = [=]() {
        auto r2_p = this->_get_register(r2);
        this->_write_memory(this->SP, *r2_p);
    };

    this->M_operations.push_back(m3);
    this->M_operations.push_back(m2);
    this->M_operations.push_back(m1);
}

void cpu::ret() {

    auto m1 = [=]() {
        this->Z = _read_memory(this->SP);
        this->SP++;
    };

    auto m2 = [=]() {
        this->W = _read_memory(this->SP);
        this->SP++;
    };

    auto m3 = [=]() { this->PC = this->_combine_2_8bits(W, Z); };

    this->M_operations.push_back(m3);
    this->M_operations.push_back(m2);
    this->M_operations.push_back(m1);
}

void cpu::rla() {
    auto m1 = [=]() {
        uint8_t msbit = (this->A >> 7) & 1;    // save the "carry" bit
        uint8_t carry_flag = this->Cf ? 1 : 0; // take current carry flag
        this->A = this->A << 1;                // left shift A by 1 bit

        this->A = (this->A & 0xfe) | carry_flag; // put carry_flag into bit 0

        // set flags
        this->Zf = false;
        this->Nf = false;
        this->Hf = false;

        if (msbit == 0) {
            this->Cf = false;
        } else if (msbit == 1) {
            this->Cf = true;
        }
    };

    this->M_operations.push_back(m1);
}

void cpu::sla_r(const registers &r) {
    auto m1 = [=]() {
        uint8_t *rp =
            this->_get_register(r); // r falls out of scope for some reason
        // uint8_t* rp = &this->B;

        uint8_t msbit = (*rp >> 7) & 1;        // save the "carry" bit
        uint8_t carry_flag = this->Cf ? 1 : 0; // take current carry flag
        *rp <<= 1;                             // left shift A by 1 bit
        // bit 0 is reset to 0

        // set carry flag to msbit
        if (msbit == 0) {
            this->Cf = false;
        } else if (msbit == 1) {
            this->Cf = true;
        }
    };

    this->M_operations.push_back(m1);
}

void cpu::rl_r(const registers &r) {
    auto m1 = [=]() {
        auto rp = this->_get_register(r);
        uint8_t msbit = (*rp >> 7) & 1;
        uint8_t carry_flag = this->Cf ? 1 : 0;
        *rp <<= 1;
        // clear last bit first, then or it with the carry flag
        *rp = (*rp & 0xfe) | carry_flag;

        // set flags
        this->Zf = *rp == 0;
        this->Nf = false;
        this->Hf = false;

        if (msbit == 0) {
            this->Cf = false;
        } else if (msbit == 1) {
            this->Cf = true;
        }
    };

    this->M_operations.push_back(m1);
}

void cpu::sub_r(const registers &r) {
    auto p = this->_get_register(r);
    auto [result, z, n, h, c] = this->_subtraction_8bit(this->A, *p);
    this->A = result;

    // set flags
    this->Zf = z;
    this->Nf = n;
    this->Hf = h;
    this->Cf = c;
}

void cpu::xor_r(const registers &r) {
    auto p = this->_get_register(r);
    uint8_t result = this->A ^ *p;
    this->A = result;

    // set flags
    this->Zf = result == 0;
    this->Nf = false;
    this->Hf = false;
    this->Cf = false;
}

void cpu::ld_c_a(const bool &to_a) {
    auto m1 = [=]() {
        const uint16_t address = this->C | 0xff00;
        if (to_a) {
            this->A = this->_read_memory(address);
        } else {
            this->_write_memory(address, this->A);
        }
    };

    this->M_operations.push_back(m1);
}

void cpu::ld_imm8_a(const bool &to_a) {
    // read imm8
    auto m1 = [=]() {
        this->Z = _read_memory(this->PC);
        this->PC++;
    };

    auto m2 = [=]() {
        const uint16_t address = this->Z | 0xff00;
        if (to_a) {
            this->A = _read_memory(address);
        } else {
            this->_write_memory(address, this->A);
        }
    };

    this->M_operations.push_back(m2);
    this->M_operations.push_back(m1);
}

void cpu::ld_rr_a(const registers &r1, const registers &r2) {
    // read imm8
    auto m1 = [=]() {
        uint8_t *r1_p = _get_register(r1);
        uint8_t *r2_p = _get_register(r2);
        uint16_t address = this->_combine_2_8bits(*r1_p, *r2_p);
        this->A = _read_memory(address);
    };

    this->M_operations.push_back(m1);
}

uint8_t cpu::identify_opcode(uint8_t opcode) {
    this->PC++;            // increment program counter
    handle_opcode(opcode); // handle the opcode
    if (!this->M_operations.empty()) {
        this->fetch_opcode = false; // going past fetch opcode,
    }
    return opcode;
}

void cpu::execute_M_operations() {
    // execute any further instructions
    if (!this->M_operations.empty()) {
        std::function<void()> operation = this->M_operations.back();
        this->M_operations.pop_back();
        // TODO: check if this reference is valid
        operation();
    } else if (this->M_operations.empty()) {
        this->fetch_opcode = true;
    }
}

void cpu::load_boot_rom() {
    // TODO: change path
    std::ifstream file(
        "BOOT/dmg_boot.bin",
        std::ios::binary |
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
        // this->PC = 0; // initialize program counter
    }
    // this->gb_mmu->complete_boot();
}

// cpu constructor
cpu::cpu(mmu &mmu) {
    // pass by reference
    this->gb_mmu = &mmu; // & refers to actual address to assign to the pointer
}

int cpu::handle_opcode(const uint8_t &opcode) {
    // big switch case for differnet instructions
    switch (opcode) {
    // TODO: implement
    case 0x31:
        ld_rr_address(registers::NA, registers::NA, true); // LD SP, r16
        break;

    case 0xaf:
        xor_r(registers::A); // XOR A
        break;

    case 0x21:
        ld_rr_address(registers::H, registers::L, false); // LD HL imm16
        break;

    case 0x32:
        ld_hl_a(false); // LD HL- A
        break;

    case 0xcb: {
        // extended set of instructions
        const uint8_t cb_opcode = this->_read_memory(this->PC);
        this->PC++; // increment the program counter
        this->handle_cb_opcode(cb_opcode);
        break;
    }

    case 0x20: {
        jr_s8(true, true); // JR Z imm16
        break;
    }

    case 0x0e: {
        ld_r_imm8(registers::C); // LD C, imm8
        break;
    }

    case 0x3e: {
        ld_r_imm8(registers::A); // LD A, imm8
        break;
    }

    case 0xe2: {
        ld_c_a(false); // LD (C), A, or LDH (C) A
        break;
    }

    case 0x0c: {
        inc_or_dec_r8(registers::C, true); // INC C
        break;
    }

    case 0x77: {
        ld_hl_r8(registers::A); // LD HL A
        break;
    }

    case 0xe0: {
        // LD (imm8) A
        ld_imm8_a(false); // LDH (n) A, or LD (imm8) A
        break;
    }

    case 0x11: {
        // LD DE, imm16
        ld_rr_address(registers::D, registers::E, false);
        break;
    }

    case 0x1a: {
        // LD A, (DE)
        ld_rr_a(registers::D, registers::E);
        break;
    }

    case 0xcd: {
        call(false); // CALL $0095
        break;
    }

    case 0x4f: {
        ld_r_r(registers::C, registers::A); // LD C, A
        break;
    }

    case 0x06: {
        ld_r_imm8(registers::B); // LD B, imm8
        break;
    }

    case 0xc5: {
        push_rr(registers::B, registers::C); // PUSH BC
        break;
    }

    case 0x17: {
        rla(); // RLA
        break;
    }

    case 0xc1: {
        // POP BC
        pop_rr(registers::B, registers::C);
        break;
    }

    case 0x05: {
        // DEC B
        inc_or_dec_r8(registers::B, false);
        break;
    }

    case 0x22: {
        // LD HL+ A
        ld_hl_a(true);
        break;
    }

    case 0x23: {
        // INC HL
        inc_or_dec_r16(registers::H, registers::L, true, false);
        break;
    }

    case 0xc9: {
        // RET
        ret();
        break;
    }

    case 0x13: {
        // INC DE
        inc_or_dec_r16(registers::D, registers::E, true, false);
        break;
    }

    case 0x7b: {
        // LD A, E
        ld_r_r(registers::A, registers::E);
        break;
    }

    case 0xfe: {
        // CP A, imm8
        cp_a_imm8();
        break;
    }

    case 0xea: {
        // LD imm16 A
        ld_imm16_a(false);
        break;
    }

    case 0x3d: {
        // DEC A
        inc_or_dec_r8(registers::A, false);
        break;
    }

    case 0x28: {
        // JR Z, s8
        jr_s8(true, false);
        break;
    }

    case 0x0d: {
        // DEC C
        inc_or_dec_r8(registers::C, false);
        break;
    }

    case 0x2e: {
        // LD L, imm8
        ld_r_imm8(registers::L);
        break;
    }

    case 0x18: {
        // JR 8
        jr_s8(false, false);
        break;
    }

    case 0x67: {
        // ld h, a
        ld_r_r(registers::H, registers::A);
        break;
    }

    case 0x57: {
        // LD D A
        ld_r_r(registers::D, registers::A);
        break;
    }

    case 0x04: {
        // INC B
        inc_or_dec_r8(registers::B, true);
        break;
    }

    case 0x1e: {
        // LD E, imm8
        ld_r_imm8(registers::E);
        break;
    }

    case 0xf0: {
        // LD A (imm8)
        ld_imm8_a(true);
        break;
    }

    case 0x1d: {
        // DEC E
        inc_or_dec_r8(registers::E, false);
        break;
    }

    case 0x24: {
        // INC H
        inc_or_dec_r8(registers::H, true);
        break;
    }

    case 0x7c: {
        // LD A, H
        ld_r_r(registers::A, registers::H);
        break;
    }

    case 0x90: {
        // SUB B
        sub_r(registers::B);
        break;
    }

    case 0x15: {
        // DEC D
        inc_or_dec_r8(registers::D, false);
        break;
    }

    case 0x16: {
        // LD D, imm8
        ld_r_imm8(registers::D);
        break;
    }

    case 0xbe: {
        // CP A HL
        cp_a_hl();
        break;
    }

    case 0x7d: {
        ld_r_r(registers::A, registers::L);
        break;
    }

    case 0x78: {
        ld_r_r(registers::A, registers::B);
        break;
    }

    case 0x86: {
        add_hl();
        break;
    }

    default:
        // print out the opcodes we didnt' implement
        std::cout << "not implemented: " //<< static_cast<unsigned int>(opcode)
                  << "hex: 0x" << std::hex << static_cast<unsigned int>(opcode)
                  << std::endl;
        // system("pause");
        std::cin.get();
        return 0;
        break;
    }

    return 1;
}

int cpu::handle_cb_opcode(const uint8_t &cb_opcode) {
    switch (cb_opcode) {
    case 0x7c:
        bit_b_r8(registers::H, 7); // BIT 7 H
        break;

    case 0x11: {
        rl_r(registers::C); // RL C
        break;
    }

        /*
        case 0x20:
                sla_r(registers::B); //sla b, TODO: B not working
                break;
        */

    default:
        // print out the opcodes we didnt' implement
        std::cout << "not implemented extended (cb): " << std::hex
                  << static_cast<unsigned int>(cb_opcode) << std::endl;
        // system("pause");
        std::cin.get();
        return 0;
        break;
    }

    return 1;
}

void cpu::tick() {
    this->ticks++;

    // inject ppu tick here
    // TODO: if ppu tick resets, defer the tick reset to 0 until after CPU does its thing

    if (ticks < 4) {
        return;
    }

    this->ticks = 0; // reset ticks

    if (this->fetch_opcode) {
        // fetch opcode, then execute what you can this M-cycle
        const uint8_t opcode =
            this->_read_memory(this->PC); // get current opcode
        identify_opcode(opcode);

    }
    else {
        // execute any further instructions
        this->execute_M_operations();
    }
}
