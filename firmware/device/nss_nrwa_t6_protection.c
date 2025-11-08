/**
 * @file nss_nrwa_t6_protection.c
 * @brief NSS NRWA-T6 Protection System Implementation
 */

#include "nss_nrwa_t6_protection.h"
#include "nss_nrwa_t6_model.h"
#include "fixedpoint.h"
#include <stdio.h>
#include <string.h>

// ============================================================================
// Protection Parameter Metadata
// ============================================================================

typedef struct {
    const char* name;
    const char* units;
    float default_value;
} prot_param_info_t;

static const prot_param_info_t prot_param_table[PROT_PARAM_COUNT] = {
    [PROT_PARAM_OVERVOLTAGE_THRESHOLD]  = {"Overvoltage Threshold", "V",   DEFAULT_OVERVOLTAGE_THRESHOLD_V},
    [PROT_PARAM_OVERSPEED_FAULT_RPM]    = {"Overspeed Fault", "RPM",        DEFAULT_OVERSPEED_FAULT_RPM},
    [PROT_PARAM_OVERSPEED_SOFT_RPM]     = {"Overspeed Soft Limit", "RPM",   DEFAULT_OVERSPEED_SOFT_RPM},
    [PROT_PARAM_OVERPOWER_LIMIT_W]      = {"Overpower Limit", "W",          DEFAULT_OVERPOWER_LIMIT_W},
    [PROT_PARAM_SOFT_OVERCURRENT_A]     = {"Soft Overcurrent", "A",         DEFAULT_SOFT_OVERCURRENT_A},
    [PROT_PARAM_HARD_OVERCURRENT_A]     = {"Hard Overcurrent", "A",         DEFAULT_HARD_OVERCURRENT_A},
    [PROT_PARAM_BRAKING_LOAD_V]         = {"Braking Load", "V",             DEFAULT_BRAKING_LOAD_V},
    [PROT_PARAM_MAX_DUTY_CYCLE_PCT]     = {"Max Duty Cycle", "%",           DEFAULT_MAX_DUTY_CYCLE_PCT},
};

// ============================================================================
// Fault Metadata
// ============================================================================

typedef struct {
    const char* name;
    bool latching;      // Requires CLEAR-FAULT to reset
    bool trips_lcl;     // Trips LCL (requires hardware RESET)
} fault_info_t;

static const fault_info_t fault_table[] = {
    [0] = {"Overspeed", true, true},         // FAULT_OVERSPEED (0x01)
    [1] = {"Overvoltage", true, true},       // FAULT_OVERVOLTAGE (0x02)
    [2] = {"Overcurrent", true, true},       // FAULT_OVERCURRENT (0x04)
    [3] = {"Overpower", false, false},       // FAULT_OVERPOWER (0x08)
    [4] = {"Undervoltage", false, false},    // FAULT_UNDERVOLTAGE (0x10)
    [5] = {"Reserved", false, false},        // Reserved
    [6] = {"Reserved", false, false},        // Reserved
    [7] = {"Reserved", false, false},        // Reserved
};

// ============================================================================
// Initialization
// ============================================================================

void protection_init(wheel_state_t* state) {
    if (!state) return;

    // Set default thresholds
    state->overvoltage_threshold_v = DEFAULT_OVERVOLTAGE_THRESHOLD_V;
    state->overspeed_fault_rpm = DEFAULT_OVERSPEED_FAULT_RPM;
    state->overspeed_soft_rpm = DEFAULT_OVERSPEED_SOFT_RPM;
    state->motor_overpower_limit_w = DEFAULT_OVERPOWER_LIMIT_W;
    state->soft_overcurrent_a = DEFAULT_SOFT_OVERCURRENT_A;
    // Note: hard_overcurrent not in model yet, using soft for now
    state->braking_load_setpoint_v = DEFAULT_BRAKING_LOAD_V;
    state->max_duty_cycle_pct = DEFAULT_MAX_DUTY_CYCLE_PCT;

    // Enable all protections by default (per SPEC.md §13)
    state->protection_enable = PROT_ENABLE_ALL;

    printf("[PROTECTION] Initialized with default thresholds:\n");
    printf("  Overvoltage: %.1f V\n", state->overvoltage_threshold_v);
    printf("  Overspeed Fault: %.0f RPM (latching)\n", state->overspeed_fault_rpm);
    printf("  Overspeed Soft: %.0f RPM (warning)\n", state->overspeed_soft_rpm);
    printf("  Overpower: %.0f W\n", state->motor_overpower_limit_w);
    printf("  Soft Overcurrent: %.1f A\n", state->soft_overcurrent_a);
    printf("  Braking Load: %.1f V\n", state->braking_load_setpoint_v);
    printf("  Max Duty Cycle: %.2f%%\n", state->max_duty_cycle_pct);
    printf("  All protections: ENABLED\n");
}

