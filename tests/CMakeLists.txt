include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/03597a01ee50ed33e9dfd640b249b4be3799d395.zip
)
# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz)
FetchContent_MakeAvailable(json)

enable_testing()
add_executable(GBTests sst.cpp)

target_link_libraries(GBTests PRIVATE GTest::gtest_main PRIVATE nlohmann_json::nlohmann_json)

add_custom_target(copy_sst_tests
    COMMAND ${CMAKE_COMMAND} -P ${CMAKE_CURRENT_LIST_DIR}/copy-sst-tests.cmake
)
add_dependencies(GBTests copy_sst_tests)

include(GoogleTest)

gtest_discover_tests(GBTests)

