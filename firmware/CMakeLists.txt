# Firmware build for NRWA-T6 Emulator

# Main executable
add_executable(nrwa_t6_emulator
    app_main.c
    nsp_handler.c
    # Platform layer
    platform/gpio_map.c
    platform/timebase.c
    # Drivers (Phase 3)
    drivers/crc_ccitt.c
    drivers/slip.c
    drivers/rs485_uart.c
    drivers/nsp.c
    # Utilities (Phase 4)
    util/ringbuf.c
    util/core_sync.c
    # Device model (Phase 5)
    device/nss_nrwa_t6_regs.c
    device/nss_nrwa_t6_model.c
    # Device commands & telemetry (Phase 6)
    device/nss_nrwa_t6_commands.c
    device/nss_nrwa_t6_telemetry.c
    # Protection system (Phase 7)
    device/nss_nrwa_t6_protection.c
    # Test modes (operating scenarios)
    device/nss_nrwa_t6_test_modes.c
    # Fault injection (Phase 9)
    config/json_loader.c
    config/scenario.c
    config/scenario_registry.c
    # Test mode (runs at boot, results cached)
    test_mode.c
    test_results.c
    test_phase9.c
    # Console & TUI (Phase 8)
    console/tui.c
    console/tables.c
    console/console_format.c
    console/table_tests.c
    console/table_serial.c
    console/table_nsp.c
    console/table_control.c
    console/table_protection_limits.c
    console/table_protection_status.c
    console/table_telemetry.c
    console/table_config.c
    console/table_fault_injection.c
    console/table_core1_stats.c
    console/table_test_modes.c
)

# Pass version string as compile definition
target_compile_definitions(nrwa_t6_emulator PRIVATE
    FIRMWARE_VERSION="${GIT_VERSION}"
)

# Compiler optimizations for size reduction
target_compile_options(nrwa_t6_emulator PRIVATE
    -Os                      # Optimize for size
    -fdata-sections          # Place data in separate sections
    -ffunction-sections      # Place functions in separate sections
    -fmerge-all-constants    # Merge duplicate string/constant literals
)

# Linker optimizations for size reduction
target_link_options(nrwa_t6_emulator PRIVATE
    -Wl,--gc-sections        # Remove unused sections
)

# Include directories
target_include_directories(nrwa_t6_emulator PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/platform
    ${CMAKE_CURRENT_SOURCE_DIR}/drivers
    ${CMAKE_CURRENT_SOURCE_DIR}/device
    ${CMAKE_CURRENT_SOURCE_DIR}/console
    ${CMAKE_CURRENT_SOURCE_DIR}/config
    ${CMAKE_CURRENT_SOURCE_DIR}/util
)

# Link Pico SDK libraries
target_link_libraries(nrwa_t6_emulator
    pico_stdlib          # Standard library (stdio, time, etc.)
    pico_multicore       # Dual-core support
    pico_sync            # Spinlocks and synchronization
    hardware_uart        # UART driver
    hardware_gpio        # GPIO driver
    hardware_timer       # Hardware timers
    hardware_irq         # Interrupt handling
    hardware_flash       # Flash access (for scenarios)
    hardware_sync        # Hardware sync primitives
    pico_unique_id       # Unique board ID
)

# Enable USB output, disable UART output (we need UART1 for RS-485)
pico_enable_stdio_usb(nrwa_t6_emulator 1)
pico_enable_stdio_uart(nrwa_t6_emulator 0)

# Enable float support in printf (required for Table 10 physics stats)
target_compile_definitions(nrwa_t6_emulator PRIVATE
    PICO_PRINTF_SUPPORT_FLOAT=1
    PICO_PRINTF_SUPPORT_LONG_LONG=1
    PICO_PRINTF_SUPPORT_PTRDIFF_T=1
)

# Create map/bin/hex/uf2 files
# Disabled - using system picotool for manual UF2 conversion (see build.sh)
# pico_add_extra_outputs(nrwa_t6_emulator)

# Print memory usage
add_custom_command(TARGET nrwa_t6_emulator POST_BUILD
    COMMAND ${CMAKE_SIZE} $<TARGET_FILE:nrwa_t6_emulator>
)
