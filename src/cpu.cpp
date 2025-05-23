#include "cpu.h"
#include <array>
#include <cassert>
#include <fstream>
#include <iostream>

// TODO: abstract utility later
template <typename... Args>
std::tuple<uint8_t, bool, bool, bool, bool> cpu::_addition_8bit(Args... args) {
    uint8_t result = 0;
    // Iterate over all arguments and add them to the result
    result += (... + args);
    // Check if the result is zero
    bool zero = result == 0;
    bool half_carry =
        ((args & 0xf) + ...) > 0xf; // see if 4 bit sum fits in 4 bits
    bool full_carry = (static_cast<uint16_t>(args) + ...) > 0xff;
    return {result, zero, false, half_carry, full_carry};
}

std::tuple<uint16_t, bool, bool, bool, bool>
cpu::_addition_16bit(uint16_t x, uint16_t y, bool s8) {

    uint16_t Hf_sum = 0xfff;
    if (s8) {
        Hf_sum = 0xf;
    }

    bool full_carry{};
    bool half_carry{};

    uint16_t result{};
    if (s8) {
        int8_t imm8 = static_cast<int8_t>(y);
        result = x + imm8;
    } else {
        result = x + y;
    }

    bool zero = result == 0;

    half_carry = (x & Hf_sum) + (y & Hf_sum) > Hf_sum;

    if (!s8) {
        full_carry = static_cast<uint32_t>(x) + static_cast<uint32_t>(y) >
                     0xffff; // see if added numbers > 16 bits
    } else {
        full_carry = (x & 0xff) + (y & 0xff) > 0xff;
    }

    return {result, zero, false, half_carry, full_carry};
}

std::tuple<uint8_t, bool, bool, bool, bool>
cpu::_subtraction_8bit(uint8_t x, uint8_t y, uint8_t Cf) {
    // return result, zero, sub, half_carry, full_carry
    uint8_t result = x - y - Cf;
    bool zero = result == 0;
    bool half_carry = (x & 0xf) < (y & 0xf) + Cf;
    bool full_carry =
        (static_cast<int>(x) - static_cast<int>(y) - static_cast<int>(Cf)) < 0;
    return {result, zero, true, half_carry, full_carry};
}

uint8_t cpu::_flags_to_byte() const {
    uint8_t Z = this->Zf;
    uint8_t N = this->Nf;
    uint8_t H = this->Hf;
    uint8_t C = this->Cf;
    return (Z << 7) | (N << 6) | (H << 5) | (C << 4);
}

std::tuple<bool, bool, bool, bool> cpu::_byte_to_flags(uint8_t byte) {
    bool z = (byte >> 7) & 1;
    bool n = (byte >> 6) & 1;
    bool h = (byte >> 5) & 1;
    bool c = (byte >> 4) & 1;

    return {z, n, h, c};
}

// TODO: all pairs can be converted to const expr inside constructor ? maybe not
uint16_t cpu::_combine_2_8bits(const uint8_t msb, const uint8_t lsb) {
    return (msb << 8) | lsb;
}

std::tuple<uint8_t, uint8_t> cpu::_split_16bit(const uint16_t r16) {
    uint8_t msb = (r16 >> 8) & 0x00ff; // msb
    uint8_t lsb = r16 & 0x00ff;        // lsb

    return {msb, lsb};
    // return (r1 << 8) | r2;
}

uint8_t *cpu::_get_register(const registers r8) {
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
    default: break;
    }

    return register_value;
}

uint8_t cpu::_get(const uint16_t address) {
    return this->gb_mmu->bus_read_memory(address);
}

void cpu::_set(const uint16_t address, const uint8_t value) {
    this->gb_mmu->bus_write_memory(address, value);
}

// abstracted M-operations

