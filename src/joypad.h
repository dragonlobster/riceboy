#pragma once

#include <cstdint>

class joypad {

  public:
    bool dpad{true};
    bool buttons{true};

    bool start{false};
    bool select{false};
    bool b{false};
    bool a{false};
    
    bool down{false};
    bool up{false};
    bool left{false};
    bool right{false};
    //uint16_t ff00_joyp{0xcf};

    uint8_t handle_read();
    void handle_write(uint8_t value);
};
