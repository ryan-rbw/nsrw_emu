/**
 * @file table_telemetry.c
 * @brief Telemetry Blocks Table Implementation
 *
 * Table 7: Telemetry Blocks (decoded STANDARD/TEMP/VOLT/CURR/DIAG)
 */

#include "table_telemetry.h"
#include "tables.h"
#include <stdio.h>

// ============================================================================
// Live Data (Stubs - will be connected to telemetry system in future)
// ============================================================================

static volatile uint32_t telem_block_id = 0;           // Last block ID
static volatile uint32_t telem_uptime_s = 0;           // Uptime (seconds)
static volatile uint32_t telem_temp_c = 25000;         // Temperature (mC)
static volatile uint32_t telem_voltage_v = 28000;      // Bus voltage (mV)
static volatile uint32_t telem_current_ma = 0;         // Current (mA)
static volatile uint32_t telem_speed_rpm = 0;          // Speed (RPM)

// ============================================================================
// Field Definitions
// ============================================================================

static const field_meta_t telemetry_fields[] = {
    {
        .id = 701,
        .name = "block_id",
        .type = FIELD_TYPE_HEX,
        .units = "",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&telem_block_id,
        .dirty = false,
        .enum_values = NULL,
        .enum_count = 0,
    },
    {
        .id = 702,
        .name = "uptime_s",
        .type = FIELD_TYPE_U32,
        .units = "s",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&telem_uptime_s,
        .dirty = false,
        .enum_values = NULL,
        .enum_count = 0,
    },
    {
        .id = 703,
        .name = "temp_c",
        .type = FIELD_TYPE_U32,
        .units = "mC",
        .access = FIELD_ACCESS_RO,
        .default_val = 25000,
        .ptr = (volatile uint32_t*)&telem_temp_c,
        .dirty = false,
        .enum_values = NULL,
        .enum_count = 0,
    },
    {
        .id = 704,
        .name = "voltage_v",
        .type = FIELD_TYPE_U32,
        .units = "mV",
        .access = FIELD_ACCESS_RO,
        .default_val = 28000,
        .ptr = (volatile uint32_t*)&telem_voltage_v,
        .dirty = false,
        .enum_values = NULL,
        .enum_count = 0,
    },
    {
        .id = 705,
        .name = "current_ma",
        .type = FIELD_TYPE_U32,
        .units = "mA",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&telem_current_ma,
        .dirty = false,
        .enum_values = NULL,
        .enum_count = 0,
    },
    {
        .id = 706,
        .name = "speed_rpm",
        .type = FIELD_TYPE_U32,
        .units = "RPM",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&telem_speed_rpm,
        .dirty = false,
        .enum_values = NULL,
        .enum_count = 0,
    },
};

// ============================================================================
// Table Definition
// ============================================================================

static const table_meta_t telemetry_table = {
    .id = 7,
    .name = "Telemetry Status",
    .description = "Decoded telemetry (STANDARD/TEMP/VOLT/CURR)",
    .fields = telemetry_fields,
    .field_count = sizeof(telemetry_fields) / sizeof(telemetry_fields[0]),
};

// ============================================================================
// Initialization
// ============================================================================

void table_telemetry_init(void) {
    // Register table with catalog
    catalog_register_table(&telemetry_table);
}
