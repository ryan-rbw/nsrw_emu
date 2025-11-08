/**
 * @file test_results.h
 * @brief Built-In Test Results Storage
 *
 * Collects test results from all checkpoints at boot time.
 * Results are cached and displayed in TUI "Built-In Tests" table.
 */

#ifndef TEST_RESULTS_H
#define TEST_RESULTS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>  // For size_t

// ============================================================================
// Test Result Storage
// ============================================================================

/**
 * @brief Individual test result
 */
typedef struct {
    const char* name;       // Test name (e.g., "CRC-CCITT: Empty buffer")
    bool passed;            // true = PASS, false = FAIL
    uint32_t duration_us;   // Test execution time (microseconds)
} test_result_t;

/**
 * @brief Checkpoint test suite results
 */
typedef struct {
    const char* checkpoint_name;    // e.g., "3.1: CRC-CCITT"
    uint8_t phase;                  // Phase number (3-7)
    uint8_t checkpoint;             // Checkpoint number (1-3)
    test_result_t* tests;           // Array of test results
    uint8_t test_count;             // Number of tests
    uint8_t passed_count;           // Number of passed tests
    uint32_t total_duration_us;     // Total execution time
} checkpoint_results_t;

// ============================================================================
// Global Test Results Storage
// ============================================================================

/**
 * @brief Maximum number of checkpoints (Phases 3-7, ~14 checkpoints)
 */
#define MAX_CHECKPOINTS  16

/**
 * @brief Maximum tests per checkpoint
 */
#define MAX_TESTS_PER_CHECKPOINT  32

/**
 * @brief Global test results registry
 */
typedef struct {
    checkpoint_results_t checkpoints[MAX_CHECKPOINTS];
    uint8_t checkpoint_count;
    uint8_t total_tests;
    uint8_t total_passed;
    uint32_t total_duration_ms;
    bool all_passed;
} test_registry_t;

// Global test registry (allocated at boot)
extern test_registry_t g_test_results;

// ============================================================================
// Test Registration API
// ============================================================================

/**
 * @brief Initialize test results registry
 *
 * Call before running any tests.
 */
void test_results_init(void);

/**
 * @brief Begin a checkpoint test suite
 *
 * @param phase Phase number (3-7)
 * @param checkpoint Checkpoint number (1-3)
 * @param name Checkpoint name (e.g., "CRC-CCITT Implementation")
 */
void test_checkpoint_begin(uint8_t phase, uint8_t checkpoint, const char* name);

/**
 * @brief Record a test result
 *
 * @param name Test name
 * @param passed true = PASS, false = FAIL
 * @param duration_us Execution time in microseconds
 */
void test_record_result(const char* name, bool passed, uint32_t duration_us);

/**
 * @brief End current checkpoint test suite
 *
 * Computes summary statistics (total tests, passed, duration).
 */
void test_checkpoint_end(void);

/**
 * @brief Get overall test summary
 *
 * @param buf Output buffer for formatted summary
 * @param buflen Buffer size
 * @return Pointer to buf
 */
char* test_get_summary(char* buf, size_t buflen);

/**
 * @brief Get checkpoint results by index
 *
 * @param index Checkpoint index (0 to g_test_results.checkpoint_count-1)
 * @return Pointer to checkpoint results, or NULL if invalid
 */
const checkpoint_results_t* test_get_checkpoint(uint8_t index);

#endif // TEST_RESULTS_H
