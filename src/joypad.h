#pragma once

#include <cstdint>

class joypad {

  public:
    uint8_t ff00_joyp{0xcf};

    bool button_select{false};
    bool dpad_select{false};

    enum class buttons { Up, Down, Left, Right, Start, Select, A, B };

    bool up{false};
    bool down{false};
    bool left{false};
    bool right{false};
    bool start{false};
    bool select{false};
    bool a{false};
    bool b{false};

    void handle_button(buttons button, bool pressed);

    void handle_write(uint8_t value);

    void update_joyp_register();
};
