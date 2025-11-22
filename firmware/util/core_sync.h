/**
 * @file core_sync.h
 * @brief Inter-Core Synchronization for Dual-Core Architecture
 *
 * Provides safe communication between Core0 (communications) and Core1 (physics).
 *
 * Architecture:
 * - Core0 → Core1: Command mailbox (spinlock-protected)
 * - Core1 → Core0: Telemetry ring buffer (lock-free SPSC)
 *
 * Usage:
 * 1. Call core_sync_init() during startup (before launching Core1)
 * 2. Core0: Use core_sync_send_command() to send commands
 * 3. Core1: Use core_sync_read_command() to read commands
 * 4. Core1: Use core_sync_publish_telemetry() to publish state
 * 5. Core0: Use core_sync_read_telemetry() to read telemetry
 */

#ifndef CORE_SYNC_H
#define CORE_SYNC_H

#include <stdint.h>
#include <stdbool.h>
#include "nss_nrwa_t6_model.h"

// ============================================================================
// Command Mailbox (Core0 → Core1)
// ============================================================================

/**
 * @brief Command types that can be sent from Core0 to Core1
 */
typedef enum {
    CMD_NONE = 0,           // No command pending
    CMD_SET_MODE,           // Change control mode
    CMD_SET_SPEED,          // Set speed setpoint (SPEED mode)
    CMD_SET_CURRENT,        // Set current setpoint (CURRENT mode)
    CMD_SET_TORQUE,         // Set torque setpoint (TORQUE mode)
    CMD_SET_PWM,            // Set PWM duty cycle (PWM mode)
    CMD_CLEAR_FAULT,        // Clear latched faults
    CMD_RESET,              // Soft reset
} command_type_t;

/**
 * @brief Command mailbox structure (spinlock-protected)
 */
typedef struct {
    command_type_t type;    // Command type
    float param1;           // Primary parameter (mode, speed, current, etc.)
    float param2;           // Secondary parameter (reserved)
    uint32_t timestamp_us;  // Command timestamp (for diagnostics)
} command_mailbox_t;

// ============================================================================
// Telemetry Snapshot (Core1 → Core0)
// ============================================================================

/**
 * @brief Telemetry snapshot structure
 *
 * Simplified state snapshot for display in TUI and NSP telemetry.
 * Published by Core1 at 100 Hz, read by Core0 as needed.
 */
typedef struct {
    // Dynamic state
    float omega_rad_s;          // Angular velocity (rad/s)
    float speed_rpm;            // Speed in RPM
    float momentum_nms;         // Angular momentum (N·m·s)

    // Control outputs
    float current_a;            // Actual current (A)
    float torque_mnm;           // Output torque (mN·m)
    float power_w;              // Electrical power (W)
    float voltage_v;            // Bus voltage (V)

    // Control mode
    control_mode_t mode;        // Active control mode
    direction_t direction;      // Rotation direction

    // Protection status
    uint32_t fault_status;      // Active faults (bitmask)
    uint32_t fault_latch;       // Latched faults (bitmask)
    uint32_t warning_status;    // Active warnings (bitmask)
    bool lcl_tripped;           // LCL trip flag

    // Statistics
    uint32_t tick_count;        // Physics tick counter
    uint32_t jitter_us;         // Last tick jitter (µs)
    uint32_t max_jitter_us;     // Maximum jitter observed (µs)

    // Timestamp
    uint64_t timestamp_us;      // Snapshot timestamp
} telemetry_snapshot_t;

// ============================================================================
// Initialization & Management
// ============================================================================

/**
 * @brief Initialize inter-core synchronization
 *
 * Call this once during startup, before launching Core1.
 * Initializes spinlock and ring buffer.
 */
void core_sync_init(void);

// ============================================================================
// Command Mailbox API (Core0 → Core1)
// ============================================================================

/**
 * @brief Send command from Core0 to Core1
 *
 * Writes command to mailbox (spinlock-protected). Returns false if
 * previous command is still pending.
 *
 * @param type Command type
 * @param param1 Primary parameter (mode, speed, current, etc.)
 * @param param2 Secondary parameter (reserved)
 * @return true if command was sent, false if mailbox full
 */
bool core_sync_send_command(command_type_t type, float param1, float param2);

/**
 * @brief Read command from Core1 (called by Core1 physics loop)
 *
 * Reads and clears pending command from mailbox (spinlock-protected).
 * Returns CMD_NONE if no command pending.
 *
 * @param cmd Pointer to command mailbox structure (output)
 * @return true if command was read, false if no command pending
 */
bool core_sync_read_command(command_mailbox_t* cmd);

// ============================================================================
// Telemetry API (Core1 → Core0)
// ============================================================================

/**
 * @brief Publish telemetry snapshot from Core1
 *
 * Writes telemetry snapshot to ring buffer (lock-free).
 * Called by Core1 physics loop after each tick.
 *
 * @param snapshot Pointer to telemetry snapshot
 */
void core_sync_publish_telemetry(const telemetry_snapshot_t* snapshot);

/**
 * @brief Read latest telemetry snapshot from Core0
 *
 * Reads most recent telemetry snapshot from ring buffer (lock-free).
 * Returns false if no new data available.
 *
 * @param snapshot Pointer to telemetry snapshot (output)
 * @return true if new data was read, false if no new data
 */
bool core_sync_read_telemetry(telemetry_snapshot_t* snapshot);

/**
 * @brief Get number of telemetry snapshots available
 *
 * @return Number of snapshots in ring buffer
 */
uint32_t core_sync_telemetry_available(void);

#endif // CORE_SYNC_H
