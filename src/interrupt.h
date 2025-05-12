#pragma once

#include <cstdint>
#include <tuple>

class interrupt {
  public:
    // if
    uint8_t interrupt_flags{};

    // ie
    uint8_t interrupt_enable_flag{false};

    // ime
    bool ime{false};

    // ei delay
    bool ei_delay{false};

    // interrupts and masks
    enum class interrupts {
        none = 0,
        vblank = 0x40,
        lcd = 0x48,
        timer = 0x50,
        serial = 0x58,
        joypad = 0x60
    };

    enum class if_mask {
        none = 0xff,   // should be 0xff so IF stays unmodified
        vblank = 0xfe, // 1111 1110
        lcd = 0xfd,    // 1111 1101
        timer = 0xfb,  // 1111 1011
        serial = 0xf7, // 1111 0111
        joypad = 0xef  // 1110 1111
    };

    // set current interrupt based on the new IE / IF
    void check_current_interrupt(); 

    // current interrupt and current if mask
    interrupts current_interrupt{interrupts::none};
    if_mask current_if_mask{if_mask::none};
};
