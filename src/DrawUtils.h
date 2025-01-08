#pragma once

#include <SFML/Graphics.hpp>
//#include "Chip8.h"

class DrawUtils
{
public:
	static const int SCALE{ 16 };
	static std::vector<sf::RectangleShape> draw_display_buffer(const uint8_t(&display)[64*32]);

private:
	static sf::RectangleShape add_pixel(sf::Vector2f position, std::uint8_t red, std::uint8_t green, std::uint8_t blue);

};
