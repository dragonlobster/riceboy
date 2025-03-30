#include "draw.h"

// TODO: can be constexpr
sf::RectangleShape draw::add_pixel(sf::Vector2f position, std::uint8_t red,
                                        std::uint8_t green, std::uint8_t blue) {
    sf::RectangleShape pixel{};
    pixel.setSize({1.f, 1.f});
    pixel.setFillColor({red, green, blue});

    float x = position.x * draw::SCALE;
    float y = position.y * draw::SCALE;

    pixel.setPosition({x, y});

    // TODO: make scale a float
    pixel.setScale({static_cast<float>(draw::SCALE),
                    static_cast<float>(draw::SCALE)});
    return pixel;
}

sf::Vertex draw::add_vertex(sf::Vector2f position, std::uint8_t red, std::uint8_t green, std::uint8_t blue) {

    sf::Vertex vertex{};

    float x = position.x * draw::SCALE;
    float y = position.y * draw::SCALE;

    vertex.position = {x, y};
    vertex.color = {red, green, blue};

    return vertex;

}
