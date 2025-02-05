#include "cpu.h"
#include <array>

int cpu::handle_opcode(const uint8_t opcode) {
    /*
    x = the opcode's 1st octal digit (i.e. bits 7-6)
    y = the opcode's 2nd octal digit (i.e. bits 5-3)
    z = the opcode's 3rd octal digit (i.e. bits 2-0)
    p = y rightshifted one position (i.e. bits 5-4)
    q = y modulo 2 (i.e. bit 3)
    */
    uint8_t x = opcode >> 6 & 3; // bits 7 - 6
    uint8_t y = opcode >> 3 & 7; // bits 3 - 5
    uint8_t z = opcode & 7;      // bits 2 - 0

    uint8_t p = y >> 1; // bit 5 - 4
    uint8_t q = y % 2;  // bit 3


    const registers rp_r1 = rp_table[2 * p];
    const registers rp_r2 = rp_table[(2 * p) + 1];

    switch (x) {
    // beginning of highest switch case (x)
    case 0:
        // x == 0
        switch (z) {
        // beginning of switch case for z
        case 0:
            switch (y) {
            // beginning of switch case for y
            case 0: break; // NOP

            case 1: ld_imm16_sp(); break;
            case 2:
                // TODO: STOP
                break;

            case 3: jr_s8(conditions::NA); break;

            case 4: jr_s8(conditions::NZ); break;

            case 5: jr_s8(conditions::Z); break;

            case 6: jr_s8(conditions::NC); break;
            case 7:
                jr_s8(conditions::C);
                break;
                // end of switch case for y
            }
            break;

        case 1:
            switch (q) {
            case 0: ld_rr_address(rp_r1, rp_r2, rp_r1 == registers::NA); break;

            case 1: add_hl_rr(rp_r1, rp_r2, rp_r1 == registers::NA); break;
            }
            break;

        case 2:
            switch (q) {
            case 0:
                switch (p) {
                case 0: ld_a_rr(registers::B, registers::C, false); break;

                case 1: ld_a_rr(registers::D, registers::E, false); break;

                case 2: ld_hl_a(true, false); break;

                case 3: ld_hl_a(false, false); break;
                }
                break;

            case 1:
                switch (p) {
                case 0: ld_a_rr(registers::B, registers::C, true); break;

                case 1: ld_a_rr(registers::D, registers::E, true); break;

                case 2: ld_hl_a(true, true); break;

                case 3: ld_hl_a(false, true); break;
                }
                break;
            }
            break;

        case 3:
            switch (q) {
            case 0:
                inc_or_dec_r16(rp_r1, rp_r2, true, rp_r1 == registers::NA);
                break;

            case 1:
                inc_or_dec_r16(rp_r1, rp_r2, false, rp_r1 == registers::NA);
                break;
            }
            break;

        case 4:
            // inc hl, or others
            if (r_table[y] == registers::NA) {
                inc_or_dec_hl(true);
            } else {
                inc_or_dec_r8(r_table[y], true);
            }
            break;

        case 5:
            // inc hl, or others
            if (r_table[y] == registers::NA) {
                inc_or_dec_hl(false);
            } else {
                inc_or_dec_r8(r_table[y], false);
            }
            break;

        case 6:
            if (r_table[y] == registers::NA) {
                ld_hl_imm8();
            } else {
                ld_r_imm8(r_table[y]);
            }
            break;

        case 7:
            switch (y) {
            case 0: rlc_r(registers::A); break;

            case 1: rrc_r(registers::A); break;

            case 2: rl_r(registers::A); break;

            case 3: rr_r(registers::A); break;

            case 4: daa(); break;

            case 5: cpl(); break;

            case 6: scf(); break;

            case 7: ccf(); break;
            }
            break;
            // end of switch case for z
        }
        break;

    case 1:
        if (z == 6 && y == 6) {
            // HALT
            // TODO: halt bug
            this->halt = true;
        } else {
            if (r_table[y] == registers::NA) {
                ld_hl_r8(r_table[z], true);
            } else if (r_table[z] == registers::NA) {
                // LD r8 hl
                ld_hl_r8(r_table[y], false);
            } else {
                ld_r_r(r_table[y], r_table[z]);
            }
        }
        break;

    case 2:
        // alu[y], z
        switch (y) {
        case 0:
            if (r_table[z] == registers::NA) {
                add_a_hl();
            } else {
                add_a_r8(r_table[z]);
            }
            break;

        case 1:
            adc_or_sbc(r_table[z], r_table[z] == registers::NA, true);
            break;

        case 2: sub(r_table[z], r_table[z] == registers::NA); break;

        case 3:
            adc_or_sbc(r_table[z], r_table[z] == registers::NA, false);
            break;

        case 4: and_r(r_table[z], r_table[z] == registers::NA); break;

        case 5: xor_r(r_table[z], r_table[z] == registers::NA); break;

        case 6: or_r(r_table[z], r_table[z] == registers::NA); break;

        case 7: cp_a_r(r_table[z], r_table[z] == registers::NA); break;
        }
        break;

    case 3: {
        switch (z) {
        case 0:
            switch (y) {
            case 0: ret(conditions::NZ); break;

            case 1: ret(conditions::Z); break;

            case 2: ret(conditions::NC); break;

            case 3: ret(conditions::C); break;

            case 4: ld_imm8_a(false); break;

            case 5: add_sp_s8(); break;

            case 6: ld_imm8_a(true); break;

            case 7: ld_hl_sp_s8(); break;
            }
            break;

        case 1:
            switch (q) {
            case 0: pop_rr(rp_r1, rp_r2, rp_r1 == registers::NA); break;

            case 1:
                switch (p) {
                case 0: ret(conditions::NA); break;

                case 1: ret(conditions::NA, true); break;

                case 2: jp_hl(); break;

                case 3: ld_sp_hl(); break;
                }
                break;
            }
            break;

        case 2:
            switch (y) {
            case 0: jp_imm16(conditions::NZ); break;

            case 1: jp_imm16(conditions::Z); break;

            case 2: jp_imm16(conditions::NC); break;

            case 3: jp_imm16(conditions::C); break;

            case 4: ld_c_a(false); break;

            case 5: ld_imm16_a(false); break;

            case 6: ld_c_a(true); break;

            case 7: ld_imm16_a(true); break;
            }
            break;

        case 3:
            switch (y) {
            case 0: jp_imm16(conditions::NA); break;

            case 1: {
                // TODO: cb create another CB handler
                const uint8_t cb_opcode = this->_read_memory(this->PC);
                this->PC++; // increment the program counter
                this->handle_cb_opcode_v2(cb_opcode);
                break;
            }
            case 6: ei_or_di(false); break;

            case 7: ei_or_di(true); break;
            }
            break;

        case 4:
            // call conditions
            switch (y) {
            case 0: call(conditions::NZ); break;

            case 1: call(conditions::Z); break;

            case 2: call(conditions::NC); break;

            case 3: call(conditions::C); break;
            }
            break;

        case 5:
            switch (q) {
            case 0: push_rr(rp_r1, rp_r2, rp_r1 == registers::NA); break;

            case 1:
                if (p == 0) {
                    call(conditions::NA);
                }
                break;
            }
            break;

        case 6:
            switch (y) {
            case 0: add_or_sub_a_imm8(true); break;

            case 1: adc_or_sbc_imm8(true); break;

            case 2: add_or_sub_a_imm8(false); break;

            case 3: adc_or_sbc_imm8(false); break;

            case 4: and_xor_or_imm8(bitops::AND); break;

            case 5: and_xor_or_imm8(bitops::XOR); break;

            case 6: and_xor_or_imm8(bitops::OR); break;

            case 7: cp_a_imm8(); break;
            }
            break;

        case 7: rst(opcode); break;
        }
        break;
    }
    // end of highest switch case (x)
    default: {
        // print out the opcodes we didnt' implement
        std::cout << "not implemented: " //<< static_cast<unsigned int>(opcode)
                  << "hex: 0x" << std::hex << static_cast<unsigned int>(opcode)
                  << std::endl;
        // system("pause");
        std::cin.get();
        return 0;
        break;
    }
    }

    return 1;
}

