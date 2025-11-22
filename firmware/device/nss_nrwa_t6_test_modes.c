/**
 * @file nss_nrwa_t6_test_modes.c
 * @brief Test Mode Framework Implementation
 */

#include "nss_nrwa_t6_test_modes.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

// ============================================================================
// Test Mode Table
// ============================================================================

static const test_mode_desc_t test_mode_table[TEST_MODE_COUNT] = {
    [TEST_MODE_NONE] = {
        .id = TEST_MODE_NONE,
        .name = "NONE",
        .description = "No test mode active (normal operation)",
        .mode = CONTROL_MODE_CURRENT,
        .setpoint = 0.0f,
        .duration_s = 0.0f,
        .expect_fault = false
    },
    [TEST_MODE_SPEED_5000RPM] = {
        .id = TEST_MODE_SPEED_5000RPM,
        .name = "SPEED_5000RPM",
        .description = "Speed mode at 5000 RPM (soft overspeed limit test)",
        .mode = CONTROL_MODE_SPEED,
        .setpoint = 5000.0f,  // RPM (triggers soft overspeed warning)
        .duration_s = 10.0f,
        .expect_fault = false  // Warning, not fault
    },
    [TEST_MODE_SPEED_3000RPM] = {
        .id = TEST_MODE_SPEED_3000RPM,
        .name = "SPEED_3000RPM",
        .description = "Speed mode at 3000 RPM (nominal operation)",
        .mode = CONTROL_MODE_SPEED,
        .setpoint = 3000.0f,  // RPM
        .duration_s = 8.0f,
        .expect_fault = false
    },
    [TEST_MODE_CURRENT_2A] = {
        .id = TEST_MODE_CURRENT_2A,
        .name = "CURRENT_2A",
        .description = "Current mode at 2.0 A (moderate torque)",
        .mode = CONTROL_MODE_CURRENT,
        .setpoint = 2.0f,  // A
        .duration_s = 5.0f,
        .expect_fault = false
    },
    [TEST_MODE_TORQUE_50MNM] = {
        .id = TEST_MODE_TORQUE_50MNM,
        .name = "TORQUE_50MNM",
        .description = "Torque mode at 50 mN·m (momentum building)",
        .mode = CONTROL_MODE_TORQUE,
        .setpoint = 50.0f,  // mN·m
        .duration_s = 0.0f,  // Continuous
        .expect_fault = false
    },
    [TEST_MODE_OVERSPEED_FAULT] = {
        .id = TEST_MODE_OVERSPEED_FAULT,
        .name = "OVERSPEED_FAULT",
        .description = "Speed mode at 6500 RPM (trigger overspeed fault)",
        .mode = CONTROL_MODE_SPEED,
        .setpoint = 6500.0f,  // RPM (exceeds 6000 RPM fault limit)
        .duration_s = 15.0f,
        .expect_fault = true  // Should trip overspeed fault
    },
    [TEST_MODE_POWER_LIMIT] = {
        .id = TEST_MODE_POWER_LIMIT,
        .name = "POWER_LIMIT",
        .description = "High speed + high current (power limit test)",
        .mode = CONTROL_MODE_SPEED,
        .setpoint = 4500.0f,  // RPM (high speed, will hit power limit)
        .duration_s = 10.0f,
        .expect_fault = false  // Should regulate at power limit
    },
    [TEST_MODE_ZERO_CROSS] = {
        .id = TEST_MODE_ZERO_CROSS,
        .name = "ZERO_CROSS",
        .description = "Oscillate through zero speed (friction/loss test)",
        .mode = CONTROL_MODE_SPEED,
        .setpoint = 0.0f,  // RPM (will oscillate if started from non-zero)
        .duration_s = 5.0f,
        .expect_fault = false
    }
};

// ============================================================================
// Global State
// ============================================================================

static test_mode_id_t active_test_mode = TEST_MODE_NONE;

// ============================================================================
// Settling Tolerances
// ============================================================================

#define SPEED_SETTLING_TOLERANCE_RPM    50.0f   // ±50 RPM
#define CURRENT_SETTLING_TOLERANCE_A    0.1f    // ±0.1 A
#define TORQUE_SETTLING_TOLERANCE_MNM   5.0f    // ±5 mN·m

// ============================================================================
// Public API Implementation
// ============================================================================

void test_mode_init(void) {
    active_test_mode = TEST_MODE_NONE;
    printf("[TEST_MODE] Initialized (no test active)\n");
}

