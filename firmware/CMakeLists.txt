# Firmware build for NRWA-T6 Emulator

# Main executable
add_executable(nrwa_t6_emulator
    app_main.c
    # Platform layer
    platform/gpio_map.c
    platform/timebase.c
    # Drivers (Phase 3)
    drivers/crc_ccitt.c
    # Test mode
    test_mode.c
)

# Pass version string as compile definition
target_compile_definitions(nrwa_t6_emulator PRIVATE
    FIRMWARE_VERSION="${GIT_VERSION}"
)

# Include directories
target_include_directories(nrwa_t6_emulator PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/platform
    ${CMAKE_CURRENT_SOURCE_DIR}/drivers
    ${CMAKE_CURRENT_SOURCE_DIR}/device
    ${CMAKE_CURRENT_SOURCE_DIR}/console
    ${CMAKE_CURRENT_SOURCE_DIR}/util
)

# Link Pico SDK libraries
target_link_libraries(nrwa_t6_emulator
    pico_stdlib          # Standard library (stdio, time, etc.)
    pico_multicore       # Dual-core support
    hardware_uart        # UART driver
    hardware_gpio        # GPIO driver
    hardware_timer       # Hardware timers
    hardware_irq         # Interrupt handling
    hardware_flash       # Flash access (for scenarios)
    pico_unique_id       # Unique board ID
)

# Enable USB output, disable UART output (we need UART1 for RS-485)
pico_enable_stdio_usb(nrwa_t6_emulator 1)
pico_enable_stdio_uart(nrwa_t6_emulator 0)

# Create map/bin/hex/uf2 files
pico_add_extra_outputs(nrwa_t6_emulator)

# Print memory usage
add_custom_command(TARGET nrwa_t6_emulator POST_BUILD
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:nrwa_t6_emulator>
)
