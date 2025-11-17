# NRWA-T6 Reaction Wheel Emulator
# Target: Raspberry Pi Pico (RP2040)

cmake_minimum_required(VERSION 3.13)

# Pull in Pico SDK (must be before project)
include(pico_sdk_import.cmake)

# Project definition
project(nrwa_t6_emulator C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Initialize the Pico SDK
pico_sdk_init()

# Version string from git
execute_process(
    COMMAND git describe --tags --always --dirty
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE GIT_VERSION
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

if(NOT GIT_VERSION)
    set(GIT_VERSION "v0.1.0-dev-PHASE10-TEST")
endif()

message(STATUS "Building NRWA-T6 Emulator ${GIT_VERSION}")

# Firmware subdirectory
add_subdirectory(firmware)

# Optional: Tests
option(BUILD_TESTS "Build unit tests" OFF)
if(BUILD_TESTS)
    add_subdirectory(tests/unit)
endif()
