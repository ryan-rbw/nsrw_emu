/**
 * @file nss_nrwa_t6_commands.h
 * @brief NSS NRWA-T6 NSP Command Handlers
 *
 * Implements all 8 NSP commands per ICD:
 * - 0x00 PING
 * - 0x02 PEEK (register read)
 * - 0x03 POKE (register write)
 * - 0x07 APPLICATION-TELEMETRY (request telemetry block)
 * - 0x08 APPLICATION-COMMAND (mode/setpoint changes)
 * - 0x09 CLEAR-FAULT (clear latched faults)
 * - 0x0A CONFIGURE-PROTECTION (update protection thresholds)
 * - 0x0B TRIP-LCL (test LCL trip)
 */

#ifndef NSS_NRWA_T6_COMMANDS_H
#define NSS_NRWA_T6_COMMANDS_H

#include <stdint.h>
#include <stdbool.h>
#include "nss_nrwa_t6_model.h"

// ============================================================================
// NSP Command Codes (from ICD)
// ============================================================================

#define NSP_CMD_PING                    0x00
#define NSP_CMD_PEEK                    0x02
#define NSP_CMD_POKE                    0x03
#define NSP_CMD_APPLICATION_TELEMETRY   0x07
#define NSP_CMD_APPLICATION_COMMAND     0x08
#define NSP_CMD_CLEAR_FAULT             0x09
#define NSP_CMD_CONFIGURE_PROTECTION    0x0A
#define NSP_CMD_TRIP_LCL                0x0B

// ============================================================================
// Command Response Types
// ============================================================================

/**
 * @brief Command response status
 */
typedef enum {
    CMD_ACK = 0,        // Command successful
    CMD_NACK = 1,       // Command failed (invalid parameter, out of range, etc.)
} cmd_response_t;

/**
 * @brief Command handler result
 */
typedef struct {
    cmd_response_t status;      // ACK or NACK
    uint8_t* data;              // Response payload (NULL if none)
    uint16_t data_len;          // Payload length in bytes
} cmd_result_t;

// ============================================================================
// Command Handler API
// ============================================================================

/**
 * @brief Initialize command handler subsystem
 *
 * @param state Pointer to global wheel state
 */
void commands_init(wheel_state_t* state);

/**
 * @brief Dispatch NSP command to appropriate handler
 *
 * @param command Command code (0x00-0x0B)
 * @param payload Command payload data
 * @param payload_len Payload length in bytes
 * @param result Pointer to result structure (filled by handler)
 * @return true if command was recognized and handled
 */
bool commands_dispatch(uint8_t command, const uint8_t* payload, uint16_t payload_len, cmd_result_t* result);

// ============================================================================
// Individual Command Handlers
// ============================================================================

/**
 * @brief PING [0x00]: Simple ACK responder
 *
 * Payload: None
 * Response: ACK with no data
 *
 * @param payload Command payload (unused)
 * @param payload_len Payload length (should be 0)
 * @param result Pointer to result structure
 */
void cmd_ping(const uint8_t* payload, uint16_t payload_len, cmd_result_t* result);

/**
 * @brief PEEK [0x02]: Read register(s)
 *
 * Payload format:
 *   [start_addr:2] [count:1]
 *
 * Response: ACK with register data
 *   [reg0_value] [reg1_value] ...
 *
 * @param payload Command payload
 * @param payload_len Payload length (should be 3)
 * @param result Pointer to result structure
 */
void cmd_peek(const uint8_t* payload, uint16_t payload_len, cmd_result_t* result);

/**
 * @brief POKE [0x03]: Write register(s)
 *
 * Payload format:
 *   [start_addr:2] [count:1] [reg0_value] [reg1_value] ...
 *
 * Response: ACK if successful, NACK if invalid address or read-only
 *
 * @param payload Command payload
 * @param payload_len Payload length (3 + register data)
 * @param result Pointer to result structure
 */
void cmd_poke(const uint8_t* payload, uint16_t payload_len, cmd_result_t* result);

/**
 * @brief APPLICATION-TELEMETRY [0x07]: Request telemetry block
 *
 * Payload format:
 *   [block_id:1]
 *
 * Block IDs:
 *   0x00: STANDARD (status, mode, speed, current, torque, power)
 *   0x01: TEMPERATURES (motor, driver, board temps)
 *   0x02: VOLTAGES (bus, phase voltages)
 *   0x03: CURRENTS (phase, bus currents)
 *   0x04: DIAGNOSTICS (tick count, uptime, stats)
 *
 * Response: ACK with telemetry block data
 *
 * @param payload Command payload
 * @param payload_len Payload length (should be 1)
 * @param result Pointer to result structure
 */
void cmd_application_telemetry(const uint8_t* payload, uint16_t payload_len, cmd_result_t* result);

/**
 * @brief APPLICATION-COMMAND [0x08]: Mode/setpoint changes
 *
 * Payload format:
 *   [subcommand:1] [params...]
 *
 * Subcommands:
 *   0x00: SET-MODE [mode:1]
 *   0x01: SET-SPEED [speed_rpm:4 (UQ14.18)]
 *   0x02: SET-CURRENT [current_ma:4 (UQ18.14)]
 *   0x03: SET-TORQUE [torque_mnm:4 (UQ18.14)]
 *   0x04: SET-PWM [duty_pct:2 (UQ8.8)]
 *   0x05: SET-DIRECTION [direction:1]
 *
 * Response: ACK if successful, NACK if invalid mode/value
 *
 * @param payload Command payload
 * @param payload_len Payload length (varies by subcommand)
 * @param result Pointer to result structure
 */
void cmd_application_command(const uint8_t* payload, uint16_t payload_len, cmd_result_t* result);

/**
 * @brief CLEAR-FAULT [0x09]: Clear latched faults
 *
 * Payload format:
 *   [fault_mask:4]
 *
 * Clears faults specified by bitmask (0xFFFFFFFF = all faults).
 * Does NOT clear LCL trip flag (requires hardware RESET).
 *
 * Response: ACK
 *
 * @param payload Command payload
 * @param payload_len Payload length (should be 4)
 * @param result Pointer to result structure
 */
void cmd_clear_fault(const uint8_t* payload, uint16_t payload_len, cmd_result_t* result);

/**
 * @brief CONFIGURE-PROTECTION [0x0A]: Update protection thresholds
 *
 * Payload format:
 *   [param_id:1] [value:4]
 *
 * Parameter IDs:
 *   0x00: Overvoltage threshold (V, UQ16.16)
 *   0x01: Overspeed fault (RPM, UQ14.18)
 *   0x02: Overspeed soft (RPM, UQ14.18)
 *   0x03: Max duty cycle (%, UQ8.8)
 *   0x04: Motor overpower (W, UQ16.16)
 *   0x05: Soft overcurrent (A, UQ16.16)
 *
 * Response: ACK if successful, NACK if invalid parameter ID
 *
 * @param payload Command payload
 * @param payload_len Payload length (should be 5)
 * @param result Pointer to result structure
 */
void cmd_configure_protection(const uint8_t* payload, uint16_t payload_len, cmd_result_t* result);

/**
 * @brief TRIP-LCL [0x0B]: Test LCL trip
 *
 * Payload: None
 * Response: ACK (LCL will be tripped, motor disabled)
 *
 * Simulates LCL trip condition. Requires hardware RESET to clear.
 *
 * @param payload Command payload (unused)
 * @param payload_len Payload length (should be 0)
 * @param result Pointer to result structure
 */
void cmd_trip_lcl(const uint8_t* payload, uint16_t payload_len, cmd_result_t* result);

#endif // NSS_NRWA_T6_COMMANDS_H
