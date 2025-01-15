cmake_minimum_required(VERSION 3.28)
project(RiceBoy LANGUAGES CXX)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# gb components 
add_subdirectory(src) 
add_library(vendor INTERFACE)
target_include_directories(vendor INTERFACE includes)
target_link_libraries(gb_components PUBLIC vendor)

# rice boy executable
add_executable(RiceBoy src/main.cpp)
target_link_libraries(RiceBoy PRIVATE gb_components)

# testing
add_subdirectory(tests)
target_link_libraries(GBTests PRIVATE gb_components)
