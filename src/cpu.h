#pragma once

#include "mmu.h"
#include <cstdint>
#include <functional>
#include <iomanip>
#include <iostream>
#include <tuple>
#include <vector>

class cpu {
    // TODO: make private

  public:
    // pointer to mmu
    cpu(mmu &mmu);    // pass by reference
    mmu *gb_mmu{}; // the central mmu

    // main 8-bit registers
    uint8_t A{0};
    uint8_t B{0};
    uint8_t C{0};
    uint8_t D{0};
    uint8_t E{0};

    // flags register, 4 MSBs: Z(ero) N(subtract) H(half-carry) C(carry), 4 LSB
    // 0 0 0 0 unused
    // uint8_t F{};
    bool Zf{}; // zero flag
    bool Nf{}; // subtract flag
    bool Hf{}; // half-carry flag
    bool Cf{}; // carry-flag

    uint8_t H{};
    uint8_t L{};

    // 16-bit registers - stack pointer, program counter
    uint16_t SP{0};
    uint16_t PC{0};

    // temporary registers for complex operations
    uint8_t W{};
    uint8_t Z{};

    std::vector<std::function<void()>> M_operations{};

    // execute instructions
    // int is status (0 = success, 1 = error)
    int handle_opcode(const uint8_t &opcode);
    int handle_cb_opcode(const uint8_t &opcode);

    void execute_M_operations(); // execute M operations and set state of fetch_opcode to true or false depending on whether all M operations have completed

    uint8_t identify_opcode(uint8_t opcode); // get the next opcode and increment the PC

    void tick(); // single tick

    // load boot rom
    void load_boot_rom();

    // read write memory
    uint8_t _read_memory(const uint16_t &address);
    void _write_memory(const uint16_t &address, const uint8_t &value);

    // registers
    enum class registers {
        A,
        B,
        C,
        D,
        E,
        H,
        L,
        NA // NA means null (if register is SP
    };

  private:
    // debugging only
    std::vector<uint8_t> opcodes{};
    std::vector<uint16_t> pcs{};

    // tick counter
    uint16_t ticks{0};

    // state of action, fetch opcode = true or execute further instructions
    bool fetch_opcode{true};

    // TODO: convert all arguments to pass by reference and make them const
    // micro operations
    // NOTE: d16 = address
    void add_hl();                      // add content from address HL to A
    void add_a_r8(const registers &r8); // add content from register r8 to A
    void bit_b_r8(const registers &r8, uint8_t b); // b is 0 or 1
    void call(const bool &check_z_flag);
    void cp_a_imm8(); // compare immediate next byte with A no effect on A
    void cp_a_hl();   // compare memory[hl] with A no effect on A
    void inc_or_dec_r8(const registers &r8, const bool &inc); // decrement register
    void inc_or_dec_r16(const registers &r1, const registers &r2, const bool &inc,
                        const bool &sp); // inc or dec r16 register, either SP (stack
                                  // pointer) or 2 individual registers
    void jp_imm16(const bool &check_z_flag); // absolute jump, check z flag
    void jr_s8(const bool &check_z_flag, const bool &nz); // relative jump, check z flag, nz means check if z flag is not 0
    void ld_imm16_a(const bool &to_a); // to a means should i load imm16 to a, or a to
                                // imm16, covers ld_imm16_a and ld_a_imm16
    void ld_r_r(const registers &r_to,
                const registers
                    &r_from); // load value from 1 register to another register
    void ld_r_imm8(const registers &r);
    void ld_rr_address(const registers &r1, const registers &r2, const bool &sp);
    void ld_hl_a(const bool &increment);      // hl+ and hl-, and hl
    void ld_hl_r8(const registers &r); // hl+ and hl-, and hl
    void pop_rr(const registers &r1, const registers &r2);
    void push_rr(const registers &r1, const registers &r2);
    void ret();                     // return
    void rla();                     // rotate left accumulator
    void sla_r(const registers &r); // rotate left accumulator
    void rl_r(const registers &r);
    void sub_r(const registers &r);
    void xor_r(const registers &r);
    void ld_c_a(const bool &to_a);    // also known as LDH (C), A, to_a reverses LD
    void ld_imm8_a(const bool &to_a); // also known as LDH (n), A, to_a reverses LD
    void ld_rr_a(const registers &r1,
                 const registers
                     &r2); // load memory value from register pair address to A

    // utility //
    uint16_t
    _combine_2_8bits(const uint8_t &r1,
                     const uint8_t &r2); // get the register pairs value by
                                         // combining them into a 16 bit uint
    std::tuple<uint8_t, uint8_t> _split_16bit(const uint16_t &r16);

    // all return result, zero, subtract, half-carry, carry
    std::tuple<uint8_t, bool, bool, bool, bool> _addition_8bit(uint8_t x,
                                                               uint8_t y);
    std::tuple<uint8_t, bool, bool, bool, bool> _subtraction_8bit(uint8_t x,
                                                                  uint8_t y);
    std::tuple<uint16_t, bool, bool, bool, bool> _addition_16bit(uint16_t x,
                                                                 uint16_t y);

    uint8_t _flags_to_byte()
        const; // const after function declaration makes it a compiler error to
               // edit class members inside the function

    // TODO: make enum
    uint8_t *_get_register(
        const registers
            &r8); // get the pointer to the register value based on char8
};
