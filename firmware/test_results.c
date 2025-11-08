/**
 * @file test_results.c
 * @brief Built-In Test Results Storage Implementation
 */

#include "test_results.h"
#include <string.h>
#include <stdio.h>

// ============================================================================
// Global State
// ============================================================================

test_registry_t g_test_results;

// Current checkpoint being recorded
static checkpoint_results_t* current_checkpoint = NULL;

// Static storage for test results (avoids malloc)
static test_result_t test_storage[MAX_CHECKPOINTS][MAX_TESTS_PER_CHECKPOINT];
static uint8_t test_counts[MAX_CHECKPOINTS];

// ============================================================================
// Initialization
// ============================================================================

void test_results_init(void) {
    memset(&g_test_results, 0, sizeof(g_test_results));
    memset(test_storage, 0, sizeof(test_storage));
    memset(test_counts, 0, sizeof(test_counts));
    current_checkpoint = NULL;
}

// ============================================================================
// Checkpoint Management
// ============================================================================

void test_checkpoint_begin(uint8_t phase, uint8_t checkpoint, const char* name) {
    if (g_test_results.checkpoint_count >= MAX_CHECKPOINTS) {
        printf("[TEST] ERROR: Checkpoint limit reached (%d)\n", MAX_CHECKPOINTS);
        return;
    }

    uint8_t idx = g_test_results.checkpoint_count;
    current_checkpoint = &g_test_results.checkpoints[idx];

    current_checkpoint->checkpoint_name = name;
    current_checkpoint->phase = phase;
    current_checkpoint->checkpoint = checkpoint;
    current_checkpoint->tests = test_storage[idx];
    current_checkpoint->test_count = 0;
    current_checkpoint->passed_count = 0;
    current_checkpoint->total_duration_us = 0;

    test_counts[idx] = 0;
}

void test_checkpoint_end(void) {
    if (!current_checkpoint) {
        return;
    }

    // Update global stats
    g_test_results.total_tests += current_checkpoint->test_count;
    g_test_results.total_passed += current_checkpoint->passed_count;
    g_test_results.total_duration_ms += (current_checkpoint->total_duration_us / 1000);

    // Check if all tests passed
    if (current_checkpoint->test_count == current_checkpoint->passed_count) {
        // All passed for this checkpoint
    } else {
        g_test_results.all_passed = false;
    }

    g_test_results.checkpoint_count++;
    current_checkpoint = NULL;
}

// ============================================================================
// Test Recording
// ============================================================================

void test_record_result(const char* name, bool passed, uint32_t duration_us) {
    if (!current_checkpoint) {
        printf("[TEST] ERROR: No active checkpoint\n");
        return;
    }

    uint8_t idx = g_test_results.checkpoint_count;
    if (test_counts[idx] >= MAX_TESTS_PER_CHECKPOINT) {
        printf("[TEST] ERROR: Test limit reached for checkpoint\n");
        return;
    }

    test_result_t* test = &current_checkpoint->tests[test_counts[idx]];
    test->name = name;
    test->passed = passed;
    test->duration_us = duration_us;

    current_checkpoint->test_count++;
    if (passed) {
        current_checkpoint->passed_count++;
    }
    current_checkpoint->total_duration_us += duration_us;

    test_counts[idx]++;
}

// ============================================================================
// Query API
// ============================================================================

char* test_get_summary(char* buf, size_t buflen) {
    snprintf(buf, buflen,
             "Built-In Tests: %d/%d passed (%d checkpoints, %.2f ms)",
             g_test_results.total_passed,
             g_test_results.total_tests,
             g_test_results.checkpoint_count,
             g_test_results.total_duration_ms / 1000.0f);
    return buf;
}

const checkpoint_results_t* test_get_checkpoint(uint8_t index) {
    if (index >= g_test_results.checkpoint_count) {
        return NULL;
    }
    return &g_test_results.checkpoints[index];
}
