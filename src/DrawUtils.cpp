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

std::vector<sf::RectangleShape>
DrawUtils::draw_display_buffer(const uint8_t (&display)[64 * 32]) {
    // TODO: see if there's a way to avoid redrawing everything
    std::vector<sf::RectangleShape> pixels{};

    for (unsigned int i = 0; i < sizeof(display); ++i) {
        if (display[i] == 1) {
            int x = i % 64;
            int y = i / 64;
            pixels.push_back(DrawUtils::add_pixel(
                {static_cast<float>(x), static_cast<float>(y)}, 255, 255, 255));
        }
    }
    return pixels;
}