int cpu::handle_cb_opcode_v2(const uint8_t cb_opcode) {

    uint8_t x = cb_opcode >> 6 & 3; // bits 7 - 6
    uint8_t y = cb_opcode >> 3 & 7; // bits 3 - 5
    uint8_t z = cb_opcode & 7;      // bits 2 - 0

    switch (x) {
    case 0:
        switch (y) {
        case 0: rlc_r(r_table[z], r_table[z] == registers::NA, true); break;
        case 1: rrc_r(r_table[z], r_table[z] == registers::NA, true); break;
        case 2: rl_r(r_table[z], r_table[z] == registers::NA, true); break;
        case 3: rr_r(r_table[z], r_table[z] == registers::NA, true); break;
        case 4: sla_r(r_table[z], r_table[z] == registers::NA); break;
        case 5: sra_r(r_table[z], r_table[z] == registers::NA); break;
        case 6: swap_r(r_table[z], r_table[z] == registers::NA); break;
        case 7: srl_r(r_table[z], r_table[z] == registers::NA); break;
        }
        break;
    case 1: bit(y, r_table[z], r_table[z] == registers::NA); break;
    case 2: res_or_set(y, r_table[z], false, r_table[z] == registers::NA); break;
    case 3: res_or_set(y, r_table[z], true, r_table[z] == registers::NA); break;
    }

    return 1;
}

int cpu::handle_cb_opcode(const uint8_t cb_opcode) {
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
