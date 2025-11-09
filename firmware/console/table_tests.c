/**
 * @file table_tests.c
 * @brief Built-In Tests Table
 *
 * Displays cached test results from boot-time checkpoint tests.
 */

#include "tables.h"
#include "test_results.h"
#include <stdio.h>

// ============================================================================
// Live Value Storage (dummy for now - will show test summary)
// ============================================================================

static volatile uint32_t tests_total = 0;
static volatile uint32_t tests_passed = 0;
static volatile uint32_t tests_failed = 0;
static volatile uint32_t tests_duration_ms = 0;

// ============================================================================
// Field Definitions
// ============================================================================

static const field_meta_t test_fields[] = {
    {
        .id = 100,
        .name = "total_tests",
        .type = FIELD_TYPE_U32,
        .units = "tests",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = &tests_total,
        .dirty = false,
        .enum_values = NULL,
        .enum_count = 0,
    },
    {
        .id = 101,
        .name = "passed",
        .type = FIELD_TYPE_U32,
        .units = "tests",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = &tests_passed,
        .dirty = false,
        .enum_values = NULL,
        .enum_count = 0,
    },
    {
        .id = 102,
        .name = "failed",
        .type = FIELD_TYPE_U32,
        .units = "tests",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = &tests_failed,
        .dirty = false,
        .enum_values = NULL,
        .enum_count = 0,
    },
    {
        .id = 103,
        .name = "duration",
        .type = FIELD_TYPE_U32,
        .units = "ms",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = &tests_duration_ms,
        .dirty = false,
        .enum_values = NULL,
        .enum_count = 0,
    },
};

// ============================================================================
// Table Definition
// ============================================================================

static const table_meta_t test_table = {
    .id = 1,
    .name = "Boot Test Results",
    .description = "Boot-time checkpoint test results",
    .fields = test_fields,
    .field_count = sizeof(test_fields) / sizeof(test_fields[0]),
};

// ============================================================================
// Registration
// ============================================================================

void table_tests_init(void) {
    // Update live values from test results
    tests_total = g_test_results.total_tests;
    tests_passed = g_test_results.total_passed;
    tests_failed = g_test_results.total_tests - g_test_results.total_passed;
    tests_duration_ms = g_test_results.total_duration_ms;

    // Register table
    catalog_register_table(&test_table);
}
