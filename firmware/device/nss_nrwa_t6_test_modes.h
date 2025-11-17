/**
 * @file nss_nrwa_t6_test_modes.h
 * @brief Test Mode Framework for NRWA-T6 Emulator
 *
 * Provides predefined operating scenarios for validation and HIL testing.
 * Test modes set the wheel to specific operating conditions (mode + setpoint)
 * and can be activated via console commands or NSP APPLICATION-COMMAND.
 */

#ifndef NSS_NRWA_T6_TEST_MODES_H
#define NSS_NRWA_T6_TEST_MODES_H

#include <stdint.h>
#include <stdbool.h>
#include "nss_nrwa_t6_model.h"

// ============================================================================
// Test Mode Definitions
// ============================================================================

/**
 * @brief Test mode identifiers
 */
typedef enum {
    TEST_MODE_NONE = 0,           // No test mode active (normal operation)
    TEST_MODE_SPEED_5000RPM,      // Speed mode at 5000 RPM (soft limit test)
    TEST_MODE_SPEED_3000RPM,      // Speed mode at 3000 RPM (nominal operation)
    TEST_MODE_CURRENT_2A,         // Current mode at 2.0 A (moderate torque)
    TEST_MODE_TORQUE_50MNM,       // Torque mode at 50 mN·m (momentum building)
    TEST_MODE_OVERSPEED_FAULT,    // Speed mode at 6500 RPM (trigger fault)
    TEST_MODE_POWER_LIMIT,        // High speed + high current (power limit test)
    TEST_MODE_ZERO_CROSS,         // Oscillate through zero speed (friction test)
    TEST_MODE_COUNT               // Total number of test modes
} test_mode_id_t;

/**
 * @brief Test mode descriptor
 */
typedef struct {
    test_mode_id_t id;
    const char* name;
    const char* description;
    control_mode_t mode;
    float setpoint;               // Mode-specific setpoint (RPM, A, mN·m, %)
    float duration_s;             // Expected settling time (0 = indefinite)
    bool expect_fault;            // True if this mode should trigger a fault
} test_mode_desc_t;

// ============================================================================
// Test Mode API
// ============================================================================

/**
 * @brief Initialize test mode system
 */
void test_mode_init(void);

/**
 * @brief Activate a test mode
 *
 * Configures the wheel state to the specified test scenario.
 * Does NOT clear existing faults - use wheel_model_clear_faults() if needed.
 *
 * @param state Pointer to wheel state structure
 * @param mode_id Test mode to activate
 * @return true if mode was successfully activated
 */
bool test_mode_activate(wheel_state_t* state, test_mode_id_t mode_id);

/**
 * @brief Deactivate current test mode
 *
 * Returns wheel to safe idle state (CURRENT mode, zero setpoint).
 *
 * @param state Pointer to wheel state structure
 */
void test_mode_deactivate(wheel_state_t* state);

/**
 * @brief Get current active test mode
 *
 * @return Current test mode ID (TEST_MODE_NONE if no test active)
 */
test_mode_id_t test_mode_get_active(void);

/**
 * @brief Check if test mode has reached target condition
 *
 * For speed modes: Check if |ω_actual - ω_setpoint| < tolerance
 * For current modes: Check if |i_actual - i_setpoint| < tolerance
 * For torque modes: Check if torque is being applied correctly
 *
 * @param state Pointer to wheel state structure
 * @return true if test mode has settled to target
 */
bool test_mode_is_settled(const wheel_state_t* state);

/**
 * @brief Get test mode descriptor by ID
 *
 * @param mode_id Test mode ID
 * @return Pointer to test mode descriptor (NULL if invalid ID)
 */
const test_mode_desc_t* test_mode_get_descriptor(test_mode_id_t mode_id);

/**
 * @brief Get test mode descriptor by name
 *
 * @param name Test mode name string
 * @return Pointer to test mode descriptor (NULL if not found)
 */
const test_mode_desc_t* test_mode_get_descriptor_by_name(const char* name);

/**
 * @brief List all available test modes
 *
 * @param buf Output buffer for mode list (one per line)
 * @param buf_size Size of output buffer
 * @return Number of test modes listed
 */
int test_mode_list_all(char* buf, size_t buf_size);

#endif // NSS_NRWA_T6_TEST_MODES_H
