/**
 * @file nss_nrwa_t6_test_modes.c
 * @brief Test Mode Framework Implementation
 */

#include "nss_nrwa_t6_test_modes.h"
#include "util/core_sync.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

// ============================================================================
// Test Mode Table
// ============================================================================

static const test_mode_desc_t test_mode_table[TEST_MODE_COUNT] = {
    // === NONE (Idle) ===
    [TEST_MODE_NONE] = {
        .id = TEST_MODE_NONE,
        .name = "NONE",
        .description = "No test mode active (idle, 0 current)",
        .mode = CONTROL_MODE_CURRENT,
        .setpoint = 0.0f,
        .duration_s = 0.0f,
        .expect_fault = false
    },

    // === Nominal Speed Operations ===
    [TEST_MODE_SPEED_1000RPM] = {
        .id = TEST_MODE_SPEED_1000RPM,
        .name = "SPEED_1000",
        .description = "Low speed - attitude fine control",
        .mode = CONTROL_MODE_SPEED,
        .setpoint = 1000.0f,  // RPM
        .duration_s = 5.0f,
        .expect_fault = false
    },
    [TEST_MODE_SPEED_2000RPM] = {
        .id = TEST_MODE_SPEED_2000RPM,
        .name = "SPEED_2000",
        .description = "Medium speed - typical ADCS operation",
        .mode = CONTROL_MODE_SPEED,
        .setpoint = 2000.0f,  // RPM
        .duration_s = 6.0f,
        .expect_fault = false
    },
    [TEST_MODE_SPEED_3000RPM] = {
        .id = TEST_MODE_SPEED_3000RPM,
        .name = "SPEED_3000",
        .description = "Nominal cruise - momentum storage",
        .mode = CONTROL_MODE_SPEED,
        .setpoint = 3000.0f,  // RPM
        .duration_s = 8.0f,
        .expect_fault = false
    },
    [TEST_MODE_SPEED_4000RPM] = {
        .id = TEST_MODE_SPEED_4000RPM,
        .name = "SPEED_4000",
        .description = "High speed - large slew maneuver",
        .mode = CONTROL_MODE_SPEED,
        .setpoint = 4000.0f,  // RPM
        .duration_s = 10.0f,
        .expect_fault = false
    },

    // === Limit Testing ===
    [TEST_MODE_SPEED_5000RPM] = {
        .id = TEST_MODE_SPEED_5000RPM,
        .name = "SPEED_5000",
        .description = "Soft overspeed limit (triggers warning)",
        .mode = CONTROL_MODE_SPEED,
        .setpoint = 5000.0f,  // RPM (at soft limit)
        .duration_s = 12.0f,
        .expect_fault = false  // Warning only, not fault
    },
    [TEST_MODE_OVERSPEED_FAULT] = {
        .id = TEST_MODE_OVERSPEED_FAULT,
        .name = "OVERSPEED",
        .description = "Hard overspeed fault (6500 RPM, trips LCL)",
        .mode = CONTROL_MODE_SPEED,
        .setpoint = 6500.0f,  // RPM (exceeds 6000 RPM hard limit)
        .duration_s = 15.0f,
        .expect_fault = true  // Will trip overspeed fault + LCL
    },

    // === Current/Torque Operations ===
    [TEST_MODE_CURRENT_0_5A] = {
        .id = TEST_MODE_CURRENT_0_5A,
        .name = "CURR_0.5A",
        .description = "Low current - minimal torque (~27 mN·m)",
        .mode = CONTROL_MODE_CURRENT,
        .setpoint = 0.5f,  // A (produces ~27 mN·m via k_t)
        .duration_s = 3.0f,
        .expect_fault = false
    },
    [TEST_MODE_CURRENT_1A] = {
        .id = TEST_MODE_CURRENT_1A,
        .name = "CURR_1.0A",
        .description = "Moderate current - typical operation (~53 mN·m)",
        .mode = CONTROL_MODE_CURRENT,
        .setpoint = 1.0f,  // A (produces ~53 mN·m via k_t)
        .duration_s = 5.0f,
        .expect_fault = false
    },
    [TEST_MODE_CURRENT_2A] = {
        .id = TEST_MODE_CURRENT_2A,
        .name = "CURR_2.0A",
        .description = "High current - aggressive maneuver (~107 mN·m)",
        .mode = CONTROL_MODE_CURRENT,
        .setpoint = 2.0f,  // A (produces ~107 mN·m via k_t)
        .duration_s = 5.0f,
        .expect_fault = false
    },
    [TEST_MODE_TORQUE_10MNM] = {
        .id = TEST_MODE_TORQUE_10MNM,
        .name = "TORQ_10mNm",
        .description = "Fine torque control - precision pointing",
        .mode = CONTROL_MODE_TORQUE,
        .setpoint = 10.0f,  // mN·m
        .duration_s = 0.0f,  // Continuous until deactivated
        .expect_fault = false
    },
    [TEST_MODE_TORQUE_50MNM] = {
        .id = TEST_MODE_TORQUE_50MNM,
        .name = "TORQ_50mNm",
        .description = "Medium torque - momentum building",
        .mode = CONTROL_MODE_TORQUE,
        .setpoint = 50.0f,  // mN·m
        .duration_s = 0.0f,  // Continuous until deactivated
        .expect_fault = false
    },

    // === Special Tests ===
    [TEST_MODE_ZERO_CROSS] = {
        .id = TEST_MODE_ZERO_CROSS,
        .name = "ZERO_CROSS",
        .description = "Coast to zero - friction/loss characterization",
        .mode = CONTROL_MODE_SPEED,
        .setpoint = 0.0f,  // RPM (will coast down from current speed)
        .duration_s = 30.0f,  // Long duration to observe coast-down
        .expect_fault = false
    },
    [TEST_MODE_POWER_LIMIT] = {
        .id = TEST_MODE_POWER_LIMIT,
        .name = "PWR_LIMIT",
        .description = "Power limit test - accelerate until 100W cap",
        .mode = CONTROL_MODE_CURRENT,
        .setpoint = 3.0f,  // A (high current to hit power limit quickly)
        .duration_s = 10.0f,
        .expect_fault = false  // Should hit power limit, not fault
    },
    [TEST_MODE_REVERSE] = {
        .id = TEST_MODE_REVERSE,
        .name = "REVERSE",
        .description = "Negative direction - reverse rotation test",
        .mode = CONTROL_MODE_SPEED,
        .setpoint = -2000.0f,  // RPM (negative = reverse)
        .duration_s = 8.0f,
        .expect_fault = false
    },
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
    (void)state;  // State pointer not used - commands go through mailbox

    // CRITICAL: Use inter-core command mailbox to send commands to Core1
    // Direct writes to g_wheel_state cause race conditions because Core1
    // is continuously updating the wheel state at 100 Hz.

    // Step 1: Set control mode via command mailbox
    // Retry loop in case mailbox is busy (Core1 runs at 100Hz, so max ~10ms wait)
    int retries = 20;  // 20 * 1ms = 20ms max wait
    while (!core_sync_send_command(CMD_SET_MODE, (float)desc->mode, 0.0f) && retries > 0) {
        sleep_ms(1);
        retries--;
    }
    if (retries == 0) {
        printf("[TEST_MODE] Failed to send mode command (mailbox busy)\n");
        return false;
    }

    // Wait for Core1 to process the mode command (runs at 100Hz = 10ms period)
    sleep_ms(15);

    // Step 2: Set mode-specific setpoint via command mailbox
    command_type_t setpoint_cmd;
    switch (desc->mode) {
        case CONTROL_MODE_CURRENT:
            setpoint_cmd = CMD_SET_CURRENT;
            break;

        case CONTROL_MODE_SPEED:
            setpoint_cmd = CMD_SET_SPEED;
            break;

        case CONTROL_MODE_TORQUE:
            setpoint_cmd = CMD_SET_TORQUE;
            break;

        case CONTROL_MODE_PWM:
            setpoint_cmd = CMD_SET_PWM;
            break;

        default:
            return false;
    }

    retries = 20;
    while (!core_sync_send_command(setpoint_cmd, desc->setpoint, 0.0f) && retries > 0) {
        sleep_ms(1);
        retries--;
    }
    if (retries == 0) {
        printf("[TEST_MODE] Failed to send setpoint command (mailbox busy)\n");
        return false;
    }

    active_test_mode = mode_id;

    // Single consolidated printf to avoid blocking NSP handler
    printf("[TEST_MODE] %s: %.1f %s\n", desc->name, desc->setpoint,
           desc->mode == CONTROL_MODE_CURRENT ? "A" :
           desc->mode == CONTROL_MODE_SPEED ? "RPM" :
           desc->mode == CONTROL_MODE_TORQUE ? "mNm" : "%");

    return true;
}

void test_mode_deactivate(wheel_state_t* state) {
    (void)state;  // State pointer not used - commands go through mailbox

    // Return to safe idle state via inter-core command mailbox
    core_sync_send_command(CMD_SET_MODE, (float)CONTROL_MODE_CURRENT, 0.0f);
    core_sync_send_command(CMD_SET_CURRENT, 0.0f, 0.0f);

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
