#include <SFML/Graphics.hpp>
#include "DrawUtils.h"
#include "Gameboy.h"
#include "HandleInput.h"
#include "vendor/tinyfiledialogs.h"

int main() {
    // TODO: gb

    // open file dialog to load ROM
    // const char* lFilterPatterns[1] = { "*.gb" };
    // const char* ROM = tinyfd_openFileDialog("Open a Gameboy ROM", NULL, 1,
    // lFilterPatterns, "*.gb", 0); if (!ROM) { exit(0); }
    sf::RenderWindow window(sf::VideoMode({160 * DrawUtils::SCALE, 144 * DrawUtils::SCALE}), "RiceBoy");

    // window.setFramerateLimit(60);
    window.setVerticalSyncEnabled(true);

    // initialize gameboy on heap
    std::unique_ptr<Gameboy> riceboy = std::make_unique<Gameboy>(window);

    // TODO: load the BOOT ROM
    riceboy->gb_cpu.load_boot_rom();

    // TODO: load chosen cartridge

    // sf::RenderWindow window(sf::VideoMode({ Chip8::DISPLAY_WIDTH *
    // DrawUtils::SCALE, Chip8::DISPLAY_HEIGHT * DrawUtils::SCALE }),
    // "RiceBoy");

    double accumulator{0};
    double last_frame_time{0};

    // frame clock (avoiding setFrameRateLimit imprecision)
    sf::Clock frame_clock;

    while (window.isOpen()) {

        window.handleEvents(
            // on close
            [&](const sf::Event::Closed &) { window.close(); },

            // on key pressed
            [&](const sf::Event::KeyPressed &key) {
                // keypad 16 keys: 0 - F -> 1234,qwer,asdf,zxcv, either pressed
                // or not pressed (true, false)
                // HandleInput<sf::Event::KeyPressed>(gb.keypad, key, true);
            },

            // on key released
            [&](const sf::Event::KeyReleased &key) {
                // HandleInput<sf::Event::KeyReleased>(gb.keypad, key, false);
            });

        const double current_time =
            frame_clock.getElapsedTime().asMilliseconds(); // get current time
        const double delta_time =
            current_time -
            last_frame_time; // delta always calculates the difference between
                             // now within this frame and last frame
        accumulator += delta_time; // add the delta time to the accumulator, we
                                   // need to wait until it reachs ~1.67
                                   // milliseconds (59.7 fps, ~1.67ms / frame)

        while (accumulator >=
               1000.f /
                   59.7f) { // why while and not if? in the case that we missed
                            // out a whole frame for some reason, double the
                            // instructions will be processed, which is
                            // desirable in case we missed a set of instructions

            // TODO: Gameboy::clockspeed / 59.7275 should be the correct ipf
            for (unsigned int i = 0; i < 70224; ++i) {
                riceboy->gb_cpu.tick();
                riceboy->gb_ppu.tick();
            }

            window.clear(sf::Color::White);
            for (const sf::RectangleShape &d : riceboy->gb_ppu.lcd_dots) {
                window.draw(d);
            }
            window.display();

            // 1pf concept?
            // for (unsigned int i = 0; i < 11; ++i) {
            //	chip8.Cycle();
            //}

            // subtract it down to 0 again (or offset by screen refresh rate /
            // error), basically resetting accumulator
            accumulator -= 1000.f / 59.7f;
        }

        // clear/draw/display

        // TODO: draw

        // display
        // window.display();

        // we are setting the "last frame time" as the current time before going
        // to the next frame
        last_frame_time = current_time;
    }

    return 0;
}
