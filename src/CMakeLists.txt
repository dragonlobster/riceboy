include(FetchContent)
FetchContent_Declare(SFML
    GIT_REPOSITORY https://github.com/SFML/SFML.git
    GIT_TAG 3.0.0
    GIT_SHALLOW ON
    EXCLUDE_FROM_ALL
    SYSTEM)
FetchContent_MakeAvailable(SFML)

set(SOURCES "DrawUtils.cpp" "Gameboy.cpp" "Cartridge.cpp" "CPU.cpp" "MMU.cpp" "PPU.cpp" "opcodes.cpp" "MBC1.cpp" "HandleInput.h" "DrawUtils.h" "Gameboy.h" "CPU.h" "MMU.h" "PPU.h" "Cartridge.h" "MBC1.h" "ppu2.cpp")

add_library(gb_components STATIC ${SOURCES})

target_link_libraries(gb_components PUBLIC SFML::Graphics PUBLIC SFML::Audio PUBLIC vendor)

target_include_directories(gb_components PUBLIC .)