// ============================================================================
// Parameter Management
// ============================================================================

bool protection_set_parameter(wheel_state_t* state, uint8_t param_id, uint32_t value_fixed) {
    if (!state || param_id >= PROT_PARAM_COUNT) {
        return false;
    }

    // Convert fixed-point to float based on parameter type
    float value_float;

    switch (param_id) {
        case PROT_PARAM_OVERVOLTAGE_THRESHOLD:
        case PROT_PARAM_BRAKING_LOAD_V:
        case PROT_PARAM_MAX_DUTY_CYCLE_PCT:
            // UQ16.16 format
            value_float = uq16_16_to_float(value_fixed);
            break;

        case PROT_PARAM_OVERSPEED_FAULT_RPM:
        case PROT_PARAM_OVERSPEED_SOFT_RPM:
            // UQ14.18 format (RPM)
            value_float = uq14_18_to_float(value_fixed);
            break;

        case PROT_PARAM_OVERPOWER_LIMIT_W:
        case PROT_PARAM_SOFT_OVERCURRENT_A:
        case PROT_PARAM_HARD_OVERCURRENT_A:
            // UQ18.14 format (power in mW, current in mA)
            value_float = uq18_14_to_float(value_fixed);
            // For current and power, input is in milli-units
            if (param_id == PROT_PARAM_OVERPOWER_LIMIT_W) {
                value_float /= 1000.0f;  // mW → W
            } else {
                value_float /= 1000.0f;  // mA → A
            }
            break;

        default:
            return false;
    }

    // Update the appropriate threshold
    switch (param_id) {
        case PROT_PARAM_OVERVOLTAGE_THRESHOLD:
            state->overvoltage_threshold_v = value_float;
            printf("[PROTECTION] Overvoltage threshold updated: %.1f V\n", value_float);
            break;

        case PROT_PARAM_OVERSPEED_FAULT_RPM:
            state->overspeed_fault_rpm = value_float;
            printf("[PROTECTION] Overspeed fault threshold updated: %.0f RPM\n", value_float);
            break;

        case PROT_PARAM_OVERSPEED_SOFT_RPM:
            state->overspeed_soft_rpm = value_float;
            printf("[PROTECTION] Overspeed soft limit updated: %.0f RPM\n", value_float);
            break;

        case PROT_PARAM_OVERPOWER_LIMIT_W:
            state->motor_overpower_limit_w = value_float;
            printf("[PROTECTION] Overpower limit updated: %.0f W\n", value_float);
            break;

        case PROT_PARAM_SOFT_OVERCURRENT_A:
            state->soft_overcurrent_a = value_float;
            printf("[PROTECTION] Soft overcurrent updated: %.1f A\n", value_float);
            break;

        case PROT_PARAM_HARD_OVERCURRENT_A:
            // Note: hard_overcurrent not in model yet
            printf("[PROTECTION] Hard overcurrent: Not implemented yet\n");
            break;

        case PROT_PARAM_BRAKING_LOAD_V:
            state->braking_load_setpoint_v = value_float;
            printf("[PROTECTION] Braking load updated: %.1f V\n", value_float);
            break;

        case PROT_PARAM_MAX_DUTY_CYCLE_PCT:
            state->max_duty_cycle_pct = value_float;
            printf("[PROTECTION] Max duty cycle updated: %.2f%%\n", value_float);
            break;

        default:
            return false;
    }

    return true;
}

