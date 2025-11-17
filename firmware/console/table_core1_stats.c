/**
 * @file table_core1_stats.c
 * @brief Table 11: Core1 Physics Statistics
 *
 * Displays live telemetry from the Core1 physics engine at 100 Hz.
 * All values are read-only snapshots from the inter-core telemetry system.
 */

// Suppress harmless alignment warning for enum field pointers
#pragma GCC diagnostic ignored "-Waddress-of-packed-member"

#include "tables.h"
#include "util/core_sync.h"
#include "nss_nrwa_t6_regs.h"
#include "pico/stdlib.h"
#include "hardware/sync.h"
#include <stdio.h>
#include <string.h>

// ============================================================================
// Local State (cached telemetry snapshot)
// ============================================================================

// Double-buffered snapshot to prevent race conditions during TUI rendering
// IMPORTANT: Explicitly zero-initialized to prevent garbage data on first read
static telemetry_snapshot_t g_display_snapshot = {0};  // Safe for TUI to read
static telemetry_snapshot_t g_update_buffer = {0};     // Temporary buffer for updates
static bool g_snapshot_valid = false;
static uint32_t g_jitter_violations = 0;  // Count of ticks >200µs

// ============================================================================
// Enum Values
// ============================================================================

static const char* mode_enum_values[] = {
    "CURRENT",
    "SPEED",
    "TORQUE",
    "PWM"
};

static const char* direction_enum_values[] = {
    "POSITIVE",
    "NEGATIVE"
};

// ============================================================================
// Table Definition
// ============================================================================

static field_meta_t table_core1_stats_fields[] = {
    // Dynamic state (from physics simulation)
    {
        .id = 1101,
        .name = "speed_rpm",
        .type = FIELD_TYPE_FLOAT,
        .units = "RPM",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&g_display_snapshot.speed_rpm,
        .dirty = false,
        .enum_values = NULL,
        .enum_count = 0,
    },
    {
        .id = 1102,
        .name = "current_a",
        .type = FIELD_TYPE_FLOAT,
        .units = "A",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&g_display_snapshot.current_a,
        .dirty = false,
        .enum_values = NULL,
        .enum_count = 0,
    },
    {
        .id = 1103,
        .name = "torque_mnm",
        .type = FIELD_TYPE_FLOAT,
        .units = "mN·m",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&g_display_snapshot.torque_mnm,
        .dirty = false,
        .enum_values = NULL,
        .enum_count = 0,
    },
    {
        .id = 1104,
        .name = "power_w",
        .type = FIELD_TYPE_FLOAT,
        .units = "W",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&g_display_snapshot.power_w,
        .dirty = false,
        .enum_values = NULL,
        .enum_count = 0,
    },
    {
        .id = 1105,
        .name = "voltage_v",
        .type = FIELD_TYPE_FLOAT,
        .units = "V",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&g_display_snapshot.voltage_v,
        .dirty = false,
        .enum_values = NULL,
        .enum_count = 0,
    },
    {
        .id = 1106,
        .name = "momentum_nms",
        .type = FIELD_TYPE_FLOAT,
        .units = "N·m·s",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&g_display_snapshot.momentum_nms,
        .dirty = false,
        .enum_values = NULL,
        .enum_count = 0,
    },
    {
        .id = 1107,
        .name = "omega_rad_s",
        .type = FIELD_TYPE_FLOAT,
        .units = "rad/s",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&g_display_snapshot.omega_rad_s,
        .dirty = false,
        .enum_values = NULL,
        .enum_count = 0,
    },

    // Control mode (from physics)
    {
        .id = 1108,
        .name = "mode",
        .type = FIELD_TYPE_ENUM,
        .units = "",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&g_display_snapshot.mode,
        .dirty = false,
        .enum_values = mode_enum_values,
        .enum_count = 4,
    },
    {
        .id = 1109,
        .name = "direction",
        .type = FIELD_TYPE_ENUM,
        .units = "",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&g_display_snapshot.direction,
        .dirty = false,
        .enum_values = direction_enum_values,
        .enum_count = 2,
    },

    // Protection status
    {
        .id = 1110,
        .name = "fault_status",
        .type = FIELD_TYPE_HEX,
        .units = "",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&g_display_snapshot.fault_status,
        .dirty = false,
        .enum_values = NULL,
        .enum_count = 0,
    },
    {
        .id = 1111,
        .name = "warning_status",
        .type = FIELD_TYPE_HEX,
        .units = "",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&g_display_snapshot.warning_status,
        .dirty = false,
        .enum_values = NULL,
        .enum_count = 0,
    },
    {
        .id = 1112,
        .name = "lcl_tripped",
        .type = FIELD_TYPE_BOOL,
        .units = "",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&g_display_snapshot.lcl_tripped,
        .dirty = false,
        .enum_values = NULL,
        .enum_count = 0,
    },

    // Statistics
    {
        .id = 1113,
        .name = "tick_count",
        .type = FIELD_TYPE_U32,
        .units = "ticks",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&g_display_snapshot.tick_count,
        .dirty = false,
        .enum_values = NULL,
        .enum_count = 0,
    },
    {
        .id = 1114,
        .name = "jitter_us",
        .type = FIELD_TYPE_U32,
        .units = "µs",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&g_display_snapshot.jitter_us,
        .dirty = false,
        .enum_values = NULL,
        .enum_count = 0,
    },
    {
        .id = 1115,
        .name = "max_jitter_us",
        .type = FIELD_TYPE_U32,
        .units = "µs",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&g_display_snapshot.max_jitter_us,
        .dirty = false,
        .enum_values = NULL,
        .enum_count = 0,
    },
    {
        .id = 1116,
        .name = "jitter_violations",
        .type = FIELD_TYPE_U32,
        .units = "count",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&g_jitter_violations,
        .dirty = false,
        .enum_values = NULL,
        .enum_count = 0,
    },
    {
        .id = 1117,
        .name = "timestamp_us",
        .type = FIELD_TYPE_U32,
        .units = "µs",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&g_display_snapshot.timestamp_us,
        .dirty = false,
        .enum_values = NULL,
        .enum_count = 0,
    },
};

