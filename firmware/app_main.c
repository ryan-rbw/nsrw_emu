/**
 * @file app_main.c
 * @brief NRWA-T6 Reaction Wheel Emulator - Main Application
 *
 * Target: Raspberry Pi Pico (RP2040)
 *
 * Boot sequence:
 * 1. Initialize hardware (GPIO, timebase, drivers)
 * 2. Run all checkpoint tests (results cached)
 * 3. Enter interactive TUI (non-scrolling console)
 */

#include <stdio.h>
#include <string.h>
#include "pico/stdlib.h"
#include "pico/multicore.h"
#include "pico/unique_id.h"
#include "hardware/watchdog.h"

// Platform layer
#include "board_pico.h"
#include "gpio_map.h"
#include "timebase.h"

// Test system
#include "test_mode.h"
#include "test_results.h"
#include "test_phase9.h"

// Console TUI
#include "tui.h"
#include "tables.h"
#include "logo.h"

// Uncomment to run Phase 9 tests at boot
#define RUN_PHASE9_TESTS

// Firmware version (passed from CMake)
#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "v0.1.0-unknown"
#endif

// Build timestamp
#define BUILD_DATE __DATE__
#define BUILD_TIME __TIME__

/**
 * @brief Print startup banner with version info
 */
static void print_banner(void) {
    // Get unique board ID
    pico_unique_board_id_t board_id;
    pico_get_unique_board_id(&board_id);

    // Print company logo
    printf("\n%s\n", LOGO_ART);

    // Print banner
    printf("NRWA-T6 Emulator %s\n", FIRMWARE_VERSION);
    printf("NewSpace NRWA-T6 Compatible | RP2040\n");
    printf("Board: %02X%02X%02X%02X%02X%02X%02X%02X\n",
           board_id.id[0], board_id.id[1], board_id.id[2], board_id.id[3],
           board_id.id[4], board_id.id[5], board_id.id[6], board_id.id[7]);
    printf("Build: %s %s\n\n", BUILD_DATE, BUILD_TIME);
}

/**
 * @brief Core 1 entry point (will run physics simulation at 100 Hz)
 */
void core1_main(void) {
    // TODO: Phase 10 - Implement physics loop
    printf("[Core1] Started (physics simulation)\n");

    while (1) {
        // Placeholder: just sleep for now
        sleep_ms(1000);
    }
}

/**
 * @brief Main entry point
 */
int main(void) {
    // Initialize stdio (USB-CDC only, UART1 reserved for RS-485)
    stdio_init_all();

    // Small delay for USB enumeration
    sleep_ms(2000);

    // Print startup banner
    print_banner();

    printf("[Core0] Initializing hardware...\n");

    // Phase 2: Initialize platform layer
    gpio_init_all();
    uint8_t device_addr = gpio_read_address();
    printf("[Core0] Device address: 0x%02X (from ADDR pins)\n", device_addr);

    // Initialize timebase (100 Hz tick for Core1 physics)
    timebase_init(NULL);  // Callback will be set in Core1

    // TODO: Phase 3 - Initialize drivers (RS-485, SLIP, NSP)

    printf("[Core0] Hardware initialization complete.\n");
    printf("\n");

    // ========================================================================
    // PHASE 1: Run Built-In Tests
    // ========================================================================

    test_results_init();
    run_all_checkpoint_tests();

#ifdef RUN_PHASE9_TESTS
    // Run Phase 9 scenario engine tests
    run_phase9_tests();
#endif

    // Wait for user to acknowledge test results
    printf("Waiting for keypress...\n");
    while (getchar_timeout_us(0) == PICO_ERROR_TIMEOUT) {
        sleep_ms(100);
    }

    // ========================================================================
    // PHASE 2: Initialize Console & TUI
    // ========================================================================

    printf("\n");
    printf("[Core0] Initializing console & TUI...\n");

    // Initialize catalog (register tables)
    catalog_init();

    // Initialize TUI (clears screen, enters interactive mode)
    tui_init();

    // ========================================================================
    // MAIN LOOP: TUI Update
    // ========================================================================

    uint32_t heartbeat_counter = 0;
    uint32_t tui_refresh_counter = 0;
    bool led_state = false;

    while (1) {
        // Heartbeat LED: Toggle every 1 second (20 iterations × 50ms = 1000ms)
        if (heartbeat_counter++ >= 20) {
            heartbeat_counter = 0;
            led_state = !led_state;
            gpio_set_heartbeat_led(led_state);
        }

        // Periodic TUI refresh: Every 500ms to update uptime
        if (tui_refresh_counter++ >= 10) {
            tui_refresh_counter = 0;
            tui_update(true);  // Force periodic refresh for live values
        }

        // Handle keyboard input
        if (tui_handle_input()) {
            // Input was processed - force immediate redraw
            tui_update(true);
            tui_refresh_counter = 0;  // Reset periodic counter after input
        }

        // TODO: Phase 3 - Poll RS-485 for incoming NSP packets
        // TODO: Phase 9 - Update fault injection scenarios

        // Small delay to avoid busy-waiting
        sleep_ms(50);  // 20 Hz update rate
    }

    return 0;
}
