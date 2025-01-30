#include "cpu.h"

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
    uint8_t z = opcode & 7; // bits 2 - 0

    uint8_t p = y >> 1; // bit 5 - 4
    uint8_t q = y % 2; // bit 3

    std::array<registers, 8> rp_table{registers::B, registers::C, registers::D,
                                      registers::E, registers::H, registers::L,
                                      registers::NA,
                                      registers::NA}; // sp is the last

    std::array<registers, 8> r_table{registers::B, registers::C, registers::D,
                                     registers::E, registers::H, registers::L,
                                     registers::NA, registers::A};

    const registers rp_r1 = rp_table[2 * p];
    const registers rp_r2 = rp_table[(2 * p) + 1];

    switch (x) {
    // beginning of highest switch case (x)
    case 0: {
        // x == 0
        switch (z) {
        // beginning of switch case for z
        case 0: {
            switch (y) {
            // beginning of switch case for y
            case 0: {
                // NOP
                break;
            }
            case 1: {
                // LD (imm16), SP
                ld_imm16_sp();
                break;
            }
            case 2: {
                // STOP
                break;
            }
            case 3: {
                // JR d
                jr_s8(conditions::NA);
                break;
            }
            case 4: {
                jr_s8(conditions::NZ);
                break;
            }
            case 5: {
                jr_s8(conditions::Z);
                break;
            }
            case 6: {
                // jr nc s8
                break;
            }
            case 7: {
                // jr c s8
                break;
            }
            // case 4, 5, 6, 7
            // end of switch case for y
            }
            break;
        }
        case 1: {
            switch (q) {
            case 0: {
                if (rp_r1 == registers::NA) {
                    // sp case
                    ld_rr_address(rp_r1, rp_r2, true);
                } else { ld_rr_address(rp_r1, rp_r2, false); }
                break;
            }
            case 1: {
                // ADD HL, rp[p]
                break;
            }
            }
            break;
        }
        case 2: {
            switch (q) {
            case 0: {
                switch (p) {
                case 0: {
                    ld_a_rr(registers::B, registers::C, false);
                    break;
                }
                case 1: {
                    ld_a_rr(registers::D, registers::E, false);
                    break;
                }
                case 2: {
                    ld_hl_a(true, false);
                    break;
                }
                case 3: {
                    ld_hl_a(false, false);
                    break;
                }
                }
                break;
            }
            case 1: {
                switch (p) {
                case 0: {
                    ld_a_rr(registers::B, registers::C, true);
                    break;
                }
                case 1: {
                    ld_a_rr(registers::D, registers::E, true);
                    break;
                }
                case 2: {
                    ld_hl_a(true, true);
                    break;
                }
                case 3: {
                    ld_hl_a(false, true);
                    break;
                }
                }
                break;
            }
            }
            break;
        }
        case 3: {
            switch (q) {
            case 0: {
                if (rp_r1 == registers::NA) {
                    inc_or_dec_r16(rp_r1, rp_r2, true, true);
                } else { inc_or_dec_r16(rp_r1, rp_r2, true, false); }
                break;
            }
            case 1: {
                if (rp_r1 == registers::NA) {
                    inc_or_dec_r16(rp_r1, rp_r2, false, true);
                } else { inc_or_dec_r16(rp_r1, rp_r2, false, false); }
                break;
            }
            }
            break;
        }
        case 4: {
            // inc hl, or others
            if (r_table[y] == registers::NA) { inc_or_dec_hl(true); } else {
                inc_or_dec_r8(r_table[y], true);
            }
            break;
        }
        case 5: {
            // inc hl, or others
            if (r_table[y] == registers::NA) { inc_or_dec_hl(false); } else {
                inc_or_dec_r8(r_table[y], false);
            }
            break;
        }
        case 6: {
            if (r_table[y] == registers::NA) { ld_hl_imm8(); } else {
                ld_r_imm8(r_table[y]);
            }
            break;
        }
        case 7: {
            // TODO: R operations
            switch (y) {
            case 0: {
                // RLCA
                break;
            }
            case 1: {
                // RRCA
                break;
            }
            case 2: {
                rla();
                break;
            }
            case 3: {
                // RRA
                break;
            }
            case 4: {
                // DAA
                break;
            }
            case 5: {
                // CPL
                break;
            }
            case 6: {
                // SCF
                break;
            }
            case 7: {
                // CCF
                break;
            }
            }
            break;
        }
        // end of switch case for z
        }
        break;
    }
    case 1: {
        if (z == 6 && y == 6) {
            // HALT
        } else {
            if (r_table[y] == registers::NA) {
                ld_hl_r8(r_table[z]);
            }
            else if (r_table[z] == registers::NA) {
                // LD r8 hl
            }
            else {
                ld_r_r(r_table[y], r_table[z]);
            }
        }
        break;
    }
    case 2: {
        // alu[y], z
        switch (y) {
        case 0: {
            if (r_table[z] == registers::NA) {
                // add a, hl
                add_a_hl();
            } else { add_a_r8(r_table[z]); }
            break;
        }
        case 1: {
            // ADC
            break;
        }
        case 2: {
            if (r_table[z] == registers::NA) {
                // sub hl
            } else { sub_r(r_table[z]); }
            break;
        }
        case 3: {
            //sbc
            break;
        }
        case 4: {
            // and
            break;
        }
        case 5: {
            if (r_table[z] == registers::NA) {
                // xor hl
            } else { xor_r(r_table[z]); }
            break;
        }
        case 6: {
            // or
            break;
        }
        case 7: {
            if (r_table[z] == registers::NA) { cp_a_hl(); } else {
                // cp r
            }
            break;
        }
        }
        break;
    }
    case 3: {
        switch (z) {
        case 0: {
            switch (y) {
            case 0: {
                // ret NZ
                break;
            }
            case 1: {
                // ret Z
                break;
            }
            case 2: {
                // ret NC
                break;
            }
            case 3: {
                // ret C
                break;
            }
            case 4: {
                // LD (n), A or LDH (n), A
                ld_imm8_a(false);
                break;
            }
            case 5: {
                // add sp, s8
                break;
            }
            case 6: {
                // ld a (imm8) or ldh a (imm8)
                ld_imm8_a(true);
                break;
            }
            case 7: {
                // ld hl, sp+s8
                break;
            }
            }
            break;
        }
        case 1: {
            switch (q) {
            case 0: {
                // TODO: handle pop AF
                if (rp_r1 == registers::NA) {
                    // pop AF
                } else { pop_rr(rp_r1, rp_r2); }
                break;
            }
            case 1: {
                switch (p) {
                case 0: {
                    ret();
                    break;
                }
                case 1: {
                    // reti
                    break;
                }
                case 2: {
                    // jp hl
                    break;
                }
                case 3: {
                    // ld sp, hl
                    break;
                }
                }
                break;
            }
            }
            break;
        }
        case 2: {
            switch (y) {
            case 0: {
                // jmp nz
                break;
            }
            case 1: {
                jp_imm16(true);
                break;
            }
            case 2: {
                // jmp NC
                break;
            }
            case 3: {
                // jmp C
                break;
            }
            case 4: {
                ld_c_a(false);
                break;
            }
            case 5: {
                ld_imm16_a(false);
                break;
            }
            case 6: {
                ld_c_a(true);
                break;
            }
            case 7: {
                ld_imm16_a(true);
                break;
            }
            }
            break;
        }
        case 3: {
            switch (y) {
            case 0: {
                jp_imm16(false);
                break;
            }
            case 1: {
                // TODO: cb create another CB handler
                const uint8_t cb_opcode = this->_read_memory(this->PC);
                this->PC++; // increment the program counter
                this->handle_cb_opcode(cb_opcode);
                break;
            }
            case 6: {
                // DI
                break;
            }
            case 7: {
                // EI
                break;
            }
            }
            break;
        }
        case 4: {
            // call conditions
            switch (y) {
            case 0: {
                // call NZ
                break;
            }
            case 1: {
                call(true);
                break;
            }
            case 2: {
                // call nc
                break;
            }
            case 3: {
                // call c
                break;
            }
            }
            break;
        }
        case 5: {
            switch (q) {
            case 0: {
                if (rp_r1 == registers::NA) {
                    // handle push AF
                }
                else {
                    push_rr(rp_r1, rp_r2);
                }
                break;
            }
                case 1: {
                if (p == 0) {
                    call(false);
                }
                break;
            }
            }
            break;
        }
        case 6: {
            switch (y) {
            case 0: {
                // add a imm8
                break;
            }
            case 1: {
                // adc a imm8
                break;
            }
            case 2: {
                // sub imm8
                break;
            }
            case 3: {
                // sbc a, imm8
                break;
            }
            case 4: {
                // and a, imm8
                break;
            }
            case 5: {
                // xor imm8
                break;
            }
            case 6: {
                // or imm8
                break;
            }
            case 7: {
                cp_a_imm8();
                break;
            }
            }
            break;
        }
        case 7: {
            // rst y*8
            break;
        }
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
