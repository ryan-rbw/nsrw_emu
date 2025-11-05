/**
 * @file app_main.c
 * @brief NRWA-T6 Reaction Wheel Emulator - Main Application
 *
 * Target: Raspberry Pi Pico (RP2040)
 *
 * This emulator replicates the NewSpace Systems NRWA-T6 reaction wheel,
 * providing protocol-perfect RS-485 communication and accurate physics
 * simulation for spacecraft hardware-in-the-loop testing.
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

    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║     NRWA-T6 Reaction Wheel Emulator                       ║\n");
    printf("║     NewSpace Systems NRWA-T6 Compatible                   ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    printf("Firmware Version : %s\n", FIRMWARE_VERSION);
    printf("Build Date       : %s %s\n", BUILD_DATE, BUILD_TIME);
    printf("Target Platform  : RP2040 (Pico)\n");
    printf("Board ID         : %02X%02X%02X%02X%02X%02X%02X%02X\n",
           board_id.id[0], board_id.id[1], board_id.id[2], board_id.id[3],
           board_id.id[4], board_id.id[5], board_id.id[6], board_id.id[7]);
    printf("\n");
    printf("Status           : Initializing...\n");
    printf("\n");
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
    // Note: Not starting timer yet, will start when Core1 is launched
    timebase_init(NULL);  // Callback will be set in Core1

    // TODO: Phase 3 - Initialize drivers (RS-485, SLIP, NSP)
    // TODO: Phase 8 - Initialize console/TUI

    printf("[Core0] Launching Core1 for physics simulation...\n");

    // Launch Core 1 for physics loop
    // TODO: Phase 10 - Uncomment when ready
    // multicore_launch_core1(core1_main);

    printf("[Core0] Entering main loop (communications)\n");
    printf("\n");
    printf("Ready. Waiting for commands...\n");
    printf("Type 'help' for console commands.\n");
    printf("\n");

    // Main loop (Core 0 handles communications and console)
    uint32_t heartbeat_counter = 0;
    bool led_state = false;

    while (1) {
        // Heartbeat blink (1 Hz) using platform layer
        if (heartbeat_counter++ >= 500) {
            heartbeat_counter = 0;
            led_state = !led_state;
            gpio_set_heartbeat_led(led_state);
        }

        // TODO: Phase 3 - Poll RS-485 for incoming NSP packets
        // TODO: Phase 8 - Poll USB-CDC for console input
        // TODO: Phase 9 - Update fault injection scenarios

        // Status message with jitter stats
        static uint32_t status_counter = 0;
        if (status_counter++ >= 5000) {
            status_counter = 0;
            uint32_t uptime_ms = timebase_get_ms();
            uint32_t jitter_us = timebase_get_max_jitter_us();
            printf("[Status] Uptime: %u ms, Heartbeat: %s, Max jitter: %u us\n",
                   uptime_ms, led_state ? "ON" : "OFF", jitter_us);
        }

        // Small delay to avoid busy-waiting
        sleep_ms(1);
    }

    return 0;
}
