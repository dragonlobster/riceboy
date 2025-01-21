#pragma once

#include <SFML/Graphics.hpp>

class DrawUtils {
  public:
    static const int SCALE{5};
    static sf::RectangleShape add_pixel(sf::Vector2f position, std::uint8_t red,
                                        std::uint8_t green, std::uint8_t blue);

    static sf::Vertex add_vertex(sf::Vector2f position, std::uint8_t red,
                                        std::uint8_t green, std::uint8_t blue);
};