static table_meta_t table_core1_stats = {
    .id = 11,
    .name = "Core1 Physics Stats",
    .fields = table_core1_stats_fields,
    .field_count = sizeof(table_core1_stats_fields) / sizeof(field_meta_t),
};

// ============================================================================
// Public API
// ============================================================================

table_meta_t* table_core1_stats_get(void) {
    return &table_core1_stats;
}

void table_core1_stats_init(void) {
    // Initialize snapshots to zeros (prevent reading garbage on first display)
    memset(&g_display_snapshot, 0, sizeof(g_display_snapshot));
    memset(&g_update_buffer, 0, sizeof(g_update_buffer));
    g_snapshot_valid = false;
    g_jitter_violations = 0;

    // Register table with catalog
    catalog_register_table(&table_core1_stats);
}

void table_core1_stats_update(void) {
    // Read latest telemetry snapshot from Core1 into temporary buffer
    if (core_sync_read_telemetry(&g_update_buffer)) {
        // Track jitter violations (>200µs)
        if (g_update_buffer.jitter_us > 200) {
            g_jitter_violations++;
        }

        // Atomically swap buffers - disable interrupts to prevent TUI from
        // reading partially-updated display snapshot during struct copy
        uint32_t save = save_and_disable_interrupts();
        g_display_snapshot = g_update_buffer;
        g_snapshot_valid = true;
        restore_interrupts(save);
    }
}

bool table_core1_stats_is_valid(void) {
    return g_snapshot_valid;
}

uint32_t table_core1_stats_get_age_ms(void) {
    if (!g_snapshot_valid) {
        return 0xFFFFFFFF;  // Invalid
    }

    uint64_t now = time_us_64();
    uint64_t age_us = now - g_display_snapshot.timestamp_us;
    return (uint32_t)(age_us / 1000);  // Convert to ms
}
