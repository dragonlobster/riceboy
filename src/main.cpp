#include "DrawUtils.h"
#include "Gameboy.h"
#include "HandleInput.h"
#include "vendor/tinyfiledialogs.h"
#include <SFML/Graphics.hpp>

int main() {
    // TODO: gb

    // open file dialog to load ROM
    // const char* lFilterPatterns[1] = { "*.gb" };
    // const char* ROM = tinyfd_openFileDialog("Open a Gameboy ROM", NULL, 1,
    // lFilterPatterns, "*.gb", 0); if (!ROM) { exit(0); }
    sf::RenderWindow window(
        sf::VideoMode({160 * DrawUtils::SCALE, 144 * DrawUtils::SCALE}),
        "RiceBoy");

    // window.setFramerateLimit(60);
    window.setVerticalSyncEnabled(true);

    // initialize gameboy on heap
    std::unique_ptr<Gameboy> riceboy = std::make_unique<Gameboy>(window);

    // TODO: load the BOOT ROM
    riceboy->gb_cpu.load_boot_rom();

    // TODO: load chosen cartridge
    // std::string rom =
    // "BOOT/mooneye-gb_hwtests/acceptance/timer/rapid_toggle.gb";

    std::array<std::string, 10> blargg{
        "BOOT/blargg/cpu_instrs/cpu_instrs.gb",
        "BOOT/blargg/instr_timing/instr_timing.gb",
        //"BOOT/blargg/interrupt_time/interrupt_time.gb", // fail (expected,
        // need CGB)
        "BOOT/blargg/mem_timing/mem_timing.gb",
        "BOOT/blargg/mem_timing-2/mem_timing.gb",
        "BOOT/blargg/oam_bug/oam_bug.gb", // not implemented yet
        "BOOT/blargg/halt_bug.gb"};       // not implemented yet

    std::array<std::string, 13> mooneye_timing{
        "BOOT/mooneye-gb_hwtests/acceptance/timer/div_write.gb",
        "BOOT/mooneye-gb_hwtests/acceptance/timer/rapid_toggle.gb",
        "BOOT/mooneye-gb_hwtests/acceptance/timer/tim00.gb",
        "BOOT/mooneye-gb_hwtests/acceptance/timer/tim00_div_trigger.gb",
        "BOOT/mooneye-gb_hwtests/acceptance/timer/tim01.gb",
        "BOOT/mooneye-gb_hwtests/acceptance/timer/tim01_div_trigger.gb",
        "BOOT/mooneye-gb_hwtests/acceptance/timer/tim10.gb",
        "BOOT/mooneye-gb_hwtests/acceptance/timer/tim10_div_trigger.gb",
        "BOOT/mooneye-gb_hwtests/acceptance/timer/tim11.gb",
        "BOOT/mooneye-gb_hwtests/acceptance/timer/tim11_div_trigger.gb",
        "BOOT/mooneye-gb_hwtests/acceptance/timer/tima_reload.gb",
        "BOOT/mooneye-gb_hwtests/acceptance/timer/tima_write_reloading.gb",
        "BOOT/mooneye-gb_hwtests/acceptance/timer/tma_write_reloading.gb"};

    std::array<std::string, 1> mooneye_interrupts{
        "BOOT/mooneye-gb_hwtests/acceptance/interrupts/ie_push.gb"};

    riceboy->gb_cpu.prepare_rom("BOOT/dmg-acid2.gb");
    // riceboy->gb_cpu.prepare_rom(mooneye_timing[12]);

    // sf::RenderWindow window(sf::VideoMode({ Chip8::DISPLAY_WIDTH *
    // DrawUtils::SCALE, Chip8::DISPLAY_HEIGHT * DrawUtils::SCALE }),
    // "RiceBoy");

    // double accumulator{0};
    // double last_frame_time{0};

    // frame clock (avoiding setFrameRateLimit imprecision)
    sf::Clock frame_clock;

    while (window.isOpen()) {

        while (const std::optional event = window.pollEvent()) {
            // Close window: exit
            if (event->is<sf::Event::Closed>())
                window.close();
        }

        double frame_time = frame_clock.getElapsedTime().asMilliseconds();

        double target_frame_time = 1.f / (((1 << 22) / 70224) * 1000);
        // double target_frame_time = 60;

        // 70224 ipf - clock speed 4194304Hz
        while (frame_time >= target_frame_time) {
            for (unsigned int i = 0; i < 70224; ++i) {
                riceboy->tick();
            }
            // std::cout << "hello" << frame_time << '\n';
            frame_time -= target_frame_time;
        }
        // 70224
    }

    return 0;
}