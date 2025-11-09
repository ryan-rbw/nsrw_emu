/**
 * @file table_fault_injection.c
 * @brief Fault Injection Control Table Implementation
 *
 * Table 10: Fault Injection Control (scenario selection and execution)
 */

#include "table_fault_injection.h"
#include "tables.h"
#include "../config/scenario.h"
#include "../config/scenario_registry.h"
#include "../config/json_loader.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <string.h>

// ============================================================================
// Live Data
// ============================================================================

static volatile uint32_t fic_scenario_index = 0;        // Selected scenario index
static volatile uint32_t fic_scenario_count = 0;        // Total scenarios available
static volatile uint32_t fic_trigger = 0;                // Execute trigger (write 1 to execute)
static char fic_selected_name[64] = "";                  // Selected scenario name

// ============================================================================
// Field Definitions
// ============================================================================

static const field_meta_t fault_injection_fields[] = {
    {
        .id = 1001,
        .name = "scenario_index",
        .type = FIELD_TYPE_U8,
        .units = "",
        .access = FIELD_ACCESS_RW,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&fic_scenario_index,
        .dirty = false,
    },
    {
        .id = 1002,
        .name = "scenario_count",
        .type = FIELD_TYPE_U8,
        .units = "scenarios",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&fic_scenario_count,
        .dirty = false,
    },
    {
        .id = 1003,
        .name = "selected_name",
        .type = FIELD_TYPE_STRING,
        .units = "",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)fic_selected_name,
        .dirty = false,
    },
    {
        .id = 1004,
        .name = "trigger",
        .type = FIELD_TYPE_BOOL,
        .units = "",
        .access = FIELD_ACCESS_RW,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&fic_trigger,
        .dirty = false,
    },
};

// ============================================================================
// Table Definition
// ============================================================================

static const table_meta_t fault_injection_table = {
    .id = 10,
    .name = "Fault Injection Control",
    .description = "Scenario selection and execution",
    .fields = fault_injection_fields,
    .field_count = sizeof(fault_injection_fields) / sizeof(fault_injection_fields[0]),
};

// ============================================================================
// Initialization
// ============================================================================

void table_fault_injection_init(void) {
    // Initialize scenario count
    fic_scenario_count = scenario_registry_count();
    fic_scenario_index = 0;
    fic_trigger = 0;

    // Set initial selected name
    const scenario_entry_t* entry = scenario_registry_get(fic_scenario_index);
    if (entry) {
        strncpy(fic_selected_name, entry->name, sizeof(fic_selected_name) - 1);
        fic_selected_name[sizeof(fic_selected_name) - 1] = '\0';
    } else {
        strcpy(fic_selected_name, "(none)");
    }

    // Register table with catalog
    catalog_register_table(&fault_injection_table);
}

// ============================================================================
// Update Function
// ============================================================================

void table_fault_injection_update(void) {
    // Clamp scenario index to valid range
    if (fic_scenario_index >= fic_scenario_count) {
        fic_scenario_index = 0;
    }

    // Update selected scenario name
    const scenario_entry_t* entry = scenario_registry_get(fic_scenario_index);
    if (entry) {
        strncpy(fic_selected_name, entry->name, sizeof(fic_selected_name) - 1);
        fic_selected_name[sizeof(fic_selected_name) - 1] = '\0';
    } else {
        strcpy(fic_selected_name, "(none)");
    }

    // Check trigger - if set to 1, execute scenario
    if (fic_trigger) {
        fic_trigger = 0;  // Clear trigger
        fault_injection_execute();
    }
}

// ============================================================================
// Scenario Execution with Live Playback
// ============================================================================

/**
 * @brief Execute selected scenario with live console playback
 *
 * This function:
 * 1. Clears screen and shows scenario details
 * 2. Loads and activates scenario
 * 3. Runs timeline with live event logging
 * 4. Waits for keypress to return to TUI
 */