bool protection_get_parameter(const wheel_state_t* state, uint8_t param_id, uint32_t* value_fixed) {
    if (!state || !value_fixed || param_id >= PROT_PARAM_COUNT) {
        return false;
    }

    float value_float;

    // Get the current threshold value
    switch (param_id) {
        case PROT_PARAM_OVERVOLTAGE_THRESHOLD:
            value_float = state->overvoltage_threshold_v;
            *value_fixed = float_to_uq16_16(value_float);
            break;

        case PROT_PARAM_OVERSPEED_FAULT_RPM:
            value_float = state->overspeed_fault_rpm;
            *value_fixed = float_to_uq14_18(value_float);
            break;

        case PROT_PARAM_OVERSPEED_SOFT_RPM:
            value_float = state->overspeed_soft_rpm;
            *value_fixed = float_to_uq14_18(value_float);
            break;

        case PROT_PARAM_OVERPOWER_LIMIT_W:
            value_float = state->motor_overpower_limit_w * 1000.0f;  // W → mW
            *value_fixed = float_to_uq18_14(value_float);
            break;

        case PROT_PARAM_SOFT_OVERCURRENT_A:
            value_float = state->soft_overcurrent_a * 1000.0f;  // A → mA
            *value_fixed = float_to_uq18_14(value_float);
            break;

        case PROT_PARAM_HARD_OVERCURRENT_A:
            // Note: hard_overcurrent not in model yet, use soft
            value_float = state->soft_overcurrent_a * 1000.0f;  // A → mA
            *value_fixed = float_to_uq18_14(value_float);
            break;

        case PROT_PARAM_BRAKING_LOAD_V:
            value_float = state->braking_load_setpoint_v;
            *value_fixed = float_to_uq16_16(value_float);
            break;

        case PROT_PARAM_MAX_DUTY_CYCLE_PCT:
            value_float = state->max_duty_cycle_pct;
            *value_fixed = float_to_uq16_16(value_float);
            break;

        default:
            return false;
    }

    return true;
}

// ============================================================================
// Protection Enable/Disable
// ============================================================================

void protection_set_enable(wheel_state_t* state, uint32_t prot_mask, bool enable) {
    if (!state) return;

    if (enable) {
        state->protection_enable |= prot_mask;
        printf("[PROTECTION] Enabled protection(s): 0x%08X\n", prot_mask);
    } else {
        state->protection_enable &= ~prot_mask;
        printf("[PROTECTION] Disabled protection(s): 0x%08X\n", prot_mask);
    }
}

bool protection_is_enabled(const wheel_state_t* state, uint32_t prot_mask) {
    if (!state) return false;
    return (state->protection_enable & prot_mask) != 0;
}

// ============================================================================
// Defaults
// ============================================================================

void protection_restore_defaults(wheel_state_t* state) {
    if (!state) return;

    printf("[PROTECTION] Restoring default thresholds\n");
    protection_init(state);
}

// ============================================================================
// Metadata Query
// ============================================================================

const char* protection_get_param_name(uint8_t param_id) {
    if (param_id >= PROT_PARAM_COUNT) {
        return "UNKNOWN";
    }
    return prot_param_table[param_id].name;
}

const char* protection_get_param_units(uint8_t param_id) {
    if (param_id >= PROT_PARAM_COUNT) {
        return "";
    }
    return prot_param_table[param_id].units;
}

const char* protection_get_fault_name(uint32_t fault_bit) {
    // Find the bit position (0-7)
    for (int i = 0; i < 8; i++) {
        if (fault_bit == (1U << i)) {
            return fault_table[i].name;
        }
    }
    return "UNKNOWN";
}

bool protection_is_latching_fault(uint32_t fault_bit) {
    for (int i = 0; i < 8; i++) {
        if (fault_bit == (1U << i)) {
            return fault_table[i].latching;
        }
    }
    return false;
}

bool protection_trips_lcl(uint32_t fault_bit) {
    for (int i = 0; i < 8; i++) {
        if (fault_bit == (1U << i)) {
            return fault_table[i].trips_lcl;
        }
    }
    return false;
}
