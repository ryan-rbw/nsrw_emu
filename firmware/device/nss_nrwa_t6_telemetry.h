/**
 * @file nss_nrwa_t6_telemetry.h
 * @brief NSS NRWA-T6 Telemetry Block Builder
 *
 * Implements 5 telemetry blocks per ICD:
 * - Block 0x00: STANDARD (status, mode, speed, current, torque, power)
 * - Block 0x01: TEMPERATURES (motor, driver, board temps)
 * - Block 0x02: VOLTAGES (bus, phase voltages)
 * - Block 0x03: CURRENTS (phase, bus currents)
 * - Block 0x04: DIAGNOSTICS (tick count, uptime, stats)
 */

#ifndef NSS_NRWA_T6_TELEMETRY_H
#define NSS_NRWA_T6_TELEMETRY_H

#include <stdint.h>
#include "nss_nrwa_t6_model.h"

// ============================================================================
// Telemetry Block IDs
// ============================================================================

#define TELEM_BLOCK_STANDARD        0x00
#define TELEM_BLOCK_TEMPERATURES    0x01
#define TELEM_BLOCK_VOLTAGES        0x02
#define TELEM_BLOCK_CURRENTS        0x03
#define TELEM_BLOCK_DIAGNOSTICS     0x04

// ============================================================================
// Telemetry API
// ============================================================================

/**
 * @brief Build telemetry block
 *
 * @param block_id Telemetry block ID (0x00-0x04)
 * @param state Pointer to wheel state
 * @param buffer Output buffer
 * @param buffer_size Buffer size in bytes
 * @return Block size in bytes (0 if invalid block ID or error)
 */
uint16_t telemetry_build_block(uint8_t block_id, const wheel_state_t* state, uint8_t* buffer, uint16_t buffer_size);

/**
 * @brief Build STANDARD telemetry block
 *
 * Block format (~30 bytes):
 *   [status:4]          // Operational state
 *   [fault_status:4]    // Active faults
 *   [fault_latch:4]     // Latched faults
 *   [warning_status:4]  // Warnings
 *   [mode:1]            // Control mode
 *   [direction:1]       // Rotation direction
 *   [speed_rpm:4]       // Speed (UQ14.18)
 *   [current_ma:4]      // Current (UQ18.14)
 *   [torque_mnm:4]      // Torque (UQ18.14)
 *   [power_mw:4]        // Power (UQ18.14)
 *   [momentum_unms:4]   // Momentum (UQ18.14)
 *
 * @param state Pointer to wheel state
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Block size in bytes
 */
uint16_t telemetry_build_standard(const wheel_state_t* state, uint8_t* buffer, uint16_t buffer_size);

/**
 * @brief Build TEMPERATURES telemetry block
 *
 * Block format (~6 bytes):
 *   [motor_temp:2]      // Motor temp (°C, UQ8.8)
 *   [driver_temp:2]     // Driver temp (°C, UQ8.8)
 *   [board_temp:2]      // Board temp (°C, UQ8.8)
 *
 * Note: Simulated temps for Phase 6 (fixed at 25°C)
 *
 * @param state Pointer to wheel state
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Block size in bytes
 */
uint16_t telemetry_build_temperatures(const wheel_state_t* state, uint8_t* buffer, uint16_t buffer_size);

/**
 * @brief Build VOLTAGES telemetry block
 *
 * Block format (~12 bytes):
 *   [bus_voltage:4]     // Bus voltage (V, UQ16.16)
 *   [phase_a_voltage:4] // Phase A voltage (V, UQ16.16)
 *   [phase_b_voltage:4] // Phase B voltage (V, UQ16.16)
 *
 * Note: Phase voltages derived from bus voltage
 *
 * @param state Pointer to wheel state
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Block size in bytes
 */
uint16_t telemetry_build_voltages(const wheel_state_t* state, uint8_t* buffer, uint16_t buffer_size);

/**
 * @brief Build CURRENTS telemetry block
 *
 * Block format (~12 bytes):
 *   [phase_a_current:4] // Phase A current (mA, UQ18.14)
 *   [phase_b_current:4] // Phase B current (mA, UQ18.14)
 *   [bus_current:4]     // Bus current (mA, UQ18.14)
 *
 * Note: Phase currents derived from motor current
 *
 * @param state Pointer to wheel state
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Block size in bytes
 */
uint16_t telemetry_build_currents(const wheel_state_t* state, uint8_t* buffer, uint16_t buffer_size);

/**
 * @brief Build DIAGNOSTICS telemetry block
 *
 * Block format (~20 bytes):
 *   [tick_count:4]      // Physics tick count
 *   [uptime_seconds:4]  // Uptime in seconds
 *   [fault_count:4]     // Total fault occurrences (TBD)
 *   [command_count:4]   // Total commands processed (TBD)
 *   [max_jitter_us:2]   // Max jitter (µs)
 *
 * @param state Pointer to wheel state
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Block size in bytes
 */
uint16_t telemetry_build_diagnostics(const wheel_state_t* state, uint8_t* buffer, uint16_t buffer_size);

#endif // NSS_NRWA_T6_TELEMETRY_H
