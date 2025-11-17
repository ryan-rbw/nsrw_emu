/**
 * @file table_test_modes.c
 * @brief Table 11: Test Modes
 *
 * Simplified display showing current test mode status
 */

#include "tables.h"
#include "console_format.h"
#include "nss_nrwa_t6_test_modes.h"
#include "nss_nrwa_t6_model.h"
#include <stdio.h>
#include <string.h>

// External reference to wheel state (shared across tables)
extern wheel_state_t g_wheel_state;

// ============================================================================
// Test Mode Status (Shadow State for Table Display)
// ============================================================================

// These variables hold formatted status for display in the table
static uint32_t active_mode_id = 0;  // Current active test mode ID

// ============================================================================
// Field Definitions
// ============================================================================

static const field_meta_t test_mode_fields[] = {
    {
        .id = 1101,
        .name = "active_mode_id",
        .type = FIELD_TYPE_U32,
        .units = "",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&active_mode_id,
        .dirty = false,
        .enum_values = NULL,
        .enum_count = 0,
    }
};

#define NUM_TEST_MODE_FIELDS (sizeof(test_mode_fields) / sizeof(field_meta_t))

// ============================================================================
// Table Metadata
// ============================================================================

static const table_meta_t test_modes_table_meta = {
    .id = 11,
    .name = "Test Modes",
    .description = "Predefined operating scenarios for validation",
    .fields = test_mode_fields,
    .field_count = NUM_TEST_MODE_FIELDS
};

// ============================================================================
// Table Registration
// ============================================================================

void table_test_modes_init(void) {
    // Register table with catalog
    catalog_register_table(&test_modes_table_meta);

    printf("[TABLE] Test Modes table registered\n");
}

// ============================================================================
// Update Function (Call periodically to refresh display)
// ============================================================================

void table_test_modes_update(void) {
    // Update shadow state from test mode framework
    active_mode_id = (uint32_t)test_mode_get_active();
}

// ============================================================================
// Command Handlers
// ============================================================================

/**
 * @brief Activate a test mode by ID
 *
 * Usage: test <mode_id>
 * Example: test 1  (activates TEST_MODE_SPEED_5000RPM)
 */
bool table_test_modes_activate(int mode_id) {
    if (mode_id < 0 || mode_id >= TEST_MODE_COUNT) {
        printf("[TEST] Invalid mode ID: %d\n", mode_id);
        return false;
    }

    bool success = test_mode_activate(&g_wheel_state, (test_mode_id_t)mode_id);
    if (success) {
        table_test_modes_update();
    }
    return success;
}

/**
 * @brief Deactivate current test mode
 *
 * Usage: test 0  (or test stop)
 */
bool table_test_modes_deactivate(void) {
    test_mode_deactivate(&g_wheel_state);
    table_test_modes_update();
    return true;
}

/**
 * @brief List all available test modes
 */
void table_test_modes_list(void) {
    char buf[512];
    test_mode_list_all(buf, sizeof(buf));
    printf("%s", buf);
}

/**
 * @brief Print detailed info about current test mode
 */
void table_test_modes_print_status(void) {
    test_mode_id_t active_id = test_mode_get_active();
    const test_mode_desc_t* desc = test_mode_get_descriptor(active_id);

    printf("\n=== Test Mode Status ===\n");

    if (!desc || active_id == TEST_MODE_NONE) {
        printf("No test mode active\n");
        return;
    }

    printf("Active Mode: %d (%s)\n", active_id, desc->name);
    printf("Description: %s\n", desc->description);

    // Control mode
    const char* mode_names[] = {"CURRENT", "SPEED", "TORQUE", "PWM"};
    if (desc->mode < 4) {
        printf("Control Mode: %s\n", mode_names[desc->mode]);
    }

    // Setpoint with units
    printf("Setpoint: ");
    switch (desc->mode) {
        case CONTROL_MODE_CURRENT:
            printf("%.2f A\n", desc->setpoint);
            break;
        case CONTROL_MODE_SPEED:
            printf("%.0f RPM\n", desc->setpoint);
            break;
        case CONTROL_MODE_TORQUE:
            printf("%.1f mN·m\n", desc->setpoint);
            break;
        case CONTROL_MODE_PWM:
            printf("%.2f%%\n", desc->setpoint);
            break;
        default:
            printf("%.2f\n", desc->setpoint);
    }

    // Settling status
    bool settled = test_mode_is_settled(&g_wheel_state);
    printf("Settled: %s\n", settled ? "YES" : "NO");

    // Current actual values
    printf("\nActual Values:\n");
    printf("  Speed: %.1f RPM\n", wheel_model_get_speed_rpm(&g_wheel_state));
    printf("  Current: %.3f A\n", g_wheel_state.current_out_a);
    printf("  Torque: %.1f mN·m\n", g_wheel_state.torque_out_mnm);
    printf("  Power: %.2f W\n", g_wheel_state.power_w);

    if (desc->expect_fault) {
        printf("\n⚠️  This test mode expects to trigger a fault\n");
    }

    printf("\n");
}
