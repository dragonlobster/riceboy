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
    std::string rom = "BOOT/instr_timing.gb";
    //std::string rom = "BOOT/cpu_instrs.gb";
    //std::string rom = "BOOT/individual_cpu/01-special.gb";
    //std::string rom = "BOOT/individual_cpu/02-interrupts.gb"; // FAILED
    //std::string rom = "BOOT/individual_cpu/03-op sp,hl.gb"; // n and zero flag always cleared
    //std::string rom = "BOOT/individual_cpu/04-op r,imm.gb";
    //std::string rom = "BOOT/individual_cpu/05-op rp.gb";
    //std::string rom = "BOOT/individual_cpu/06-ld r,r.gb";
    //std::string rom = "BOOT/individual_cpu/07-jr,jp,call,ret,rst.gb";
    //std::string rom = "BOOT/individual_cpu/08-misc instrs.gb";
    //std::string rom = "BOOT/individual_cpu/09-op r,r.gb";
    //std::string rom = "BOOT/individual_cpu/10-bit ops.gb";
    //std::string rom = "BOOT/individual_cpu/11-op a,(hl).gb";
    riceboy->gb_cpu.prepare_rom(rom);

    // sf::RenderWindow window(sf::VideoMode({ Chip8::DISPLAY_WIDTH *
    // DrawUtils::SCALE, Chip8::DISPLAY_HEIGHT * DrawUtils::SCALE }),
    // "RiceBoy");

    //double accumulator{0};
    //double last_frame_time{0};

    // frame clock (avoiding setFrameRateLimit imprecision)
    sf::Clock frame_clock;

    while (window.isOpen()) {

        window.handleEvents(
            // on close
            [&](const sf::Event::Closed &) { window.close(); });

        double frame_time = frame_clock.getElapsedTime().asMilliseconds();

        double target_frame_time = 1.f / (((1 << 22) / 70224) * 1000);
        // double target_frame_time = 60;

        // 70224 ipf - clock speed 4194304Hz
        while (frame_time >= target_frame_time) {
            for (unsigned int i = 0; i < 70224; ++i) {
                riceboy->tick();
            }
            //std::cout << "hello" << frame_time << '\n';
            frame_time -= target_frame_time;
        }
        // 70224
    }

    return 0;
}
