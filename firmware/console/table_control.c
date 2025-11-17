/**
 * @file table_control.c
 * @brief Control Mode Table Implementation
 *
 * Table 4: Control Mode (mode, setpoint, direction, PWM, source)
 *
 * NOTE: This table now pulls live data from the Core1 telemetry snapshot,
 * ensuring consistency with Table 10 and the banner status line.
 */

#include "table_control.h"
#include "tables.h"
#include "../device/nss_nrwa_t6_regs.h"
#include "../util/core_sync.h"
#include "pico/stdlib.h"
#include "hardware/sync.h"
#include <stdio.h>
#include <string.h>

// ============================================================================
// Live Data (from Core1 telemetry snapshot)
// ============================================================================

// Double-buffered snapshot to prevent race conditions during TUI rendering
// IMPORTANT: Explicitly zero-initialized to prevent garbage data on first read
static telemetry_snapshot_t g_control_snapshot = {0};  // Safe for TUI to read
static telemetry_snapshot_t g_control_update_buffer = {0};  // Temporary buffer for updates
static bool g_control_snapshot_valid = false;

// Cached display values (converted from telemetry snapshot)
static volatile uint32_t control_mode = 0;            // CONTROL_MODE_CURRENT
static volatile uint32_t control_speed_rpm = 0;       // Speed in RPM
static volatile uint32_t control_current_ma = 0;      // Current in mA
static volatile uint32_t control_torque_mnm = 0;      // Torque in mN·m
static volatile uint32_t control_pwm_pct = 0;         // PWM duty cycle %
static volatile uint32_t control_direction = 0;       // DIRECTION_POSITIVE

// ============================================================================
// Enum String Tables (UPPERCASE)
// ============================================================================

// Control mode enum
static const char* control_mode_enum[] = {
    "CURRENT",
    "SPEED",
    "TORQUE",
    "PWM"
};

// Direction enum
static const char* direction_enum[] = {
    "POSITIVE",
    "NEGATIVE"
};

// ============================================================================
// Field Definitions
// ============================================================================

static const field_meta_t control_fields[] = {
    {
        .id = 401,
        .name = "mode",
        .type = FIELD_TYPE_ENUM,
        .units = "",
        .access = FIELD_ACCESS_RW,
        .default_val = 0,  // CONTROL_MODE_CURRENT
        .ptr = (volatile uint32_t*)&control_mode,
        .dirty = false,
        .enum_values = control_mode_enum,
        .enum_count = 4,
    },
    {
        .id = 402,
        .name = "speed_rpm",
        .type = FIELD_TYPE_U32,  // TODO: Use UQ14.18 format
        .units = "RPM",
        .access = FIELD_ACCESS_RW,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&control_speed_rpm,
        .dirty = false,
        .enum_values = NULL,
        .enum_count = 0,
    },
    {
        .id = 403,
        .name = "current_ma",
        .type = FIELD_TYPE_U32,  // TODO: Use UQ18.14 format
        .units = "mA",
        .access = FIELD_ACCESS_RW,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&control_current_ma,
        .dirty = false,
        .enum_values = NULL,
        .enum_count = 0,
    },
    {
        .id = 404,
        .name = "torque_mnm",
        .type = FIELD_TYPE_U32,  // TODO: Use UQ18.14 format
        .units = "mN·m",
        .access = FIELD_ACCESS_RW,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&control_torque_mnm,
        .dirty = false,
        .enum_values = NULL,
        .enum_count = 0,
    },
    {
        .id = 405,
        .name = "pwm_pct",
        .type = FIELD_TYPE_U32,  // TODO: Use UQ16.16 format
        .units = "%",
        .access = FIELD_ACCESS_RW,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&control_pwm_pct,
        .dirty = false,
        .enum_values = NULL,
        .enum_count = 0,
    },
    {
        .id = 406,
        .name = "direction",
        .type = FIELD_TYPE_ENUM,
        .units = "",
        .access = FIELD_ACCESS_RW,
        .default_val = 0,  // DIRECTION_POSITIVE
        .ptr = (volatile uint32_t*)&control_direction,
        .dirty = false,
        .enum_values = direction_enum,
        .enum_count = 2,
    },
};

// ============================================================================
// Table Definition
// ============================================================================

static const table_meta_t control_table = {
    .id = 4,
    .name = "Control Setpoints",
    .description = "Mode, setpoint, direction, PWM",
    .fields = control_fields,
    .field_count = sizeof(control_fields) / sizeof(control_fields[0]),
};

// ============================================================================
// Initialization
// ============================================================================

void table_control_init(void) {
    // Initialize snapshots to zeros (prevent reading garbage on first display)
    memset(&g_control_snapshot, 0, sizeof(g_control_snapshot));
    memset(&g_control_update_buffer, 0, sizeof(g_control_update_buffer));
    g_control_snapshot_valid = false;

    // Register table with catalog
    catalog_register_table(&control_table);
}

// ============================================================================
// Update Function (called from main loop)
// ============================================================================

void table_control_update(void) {
    // Read latest telemetry snapshot from Core1 into temporary buffer
    if (core_sync_read_telemetry(&g_control_update_buffer)) {
        // Atomically swap buffers - disable interrupts to prevent TUI from
        // reading partially-updated display snapshot during struct copy
        uint32_t save = save_and_disable_interrupts();
        g_control_snapshot = g_control_update_buffer;
        g_control_snapshot_valid = true;
        restore_interrupts(save);

        // Update cached display values from snapshot
        control_mode = (uint32_t)g_control_snapshot.mode;
        control_direction = (uint32_t)g_control_snapshot.direction;
        control_speed_rpm = (uint32_t)g_control_snapshot.speed_rpm;
        control_current_ma = (uint32_t)(g_control_snapshot.current_a * 1000.0f);  // A → mA
        control_torque_mnm = (uint32_t)g_control_snapshot.torque_mnm;
        // PWM duty cycle not in telemetry snapshot - keep as 0 for now
        control_pwm_pct = 0;
    }
}

// ============================================================================
// Getter Functions
// ============================================================================

uint32_t table_control_get_mode(void) {
    return control_mode;
}

uint32_t table_control_get_direction(void) {
    return control_direction;
}

uint32_t table_control_get_speed_rpm(void) {
    return control_speed_rpm;
}

uint32_t table_control_get_current_ma(void) {
    return control_current_ma;
}

uint32_t table_control_get_torque_mnm(void) {
    return control_torque_mnm;
}

uint32_t table_control_get_pwm_pct(void) {
    return control_pwm_pct;
}

const char* table_control_get_mode_string(uint32_t mode) {
    if (mode < 4) {
        return control_mode_enum[mode];
    }
    return "INVALID";
}

const char* table_control_get_direction_string(uint32_t direction) {
    if (direction < 2) {
        return direction_enum[direction];
    }
    return "INVALID";
}
