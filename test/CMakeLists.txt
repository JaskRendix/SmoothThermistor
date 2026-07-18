cmake_minimum_required(VERSION 3.16)
project(SmoothThermistorTests CXX)

set(CMAKE_CXX_STANDARD 20)

# Find Catch2 v3 installed via apt
find_package(Catch2 3 REQUIRED)

add_executable(tests
    test_smooththermistor.cpp
    Arduino.h
    ../src/SmoothThermistor.cpp
)

target_include_directories(tests PRIVATE ../src .)

# Link Catch2's main + test registry
target_link_libraries(tests PRIVATE Catch2::Catch2WithMain)
