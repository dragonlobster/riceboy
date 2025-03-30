#include "draw.h"
#include "gameboy.h"
#include "handleinput.h"
#include "vendor/tinyfiledialogs.h"
#include <SFML/Graphics.hpp>

int main() {
    // TODO: gb

    // open file dialog to load ROM
    // const char* lFilterPatterns[1] = { "*.gb" };
    // const char* ROM = tinyfd_openFileDialog("Open a Gameboy ROM", NULL, 1,
    // lFilterPatterns, "*.gb", 0); if (!ROM) { exit(0); }
    sf::RenderWindow window(
        sf::VideoMode({160 * draw::SCALE, 144 * draw::SCALE}), "RiceBoy");

    // window.setFramerateLimit(60);
    window.setVerticalSyncEnabled(true);

    // initialize gameboy on heap
    std::unique_ptr<gameboy> riceboy = std::make_unique<gameboy>(window);

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
        "BOOT/blargg/oam_bug/oam_bug.gb", // oam corruption bug not implemented
                                          // yet
        "BOOT/blargg/oam_bug/rom_singles/1-lcd_sync.gb", // not implemented yet
        "BOOT/blargg/halt_bug.gb"};                      // not implemented yet

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

    std::array<std::string, 7> mooneye_ppu{
        "BOOT/mooneye-gb_hwtests/manual-only/sprite_priority.gb",
        "BOOT/mooneye-gb_hwtests/acceptance/oam_dma/basic.gb",
        "BOOT/mooneye-gb_hwtests/acceptance/oam_dma/reg_read.gb",
        "BOOT/mooneye-gb_hwtests/acceptance/oam_dma/sources-dmgABCmgbS.gb",
        "BOOT/mooneye-gb_hwtests/acceptance/ppu/hblank_ly_scx_timing-GS.gb",
        "BOOT/mooneye-gb_hwtests/acceptance/ppu/intr_1_2_timing-GS.gb",
        "BOOT/mooneye-gb_hwtests/acceptance/ppu/intr_2_0_timing-GS.gb",
    };

    std::array<std::string, 1> mooneye_cpu{
        "BOOT/mooneye-gb_hwtests/acceptance/instr/daa.gb"};

    // why does mooneye sources-dmgABCmgbS.gb flash before pass (probably
    // related to ppu somehow)

    // riceboy->gb_cpu.prepare_rom(mooneye_ppu[3]);
    riceboy->gb_cpu.prepare_rom(mooneye_timing[0]);
    // riceboy->gb_cpu.prepare_rom(blargg[4]);
    // riceboy->gb_cpu.prepare_rom(mooneye_cpu[0]);
    // riceboy->gb_cpu.prepare_rom(mooneye_interrupts[0]);

    // sf::RenderWindow window(sf::VideoMode({ Chip8::DISPLAY_WIDTH *
    // draw::SCALE, Chip8::DISPLAY_HEIGHT * draw::SCALE }),
    // "RiceBoy");

    // double accumulator{0};
    // double last_frame_time{0};

    // frame clock (avoiding setFrameRateLimit imprecision)
    sf::Clock frame_clock{};

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
            frame_time -= target_frame_time;
        }
        // 70224
    }

    return 0;
}
