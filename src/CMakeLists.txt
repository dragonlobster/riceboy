include(FetchContent)
FetchContent_Declare(SFML
    GIT_REPOSITORY https://github.com/SFML/SFML.git
    GIT_TAG 3.0.0
    GIT_SHALLOW ON
    EXCLUDE_FROM_ALL
    SYSTEM)
FetchContent_MakeAvailable(SFML)

set(SOURCES "DrawUtils.cpp" "Gameboy.cpp" "Cartridge.cpp" "cpu.cpp" "mmu.cpp" "ppu.cpp" "opcodes.cpp" "MBC1.cpp" "HandleInput.h" "DrawUtils.h" "Gameboy.h" "cpu.h" "mmu.h" "ppu.h" "Cartridge.h" "MBC1.h"
        Timer.cpp
        Timer.h
        )

add_library(gb_components STATIC ${SOURCES})

target_link_libraries(gb_components PUBLIC SFML::Graphics PUBLIC SFML::Audio PUBLIC vendor)

target_include_directories(gb_components PUBLIC .)
