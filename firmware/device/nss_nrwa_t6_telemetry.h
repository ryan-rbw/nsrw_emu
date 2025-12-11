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
 * @brief Build TEMPERATURES telemetry block (8 bytes per ICD Table 12-16)
 *
 * Block format (8 bytes):
 *   [dcdc_temp:2]           // DC-DC Temperature (°C, 16-bit unsigned)
 *   [enclosure_temp:2]      // Enclosure Temperature (°C, 16-bit unsigned)
 *   [motor_driver_temp:2]   // Motor Driver Temperature (°C, 16-bit unsigned)
 *   [motor_temp:2]          // Motor Temperature (°C, 16-bit unsigned)
 *
 * Note: Simulated temps (fixed at 25°C until thermal model implemented)
 *
 * @param state Pointer to wheel state
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Block size in bytes (8)
 */
uint16_t telemetry_build_temperatures(const wheel_state_t* state, uint8_t* buffer, uint16_t buffer_size);

/**
 * @brief Build VOLTAGES telemetry block (24 bytes per ICD Table 12-17)
 *
 * Block format (24 bytes):
 *   [voltage_1v5:4]     // 1.5V supply voltage (V, UQ16.16)
 *   [voltage_3v3:4]     // 3.3V supply voltage (V, UQ16.16)
 *   [voltage_5va:4]     // Analog 5V supply voltage (V, UQ16.16)
 *   [voltage_12v:4]     // 12V supply voltage (V, UQ16.16)
 *   [voltage_30v:4]     // 30V supply voltage (V, UQ16.16)
 *   [voltage_2v5_ref:4] // 2.5V reference voltage (V, UQ16.16)
 *
 * @param state Pointer to wheel state
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Block size in bytes (24)
 */
uint16_t telemetry_build_voltages(const wheel_state_t* state, uint8_t* buffer, uint16_t buffer_size);

/**
 * @brief Build CURRENTS telemetry block (24 bytes per ICD Table 12-18)
 *
 * Block format (24 bytes):
 *   [current_1v5:4]     // 1.5V supply current (mA, UQ16.16)
 *   [current_3v3:4]     // 3.3V supply current (mA, UQ16.16)
 *   [current_5va:4]     // Analog 5V supply current (mA, UQ16.16)
 *   [current_5vd:4]     // Digital 5V supply current (mA, UQ16.16)
 *   [current_12v:4]     // 12V supply current (mA, UQ16.16)
 *   [current_30v:4]     // 30V supply current (A, Q16.16 signed)
 *
 * @param state Pointer to wheel state
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Block size in bytes (24)
 */
uint16_t telemetry_build_currents(const wheel_state_t* state, uint8_t* buffer, uint16_t buffer_size);

/**
 * @brief Build DIAGNOSTICS-GENERAL telemetry block (20 bytes per ICD Table 12-19)
 *
 * Block format (20 bytes):
 *   [uptime_counter:4]        // Uptime counter (UQ30.2 seconds)
 *   [revolution_count:4]      // Revolution count (32-bit unsigned)
 *   [hall_invalid_count:4]    // Hall sensor invalid transition count (32-bit unsigned)
 *   [drive_fault_count:4]     // Drive-Fault count (32-bit unsigned)
 *   [drive_overtemp_count:4]  // Drive-Overtemperature count (32-bit unsigned)
 *
 * @param state Pointer to wheel state
 * @param buffer Output buffer
 * @param buffer_size Buffer size
 * @return Block size in bytes (20)
 */
uint16_t telemetry_build_diagnostics(const wheel_state_t* state, uint8_t* buffer, uint16_t buffer_size);

#endif // NSS_NRWA_T6_TELEMETRY_H