bool test_mode_activate(wheel_state_t* state, test_mode_id_t mode_id) {
    if (!state || mode_id >= TEST_MODE_COUNT) {
        return false;
    }

    if (mode_id == TEST_MODE_NONE) {
        test_mode_deactivate(state);
        return true;
    }

    const test_mode_desc_t* desc = &test_mode_table[mode_id];

    // Set control mode (this will reset old mode's state)
    wheel_model_set_mode(state, desc->mode);

    // Set mode-specific setpoint
    switch (desc->mode) {
        case CONTROL_MODE_CURRENT:
            wheel_model_set_current(state, desc->setpoint);
            break;

        case CONTROL_MODE_SPEED:
            wheel_model_set_speed(state, desc->setpoint);
            break;

        case CONTROL_MODE_TORQUE:
            wheel_model_set_torque(state, desc->setpoint);
            break;

        case CONTROL_MODE_PWM:
            wheel_model_set_pwm(state, desc->setpoint);
            break;

        default:
            return false;
    }

    active_test_mode = mode_id;

    // Single consolidated printf to avoid blocking NSP handler
    // (USB-CDC printf can block if buffer fills, causing NSP timeouts)
    printf("[TEST_MODE] %s: %.1f %s\n", desc->name, desc->setpoint,
           desc->mode == CONTROL_MODE_CURRENT ? "A" :
           desc->mode == CONTROL_MODE_SPEED ? "RPM" :
           desc->mode == CONTROL_MODE_TORQUE ? "mNm" : "%");

    return true;
}

void test_mode_deactivate(wheel_state_t* state) {
    if (!state) return;

    // Return to safe idle state
    wheel_model_set_mode(state, CONTROL_MODE_CURRENT);
    wheel_model_set_current(state, 0.0f);

    // Single printf to avoid blocking NSP handler
    if (active_test_mode != TEST_MODE_NONE) {
        printf("[TEST_MODE] Deactivated %s, returned to idle\n", test_mode_table[active_test_mode].name);
    }

    active_test_mode = TEST_MODE_NONE;
}

test_mode_id_t test_mode_get_active(void) {
    return active_test_mode;
}

bool test_mode_is_settled(const wheel_state_t* state) {
    if (!state || active_test_mode == TEST_MODE_NONE) {
        return false;
    }

    const test_mode_desc_t* desc = &test_mode_table[active_test_mode];

    switch (desc->mode) {
        case CONTROL_MODE_SPEED: {
            // Check if wheel speed has settled to target
            float speed_actual = wheel_model_get_speed_rpm(state);
            float speed_error = fabsf(speed_actual - desc->setpoint);
            return (speed_error < SPEED_SETTLING_TOLERANCE_RPM);
        }

        case CONTROL_MODE_CURRENT: {
            // Check if current output has settled to target
            float current_error = fabsf(state->current_out_a - desc->setpoint);
            return (current_error < CURRENT_SETTLING_TOLERANCE_A);
        }

        case CONTROL_MODE_TORQUE: {
            // Check if torque output has settled to target
            float torque_error = fabsf(state->torque_out_mnm - desc->setpoint);
            return (torque_error < TORQUE_SETTLING_TOLERANCE_MNM);
        }

        case CONTROL_MODE_PWM: {
            // PWM mode: assume settled immediately (open-loop)
            return true;
        }

        default:
            return false;
    }
}

const test_mode_desc_t* test_mode_get_descriptor(test_mode_id_t mode_id) {
    if (mode_id >= TEST_MODE_COUNT) {
        return NULL;
    }
    return &test_mode_table[mode_id];
}

const test_mode_desc_t* test_mode_get_descriptor_by_name(const char* name) {
    if (!name) return NULL;

    for (int i = 0; i < TEST_MODE_COUNT; i++) {
        if (strcmp(test_mode_table[i].name, name) == 0) {
            return &test_mode_table[i];
        }
    }

    return NULL;
}

int test_mode_list_all(char* buf, size_t buf_size) {
    if (!buf || buf_size == 0) {
        return 0;
    }

    int offset = 0;
    int count = 0;

    offset += snprintf(buf + offset, buf_size - offset,
                       "Available Test Modes:\n");

    for (int i = 1; i < TEST_MODE_COUNT; i++) {  // Skip TEST_MODE_NONE
        const test_mode_desc_t* desc = &test_mode_table[i];

        offset += snprintf(buf + offset, buf_size - offset,
                          "  %d. %s\n     %s\n",
                          i, desc->name, desc->description);

        count++;

        if (offset >= buf_size - 100) {
            break;  // Buffer nearly full
        }
    }

    return count;
}
