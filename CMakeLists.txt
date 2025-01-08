cmake_minimum_required(VERSION 3.28)
project(RiceBoy LANGUAGES CXX)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include(FetchContent)
FetchContent_Declare(SFML
    GIT_REPOSITORY https://github.com/SFML/SFML.git
    GIT_TAG 3.0.0
    GIT_SHALLOW ON
    EXCLUDE_FROM_ALL
    SYSTEM)
FetchContent_MakeAvailable(SFML)

set(SOURCES src/main.cpp src/DrawUtils.cpp src/tinyfiledialogs.cpp src/Gameboy.cpp src/cpu.cpp src/mmu.cpp src/HandleInput.h src/tinyfiledialogs.h src/DrawUtils.h src/Gameboy.h src/cpu.h src/mmu.h)

add_executable(RiceBoy ${SOURCES})

target_link_libraries(RiceBoy PRIVATE SFML::Graphics PRIVATE SFML::Audio)
