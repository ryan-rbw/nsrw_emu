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
#include "table_config.h"
#include "table_control.h"
#include "table_fault_injection.h"
#include "table_core1_stats.h"
#include "table_test_modes.h"

// Test modes (operating scenarios)
#include "nss_nrwa_t6_test_modes.h"

// Fault injection
#include "config/scenario.h"

// Device model
#include "nss_nrwa_t6_model.h"
#include "nss_nrwa_t6_protection.h"

// Inter-core sync
#include "util/core_sync.h"

// NSP packet handler (Core0)
#include "nsp_handler.h"

// Uncomment to run Phase 9 tests at boot (normally user-triggered from TUI)
// #define RUN_PHASE9_TESTS

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
    printf("Build: %s %s | RP2040 Dual-Core @ 125MHz\n", BUILD_DATE, BUILD_TIME);
    printf("NewSpace NRWA-T6 Compatible | 100Hz Physics Engine\n");
    printf("Board: %02X%02X%02X%02X%02X%02X%02X%02X\n\n",
           board_id.id[0], board_id.id[1], board_id.id[2], board_id.id[3],
           board_id.id[4], board_id.id[5], board_id.id[6], board_id.id[7]);
}

// ============================================================================
// Global State (Shared between cores via core_sync)
// ============================================================================

wheel_state_t g_wheel_state;  // Non-static for console/test mode access
static volatile bool g_core1_ready = false;

// ============================================================================
// Core1: Physics Loop (100 Hz Hard Real-Time)
// ============================================================================

/**
 * @brief 100 Hz physics tick callback (called from ISR)
 *
 * This function is called by the hardware alarm ISR at 100 Hz.
 * It must complete in <200µs to meet jitter requirements.
 */
static volatile bool g_physics_tick_flag = false;

static void physics_tick_callback(void) {
    // Set flag for Core1 main loop to process
    // Do NOT do any work here - ISR must be fast
    g_physics_tick_flag = true;
}

/**
 * @brief Core 1 entry point - Physics simulation at 100 Hz
 *
 * This core runs a hard real-time loop at 100 Hz (10ms period).
 * It reads commands from Core0, updates physics, checks protections,
 * and publishes telemetry back to Core0.
 */
