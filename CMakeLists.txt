cmake_minimum_required(VERSION 3.28)
project(RiceBoy LANGUAGES CXX)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_library(tinyfiledialogs STATIC "includes/vendor/tinyfiledialogs.cpp" "includes/vendor/tinyfiledialogs.h")

target_include_directories(tinyfiledialogs PUBLIC "includes/vendor")

# gb components 
add_subdirectory(src) 
target_link_libraries(gb_components PUBLIC tinyfiledialogs)

# rice boy executable
add_executable(RiceBoy src/main.cpp)
target_link_libraries(RiceBoy PRIVATE gb_components)

# testing
enable_testing()
add_subdirectory(tests)
target_link_libraries(GBTests PRIVATE gb_components)