// TODO: combine with add_a_r8 later on
void cpu::add_a_hl() {
    // TODO: if capture (this) goes out of scope it could cause crashes
    auto _add_hl = [=]() {
        const uint16_t hl = this->_combine_2_8bits(this->H, this->L);
        this->W = _get(hl); // assign value of HL to temp register W
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
void cpu::add_a_r8(const registers r8) {
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
}

void cpu::add_or_sub_a_imm8(const bool add) {
    auto m1_add = [=]() {
        uint8_t imm8 = _get(this->PC);
        this->PC++;
        auto [result, z, n, h, c] = this->_addition_8bit(this->A, imm8);
        this->A = result;
        this->Zf = z;
        this->Nf = n;
        this->Hf = h;
        this->Cf = c;
    };

    auto m1_sub = [=]() {
        uint8_t imm8 = _get(this->PC);
        this->PC++;
        auto [result, z, n, h, c] = this->_subtraction_8bit(this->A, imm8);
        this->A = result;
        this->Zf = z;
        this->Nf = n;
        this->Hf = h;
        this->Cf = c;
    };

    if (add) {
        this->M_operations.push_back(m1_add);
    } else {
        this->M_operations.push_back(m1_sub);
    }
}

void cpu::adc_or_sbc_imm8(const bool add) {
    auto m1_adc = [=]() {
        uint8_t imm8 = _get(this->PC);
        this->PC++;
        auto [result, z, n, h, c] =
            this->_addition_8bit(this->A, imm8, this->Cf);
        this->A = result;
        this->Zf = z;
        this->Nf = n;
        this->Hf = h;
        this->Cf = c;
    };

    auto m1_sbc = [=]() {
        uint8_t imm8 = _get(this->PC);
        this->PC++;
        auto [result, z, n, h, c] =
            this->_subtraction_8bit(this->A, imm8, this->Cf);
        this->A = result;
        this->Zf = z;
        this->Nf = n;
        this->Hf = h;
        this->Cf = c;
    };

    if (add) {
        this->M_operations.push_back(m1_adc);
    } else {
        this->M_operations.push_back(m1_sbc);
    }
}

void cpu::and_xor_or_imm8(bitops op) {
    auto m1_and = [=]() {
        uint8_t imm8 = _get(this->PC);
        this->PC++;
        this->A = this->A & imm8;
        this->Zf = this->A == 0;
        this->Nf = false;
        this->Hf = true;
        this->Cf = false;
    };

    auto m1_or = [=]() {
        uint8_t imm8 = _get(this->PC);
        this->PC++;
        this->A = this->A | imm8;
        this->Zf = this->A == 0;
        this->Nf = false;
        this->Hf = false;
        this->Cf = false;
    };

    auto m1_xor = [=]() {
        uint8_t imm8 = _get(this->PC);
        this->PC++;
        this->A = this->A ^ imm8;
        this->Zf = this->A == 0;
        this->Nf = false;
        this->Hf = false;
        this->Cf = false;
    };

    if (op == bitops::AND) {
        this->M_operations.push_back(m1_and);
    } else if (op == bitops::OR) {
        this->M_operations.push_back(m1_or);
    } else if (op == bitops::XOR) {
        this->M_operations.push_back(m1_xor);
    }
}

void cpu::rst(const uint8_t opcode) {

    auto m1 = [=]() { this->SP--; };

    auto m2 = [=]() {
        auto [p, c] = _split_16bit(this->PC);
        this->Z = c;
        this->_set(this->SP, p);
        this->SP--;
    };

    auto m3 = [=]() {
        uint16_t address{};
        switch (opcode) {
        case 0xC7: address = 0x00; break;

        case 0xCF: address = 0x08; break;

        case 0xD7: address = 0x10; break;

        case 0xDF: address = 0x18; break;

        case 0xE7: address = 0x20; break;

        case 0xEF: address = 0x28; break;

        case 0xF7: address = 0x30; break;

        case 0xFF: address = 0x38; break;
        }
        this->_set(this->SP, this->Z);
        this->PC = address;
    };

    this->M_operations.push_back(m3);
    this->M_operations.push_back(m2);
    this->M_operations.push_back(m1);
}

void cpu::add_hl_rr(const registers r1, const registers r2, const bool sp) {
    auto m1_nonsp = [=]() {
        const uint8_t *p1 = _get_register(r1);
        const uint8_t *p2 = _get_register(r2);

        const uint16_t rr = _combine_2_8bits(*p1, *p2);
        const uint16_t hl = _combine_2_8bits(this->H, this->L);

        auto [result, z, n, h, c] = _addition_16bit(hl, rr);

        auto [msb, lsb] = _split_16bit(result);
        this->H = msb;
        this->L = lsb;

        this->Nf = n;
        this->Hf = h;
        this->Cf = c;
    };

    auto m1_sp = [=]() {
        const uint16_t hl = _combine_2_8bits(this->H, this->L);

        auto [result, z, n, h, c] = _addition_16bit(hl, this->SP);

        auto [msb, lsb] = _split_16bit(result);
        this->H = msb;
        this->L = lsb;

        this->Nf = n;
        this->Hf = h;
        this->Cf = c;
    };

    if (!sp) {
        this->M_operations.push_back(m1_nonsp);
    } else {
        this->M_operations.push_back(m1_sp);
    }
}

void cpu::call(const conditions condition) {
    // get least and most significant byte via program_counter next
    // M2
    auto get_lsb = [=]() {
        this->W = _get(this->PC);
        this->PC++;
    };

    // M3
    auto get_msb = [=]() {
        this->Z = _get(this->PC);
        this->PC++;
    };

    // M4
    auto decrement_sp = [=]() {
        this->SP--; // decrement stack pointer
    };

    // M5
    auto store_pc_msb = [=]() {
        // store the ms byte and ls byte of current pointer counter to stack
        // pointer
        uint8_t pc_msb = (this->PC >> 8) & 0x00ff;

        this->_set(SP, pc_msb);
        this->SP--;
    };

    // M6
    auto store_pc_lsb = [=]() {
        uint8_t pc_lsb = this->PC & 0x00ff;
        this->_set(SP, pc_lsb);
        // set program counter to function
        this->PC = (this->Z << 8) | this->W; // the function
    };

    switch (condition) {
    case conditions::NA: {
        this->M_operations.push_back(store_pc_lsb);
        this->M_operations.push_back(store_pc_msb);
        this->M_operations.push_back(decrement_sp);
        break;
    }
    case conditions::Z: {
        if (this->Zf) {
            this->M_operations.push_back(store_pc_lsb);
            this->M_operations.push_back(store_pc_msb);
            this->M_operations.push_back(decrement_sp);
        }
        break;
    }
    case conditions::NZ: {
        if (!this->Zf) {
            this->M_operations.push_back(store_pc_lsb);
            this->M_operations.push_back(store_pc_msb);
            this->M_operations.push_back(decrement_sp);
        }
        break;
    }
    case conditions::C: {
        if (this->Cf) {
            this->M_operations.push_back(store_pc_lsb);
            this->M_operations.push_back(store_pc_msb);
            this->M_operations.push_back(decrement_sp);
        }
        break;
    }
    case conditions::NC: {
        if (!this->Cf) {
            this->M_operations.push_back(store_pc_lsb);
            this->M_operations.push_back(store_pc_msb);
            this->M_operations.push_back(decrement_sp);
        }
        break;
    }
    }

    this->M_operations.push_back(get_msb);
    this->M_operations.push_back(get_lsb);
}

void cpu::cp_a_imm8() {
    auto read_imm8 = [=]() {
        this->W = _get(this->PC); // imm8 stored in W
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

void cpu::cp_a_r(const registers r, const bool hl) {
    if (!hl) {
        uint8_t *r1p = _get_register(r);
        auto [result, z, n, h, c] = this->_subtraction_8bit(this->A, *r1p);

        // set flags
        this->Zf = z;
        this->Nf = n;
        this->Hf = h;
        this->Cf = c;
    } else {
        auto read_hl = [=]() {
            uint16_t hl = _combine_2_8bits(this->H, this->L);
            this->W = _get(hl); // memory[hl] stored in W

            // sub an set flags
            auto [result, z, n, h, c] =
                this->_subtraction_8bit(this->A, this->W);

            // set flags
            this->Zf = z;
            this->Nf = n;
            this->Hf = h;
            this->Cf = c;
        };

        // push backwards because of FIFO
        this->M_operations.push_back(read_hl);
    }
}

void cpu::inc_or_dec_hl(const bool inc) {
    uint16_t address = this->_combine_2_8bits(this->H, this->L);

    auto m1 = [=]() { this->Z = _get(address); };

    auto m2_dec = [=]() {
        auto [result, z, n, h, c] = this->_subtraction_8bit(this->Z, 1);
        this->_set(address, result);
        this->Zf = z;
        this->Nf = n;
        this->Hf = h;
    };

    auto m2_inc = [=]() {
        auto [result, z, n, h, c] = this->_addition_8bit(this->Z, 1);
        this->_set(address, result);
        this->Zf = z;
        this->Nf = n;
        this->Hf = h;
    };

    if (inc) {
        this->M_operations.push_back(m2_inc);
    } else {
        this->M_operations.push_back(m2_dec);
    }
    this->M_operations.push_back(m1);
}

void cpu::inc_or_dec_r8(const registers r8, const bool inc) {
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

void cpu::inc_or_dec_r16(const registers r1, const registers r2, const bool inc,
                         const bool sp) {
    auto _inc_or_dec_r16 = [=]() {
        if (sp && inc) {
            this->SP++;
        } else if (sp && !inc) {
            this->SP--;
        } else {
            uint8_t *r1_pointer = this->_get_register(r1);
            uint8_t *r2_pointer = this->_get_register(r2);

            uint16_t current_value = _combine_2_8bits(*r1_pointer, *r2_pointer);

            // oam bug oam corruption bug write on r16 == bc, or de or hl
            /*
               if ((r1 == registers::B && r2 == registers::C) ||
               (r1 == registers::D || r2 == registers::E) ||
               (r1 == registers::H || r2 == registers::L)) {
               this->gb_mmu->oam_bug_write(current_value);
               }*/

            if (inc) {
                current_value++;
            } else if (!inc) {
                current_value--;
            }

            auto [r1_value, r2_value] = _split_16bit(current_value);
            *r1_pointer = r1_value;
            *r2_pointer = r2_value;
        }
    };

    this->M_operations.push_back(_inc_or_dec_r16);
}

void cpu::jp_hl() { this->PC = this->_combine_2_8bits(this->H, this->L); }

void cpu::jp_imm16(const conditions condition) {
    // get least and most significant byte via program_counter next
    auto get_lsb = [=]() {
        this->W = _get(this->PC);
        this->PC++;
    };

    auto get_msb = [=]() {
        this->Z = _get(this->PC);
        this->PC++;
    };

    auto set_pc = [=]() {
        this->PC = (this->Z << 8) | this->W; // the function
    };

    switch (condition) {
    case conditions::NA: {
        this->M_operations.push_back(set_pc);
        break;
    }
    case conditions::Z: {
        if (this->Zf) {
            this->M_operations.push_back(set_pc);
        }
        break;
    }
    case conditions::NZ: {
        if (!this->Zf) {
            this->M_operations.push_back(set_pc);
        }
        break;
    }
    case conditions::C: {
        if (this->Cf) {
            this->M_operations.push_back(set_pc);
        }
        break;
    }
    case conditions::NC: {
        if (!this->Cf) {
            this->M_operations.push_back(set_pc);
        }
        break;
    }
    }

    this->M_operations.push_back(get_msb);
    this->M_operations.push_back(get_lsb);
}

void cpu::jr_s8(conditions condition) {
    // jump relative to signed 8 bit next in memory
    auto get_value = [=]() {
        this->Z = _get(this->PC);
        this->PC++;
    };

    auto set_pc = [=]() {
        // 1001 0010
        int8_t value = this->Z;
        this->PC += value;
    };

    if (condition == conditions::NA) {
        this->M_operations.push_back(set_pc);
    } else if (condition == conditions::Z) {
        if (this->Zf) {
            this->M_operations.push_back(set_pc);
        }
    } else if (condition == conditions::NZ) {
        if (!this->Zf) {
            this->M_operations.push_back(set_pc);
        }
    }

    else if (condition == conditions::NC) {
        if (!this->Cf) {
            this->M_operations.push_back(set_pc);
        }
    }

    else if (condition == conditions::C) {
        if (this->Cf) {
            this->M_operations.push_back(set_pc);
        }
    }

    this->M_operations.push_back(get_value);
}

void cpu::ld_imm16_sp() {
    auto m1 = [=]() {
        this->Z = _get(PC); // lsb
        PC++;
    };

    auto m2 = [=]() {
        this->W = _get(PC); // msb
        PC++;
    };

    auto m3 = [=]() {
        uint16_t address = this->_combine_2_8bits(this->W, this->Z);
        auto [msb, lsb] = this->_split_16bit(this->SP);
        this->_set(address, lsb);
    };

    auto m4 = [=]() {
        uint16_t address = this->_combine_2_8bits(this->W, this->Z) + 1;
        auto [msb, lsb] = this->_split_16bit(this->SP);
        this->_set(address, msb);
    };

    this->M_operations.push_back(m4);
    this->M_operations.push_back(m3);

    this->M_operations.push_back(m2);
    this->M_operations.push_back(m1);
}

void cpu::ld_imm16_a(const bool to_a) {
    auto m1 = [=]() {
        this->Z = _get(PC); // lsb
        PC++;
    };

    auto m2 = [=]() {
        this->W = _get(PC); // msb
        PC++;
    };

    auto m3 = [=]() {
        uint16_t address = this->_combine_2_8bits(W, Z);
        if (!to_a) {
            this->_set(address, this->A); // write A to address
        } else {
            this->A = _get(address); // write value from address to A
        }
    };

    this->M_operations.push_back(m3);
    this->M_operations.push_back(m2);
    this->M_operations.push_back(m1);
}

void cpu::ld_r_r(const registers r_to, const registers r_from) {
    uint8_t *register_pointer_to = _get_register(r_to);
    uint8_t *register_pointer_from = _get_register(r_from);
    *register_pointer_to = *register_pointer_from;
}

void cpu::ld_hl_imm8() {
    auto m1 = [=]() {
        this->Z = _get(this->PC);
        this->PC++;
    };

    auto m2 = [=]() {
        uint16_t address = this->_combine_2_8bits(this->H, this->L);
        this->_set(address, this->Z);
    };

    this->M_operations.push_back(m2);
    this->M_operations.push_back(m1);
}

void cpu::ld_sp_hl() {
    auto m1 = [=]() { this->SP = this->_combine_2_8bits(this->H, this->L); };
    this->M_operations.push_back(m1);
}

void cpu::ld_r_imm8(const registers r) {
    auto m1 = [=]() {
        uint8_t *register_pointer = this->_get_register(r);
        uint8_t value = _get(this->PC);
        *register_pointer = value;
        this->PC++;
    };

    this->M_operations.push_back(m1);
}

void cpu::ld_rr_address(const registers r1, const registers r2, const bool sp) {
    auto m1 = [=]() {
        this->Z = _get(PC); // lsb
        this->PC++;
        this->W = _get(PC); // msb
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

void cpu::ld_hl_a(const bool increment, const bool to_a) {
    auto m1 = [=]() {
        uint16_t address = this->_combine_2_8bits(this->H, this->L);

        // oam bug oam corruption bug write on increment or decrement, i think
        // its already handled by set
        this->_set(address, this->A);

        if (increment) {
            address++;
        } else {
            address--;
        }
        auto [h, l] = this->_split_16bit(address);
        this->H = h;
        this->L = l;
    };

    auto m2 = [=]() {
        uint16_t address = this->_combine_2_8bits(this->H, this->L);

        // oam bug oam corruption bug read inc (corruption happens before get
        // address)
        // this->gb_mmu->oam_bug_read_inc(address);

        this->A = _get(address);

        if (increment) {
            address++;
        } else {
            address--;
        }
        auto [h, l] = this->_split_16bit(address);
        this->H = h;
        this->L = l;
    };

    if (to_a) {
        this->M_operations.push_back(m2);
    } else {
        this->M_operations.push_back(m1);
    }
} // covers hl+ and hl-

void cpu::ld_hl_r8(const registers r, const bool to_hl) {
    auto m1_to_hl = [=]() {
        uint16_t address = this->_combine_2_8bits(this->H, this->L);
        uint8_t *rp = this->_get_register(r);
        this->_set(address, *rp);
    };

    auto m2 = [=]() {
        uint16_t address = this->_combine_2_8bits(this->H, this->L);
        uint8_t *rp = this->_get_register(r);
        uint8_t value = _get(address);
        *rp = value;
    };

    if (to_hl) {
        this->M_operations.push_back(m1_to_hl);
    } else {
        this->M_operations.push_back(m2);
    }
}

void cpu::ld_hl_sp_s8() {
    auto m1 = [=]() {
        this->Z = _get(this->PC);
        this->PC++;
    };

    auto m2 = [=]() {
        auto [result, z, n, h, c] =
            this->_addition_16bit(this->SP, this->Z, true);
        auto [msb, lsb] = _split_16bit(result);
        this->H = msb;
        this->L = lsb;
        this->Zf = false;
        this->Nf = n;
        this->Hf = h;
        this->Cf = c;
    };

    this->M_operations.push_back(m2);
    this->M_operations.push_back(m1);
}

void cpu::pop_rr(const registers r1, const registers r2, const bool af) {
    auto m1 = [=]() {
        this->Z = _get(this->SP); // lsb
        this->SP++;
    };

    auto m2 = [=]() {
        this->W = _get(this->SP); // msb
        this->SP++;

        uint8_t *r_pointer1 = _get_register(r1);
        uint8_t *r_pointer2 = _get_register(r2);

        *r_pointer1 = W;
        *r_pointer2 = Z;
    };

    auto m2_af = [=]() {
        this->W = _get(this->SP); // msb
        this->SP++;

        uint8_t *r_pointer1 = _get_register(registers::A);
        // TODO: get F

        auto [z, n, h, c] = this->_byte_to_flags(Z);

        *r_pointer1 = W;
        this->Zf = z;
        this->Nf = n;
        this->Hf = h;
        this->Cf = c;
    };

    if (!af) {
        this->M_operations.push_back(m2);
    } else {
        this->M_operations.push_back(m2_af);
    }
    this->M_operations.push_back(m1);
}

void cpu::push_rr(const registers r1, const registers r2, const bool af) {
    auto m1 = [=]() { this->SP--; };

    auto m2 = [=]() {
        auto r1_p = this->_get_register(r1);
        this->_set(this->SP, *r1_p);
        this->SP--;
    };

    auto m2_af = [=]() {
        auto r1_p = this->_get_register(registers::A);
        this->_set(this->SP, *r1_p);
        this->SP--;
    };

    auto m3 = [=]() {
        auto r2_p = this->_get_register(r2);
        this->_set(this->SP, *r2_p);
    };

    auto m3_af = [=]() {
        auto f = _flags_to_byte();
        this->_set(this->SP, f);
    };

    if (af) {
        this->M_operations.push_back(m3_af);
        this->M_operations.push_back(m2_af);
    } else {
        this->M_operations.push_back(m3);
        this->M_operations.push_back(m2);
    }
    this->M_operations.push_back(m1);
}

void cpu::ret(conditions condition, bool ime_condition) {

    auto fill = [=]() {};

    auto m1 = [=]() {
        this->Z = _get(this->SP);
        this->SP++;
    };

    auto m2 = [=]() {
        this->W = _get(this->SP);
        this->SP++;
    };

    auto m3 = [=]() {
        if (ime_condition) {
            this->gb_interrupt->ime = true;
        }
        this->PC = this->_combine_2_8bits(W, Z);
    };

    switch (condition) {
    case conditions::NA: {
        this->M_operations.push_back(m3);
        this->M_operations.push_back(m2);
        this->M_operations.push_back(m1);
        break;
    }
    case conditions::Z: {
        this->M_operations.push_back(fill);
        if (this->Zf) {
            this->M_operations.push_back(m3);
            this->M_operations.push_back(m2);
            this->M_operations.push_back(m1);
        }
        break;
    }
    case conditions::NZ: {
        this->M_operations.push_back(fill);
        if (!this->Zf) {
            this->M_operations.push_back(m3);
            this->M_operations.push_back(m2);
            this->M_operations.push_back(m1);
        }
        break;
    }
    case conditions::C: {
        this->M_operations.push_back(fill);
        if (this->Cf) {
            this->M_operations.push_back(m3);
            this->M_operations.push_back(m2);
            this->M_operations.push_back(m1);
        }
        break;
    }
    case conditions::NC: {
        this->M_operations.push_back(fill);
        if (!this->Cf) {
            this->M_operations.push_back(m3);
            this->M_operations.push_back(m2);
            this->M_operations.push_back(m1);
        }
        break;
    }
    }
}

void cpu::sla_r(const registers r, const bool hl) {
    auto m1 = [=]() {
        uint8_t *rp = this->_get_register(r); // r falls out of scope for some
                                              // reason uint8_t* rp = &this->B;

        uint8_t msbit = (*rp >> 7) & 1; // save the "carry" bit
        *rp <<= 1;                      // left shift A by 1 bit
                                        // bit 0 is reset to 0

        this->Zf = *rp == 0;
        this->Nf = false;
        this->Hf = false;
        this->Cf = msbit == 1;
    };

    auto m1_hl = [=]() {
        // does nothing, get opcode after cb prefix
    };

    auto m2_hl = [=]() {
        uint16_t address = this->_combine_2_8bits(this->H, this->L);
        this->Z = _get(address);

        uint8_t msbit = (this->Z >> 7) & 1; // save the "carry" bit
        this->Z <<= 1;                      // left shift A by 1 bit
                                            // bit 0 is reset to 0

        this->Zf = this->Z == 0;
        this->Nf = false;
        this->Hf = false;
        this->Cf = msbit == 1;
    };

    auto m3_hl = [=]() {
        uint16_t address = _combine_2_8bits(this->H, this->L);
        _set(address, this->Z);
    };

    if (!hl) {
        this->M_operations.push_back(m1);
    } else {
        this->M_operations.push_back(m3_hl);
        this->M_operations.push_back(m2_hl);
        this->M_operations.push_back(m1_hl);
    }
}

void cpu::rl_r(const registers r, const bool hl, const bool z_flag,
               const bool one_cycle) {
    auto m1 = [=]() {
        uint8_t *rp = _get_register(r);
        uint8_t msbit = (*rp >> 7) & 1;  // save the "carry" bit
        uint8_t carry_flag = this->Cf;   // take current carry flag
        *rp = *rp << 1;                  // left shift A by 1 bit
        *rp = (*rp & 0xfe) | carry_flag; // put carry_flag into bit 0
                                         // set flags
        if (!z_flag) {
            this->Zf = false;
        } else {
            this->Zf = *rp == 0;
        }
        this->Nf = false;
        this->Hf = false;

        this->Cf = msbit == 1;
    };

    if (one_cycle) {
        assert(r == registers::A &&
               "You are running RLA but the register is not A!");
        m1();
        return;
    }

    auto m1_hl = [=]() {
        // does nothign, get opcode after cb prefix
    };

    auto m2_hl = [=]() {
        uint16_t address = _combine_2_8bits(this->H, this->L);
        this->Z = _get(address);

        uint8_t msbit = (this->Z >> 7) & 1;      // save the "carry" bit
        uint8_t carry_flag = this->Cf;           // take current carry flag
        this->Z = this->Z << 1;                  // left shift A by 1 bit
        this->Z = (this->Z & 0xfe) | carry_flag; // put carry_flag into bit 0
                                                 // set flags
        this->Zf = this->Z == 0;
        this->Nf = false;
        this->Hf = false;

        this->Cf = msbit == 1;
    };

    auto m3_hl = [=]() {
        uint16_t address = _combine_2_8bits(this->H, this->L);
        _set(address, this->Z);
    };

    if (!hl) {
        this->M_operations.push_back(m1);
    } else {
        this->M_operations.push_back(m3_hl);
        this->M_operations.push_back(m2_hl);
        this->M_operations.push_back(m1_hl);
    }
}

void cpu::rlc_r(const registers r, const bool hl, const bool z_flag,
                const bool one_cycle) {
    auto m1 = [=]() {
        uint8_t *rp = _get_register(r);
        uint8_t msbit = (*rp >> 7) & 1; // save the "carry" bit
        *rp = *rp << 1;
        *rp = (*rp & 0xfe) | msbit; // put carry_flag into bit 0
                                    // set flags
        if (!z_flag) {
            this->Zf = false;
        } else {
            this->Zf = *rp == 0;
        }
        this->Nf = false;
        this->Hf = false;
        this->Cf = msbit == 1;
    };

    if (one_cycle) {
        assert(r == registers::A &&
               "You are running RLCA but the register is not A!");
        m1();
        return;
    }

    auto m1_hl = [=]() {
        // does nothing, get opcode after cb prefix
    };

    auto m2_hl = [=]() {
        uint16_t address = _combine_2_8bits(this->H, this->L);
        this->Z = _get(address);

        uint8_t msbit = (this->Z >> 7) & 1; // save the "carry" bit
        this->Z = this->Z << 1;
        this->Z = (this->Z & 0xfe) | msbit; // put carry_flag into bit 0
                                            // set flags
        this->Zf = this->Z == 0;
        this->Nf = false;
        this->Hf = false;
        this->Cf = msbit == 1;
    };

    auto m3_hl = [=]() {
        uint16_t address = _combine_2_8bits(this->H, this->L);
        _set(address, this->Z);
    };

    if (!hl) {
        this->M_operations.push_back(m1);
    } else {
        this->M_operations.push_back(m3_hl);
        this->M_operations.push_back(m2_hl);
        this->M_operations.push_back(m1_hl);
    }
}

void cpu::rr_r(const registers r, const bool hl, const bool z_flag,
               const bool one_cycle) {
    auto m1 = [=]() {
        uint8_t *rp = _get_register(r);
        uint8_t lsbit = *rp & 1;                // save the "carry" bit
        uint8_t carry_flag = this->Cf;          // take current carry flag
        *rp = *rp >> 1;                         // left shift A by 1 bit
        *rp = (*rp & 0x7f) | (carry_flag << 7); // put carry_flag into bit 0
                                                // set flags
        if (!z_flag) {
            this->Zf = false;
        } else {
            this->Zf = *rp == 0;
        }
        this->Nf = false;
        this->Hf = false;
        this->Cf = lsbit == 1;
    };

    if (one_cycle) {
        assert(r == registers::A &&
               "You are running RRA but the register is not A!");
        m1();
        return;
    }

    auto m1_hl = [=]() {
        // does nothing, get opcode after cb prefix
    };

    auto m2_hl = [=]() {
        uint16_t address = this->_combine_2_8bits(this->H, this->L);
        this->Z = _get(address);

        uint8_t lsbit = this->Z & 1;   // save the "carry" bit
        uint8_t carry_flag = this->Cf; // take current carry flag
        this->Z = this->Z >> 1;        // left shift A by 1 bit
        this->Z = (this->Z & 0x7f) | (carry_flag << 7); // put carry_flag into
                                                        // bit 0 set flags
        this->Zf = this->Z == 0;
        this->Nf = false;
        this->Hf = false;
        this->Cf = lsbit == 1;
    };

    auto m3_hl = [=]() {
        uint16_t address = _combine_2_8bits(this->H, this->L);
        _set(address, this->Z);
    };

    if (!hl) {
        this->M_operations.push_back(m1);
    } else {
        this->M_operations.push_back(m3_hl);
        this->M_operations.push_back(m2_hl);
        this->M_operations.push_back(m1_hl);
    }
}

void cpu::rrc_r(const registers r, const bool hl, const bool z_flag,
                const bool one_cycle) {
    auto m1 = [=]() {
        uint8_t *rp = _get_register(r);
        uint8_t lsbit = *rp & 1;           // save the "carry" bit
        *rp = *rp >> 1;                    // left shift A by 1 bit
        *rp = (*rp & 0x7f) | (lsbit << 7); // put carry_flag into bit 0
                                           // set flags
        if (!z_flag) {
            this->Zf = false;
        } else {
            this->Zf = *rp == 0;
        }
        this->Nf = false;
        this->Hf = false;
        this->Cf = lsbit == 1;
    };

    if (one_cycle) {
        assert(r == registers::A &&
               "You are running RRCA but the register is not A!");
        m1();
        return;
    }

    auto m1_hl = [=]() {
        // does nothing, get opcode after cb prefix
    };

    auto m2_hl = [=]() {
        uint16_t address = this->_combine_2_8bits(this->H, this->L);
        this->Z = _get(address);

        uint8_t lsbit = this->Z & 1; // save the "carry" bit
        this->Z = this->Z >> 1;
        this->Z = (this->Z & 0x7f) | (lsbit << 7); // put carry_flag into bit 0
                                                   // set flags
        this->Zf = this->Z == 0;
        this->Nf = false;
        this->Hf = false;
        this->Cf = lsbit == 1;
    };

    auto m3_hl = [=]() {
        uint16_t address = _combine_2_8bits(this->H, this->L);
        _set(address, this->Z);
    };

    if (!hl) {
        this->M_operations.push_back(m1);
    } else {
        this->M_operations.push_back(m3_hl);
        this->M_operations.push_back(m2_hl);
        this->M_operations.push_back(m1_hl);
    }
}

void cpu::sra_r(const registers r, const bool hl) {
    auto m1 = [=]() {
        uint8_t *rp = this->_get_register(r); // r falls out of scope for some
                                              // reason uint8_t* rp = &this->B;

        uint8_t msbit_retain = *rp & 0x80;

        uint8_t lsbit = *rp & 1;
        *rp = (*rp >> 1) | msbit_retain;

        this->Zf = *rp == 0;
        this->Nf = false;
        this->Hf = false;
        this->Cf = lsbit == 1;
    };

    auto m1_hl = [=]() {
        // does nothing, gets opcode fater cb prefix
    };

    auto m2_hl = [=]() {
        uint16_t address = this->_combine_2_8bits(this->H, this->L);
        this->Z = _get(address);

        uint8_t msbit_retain = this->Z & 0x80;
        uint8_t lsbit = this->Z & 1;
        this->Z = (this->Z >> 1) | msbit_retain;
        this->Zf = this->Z == 0;
        this->Nf = false;
        this->Hf = false;
        this->Cf = lsbit == 1;
    };

    auto m3_hl = [=]() {
        uint16_t address = _combine_2_8bits(this->H, this->L);
        _set(address, this->Z);
    };

    if (!hl) {
        this->M_operations.push_back(m1);
    } else {
        this->M_operations.push_back(m3_hl);
        this->M_operations.push_back(m2_hl);
        this->M_operations.push_back(m1_hl);
    }
}

void cpu::swap_r(const registers r, const bool hl) {
    auto m1 = [=]() {
        uint8_t *rp = this->_get_register(r); // r falls out of scope for some
                                              // reason uint8_t* rp = &this->B;

        uint8_t lsb = *rp & 0x0f;
        *rp >>= 4;
        *rp = *rp | (lsb << 4);

        // 0010 1011 == 1011 0010

        this->Zf = *rp == 0;
        this->Nf = false;
        this->Hf = false;
        this->Cf = false;
    };

    auto m1_hl = [=]() {
        // does nothing, gets opcode after cb prefix
    };

    auto m2_hl = [=]() {
        uint16_t address = _combine_2_8bits(this->H, this->L);
        this->Z = _get(address);

        uint8_t lsb = this->Z & 0x0f;
        this->Z >>= 4;
        this->Z = this->Z | (lsb << 4);

        // 0010 1011 == 1011 0010

        this->Zf = this->Z == 0;
        this->Nf = false;
        this->Hf = false;
        this->Cf = false;
    };

    auto m3_hl = [=]() {
        uint16_t address = _combine_2_8bits(this->H, this->L);
        _set(address, this->Z);
    };

    if (!hl) {
        this->M_operations.push_back(m1);
    } else {
        this->M_operations.push_back(m3_hl);
        this->M_operations.push_back(m2_hl);
        this->M_operations.push_back(m1_hl);
    }
}

void cpu::srl_r(const registers r, const bool hl) {
    auto m1 = [=]() {
        uint8_t *rp = _get_register(r);
        uint8_t lsbit = *rp & 1;       // save the "carry" bit
        uint8_t carry_flag = this->Cf; // take current carry flag
        *rp = *rp >> 1;                // left shift A by 1 bit
                                       // bit 7 reset to 0

        // set flags
        this->Zf = *rp == 0;
        this->Nf = false;
        this->Hf = false;
        this->Cf = lsbit == 1;
    };

    auto m1_hl = [=]() {
        // does nothing, get opcode after cb prefix
    };

    auto m2_hl = [=]() {
        uint16_t address = _combine_2_8bits(this->H, this->L);
        this->Z = _get(address);

        uint8_t lsbit = this->Z & 1;   // save the "carry" bit
        uint8_t carry_flag = this->Cf; // take current carry flag
        this->Z = this->Z >> 1;        // left shift A by 1 bit
                                       // bit 7 reset to 0

        this->Zf = this->Z == 0;
        this->Nf = false;
        this->Hf = false;
        this->Cf = lsbit == 1;
    };

    auto m3_hl = [=]() {
        uint16_t address = _combine_2_8bits(this->H, this->L);
        _set(address, this->Z);
    };

    if (!hl) {
        this->M_operations.push_back(m1);
    } else {
        this->M_operations.push_back(m3_hl);
        this->M_operations.push_back(m2_hl);
        this->M_operations.push_back(m1_hl);
    }
}

void cpu::cpl() {
    this->A = ~this->A;
    this->Nf = true;
    this->Hf = true;
}

void cpu::scf() {
    this->Nf = false;
    this->Hf = false;
    this->Cf = true;
}

void cpu::ccf() {
    this->Nf = false;
    this->Hf = false;
    this->Cf = !this->Cf;
}

void cpu::adc_or_sbc(const registers r1, const bool hl, const bool add) {
    if (!hl) {
        uint8_t *r1p = _get_register(r1);
        uint8_t result{};
        if (add) {
            auto [result, z, n, h, c] = _addition_8bit(this->A, *r1p, this->Cf);
            this->A = result;
            this->Zf = z;
            this->Nf = n;
            this->Hf = h;
            this->Cf = c;
        } else {
            auto [result, z, n, h, c] =
                _subtraction_8bit(this->A, *r1p, this->Cf);
            this->A = result;
            this->Zf = z;
            this->Nf = n;
            this->Hf = h;
            this->Cf = c;
        }
    } else {
        auto m1_adc = [=]() {
            uint16_t address = _combine_2_8bits(this->H, this->L);
            this->Z = _get(address);
            auto [result, z, n, h, c] =
                _addition_8bit(this->A, this->Z, this->Cf);
            this->A = result;
            this->Zf = z;
            this->Nf = n;
            this->Hf = h;
            this->Cf = c;
        };
        auto m1_sbc = [=]() {
            uint16_t address = _combine_2_8bits(this->H, this->L);
            this->Z = _get(address);
            auto [result, z, n, h, c] =
                _subtraction_8bit(this->A, this->Z, this->Cf);
            this->A = result;
            this->Zf = z;
            this->Nf = n;
            this->Hf = h;
            this->Cf = c;
        };
        if (add) {
            this->M_operations.push_back(m1_adc);
        } else {
            this->M_operations.push_back(m1_sbc);
        }
    }
}

void cpu::add_sp_s8() {
    // TODO: check logic
    auto m1 = [=]() {
        this->Z = this->_get(this->PC);
        this->PC++;
    };

    auto m2 = [=]() {
        auto [result, z, n, h, c] =
            this->_addition_16bit(this->SP, this->Z, true);
        this->SP = result;
        this->Zf = false;
        this->Nf = false;
        this->Hf = h;
        this->Cf = c;
    };

    auto fill = [=]() {};
    this->M_operations.push_back(fill);
    this->M_operations.push_back(m2);
    this->M_operations.push_back(m1);
}

void cpu::and_r(const registers r, const bool hl) {
    if (!hl) {
        uint8_t *r1p = _get_register(r);
        uint8_t result = this->A & *r1p;
        this->A = result;
        this->Zf = result == 0;
        this->Nf = false;
        this->Hf = true;
        this->Cf = false;
    } else {
        auto m1 = [=]() {
            uint16_t address = _combine_2_8bits(this->H, this->L);
            this->Z = _get(address);
            uint8_t result = this->A & this->Z;
            this->A = result;
            this->Zf = result == 0;
            this->Nf = false;
            this->Hf = true;
            this->Cf = false;
        };
        this->M_operations.push_back(m1);
    }
}

void cpu::sub(const registers r, const bool hl) {
    if (!hl) {
        auto p = this->_get_register(r);
        auto [result, z, n, h, c] = this->_subtraction_8bit(this->A, *p);
        this->A = result;

        // set flags
        this->Zf = z;
        this->Nf = n;
        this->Hf = h;
        this->Cf = c;
    } else {
        auto m1 = [=]() {
            uint16_t address = _combine_2_8bits(this->H, this->L);
            this->Z = _get(address);
            auto [result, z, n, h, c] =
                this->_subtraction_8bit(this->A, this->Z);
            this->A = result;

            // set flags
            this->Zf = z;
            this->Nf = n;
            this->Hf = h;
            this->Cf = c;
        };
        this->M_operations.push_back(m1);
    }
}

void cpu::xor_r(const registers r, const bool hl) {
    if (!hl) {
        auto p = this->_get_register(r);
        uint8_t result = this->A ^ *p;
        this->A = result;
        // set flags
        this->Zf = result == 0;
        this->Nf = false;
        this->Hf = false;
        this->Cf = false;
    } else {
        auto m1 = [=]() {
            uint16_t address = _combine_2_8bits(this->H, this->L);
            this->Z = _get(address);
            uint8_t result = this->A ^ this->Z;
            this->A = result;
            // set flags
            this->Zf = result == 0;
            this->Nf = false;
            this->Hf = false;
            this->Cf = false;
        };
        this->M_operations.push_back(m1);
    }
}

void cpu::or_r(const registers r, const bool hl) {
    if (!hl) {
        auto p = this->_get_register(r);
        uint8_t result = this->A | *p;
        this->A = result;
        // set flags
        this->Zf = result == 0;
        this->Nf = false;
        this->Hf = false;
        this->Cf = false;
    } else {
        auto m1 = [=]() {
            uint16_t address = _combine_2_8bits(this->H, this->L);
            this->Z = _get(address);
            uint8_t result = this->A | this->Z;
            this->A = result;
            // set flags
            this->Zf = result == 0;
            this->Nf = false;
            this->Hf = false;
            this->Cf = false;
        };
        this->M_operations.push_back(m1);
    }
}

void cpu::ld_c_a(const bool to_a) {
    auto m1 = [=]() {
        const uint16_t address = this->C | 0xff00;
        if (to_a) {
            this->A = this->_get(address);
        } else {
            this->_set(address, this->A);
        }
    };

    this->M_operations.push_back(m1);
}

void cpu::ld_imm8_a(const bool to_a) {
    // read imm8
    auto m1 = [=]() {
        this->Z = _get(this->PC);
        this->PC++;
    };

    auto m2 = [=]() {
        const uint16_t address = this->Z | 0xff00;
        if (to_a) {
            this->A = _get(address);
            std::cout << "";
        } else {
            this->_set(address, this->A);
            std::cout << "";
        }
    };

    this->M_operations.push_back(m2);
    this->M_operations.push_back(m1);
}

void cpu::ld_a_rr(const registers r1, const registers r2, const bool to_a) {
    // read imm8
    auto m1 = [=]() {
        uint8_t *r1_p = _get_register(r1);
        uint8_t *r2_p = _get_register(r2);
        uint16_t address = this->_combine_2_8bits(*r1_p, *r2_p);
        this->A = _get(address);
    };

    auto m2 = [=]() {
        uint8_t *r1_p = _get_register(r1);
        uint8_t *r2_p = _get_register(r2);
        uint16_t address = this->_combine_2_8bits(*r1_p, *r2_p);
        this->_set(address, this->A);
    };

    if (to_a) {
        this->M_operations.push_back(m1);
    } else {
        this->M_operations.push_back(m2);
    }
}

void cpu::res_or_set(const uint8_t bit, const registers r, const bool set,
                     const bool hl) {

    auto m1 = [=]() {
        uint8_t *rp = _get_register(r);

        // res
        // 1111 1110
        // 0 = 1, 1 = 2, 2 = 4, 3 = 8, 4 = 16, 5 = 32, 6 = 64, 7 = 128
        // 0000 0001, 0000 0010, 0000 0011, 0000 0100, 0000 0101, 00000
        if (!set) {
            uint8_t mask = ~(1 << bit);
            *rp = *rp & mask;
        } else {
            // 0100 1010 , set 2
            uint8_t mask = 1 << bit;
            *rp = *rp | mask;
        }
    };

    auto m1_hl = [=]() {
        // does nothing, get opcode after cb
    };

    auto m2_hl = [=]() {
        // res
        // 1111 1110
        // 0 = 1, 1 = 2, 2 = 4, 3 = 8, 4 = 16, 5 = 32, 6 = 64, 7 = 128
        // 0000 0001, 0000 0010, 0000 0011, 0000 0100, 0000 0101, 00000
        uint16_t address = _combine_2_8bits(this->H, this->L);
        this->Z = _get(address);

        if (!set) {
            uint8_t mask = ~(1 << bit);
            this->Z = this->Z & mask;
        } else {
            // 0100 1010 , set 2
            uint8_t mask = 1 << bit;
            this->Z = this->Z | mask;
        }
    };

    auto m3_hl = [=]() {
        uint16_t address = _combine_2_8bits(this->H, this->L);
        _set(address, this->Z);
    };

    if (!hl) {
        this->M_operations.push_back(m1);
    } else {
        this->M_operations.push_back(m3_hl);
        this->M_operations.push_back(m2_hl);
        this->M_operations.push_back(m1_hl);
    }
}

void cpu::bit(const uint8_t bit, const registers r, const bool hl) {

    auto m1 = [=]() {
        uint8_t *rp = _get_register(r);

        this->Zf = !((*rp >> bit) & 1);
        this->Nf = false;
        this->Hf = true;
    };

    auto m1_hl = [=]() {
        // does nothing, get prefix after cb opcode
    };

    auto m2_hl = [=]() {
        uint16_t address = _combine_2_8bits(this->H, this->L);
        this->Z = _get(address);
        this->Zf = !((this->Z >> bit) & 1);
        this->Nf = false;
        this->Hf = true;
    };

    if (!hl) {
        this->M_operations.push_back(m1);
    } else {
        this->M_operations.push_back(m2_hl);
        this->M_operations.push_back(m1_hl);
    }
}

void cpu::daa() {
    if (!this->Nf) {
        if (this->Cf || this->A > 0x99) {
            this->A += 0x60;
            this->Cf = true;
        }
        if (this->Hf || (this->A & 0x0f) > 0x09) {
            this->A += 0x6;
        }
    } else {
        if (this->Cf) {
            this->A -= 0x60;
        }
        if (this->Hf) {
            this->A -= 0x6;
        }
    }
    this->Zf = (this->A == 0);
    this->Hf = false;
}

void cpu::ei_or_di(const bool ei) {
    if (ei) {
        // this->ime = true;
        this->gb_interrupt->ei_delay = true;

    } else {
        this->gb_interrupt->ime = false;
    }
}

uint8_t cpu::identify_opcode(const uint8_t opcode) {
    if (!this->halt_bug) {
        this->PC++; // increment program counter, fails to increment if halt bug
                    // is active
    } else if (this->halt_bug) {
        this->halt_bug = false; // reset halt bug
    }

    handle_opcode(opcode); // handle the opcode
    if (!this->M_operations.empty()) {
        this->fetch_opcode = false; // going past fetch opcode,
    }
    return opcode;
}

void cpu::execute_M_operations() {
    // execute any further instructions
    if (!this->M_operations.empty()) {
        this->fetch_opcode = false;
        std::function<void()> operation = this->M_operations.back();
        this->M_operations.pop_back();
        // TODO: check if this reference is valid
        operation();
    }
    if (this->M_operations.empty()) {
        this->fetch_opcode = true;
    }
}

void cpu::execute_I_operations() {
    // execute any further instructions
    if (!this->I_operations.empty()) {
        this->fetch_opcode = false;
        std::function<void()> operation = this->I_operations.back();
        this->I_operations.pop_back();
        // TODO: check if this reference is valid
        operation();
    }
    if (this->I_operations.empty()) {
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
            this->_set(i, buffer[i]);
        }
        // this->PC = 0; // initialize program counter
    }
    // this->gb_mmu->complete_boot();
}

void cpu::prepare_rom(std::string path) {
    this->rom = path;
    // TODO: right now i'm only loading the logo. fix this later.
    std::ifstream file(
        path,
        std::ios::binary |
            std::ios::ate); // read file from end, in binary format

    if (file.is_open()) {
        int size = file.tellg(); // check position of cursor (file size)

        std::vector<char> buffer(
            size); // prepare buffer with size = size of file

        file.seekg(0, std::ios::beg); // move cursor to beginning of file
        file.read(buffer.data(), size);

        for (long i = 0x0100; i < 0x0150; ++i) {
            // 0104 - 0133 - logo
            // TODO: make sure rom doesnt take up more space than it should
            this->_set(i, buffer[i]);
        }
        // this->PC = 0; // initialize program counter
    }
}

void cpu::load_rom() {
    std::ifstream file(
        this->rom,
        std::ios::binary |
            std::ios::ate); // read file from end, in binary format

    if (file.is_open()) {
        int size = file.tellg(); // check position of cursor (file size)

        std::vector<char> buffer(
            size); // prepare buffer with size = size of file

        file.seekg(0, std::ios::beg); // move cursor to beginning of file
        file.read(buffer.data(), size);

        // cartridge type defined in 147
        this->gb_mmu->set_cartridge_type(buffer[0x0147]);
        // this->gb_mmu->set_cartridge_type(0);

        for (long i = 0; i < size; ++i) {
            // TODO: make sure rom doesnt take up more space than it should
            this->_set(i, buffer[i]);
        }
        // this->PC = 0; // initialize program counter
    }
}

// cpu constructor
cpu::cpu(mmu &mmu, timer &timer, interrupt &interrupt) {
    // pass by reference
    this->gb_mmu = &mmu; // & refers to actual address to assign to the pointer
    this->gb_timer = &timer;

    this->gb_interrupt = &interrupt;
}

void cpu::initialize_skip_bootrom_values() {
    // initialize values if skipping bootrom
    A = 0x01;
    B = 0x00;
    C = 0x13;
    D = 0x00;
    E = 0xd8;
    Zf = true;
    Nf = false;
    Hf = true;
    Cf = true;
    H = 0x01;
    L = 0x4d;
    SP = 0xfffe;
    PC = 0x0100;
    W = 0x00;
    Z = 0x50;
    halt = false;
    halt_bug = false;
    interrupt_ticks = 3;
    ticks = 3;
    fetch_opcode = true;
}

void cpu::tick() {
    this->ticks++;

    if (ticks < 4) {
        return;
    }

    this->ticks = 0; // reset ticks

    // tick the timer
    // this->gb_timer->tick(); // increment internal div before cpu writes

    if (this->gb_timer->tima_overflow_standby) {
        this->gb_mmu->handle_tima_overflow();
    }

    // check if boot rom is completed
    if (!boot_rom_complete && _get(0xff50)) {
        // boot rom has completed
        this->boot_rom_complete = true;
        // load the actual game rom
        load_rom();

        // complete boot rom in mmu so that writes to certain addresses are
        // blocked
        this->gb_mmu->set_load_rom_complete();
    }

    // TODO: DMA happen before or after interrupt checking (does it matter)? and
    // does it happen during halt handle DMA transfers on dma mode before
    // exeucting instructions
    if (this->gb_mmu->gb_ppu->dma_delay) {
        this->gb_mmu->set_oam_dma();
    }

    else if (this->gb_mmu->gb_ppu->dma_mode) {
        this->gb_mmu->dma_transfer();
    }

    if (M_operations.empty()) {
        handle_interrupts();
    }

    // set IME before executing instructions
    const uint8_t opcode = this->_get(this->PC); // get current opcode

    // ime should be set before execution of next opcode
    if (this->gb_interrupt->ei_delay && opcode != 0xfb) {
        this->gb_interrupt->ime = true;
        this->gb_interrupt->ei_delay = false;
    }

    if (!this->halt) {
        // fetch opcode, then execute what you can this M-cycle

        if (this->fetch_opcode) {
            identify_opcode(opcode);
        }

        else if (!M_operations.empty()) {
            // execute any further instructions
            this->execute_M_operations();
        }

        else if (!I_operations.empty()) {
            // check if IE still has interrupts

            // execute interrupt operations
            this->execute_I_operations();
        }

        // TODO: are interrupts affected by HALT?
    }

    if (this->gb_timer->tima_overflow) {
        this->gb_timer->tima_overflow_standby = true;
    }
}

void cpu::push_interrupts() {
    auto m1 = [=]() {};              // NOP
    auto m2 = [=]() { this->SP--; }; // pre-decrement SP

    auto m3 = [=]() {
        // write pc high to stack (upper byte push)
        this->_set(this->SP, this->PC >> 8);
        this->SP--;
    };

    auto m4 = [=]() {
        // write pc low to stack (lower byte push)

        // NOTE: if IE was pushed this cycle, it's too late. the reason is
        // because this whole function (handle interrupts) occurs BEFORE cpu
        // execution, so we are always looking at the IE value BEFORE the
        // current M-cycle

        this->_set(this->SP, this->PC & 0xff);

        this->PC = static_cast<uint16_t>(this->gb_interrupt->current_interrupt);

        this->gb_interrupt->interrupt_flags &=
            static_cast<uint8_t>(this->gb_interrupt->current_if_mask);
    };

    auto m5 = [=]() {
        this->gb_interrupt->ime = false;
        this->gb_interrupt->current_interrupt = interrupt::interrupts::none;
        this->gb_interrupt->current_if_mask = interrupt::if_mask::none;
    };

    this->I_operations.push_back(m5);
    this->I_operations.push_back(m4);
    this->I_operations.push_back(m3);
    this->I_operations.push_back(m2);
    this->I_operations.push_back(m1);

    this->fetch_opcode = false;
}

void cpu::handle_interrupts() {
    // takes 5 M-Cycles

    // exit halt if _ie & _if
    uint8_t _ie = this->gb_interrupt->interrupt_enable_flag;
    uint8_t _if = this->gb_interrupt->interrupt_flags;

    if ((_ie & _if & 0x1f) && this->halt) {
        this->halt = false;
        this->gb_mmu->cpu_halted = false;
    }

    // NOTE: This runs before cpu write to the interrupt (so it is based on
    // previous M cycle write)
    this->gb_interrupt->check_current_interrupt(); // interrupt, mask

    if (!this->I_operations.empty()) {
        return;
    }

    if (!this->gb_interrupt->ime || !_ie ||
        !_if) { // no interrupts and I operations is empty
        return;
    }

    if (this->gb_interrupt->current_interrupt == interrupt::interrupts::none) {
        // no interrupts, and I operations was empty
        // if I operations was not empty, then we would have returned early to
        // continue servicing our interrupt with new vectors
        assert(this->gb_interrupt->current_if_mask ==
                   interrupt::if_mask::none &&
               "if mask is not set properly!!");
        return;
    }

    this->push_interrupts();
}
