/**
 * @file test_mode.h
 * @brief Checkpoint Test Mode Functions
 *
 * This file provides test functions for each checkpoint in the implementation plan.
 * Enable specific checkpoints by defining CHECKPOINT_X_Y before including this file.
 *
 * Usage:
 *   #define CHECKPOINT_3_1
 *   #include "test_mode.h"
 *
 *   int main(void) {
 *       #ifdef CHECKPOINT_3_1
 *       test_crc_vectors();
 *       while(1) { sleep_ms(1000); }  // Halt for inspection
 *       #endif
 *   }
 */

#ifndef TEST_MODE_H
#define TEST_MODE_H

#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// Phase 3: Core Communication Drivers
// ============================================================================

#ifdef CHECKPOINT_3_1
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
 */
void test_crc_vectors(void);
#endif // CHECKPOINT_3_1

#ifdef CHECKPOINT_3_2
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
 */
void test_slip_codec(void);
#endif // CHECKPOINT_3_2

#ifdef CHECKPOINT_3_3
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
 */
void test_rs485_loopback(void);
#endif // CHECKPOINT_3_3

#ifdef CHECKPOINT_3_4
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
 */
void test_nsp_ping(void);
#endif // CHECKPOINT_3_4

// ============================================================================
// Phase 4: Utilities Foundation (future checkpoints)
// ============================================================================

#ifdef CHECKPOINT_4_1
void test_ringbuf_stress(void);
#endif

#ifdef CHECKPOINT_4_2
void test_fixedpoint_math(void);
#endif

// ============================================================================
// Phase 5: Device Model & Physics (future checkpoints)
// ============================================================================

#ifdef CHECKPOINT_5_1
void test_physics_init(void);
#endif

#ifdef CHECKPOINT_5_2
void test_wheel_spinup(void);
#endif

#ifdef CHECKPOINT_5_3
void test_control_modes(void);
#endif

// ============================================================================
// Helper Macros
// ============================================================================

/**
 * @brief Print test result with checkmark or X
 */
#define TEST_RESULT(name, passed) \
    printf("  %s: %s\n", (name), (passed) ? "✓ PASS" : "✗ FAIL")

/**
 * @brief Print test section header
 */
#define TEST_SECTION(name) \
    printf("\n=== %s ===\n", (name))

#endif // TEST_MODE_H
