include(FetchContent)
FetchContent_Declare(SFML
    GIT_REPOSITORY https://github.com/SFML/SFML.git
    GIT_TAG 3.0.0
    GIT_SHALLOW ON
    EXCLUDE_FROM_ALL
    SYSTEM)
FetchContent_MakeAvailable(SFML)

set(SOURCES "draw.cpp" "gameboy.cpp" "cartridge.cpp" "cpu.cpp" "mmu.cpp" "ppu.cpp" "opcodes.cpp" "mbc1.cpp" "timer.cpp" "interrupt.cpp" "handleinput.h" "draw.h" "gameboy.h" "cpu.h" "mmu.h" "ppu.h" "cartridge.h" "mbc1.h" "timer.h" "interrupt.h")

add_library(gb_components STATIC ${SOURCES})

target_link_libraries(gb_components PUBLIC SFML::Graphics PUBLIC SFML::Audio PUBLIC vendor)

target_include_directories(gb_components PUBLIC .)
