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
    // Register table with catalog
    catalog_register_table(&control_table);
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
