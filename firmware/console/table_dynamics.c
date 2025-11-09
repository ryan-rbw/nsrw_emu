/**
 * @file table_dynamics.c
 * @brief Dynamics Table Implementation
 *
 * Table 5: Dynamics (speed, momentum, torque, current, power, losses, alpha)
 */

#include "table_dynamics.h"
#include "tables.h"
#include <stdio.h>

// ============================================================================
// Live Data (Stubs - will be connected to wheel_state in future)
// ============================================================================

static volatile uint32_t dyn_speed_rpm = 0;           // Actual speed (RPM)
static volatile uint32_t dyn_momentum_nms = 0;        // Momentum (N·m·s)
static volatile uint32_t dyn_torque_cmd_mnm = 0;      // Torque command (mN·m)
static volatile uint32_t dyn_torque_out_mnm = 0;      // Torque output (mN·m)
static volatile uint32_t dyn_current_cmd_ma = 0;      // Current command (mA)
static volatile uint32_t dyn_current_out_ma = 0;      // Current output (mA)
static volatile uint32_t dyn_power_w = 0;             // Power (W)
static volatile uint32_t dyn_loss_visc = 0;           // Viscous loss coeff
static volatile uint32_t dyn_loss_fric = 0;           // Friction loss coeff
static volatile uint32_t dyn_alpha_rad_s2 = 0;        // Angular acceleration

// ============================================================================
// Field Definitions
// ============================================================================

static const field_meta_t dynamics_fields[] = {
    {
        .id = 501,
        .name = "speed_rpm",
        .type = FIELD_TYPE_FLOAT,  // TODO: Use UQ14.18 format
        .units = "RPM",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&dyn_speed_rpm,
        .dirty = false,
    },
    {
        .id = 502,
        .name = "momentum_nms",
        .type = FIELD_TYPE_FLOAT,  // TODO: Use UQ18.14 format
        .units = "N·m·s",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&dyn_momentum_nms,
        .dirty = false,
    },
    {
        .id = 503,
        .name = "torque_cmd_mnm",
        .type = FIELD_TYPE_FLOAT,  // TODO: Use UQ18.14 format
        .units = "mN·m",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&dyn_torque_cmd_mnm,
        .dirty = false,
    },
    {
        .id = 504,
        .name = "torque_out_mnm",
        .type = FIELD_TYPE_FLOAT,  // TODO: Use UQ18.14 format
        .units = "mN·m",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&dyn_torque_out_mnm,
        .dirty = false,
    },
    {
        .id = 505,
        .name = "current_cmd_ma",
        .type = FIELD_TYPE_FLOAT,  // TODO: Use UQ18.14 format
        .units = "mA",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&dyn_current_cmd_ma,
        .dirty = false,
    },
    {
        .id = 506,
        .name = "current_out_ma",
        .type = FIELD_TYPE_FLOAT,  // TODO: Use UQ18.14 format
        .units = "mA",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&dyn_current_out_ma,
        .dirty = false,
    },
    {
        .id = 507,
        .name = "power_w",
        .type = FIELD_TYPE_FLOAT,  // TODO: Use UQ18.14 format
        .units = "W",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&dyn_power_w,
        .dirty = false,
    },
    {
        .id = 508,
        .name = "loss_visc",
        .type = FIELD_TYPE_FLOAT,
        .units = "",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&dyn_loss_visc,
        .dirty = false,
    },
    {
        .id = 509,
        .name = "loss_fric",
        .type = FIELD_TYPE_FLOAT,
        .units = "",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&dyn_loss_fric,
        .dirty = false,
    },
    {
        .id = 510,
        .name = "alpha_rad_s2",
        .type = FIELD_TYPE_FLOAT,
        .units = "rad/s²",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&dyn_alpha_rad_s2,
        .dirty = false,
    },
};

// ============================================================================
// Table Definition
// ============================================================================

static const table_meta_t dynamics_table = {
    .id = 5,
    .name = "Dynamics Status",
    .description = "Speed, momentum, torque, current, power",
    .fields = dynamics_fields,
    .field_count = sizeof(dynamics_fields) / sizeof(dynamics_fields[0]),
};

// ============================================================================
// Initialization
// ============================================================================

void table_dynamics_init(void) {
    // Register table with catalog
    catalog_register_table(&dynamics_table);
}
