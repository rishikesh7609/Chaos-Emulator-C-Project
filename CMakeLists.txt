cmake_minimum_required(VERSION 3.12)
project(ChaosEmulatorCPP VERSION 2.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
add_compile_options(-Wall -Wextra)

find_package(Threads REQUIRED)

add_library(chaoslib STATIC
    src/util.cpp src/emulator.cpp src/profiles.cpp
    src/scheduler.cpp src/measure.cpp src/experiments.cpp)
target_include_directories(chaoslib PUBLIC include)
target_link_libraries(chaoslib PUBLIC Threads::Threads)

add_executable(chaosctl src/main.cpp)
target_link_libraries(chaosctl chaoslib)

enable_testing()
add_executable(test_emulator tests/test_emulator.cpp)
target_link_libraries(test_emulator chaoslib)
add_test(NAME EmulatorTests COMMAND test_emulator)