void core1_main(void) {
    printf("[Core1] Starting physics engine...\n");

    // Initialize wheel model with default state
    wheel_model_init(&g_wheel_state);
    printf("[Core1] Wheel model initialized\n");

    // Initialize protection system
    protection_init(&g_wheel_state);
    printf("[Core1] Protection system initialized\n");

    // Initialize test mode framework
    test_mode_init();
    printf("[Core1] Test mode framework initialized\n");

    // Set up timebase with callback
    timebase_init(physics_tick_callback);
    printf("[Core1] Timebase initialized (100 Hz)\n");

    // Start the physics tick
    timebase_start();
    printf("[Core1] Physics tick started\n");

    // Signal that Core1 is ready
    g_core1_ready = true;

    // Statistics
    uint32_t tick_count = 0;
    uint32_t max_jitter_us = 0;

    // Main physics loop
    while (1) {
        // Wait for physics tick flag
        while (!g_physics_tick_flag) {
            tight_loop_contents();
        }

        // Clear flag
        g_physics_tick_flag = false;

        // Record tick start time
        uint64_t tick_start = time_us_64();

        // ====================================================================
        // 1. Read commands from Core0
        // ====================================================================
        command_mailbox_t cmd;
        if (core_sync_read_command(&cmd)) {
            // Apply command to wheel model
            switch (cmd.type) {
                case CMD_SET_MODE:
                    wheel_model_set_mode(&g_wheel_state, (control_mode_t)cmd.param1);
                    break;

                case CMD_SET_SPEED:
                    wheel_model_set_speed(&g_wheel_state, cmd.param1);
                    break;

                case CMD_SET_CURRENT:
                    wheel_model_set_current(&g_wheel_state, cmd.param1);
                    break;

                case CMD_SET_TORQUE:
                    wheel_model_set_torque(&g_wheel_state, cmd.param1);
                    break;

                case CMD_SET_PWM:
                    wheel_model_set_pwm(&g_wheel_state, cmd.param1);
                    break;

                case CMD_CLEAR_FAULT:
                    // Clear latched faults (param1 = fault mask)
                    g_wheel_state.fault_latch &= ~((uint32_t)cmd.param1);
                    g_wheel_state.fault_status &= ~((uint32_t)cmd.param1);
                    break;

                case CMD_RESET:
                    // Soft reset: reinitialize wheel model
                    wheel_model_init(&g_wheel_state);
                    protection_init(&g_wheel_state);
                    break;

                default:
                    break;
            }
        }

        // ====================================================================
        // 2. Update physics model (10ms tick)
        // ====================================================================
        // Note: wheel_model_tick() includes protection checks
        wheel_model_tick(&g_wheel_state);

        // ====================================================================
        // 3. Publish telemetry snapshot to Core0
        // ====================================================================
        telemetry_snapshot_t snapshot;
        snapshot.omega_rad_s = g_wheel_state.omega_rad_s;
        snapshot.speed_rpm = g_wheel_state.omega_rad_s * RAD_S_TO_RPM;
        snapshot.momentum_nms = g_wheel_state.momentum_nms;
        snapshot.current_a = g_wheel_state.current_out_a;
        snapshot.torque_mnm = g_wheel_state.torque_out_mnm;
        snapshot.power_w = g_wheel_state.power_w;
        snapshot.voltage_v = g_wheel_state.voltage_v;
        snapshot.mode = g_wheel_state.mode;
        snapshot.direction = g_wheel_state.direction;
        snapshot.fault_status = g_wheel_state.fault_status;
        snapshot.warning_status = g_wheel_state.warning_status;
        snapshot.lcl_tripped = g_wheel_state.lcl_tripped;
        snapshot.tick_count = g_wheel_state.tick_count;

        // Record jitter
        uint64_t tick_end = time_us_64();
        uint32_t jitter_us = (uint32_t)(tick_end - tick_start);
        snapshot.jitter_us = jitter_us;

        if (jitter_us > max_jitter_us) {
            max_jitter_us = jitter_us;
        }
        snapshot.max_jitter_us = max_jitter_us;
        snapshot.timestamp_us = tick_end;

        // Publish snapshot
        core_sync_publish_telemetry(&snapshot);

        tick_count++;

        // ====================================================================
        // 5. Jitter monitoring (log if > 200µs)
        // ====================================================================
        if (jitter_us > 200) {
            // NOTE: Don't printf here! It will cause even more jitter.
            // Instead, log to a counter and check from Core0.
        }
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
    // Note: Callback will be set by Core1
    // timebase_init(NULL);  // Moved to Core1

    // Initialize inter-core synchronization
    printf("[Core0] Initializing inter-core communication...\n");
    core_sync_init();

    // Initialize NSP handler (RS-485, SLIP, NSP, command dispatch)
    printf("[Core0] Initializing NSP handler...\n");
    nsp_handler_init(device_addr);

    printf("[Core0] Hardware initialization complete.\n");
    printf("\n");

    // ========================================================================
    // Launch Core1 (Physics Engine)
    // ========================================================================

    printf("[Core0] Launching Core1 physics engine...\n");
    multicore_launch_core1(core1_main);

    // Wait for Core1 to be ready
    while (!g_core1_ready) {
        sleep_ms(10);
    }
    printf("[Core0] Core1 ready\n");
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

    // Enable watchdog timer (8 second timeout for fault recovery)
    // DISABLED: Uncomment after confirming system is stable on hardware
    // Watchdog can prevent recovery if system is in boot loop
    // watchdog_enable(8000, true);  // 8000ms, pause on debug
    printf("[Core0] Watchdog DISABLED (enable after hardware validation)\n");

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
        // Update watchdog (disabled until hardware validation complete)
        // watchdog_update();

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

        // Update scenario engine (check for event triggers)
        scenario_update();

        // Update table values from scenario engine
        table_config_update();
        table_fault_injection_update();

        // Update Core1 telemetry snapshot (Table 10 and Table 4 both use this)
        table_core1_stats_update();
        table_control_update();  // Keep control table in sync with telemetry

        // Poll RS-485 for incoming NSP packets (non-blocking)
        nsp_handler_poll();

        // Small delay to avoid busy-waiting
        sleep_ms(50);  // 20 Hz update rate
    }

    return 0;
}
