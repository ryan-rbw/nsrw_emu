/**
 * @file table_config.c
 * @brief Config & JSON Table Implementation
 *
 * Table 9: Config Status (scenarios, defaults, save/restore)
 */

#include "table_config.h"
#include "tables.h"
#include "../config/scenario.h"
#include <stdio.h>
#include <string.h>

// ============================================================================
// Live Data
// ============================================================================

static volatile uint32_t cfg_scenario_active = 0;       // Scenario active (bool)
static volatile uint32_t cfg_scenario_elapsed_ms = 0;   // Elapsed time (ms)
static volatile uint32_t cfg_scenario_events_total = 0; // Total events
static volatile uint32_t cfg_scenario_events_triggered = 0; // Triggered count
static volatile uint32_t cfg_defaults_dirty = 0;        // Non-default count
static volatile uint32_t cfg_json_loaded = 0;           // JSON loaded (bool)

// ============================================================================
// Field Definitions
// ============================================================================

static const field_meta_t config_fields[] = {
    {
        .id = 901,
        .name = "scenario_active",
        .type = FIELD_TYPE_BOOL,
        .units = "",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&cfg_scenario_active,
        .dirty = false,
    },
    {
        .id = 902,
        .name = "scenario_elapsed_ms",
        .type = FIELD_TYPE_U32,
        .units = "ms",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&cfg_scenario_elapsed_ms,
        .dirty = false,
    },
    {
        .id = 903,
        .name = "scenario_events_total",
        .type = FIELD_TYPE_U32,
        .units = "events",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&cfg_scenario_events_total,
        .dirty = false,
    },
    {
        .id = 904,
        .name = "scenario_events_triggered",
        .type = FIELD_TYPE_U32,
        .units = "events",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&cfg_scenario_events_triggered,
        .dirty = false,
    },
    {
        .id = 905,
        .name = "defaults_dirty",
        .type = FIELD_TYPE_U32,
        .units = "fields",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&cfg_defaults_dirty,
        .dirty = false,
    },
    {
        .id = 906,
        .name = "json_loaded",
        .type = FIELD_TYPE_BOOL,
        .units = "",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&cfg_json_loaded,
        .dirty = false,
    },
};

// ============================================================================
// Table Definition
// ============================================================================

static const table_meta_t config_table = {
    .id = 9,
    .name = "Config Status",
    .description = "Scenarios, defaults, save/restore",
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
    cfg_scenario_active = scenario_is_active() ? 1 : 0;
    cfg_scenario_elapsed_ms = scenario_get_elapsed_ms();
    cfg_scenario_events_total = scenario_get_total_events();
    cfg_scenario_events_triggered = scenario_get_triggered_count();

    // json_loaded set to 1 when scenario is loaded
    if (scenario_get_name() != NULL) {
        cfg_json_loaded = 1;
    } else {
        cfg_json_loaded = 0;
    }

    // defaults_dirty: future feature - count non-default fields
    cfg_defaults_dirty = 0;
}
