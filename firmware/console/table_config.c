/**
 * @file table_config.c
 * @brief Fault Injection Status Table Implementation
 *
 * Table 9: Fault Injection Status (scenario engine, timeline, events)
 */

#include "table_config.h"
#include "tables.h"
#include "../config/scenario.h"
#include <stdio.h>
#include <string.h>

// ============================================================================
// Live Data
// ============================================================================

static char cfg_scenario_name[64] = "";                 // Scenario name (ASCII)
static volatile uint32_t cfg_scenario_loaded = 0;       // Scenario loaded (bool)
static volatile uint32_t cfg_scenario_active = 0;       // Scenario active (bool)
static volatile uint32_t cfg_scenario_elapsed_ms = 0;   // Elapsed time (ms)
static volatile uint32_t cfg_scenario_events_total = 0; // Total events
static volatile uint32_t cfg_scenario_events_triggered = 0; // Triggered count

// ============================================================================
// Field Definitions
// ============================================================================

static const field_meta_t config_fields[] = {
    {
        .id = 901,
        .name = "scenario_name",
        .type = FIELD_TYPE_STRING,
        .units = "",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)cfg_scenario_name,
        .dirty = false,
        .enum_values = NULL,
        .enum_count = 0,
    },
    {
        .id = 902,
        .name = "scenario_loaded",
        .type = FIELD_TYPE_BOOL,
        .units = "",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&cfg_scenario_loaded,
        .dirty = false,
        .enum_values = NULL,
        .enum_count = 0,
    },
    {
        .id = 903,
        .name = "scenario_active",
        .type = FIELD_TYPE_BOOL,
        .units = "",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&cfg_scenario_active,
        .dirty = false,
        .enum_values = NULL,
        .enum_count = 0,
    },
    {
        .id = 904,
        .name = "elapsed_ms",
        .type = FIELD_TYPE_U32,
        .units = "ms",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&cfg_scenario_elapsed_ms,
        .dirty = false,
        .enum_values = NULL,
        .enum_count = 0,
    },
    {
        .id = 905,
        .name = "events_triggered",
        .type = FIELD_TYPE_U32,
        .units = "events",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&cfg_scenario_events_triggered,
        .dirty = false,
        .enum_values = NULL,
        .enum_count = 0,
    },
    {
        .id = 906,
        .name = "events_total",
        .type = FIELD_TYPE_U32,
        .units = "events",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&cfg_scenario_events_total,
        .dirty = false,
        .enum_values = NULL,
        .enum_count = 0,
    },
};

// ============================================================================
// Table Definition
// ============================================================================

static const table_meta_t config_table = {
    .id = 9,
    .name = "Fault Injection Status",
    .description = "Scenario engine, timeline, events",
    .fields = config_fields,
    .field_count = sizeof(config_fields) / sizeof(config_fields[0]),
};

// ============================================================================
// Initialization
// ============================================================================

void table_config_init(void) {
    // Initialize scenario engine
    scenario_engine_init();

    // Register table with catalog
    catalog_register_table(&config_table);
}

// ============================================================================
// Update Function
// ============================================================================

/**
 * @brief Update config table values from scenario engine
 *
 * Call this periodically to refresh TUI display
 */
void table_config_update(void) {
    // Update scenario name
    const char* name = scenario_get_name();
    if (name != NULL) {
        strncpy(cfg_scenario_name, name, sizeof(cfg_scenario_name) - 1);
        cfg_scenario_name[sizeof(cfg_scenario_name) - 1] = '\0';
        cfg_scenario_loaded = 1;
    } else {
        strcpy(cfg_scenario_name, "(none)");
        cfg_scenario_loaded = 0;
    }

    // Update scenario status
    cfg_scenario_active = scenario_is_active() ? 1 : 0;
    cfg_scenario_elapsed_ms = scenario_get_elapsed_ms();
    cfg_scenario_events_total = scenario_get_total_events();
    cfg_scenario_events_triggered = scenario_get_triggered_count();
}
