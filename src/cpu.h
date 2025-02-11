#pragma once

#include "MMU.h"
#include <cstdint>
#include <functional>
#include <iomanip>
#include <iostream>
#include <tuple>
#include <vector>
#include <array>

class CPU {
    // TODO: make private

  public:
    // pointer to mmu
    CPU(MMU &mmu);    // pass by reference
    MMU *gb_mmu{}; // the central mmu

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

    // interrupts, ime is either 0 or 1
    bool ei_delay{false};
    bool ime{false};

    // HALT flag
    bool halt{false};

    std::vector<std::function<void()>> M_operations{};

    // execute instructions
    // int is status (0 = success, 1 = error)
    int handle_opcode(const uint8_t opcode);
    int handle_cb_opcode(const uint8_t cb_opcode);

    void execute_M_operations(); // execute M operations and set state of fetch_opcode to true or false depending on whether all M operations have completed

    uint8_t identify_opcode(const uint8_t opcode); // get the next opcode and increment the PC

    void tick(); // single cpu tick
    void interrupt_tick(); // interrupt tick
    void timer_tick(); // timer tick

    void handle_interrupts(); // handle interrupts

    // load boot rom
    void load_boot_rom();
    bool boot_rom_complete{false};

    // load
    void prepare_rom(std::string path);
    void load_rom();
    std::string rom;

    // read write memory
    uint8_t _read_memory(const uint16_t address);
    uint8_t* _read_pointer(const uint16_t address);
    void _write_memory(const uint16_t address, const uint8_t value);

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

    enum class conditions {
        NA,
        Z,
        NZ,
        C,
        NC
    };

    enum class bitops {
        AND,
        XOR,
        OR
    };

  private:
    // debugging only
    std::vector<uint8_t> opcodes{};
    std::vector<uint16_t> pcs{};

    // tick counter
    uint16_t ticks{0};
    uint16_t interrupt_ticks{0};
    uint16_t timer_ticks{0};
    uint16_t tima_ticks{0};
    uint16_t div_ticks{0};

    // state of action, fetch opcode = true or execute further instructions
    bool fetch_opcode{true};

    // TODO: convert all arguments to pass by reference and make them const
    // micro operations
    // NOTE: d16 = address
    void add_a_hl();                      // add content from address HL to A
    void add_hl_rr(const registers r1, const registers r2, const bool sp);                      // add content from address HL to A
    void add_a_r8(const registers r8); // add content from register r8 to A
    void add_sp_s8();
    void bit_b_r8(const registers r8, uint8_t b); // b is 0 or 1
    void call(const conditions condition);
    void cp_a_imm8(); // compare immediate next byte with A no effect on A
    void cp_a_r(const registers r, const bool hl);   // compare memory[hl] with A no effect on A
    void inc_or_dec_r8(const registers r8, const bool inc); // decrement register
    void inc_or_dec_hl(const bool inc); // decrement register
    void inc_or_dec_r16(const registers r1, const registers r2, const bool inc,
                        const bool sp); // inc or dec r16 register, either SP (stack
                                  // pointer) or 2 individual registers
    void jp_imm16(const conditions condition); // absolute jump, check z flag
    void jp_hl();
    void jr_s8(conditions condition); // relative jump, check z flag, nz means check if z flag is not 0
    void ld_imm16_a(const bool to_a); // to a means should i load imm16 to a, or a to
    void ld_imm16_sp(); // to a means should i load imm16 to a, or a to
                                // imm16, covers ld_imm16_a and ld_a_imm16
    void ld_r_r(const registers r_to,
                const registers
                    r_from); // load value from 1 register to another register
    void ld_r_imm8(const registers r);
    void ld_hl_imm8();
    void ld_rr_address(const registers r1, const registers r2, const bool sp);
    void ld_hl_a(const bool increment, const bool to_a);      // hl+ and hl-, and hl
    void ld_hl_r8(const registers r, const bool to_hl); // hl+ and hl-, and hl
    void ld_hl_sp_s8();
    void ld_sp_hl();
    void pop_rr(const registers r1, const registers r2, const bool af = false);
    void push_rr(const registers r1, const registers r2, const bool af = false);
    void ret(conditions condition, bool ime_condition = false);                     // return
    void rl_r(const registers r, const bool hl = false, const bool z_flag = false);                     // rotate left accumulator
    void rlc_r(const registers r, const bool hl = false, const bool z_flag = false);
    void rr_r(const registers r, const bool hl = false, const bool z_flag = false);                     // rotate right accumulator
    void rrc_r(const registers r, const bool hl = false, const bool z_flag = false);                     // rotate right accumulator
    void sla_r(const registers r, const bool hl = false); // rotate left accumulator
    void sra_r(const registers r, const bool hl = false); // rotate right accumulator
    void swap_r(const registers r, const bool hl = false);
    void srl_r(const registers r, const bool hl = false);
    // TODO: combine srl and rr, try to find other combos
    void sub(const registers r, const bool hl);
    void xor_r(const registers r, const bool hl);
    void ld_c_a(const bool to_a);    // also known as LDH (C), A, to_a reverses LD
    void ld_imm8_a(const bool to_a); // also known as LDH (n), A, to_a reverses LD
    void ld_a_rr(const registers r1,
                 const registers
                     r2, const bool to_a); // load memory value from register pair address to A
    void cpl();
    void scf();
    void ccf();
    void adc_or_sbc(const registers r1, const bool hl, const bool add);
    void and_r(const registers r, const bool hl);
    void or_r(const registers r, const bool hl);

    void add_or_sub_a_imm8(const bool add);
    void adc_or_sbc_imm8(const bool add);

    void and_xor_or_imm8(bitops op);

    void rst(const uint8_t opcode);

    void res_or_set(const uint8_t bit, const registers r, const bool set, const bool hl = false);

    void bit(const uint8_t bit, const registers r, const bool hl = false);

    void daa();

    // interrupts
    void ei_or_di(const bool ei);

    // utility //
    uint16_t
    _combine_2_8bits(const uint8_t r1,
                     const uint8_t r2); // get the register pairs value by
                                         // combining them into a 16 bit uint
    std::tuple<uint8_t, uint8_t> _split_16bit(const uint16_t r16);

    // all return result, zero, subtract, half-carry, carry
    template <typename... Args>
    std::tuple<uint8_t, bool, bool, bool, bool> _addition_8bit(Args... args);

    std::tuple<uint16_t, bool, bool, bool, bool>
    _addition_16bit(uint16_t x, uint16_t y,  bool s8 = false);
    std::tuple<uint8_t, bool, bool, bool, bool> _subtraction_8bit(uint8_t x, uint8_t y, uint8_t Cf = 0);

    uint8_t _flags_to_byte()
        const; // const after function declaration makes it a compiler error to
               // edit class members inside the function

    std::tuple<bool, bool, bool, bool> _byte_to_flags(uint8_t byte);

    // TODO: make enum
    uint8_t *_get_register(
        const registers
            r8); // get the pointer to the register value based on char8


    // opcode lookup tables
    std::array<registers, 8> rp_table{
        registers::B, registers::C,  registers::D, registers::E, registers::H,
        registers::L, registers::NA, registers::NA}; // sp or af is the last

    std::array<registers, 8> r_table{
        registers::B, registers::C, registers::D,  registers::E,
        registers::H, registers::L, registers::NA, registers::A}; // na means HL
};
