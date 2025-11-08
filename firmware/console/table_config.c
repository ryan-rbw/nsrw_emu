/**
 * @file table_config.c
 * @brief Config & JSON Table Implementation
 *
 * Table 8: Config & JSON (scenarios, defaults, save/restore)
 */

#include "table_config.h"
#include "tables.h"
#include <stdio.h>

// ============================================================================
// Live Data (Stubs - will be connected to scenario system in future)
// ============================================================================

static volatile uint32_t cfg_scenario_active = 0;       // Scenario active (bool)
static volatile uint32_t cfg_scenario_id = 0;           // Active scenario ID
static volatile uint32_t cfg_defaults_dirty = 0;        // Non-default count
static volatile uint32_t cfg_json_loaded = 0;           // JSON loaded (bool)

// ============================================================================
// Field Definitions
// ============================================================================

static const field_meta_t config_fields[] = {
    {
        .id = 801,
        .name = "scenario_active",
        .type = FIELD_TYPE_BOOL,
        .units = "",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&cfg_scenario_active,
        .dirty = false,
    },
    {
        .id = 802,
        .name = "scenario_id",
        .type = FIELD_TYPE_U32,
        .units = "",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&cfg_scenario_id,
        .dirty = false,
    },
    {
        .id = 803,
        .name = "defaults_dirty",
        .type = FIELD_TYPE_U32,
        .units = "fields",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&cfg_defaults_dirty,
        .dirty = false,
    },
    {
        .id = 804,
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
    .id = 8,
    .name = "Config & JSON",
    .description = "Scenarios, defaults, save/restore",
    .fields = config_fields,
    .field_count = sizeof(config_fields) / sizeof(config_fields[0]),
};

// ============================================================================
// Initialization
// ============================================================================

void table_config_init(void) {
    // Register table with catalog
    catalog_register_table(&config_table);
}
