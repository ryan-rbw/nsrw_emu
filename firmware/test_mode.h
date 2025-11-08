/**
 * @file test_mode.h
 * @brief Checkpoint Test Mode Functions
 *
 * All checkpoint tests now run at boot and cache results for TUI display.
 * No more #define CHECKPOINT_X_Y needed - tests always execute.
 *
 * New flow:
 *   int main(void) {
 *       test_results_init();
 *       run_all_checkpoint_tests();  // Fast, results cached
 *       tui_init();                   // Enter interactive TUI
 *       main_loop();                  // TUI shows test results in menu
 *   }
 */

#ifndef TEST_MODE_H
#define TEST_MODE_H

#include <stdint.h>
#include <stdbool.h>
#include "test_results.h"

// ============================================================================
// Phase 3: Core Communication Drivers
// ============================================================================

/**
 * @brief Test CRC-CCITT implementation with known test vectors
 *
 * Validates:
 * - LSB-first bit order
 * - Polynomial 0x1021
 * - Initial value 0xFFFF
 * - Edge cases (empty buffer, single byte, etc.)
 *
 * Prints results to console.
 * Enable with: #define CHECKPOINT_3_1
 */
void test_crc_vectors(void);

/**
 * @brief Test SLIP encoder/decoder with edge cases
 *
 * Validates:
 * - Round-trip preservation
 * - END (0xC0) and ESC (0xDB) handling
 * - Consecutive escapes
 * - Empty frames
 *
 * Prints results to console.
 * Enable with: #define CHECKPOINT_3_2
 */
void test_slip_codec(void);

/**
 * @brief Test RS-485 UART with software loopback
 *
 * Validates:
 * - 460.8 kbps baud rate
 * - DE/RE timing (10µs setup/hold)
 * - Transmit and receive
 * - Buffer management
 *
 * Prints results to console.
 * Enable with: #define CHECKPOINT_3_3
 */
void test_rs485_loopback(void);

/**
 * @brief Test NSP protocol with PING/ACK exchange
 *
 * Validates:
 * - NSP frame structure
 * - CRC calculation on full packet
 * - PING command handling
 * - ACK generation
 *
 * Prints results to console.
 * Enable with: #define CHECKPOINT_3_4
 */
void test_nsp_ping(void);


// ============================================================================
// Phase 4: Utilities Foundation
// ============================================================================

/**
 * @brief Test ring buffer with stress test
 *
 * Validates:
 * - SPSC ring buffer operations
 * - FIFO ordering
 * - Full/empty detection
 * - 1M operation stress test
 *
 * Prints results to console.
 * Enable with: #define CHECKPOINT_4_1
 */
void test_ringbuf_stress(void);

/**
 * @brief Test fixed-point math accuracy
 *
 * Validates:
 * - Float ↔ fixed-point conversions (within 1 LSB)
 * - Arithmetic operations (add, sub, multiply)
 * - Saturation behavior
 *
 * Prints results to console.
 * Enable with: #define CHECKPOINT_4_2
 */
void test_fixedpoint_accuracy(void);

// ============================================================================
// Phase 5: Device Model & Physics
// ============================================================================

/**
 * @brief Test register map structure and organization
 *
 * Validates:
 * - Register address validity and ranges
 * - Read-only/read-write access permissions
 * - Register size detection (1, 2, 4 bytes)
 * - Register name lookup
 * - Non-overlapping address spaces
 *
 * Prints results to console.
 * Enable with: #define CHECKPOINT_5_1
 */
void test_register_map(void);

/**
 * @brief Test wheel physics model and control modes
 *
 * Validates:
 * - Model initialization
 * - CURRENT mode (direct torque control)
 * - SPEED mode (PI controller ramp to 1000 RPM)
 * - TORQUE mode (feed-forward control)
 * - Power limiting (100 W enforced)
 * - Loss model (deceleration)
 * - Overspeed protection (fault at 6000 RPM)
 *
 * Prints results to console.
 * Enable with: #define CHECKPOINT_5_2
 */
void test_wheel_physics(void);

/**
 * @brief Test hardware reset and LCL fault handling
 *
 * Validates per ICD Section 10.2.6 and RESET_FAULT_REQUIREMENTS.md:
 * - Reset clears LCL trip flag
 * - Reset preserves momentum (wheel coasts)
 * - Reset restores default mode/configuration
 * - LCL trip disables motor immediately
 * - CLEAR-FAULT does not affect LCL state
 * - Reset during spin-up operation
 * - Reset during overspeed fault condition
 * - Hard faults automatically trip LCL
 *
 * Prints results to console.
 * Enable with: #define CHECKPOINT_5_3
 */
void test_reset_and_faults(void);

/**
 * @brief Test NSP command handlers
 *
 * Validates all 8 NSP commands:
 * - PING [0x00]
 * - PEEK [0x02] (register read)
 * - POKE [0x03] (register write)
 * - APPLICATION-TELEMETRY [0x07]
 * - APPLICATION-COMMAND [0x08]
 * - CLEAR-FAULT [0x09]
 * - CONFIGURE-PROTECTION [0x0A]
 * - TRIP-LCL [0x0B]
 *
 * Prints results to console.
 * Enable with: #define CHECKPOINT_6_1
 */
void test_nsp_commands(void);

// ============================================================================
// Phase 7: Protection System
// ============================================================================

/**
 * @brief Test protection system thresholds and fault handling
 *
 * Validates:
 * - Protection initialization with defaults
 * - Threshold parameter get/set with fixed-point encoding
 * - Protection enable/disable flags
 * - Fault detection (overvoltage, overspeed, overpower, overcurrent)
 * - Fault latching behavior
 * - LCL trip logic
 * - Metadata functions (names, units, fault info)
 *
 * Prints results to console.
 */
void test_protection(void);

// ============================================================================
// Master Test Runner
// ============================================================================

/**
 * @brief Run all checkpoint tests and cache results
 *
 * Executes all tests from Phases 3-7 at boot time.
 * Results are stored in g_test_results for TUI display.
 *
 * Call this ONCE at startup before entering TUI.
 */
void run_all_checkpoint_tests(void);

// ============================================================================
// Helper Macros
// ============================================================================

/**
 * @brief Print test result with checkmark or X (and record in registry)
 *
 * Usage: TEST_RESULT("CRC empty buffer", crc == 0xFFFF);
 */
#define TEST_RESULT(name, passed) do { \
    bool _passed = (passed); \
    printf("  %s: %s\n", (name), _passed ? "✓ PASS" : "✗ FAIL"); \
    test_record_result((name), _passed, 0); \
} while(0)

/**
 * @brief Print test section header
 */
#define TEST_SECTION(name) \
    printf("\n=== %s ===\n", (name))

/**
 * @brief Begin checkpoint test (with result recording)
 *
 * Usage: TEST_CHECKPOINT_BEGIN(3, 1, "CRC-CCITT");
 */
#define TEST_CHECKPOINT_BEGIN(phase, cp, name) do { \
    printf("\n╔════════════════════════════════════════════════════════════╗\n"); \
    printf("║  CHECKPOINT %d.%d: %-42s║\n", (phase), (cp), (name)); \
    printf("╚════════════════════════════════════════════════════════════╝\n"); \
    test_checkpoint_begin((phase), (cp), (name)); \
} while(0)

/**
 * @brief End checkpoint test (and compute summary)
 */
#define TEST_CHECKPOINT_END() do { \
    test_checkpoint_end(); \
    printf("\n"); \
} while(0)

#endif // TEST_MODE_H
