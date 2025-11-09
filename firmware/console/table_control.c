/**
 * @file table_control.c
 * @brief Control Mode Table Implementation
 *
 * Table 4: Control Mode (mode, setpoint, direction, PWM, source)
 */

#include "table_control.h"
#include "tables.h"
#include "../device/nss_nrwa_t6_regs.h"
#include <stdio.h>

// ============================================================================
// Live Data (Stubs - will be connected to wheel_state in future)
// ============================================================================

static volatile uint32_t control_mode = 0;            // CONTROL_MODE_CURRENT
static volatile uint32_t control_speed_rpm = 0;       // Speed setpoint (stub)
static volatile uint32_t control_current_ma = 0;      // Current setpoint (stub)
static volatile uint32_t control_torque_mnm = 0;      // Torque setpoint (stub)
static volatile uint32_t control_pwm_pct = 0;         // PWM duty cycle % (stub)
static volatile uint32_t control_direction = 0;       // DIRECTION_POSITIVE

// ============================================================================
// Field Definitions
// ============================================================================

static const field_meta_t control_fields[] = {
    {
        .id = 401,
        .name = "mode",
        .type = FIELD_TYPE_U32,  // TODO: Change to ENUM type
        .units = "",
        .access = FIELD_ACCESS_RW,
        .default_val = 0,  // CONTROL_MODE_CURRENT
        .ptr = (volatile uint32_t*)&control_mode,
        .dirty = false,
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
    },
    {
        .id = 406,
        .name = "direction",
        .type = FIELD_TYPE_U32,  // TODO: Change to ENUM type
        .units = "",
        .access = FIELD_ACCESS_RW,
        .default_val = 0,  // DIRECTION_POSITIVE
        .ptr = (volatile uint32_t*)&control_direction,
        .dirty = false,
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
    // Register table with catalog
    catalog_register_table(&control_table);
}