void fault_injection_execute(void) {
    // Get selected scenario
    const scenario_entry_t* entry = scenario_registry_get(fic_scenario_index);
    if (!entry) {
        printf("\n[ERROR] Invalid scenario index: %d\n", fic_scenario_index);
        printf("Press any key to return...\n");
        while (getchar_timeout_us(0) == PICO_ERROR_TIMEOUT) {
            sleep_ms(100);
        }
        return;
    }

    // Clear screen and show header
    printf("\033[2J\033[H");  // Clear screen, move cursor to home
    printf("╔═══════════════════════════════════════════════════════════════╗\n");
    printf("║  FAULT INJECTION: %-42s  ║\n", entry->name);
    printf("╚═══════════════════════════════════════════════════════════════╝\n");
    printf("\n");

    // Load scenario
    printf("[LOAD] Loading scenario...\n");
    bool loaded = scenario_load(entry->json_data, entry->json_len);
    if (!loaded) {
        printf("[ERROR] Failed to load scenario: %s\n", json_get_last_error());
        printf("\nPress any key to return to TUI...\n");
        while (getchar_timeout_us(0) == PICO_ERROR_TIMEOUT) {
            sleep_ms(100);
        }
        return;
    }

    // Show scenario details
    const char* desc = scenario_get_description();
    if (desc) {
        printf("[INFO] %s\n", desc);
    }
    printf("[INFO] Events: %d\n", scenario_get_total_events());
    printf("\n");

    // Activate scenario
    printf("[EXEC] Activating scenario...\n");
    bool activated = scenario_activate();
    if (!activated) {
        printf("[ERROR] Failed to activate scenario\n");
        printf("\nPress any key to return to TUI...\n");
        while (getchar_timeout_us(0) == PICO_ERROR_TIMEOUT) {
            sleep_ms(100);
        }
        return;
    }

    printf("[EXEC] Scenario active - monitoring timeline...\n");
    printf("────────────────────────────────────────────────────────────────\n");

    // Run timeline with live updates
    uint32_t last_triggered = 0;
    uint32_t update_counter = 0;

    while (scenario_is_active()) {
        // Update scenario engine (checks for event triggers)
        scenario_update();

        // Check if new events triggered
        uint32_t triggered = scenario_get_triggered_count();
        if (triggered > last_triggered) {
            // New event triggered - already logged by scenario engine
            last_triggered = triggered;
        }

        // Show periodic status update (every second)
        if ((update_counter % 100) == 0) {  // 100 * 10ms = 1 second
            uint32_t elapsed = scenario_get_elapsed_ms();
            printf("[STATUS] t=%lu ms: %d/%d events triggered\n",
                   elapsed, triggered, scenario_get_total_events());
        }

        update_counter++;

        // Check for user interrupt (any key to abort)
        int c = getchar_timeout_us(0);
        if (c != PICO_ERROR_TIMEOUT) {
            printf("[ABORT] User interrupted scenario\n");
            break;
        }

        // Small delay (10ms = 100 Hz update rate)
        sleep_ms(10);
    }

    // Deactivate scenario
    printf("────────────────────────────────────────────────────────────────\n");
    scenario_deactivate();

    // Show summary
    uint32_t final_triggered = scenario_get_triggered_count();
    uint32_t total_events = scenario_get_total_events();
    printf("\n[DONE] Scenario complete\n");
    printf("[SUMMARY] %d/%d events triggered\n", final_triggered, total_events);

    if (final_triggered == total_events) {
        printf("✓ All events triggered successfully\n");
    } else {
        printf("⚠ Warning: Not all events triggered\n");
    }

    // Wait for keypress to return
    printf("\nPress any key to return to TUI...\n");
    while (getchar_timeout_us(0) == PICO_ERROR_TIMEOUT) {
        sleep_ms(100);
    }

    // Clear screen before returning to TUI
    printf("\033[2J\033[H");  // Clear screen, move cursor to home
}
