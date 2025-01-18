#include "DrawUtils.h"

// TODO: can be constexpr
sf::RectangleShape DrawUtils::add_pixel(sf::Vector2f position, std::uint8_t red,
                                        std::uint8_t green, std::uint8_t blue) {
    sf::RectangleShape pixel{};
    pixel.setSize({1.f, 1.f});
    pixel.setFillColor({red, green, blue});

    float x = position.x * DrawUtils::SCALE;
    float y = position.y * DrawUtils::SCALE;

    pixel.setPosition({x, y});

    // TODO: make scale a float
    pixel.setScale({static_cast<float>(DrawUtils::SCALE),
                    static_cast<float>(DrawUtils::SCALE)});
    return pixel;
}
