#pragma once

#include "joypad.h"

template <typename Key>
void HandleInput(joypad &joypad, const Key *key, const bool pressed) {
    if (!key)
        return;
    switch (key->scancode) {
    case sf::Keyboard::Scancode::W:
       joypad.handle_button(joypad::buttons::Up, pressed);
        break;
    case sf::Keyboard::Scancode::A:
        joypad.handle_button(joypad::buttons::Left, pressed);
        break;
    case sf::Keyboard::Scancode::S:
        joypad.handle_button(joypad::buttons::Down, pressed);
        break;
    case sf::Keyboard::Scancode::D:
        joypad.handle_button(joypad::buttons::Right, pressed);
        break;
    case sf::Keyboard::Scancode::Enter:
        joypad.handle_button(joypad::buttons::Start, pressed);
        break;
    case sf::Keyboard::Scancode::RShift:
        joypad.handle_button(joypad::buttons::Select, pressed);
        break;
    case sf::Keyboard::Scancode::J:
        joypad.handle_button(joypad::buttons::A, pressed);
        break;
    case sf::Keyboard::Scancode::K:
        joypad.handle_button(joypad::buttons::B, pressed);
        break;
    }
}
