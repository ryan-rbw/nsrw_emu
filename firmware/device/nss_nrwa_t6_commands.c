/**
 * @file nss_nrwa_t6_commands.c
 * @brief NSS NRWA-T6 NSP Command Handlers Implementation
 */

#include "nss_nrwa_t6_commands.h"
#include "nss_nrwa_t6_regs.h"
#include "nss_nrwa_t6_telemetry.h"
#include "nss_nrwa_t6_protection.h"
#include "fixedpoint.h"
#include "unaligned.h"
#include "util/core_sync.h"
#include <string.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>

// Debug flag (set to false to disable verbose command logging)
static bool debug_commands = false;

// ============================================================================
// Global State
// ============================================================================

static wheel_state_t* g_wheel_state = NULL;

// Response buffer (max telemetry block size ~60 bytes)
static uint8_t response_buffer[128];

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Build ACK response with no data
 */
static void build_ack(cmd_result_t* result) {
    result->status = CMD_ACK;
    result->data = NULL;
    result->data_len = 0;
}

/**
 * @brief Build NACK response
 */
static void build_nack(cmd_result_t* result) {
    result->status = CMD_NACK;
    result->data = NULL;
    result->data_len = 0;
}

/**
 * @brief Build ACK response with data payload
 */
static void build_ack_with_data(cmd_result_t* result, const uint8_t* data, uint16_t len) {
    result->status = CMD_ACK;
    memcpy(response_buffer, data, len);
    result->data = response_buffer;
    result->data_len = len;
}

/**
 * @brief Validate register address and count
 *
 * @param addr Starting register address
 * @param count Number of registers
 * @param write true if write operation (check read-only)
 * @return true if valid
 */
static bool validate_register_access(uint16_t addr, uint8_t count, bool write) {
    // Register map address ranges:
    // 0x0000-0x00FF: Device Information (read-only)
    // 0x0100-0x01FF: Protection Configuration (read/write)
    // 0x0200-0x02FF: Control Registers (read/write)
    // 0x0300-0x03FF: Status Registers (read-only)
    // 0x0400-0x04FF: Fault & Diagnostic Registers (read/write)

    // Check if entire access range is within valid address space
    if (addr + (count * 4) > 0x0500) {
        return false;  // Out of register map range
    }

    // Check read-only regions if write operation
    if (write) {
        // Device Information (0x0000-0x00FF) is read-only
        if (addr >= 0x0000 && addr < 0x0100) {
            return false;
        }
        // Status Registers (0x0300-0x03FF) are read-only
        if (addr >= 0x0300 && addr < 0x0400) {
            return false;
        }
    }

    return true;
}

/**
 * @brief Read register value from wheel state
 *
 * @param addr Register address
 * @param value Pointer to output value (4 bytes, little-endian)
 * @return true if successful
 */
static bool read_register(uint16_t addr, uint8_t* value) {
    // Map register addresses to wheel state fields
    // For Phase 6, implement key registers only

    switch (addr) {
        // Device Information Registers (read-only)
        case REG_DEVICE_ID:
            write_u32_le(value, 0x4E525754);  // "NRWT" in ASCII
            return true;

        case REG_FIRMWARE_VERSION:
            write_u32_le(value, 0x00000100);  // v0.1.0 in BCD
            return true;

        case REG_HARDWARE_REVISION:
            write_u16_le(value, 0x0001);  // Hardware rev 1
            return true;

        case REG_SERIAL_NUMBER:
            write_u32_le(value, 0x00000001);  // Serial #1
            return true;

        // Fault/Warning Status
        case REG_FAULT_STATUS:
            write_u32_le(value, g_wheel_state->fault_status);
            return true;

        case REG_FAULT_LATCH:
            write_u32_le(value, g_wheel_state->fault_latch);
            return true;

        case REG_WARNING_STATUS:
            write_u32_le(value, g_wheel_state->warning_status);
            return true;

        // Control Mode
        case REG_CONTROL_MODE:
            write_u32_le(value, (uint32_t)g_wheel_state->mode);
            return true;

        // Current State (Status Registers)
        case REG_CURRENT_SPEED_RPM:
            write_u32_le(value, float_to_uq14_18(wheel_model_get_speed_rpm(g_wheel_state)));
            return true;

        case REG_CURRENT_CURRENT_MA:
            write_u32_le(value, float_to_uq18_14(g_wheel_state->current_out_a * 1000.0f));
            return true;

        case REG_CURRENT_TORQUE_MNM:
            write_u32_le(value, float_to_uq18_14(g_wheel_state->torque_out_mnm));
            return true;

        case REG_CURRENT_POWER_MW:
            write_u32_le(value, float_to_uq18_14(g_wheel_state->power_w * 1000.0f));
            return true;

        case REG_CURRENT_MOMENTUM_NMS:
            write_u32_le(value, float_to_uq18_14(g_wheel_state->momentum_nms * 1000.0f));
            return true;

        default:
            // Unknown register
            return false;
    }
}

// ============================================================================
// ICD Register Address Translation
// ============================================================================

/**
 * @brief Map ICD 8-bit address to internal register and read
 *
 * ICD Table 11-1 defines the Initialized Parameters address space (0x00-0x30).
 * This function translates ICD addresses to our internal register map.
 *
 * @param icd_addr ICD 8-bit address (0x00-0x30)
 * @param value Pointer to output buffer (4 bytes, little-endian)
 * @return true if successful
 */
static bool read_register_icd(uint8_t icd_addr, uint8_t* value) {
    // Map ICD addresses to internal registers
    // ICD uses contiguous 4-byte addresses (0x00, 0x04, 0x08, etc.)
    switch (icd_addr) {
        // Device Information
        case 0x00:  // Device ID
            write_u32_le(value, 0x4E525754);  // "NRWT" in ASCII
            return true;

        case 0x04:  // Firmware version
            write_u32_le(value, 0x00010000);  // v1.0.0 in BCD
            return true;

        case 0x08:  // Control mode (current mode as ICD bitmask)
            write_u32_le(value, (uint32_t)index_to_icd_mode(g_wheel_state->mode));
            return true;

        case 0x0C:  // Speed setpoint (Q14.18 RPM)
            write_u32_le(value, float_to_uq14_18(g_wheel_state->speed_cmd_rpm));
            return true;

        case 0x10:  // Current setpoint (Q14.18 mA)
            write_u32_le(value, float_to_uq14_18(g_wheel_state->current_cmd_a * 1000.0f));
            return true;

        case 0x14:  // Torque setpoint (Q10.22 mN-m)
            write_u32_le(value, float_to_q10_22(g_wheel_state->torque_cmd_mnm));
            return true;

        case 0x18:  // PWM duty cycle
            {
                int16_t pwm_raw = (int16_t)(g_wheel_state->pwm_duty_pct * 5.12f);
                if (g_wheel_state->direction == DIRECTION_NEGATIVE) pwm_raw = -pwm_raw;
                write_u32_le(value, (uint32_t)(int32_t)pwm_raw);
            }
            return true;

        case 0x1C:  // Fault status
            write_u32_le(value, g_wheel_state->fault_status | g_wheel_state->fault_latch);
            return true;

        case 0x20:  // Warning status
            write_u32_le(value, g_wheel_state->warning_status);
            return true;

        case 0x24:  // Current measurement (Q20.12 mA)
            write_u32_le(value, float_to_q20_12(g_wheel_state->current_out_a * 1000.0f));
            return true;

        case 0x28:  // Speed measurement (Q24.8 RPM)
            write_u32_le(value, float_to_q24_8(wheel_model_get_speed_rpm(g_wheel_state)));
            return true;

        case 0x2C:  // Protection enable mask
            write_u32_le(value, g_wheel_state->protection_enable);
            return true;

        case 0x30:  // LCL status (bit 0 = tripped)
            write_u32_le(value, g_wheel_state->lcl_tripped ? 1 : 0);
            return true;

        default:
            // Unknown ICD address
            return false;
    }
}

/**
 * @brief Map ICD 8-bit address to internal register and write
 *
 * @param icd_addr ICD 8-bit address (0x00-0x30)
 * @param value 32-bit value to write
 * @return true if successful
 */
static bool write_register_icd(uint8_t icd_addr, uint32_t value) {
    // Use blocking version with retry for reliable delivery to Core1
    switch (icd_addr) {
        case 0x08:  // Control mode - send to Core1
            {
                uint8_t mode_index = icd_mode_to_index((uint8_t)value);
                if (mode_index == 0xFF && value != ICD_MODE_IDLE) {
                    return false;  // Invalid mode
                }
                return core_sync_send_command_blocking(CMD_SET_MODE, (float)mode_index, 0.0f, 0);
            }

        case 0x0C:  // Speed setpoint - send to Core1
            {
                float speed_rpm = uq14_18_to_float(value);
                return core_sync_send_command_blocking(CMD_SET_SPEED, speed_rpm, 0.0f, 0);
            }

        case 0x10:  // Current setpoint - send to Core1 (convert mA to A)
            {
                float current_a = uq14_18_to_float(value) / 1000.0f;
                return core_sync_send_command_blocking(CMD_SET_CURRENT, current_a, 0.0f, 0);
            }

        case 0x14:  // Torque setpoint - send to Core1
            {
                float torque_mnm = q10_22_to_float(value);
                return core_sync_send_command_blocking(CMD_SET_TORQUE, torque_mnm, 0.0f, 0);
            }

        case 0x18:  // PWM duty cycle - send to Core1
            {
                int32_t signed_val = (int32_t)value;
                float duty_pct = (float)(abs(signed_val) & 0x1FF) / 5.12f;
                return core_sync_send_command_blocking(CMD_SET_PWM, duty_pct, 0.0f, 0);
            }

        case 0x2C:  // Protection enable mask (direct write, no Core1 sync needed)
            g_wheel_state->protection_enable = value & 0x1F;
            return true;

        // Read-only registers
        case 0x00:  // Device ID
        case 0x04:  // Firmware version
        case 0x1C:  // Fault status
        case 0x20:  // Warning status
        case 0x24:  // Current measurement
        case 0x28:  // Speed measurement
        case 0x30:  // LCL status
            return false;  // Read-only

        default:
            return false;  // Unknown address
    }
}

/**
 * @brief Write register value to wheel state (internal 16-bit addresses)
 *
 * @param addr Register address
 * @param value Pointer to value (4 bytes, little-endian)
 * @return true if successful
 */
static bool write_register(uint16_t addr, const uint8_t* value) {
    // Use unaligned-safe read function
    uint32_t val32 = read_u32_le(value);
    printf("[DEBUG] write_register: addr=0x%04X, val32=0x%08X\n", addr, val32);

    // CRITICAL: Send commands to Core1 via command mailbox instead of
    // modifying Core0's wheel_state copy. Core1 is the source of truth!
    // Use blocking version with retry for reliable delivery
    switch (addr) {
        case REG_CONTROL_MODE:
            printf("[DEBUG] REG_CONTROL_MODE: sending CMD_SET_MODE to Core1\n");
            if (val32 <= CONTROL_MODE_PWM) {
                bool sent = core_sync_send_command_blocking(CMD_SET_MODE, (float)val32, 0.0f, 0);
                printf("[DEBUG] CMD_SET_MODE sent=%d\n", sent);
                return sent;
            }
            printf("[DEBUG] Invalid mode value %u\n", val32);
            return false;

        case REG_SPEED_SETPOINT_RPM:
            printf("[DEBUG] REG_SPEED_SETPOINT_RPM: sending CMD_SET_SPEED to Core1\n");
            return core_sync_send_command_blocking(CMD_SET_SPEED, uq14_18_to_float(val32), 0.0f, 0);

        case REG_CURRENT_SETPOINT_MA:
            printf("[DEBUG] REG_CURRENT_SETPOINT_MA: sending CMD_SET_CURRENT to Core1\n");
            return core_sync_send_command_blocking(CMD_SET_CURRENT, uq18_14_to_float(val32) / 1000.0f, 0.0f, 0);

        case REG_TORQUE_SETPOINT_MNM:
            printf("[DEBUG] REG_TORQUE_SETPOINT_MNM: sending CMD_SET_TORQUE to Core1\n");
            return core_sync_send_command_blocking(CMD_SET_TORQUE, uq18_14_to_float(val32), 0.0f, 0);

        case REG_PWM_DUTY_CYCLE:
            printf("[DEBUG] REG_PWM_DUTY_CYCLE: sending CMD_SET_PWM to Core1\n");
            return core_sync_send_command_blocking(CMD_SET_PWM, uq16_16_to_float(val32), 0.0f, 0);

        case REG_DIRECTION:
            printf("[DEBUG] REG_DIRECTION: value %u (not sent as separate command)\n", val32);
            // Direction is encoded in sign of setpoints, not a separate command
            // Update local state for display only
            if (val32 <= DIRECTION_NEGATIVE) {
                wheel_model_set_direction(g_wheel_state, (direction_t)val32);
                return true;
            }
            return false;

        default:
            // Unknown or read-only register
            return false;
    }
}

// ============================================================================
// Command Handler API
// ============================================================================

void commands_init(wheel_state_t* state) {
    g_wheel_state = state;
    printf("[COMMANDS] Initialized with wheel state at %p\n", (void*)state);
}

bool commands_dispatch(uint8_t command, const uint8_t* payload, uint16_t payload_len, cmd_result_t* result) {
    if (g_wheel_state == NULL) {
        printf("[COMMANDS] ERROR: Not initialized\n");
        build_nack(result);
        return false;
    }

    switch (command) {
        case NSP_CMD_PING:
            cmd_ping(payload, payload_len, result);
            return true;

        case NSP_CMD_PEEK:
            cmd_peek(payload, payload_len, result);
            return true;

        case NSP_CMD_POKE:
            cmd_poke(payload, payload_len, result);
            return true;

        case NSP_CMD_APPLICATION_TELEMETRY:
            cmd_application_telemetry(payload, payload_len, result);
            return true;

        case NSP_CMD_APPLICATION_COMMAND:
            cmd_application_command(payload, payload_len, result);
            return true;

        case NSP_CMD_CLEAR_FAULT:
            cmd_clear_fault(payload, payload_len, result);
            return true;

        case NSP_CMD_CONFIGURE_PROTECTION:
            cmd_configure_protection(payload, payload_len, result);
            return true;

        case NSP_CMD_TRIP_LCL:
            cmd_trip_lcl(payload, payload_len, result);
            return true;

        default:
            printf("[COMMANDS] Unknown command: 0x%02X\n", command);
            build_nack(result);
            return false;
    }
}

// ============================================================================
// Command Handlers
// ============================================================================

/**
 * @brief PING command handler - ICD compliant
 *
 * ICD Table 12-3: PING reply contains 5 bytes:
 *   Byte 0: Device Type (NRWA-T6 = 0x06)
 *   Byte 1: Device Serial Number
 *   Byte 2: Firmware Major Version
 *   Byte 3: Firmware Minor Version
 *   Byte 4: Firmware Patch Version
 */
void cmd_ping(const uint8_t* payload, uint16_t payload_len, cmd_result_t* result) {
    (void)payload;
    (void)payload_len;

    // ICD: PING returns 5-byte device info
    uint8_t ping_data[5];
    ping_data[0] = 0x06;  // Device Type: NRWA-T6
    ping_data[1] = 0x01;  // Serial Number (device-specific)
    ping_data[2] = 0x01;  // Firmware Major (v1.x.x)
    ping_data[3] = 0x00;  // Firmware Minor (v1.0.x)
    ping_data[4] = 0x00;  // Firmware Patch (v1.0.0)

    if (debug_commands) printf("[CMD] PING: returning device info\n");
    build_ack_with_data(result, ping_data, 5);
}

/**
 * @brief PEEK command handler - ICD compliant
 *
 * ICD Tables 12-5/12-6: PEEK format
 *   Request: [addr:1] - single 8-bit RAM address (0x00-0x30)
 *   Reply: [data:4] - 32-bit value (little-endian)
 */
void cmd_peek(const uint8_t* payload, uint16_t payload_len, cmd_result_t* result) {
    // ICD: Single byte address
    if (payload_len != 1) {
        if (debug_commands) printf("[CMD] PEEK: Invalid payload length %u (expected 1)\n", payload_len);
        build_nack(result);
        return;
    }

    uint8_t icd_addr = payload[0];  // 8-bit ICD address

    if (debug_commands) printf("[CMD] PEEK: icd_addr=0x%02X\n", icd_addr);

    // Validate ICD address range (0x00-0x30 per ICD Table 11-1)
    if (icd_addr > 0x30) {
        if (debug_commands) printf("[CMD] PEEK: Address 0x%02X out of range\n", icd_addr);
        build_nack(result);
        return;
    }

    // Read single 4-byte register via ICD address translation
    uint8_t data[4];
    if (!read_register_icd(icd_addr, data)) {
        if (debug_commands) printf("[CMD] PEEK: Failed to read ICD register 0x%02X\n", icd_addr);
        build_nack(result);
        return;
    }

    // ICD: Return single 4-byte value (little-endian)
    build_ack_with_data(result, data, 4);
    if (debug_commands) printf("[CMD] PEEK: Success, 4 bytes\n");
}

/**
 * @brief POKE command handler - ICD compliant
 *
 * ICD Table 12-9: POKE format
 *   Request: [addr:1][data:4] - 8-bit address + 32-bit value (LE)
 *   Reply: ACK on success, NACK on failure
 */
void cmd_poke(const uint8_t* payload, uint16_t payload_len, cmd_result_t* result) {
    // ICD: [addr:1][data:4] = 5 bytes total
    if (payload_len != 5) {
        if (debug_commands) printf("[CMD] POKE: Invalid payload length %u (expected 5)\n", payload_len);
        build_nack(result);
        return;
    }

    uint8_t icd_addr = payload[0];  // 8-bit ICD address
    uint32_t value = read_u32_le(&payload[1]);  // Little-endian 32-bit value

    if (debug_commands) printf("[CMD] POKE: icd_addr=0x%02X, value=0x%08X\n", icd_addr, value);

    // Validate ICD address range (0x00-0x30 per ICD Table 11-1)
    if (icd_addr > 0x30) {
        if (debug_commands) printf("[CMD] POKE: Address 0x%02X out of range\n", icd_addr);
        build_nack(result);
        return;
    }

    // Write via ICD address translation (sends to Core1 for state-changing regs)
    if (!write_register_icd(icd_addr, value)) {
        if (debug_commands) printf("[CMD] POKE: Failed to write ICD register 0x%02X\n", icd_addr);
        build_nack(result);
        return;
    }

    build_ack(result);
    if (debug_commands) printf("[CMD] POKE: Success\n");
}

void cmd_application_telemetry(const uint8_t* payload, uint16_t payload_len, cmd_result_t* result) {
    if (payload_len != 1) {
        if (debug_commands) printf("[CMD] APP-TELEM: Invalid payload length %u (expected 1)\n", payload_len);
        build_nack(result);
        return;
    }

    uint8_t block_id = payload[0];
    if (debug_commands) printf("[CMD] APP-TELEM: block_id=%u\n", block_id);

    // Read latest telemetry snapshot from Core1 (thread-safe)
    telemetry_snapshot_t snapshot;
    if (!core_sync_read_telemetry(&snapshot)) {
        // No telemetry available yet (Core1 not started?)
        if (debug_commands) printf("[CMD] APP-TELEM: No telemetry available\n");
        build_nack(result);
        return;
    }

    // Build temporary wheel_state from snapshot for telemetry builder
    // This avoids race condition by using atomic snapshot
    wheel_state_t temp_state = {0};
    temp_state.omega_rad_s = snapshot.omega_rad_s;
    temp_state.momentum_nms = snapshot.momentum_nms;
    temp_state.current_out_a = snapshot.current_a;
    temp_state.torque_out_mnm = snapshot.torque_mnm;
    temp_state.power_w = snapshot.power_w;
    temp_state.voltage_v = snapshot.voltage_v;
    temp_state.mode = snapshot.mode;
    temp_state.direction = snapshot.direction;
    temp_state.fault_status = snapshot.fault_status;
    temp_state.fault_latch = snapshot.fault_latch;
    temp_state.warning_status = snapshot.warning_status;
    temp_state.lcl_tripped = snapshot.lcl_tripped;
    temp_state.tick_count = snapshot.tick_count;

    // Build telemetry block from snapshot
    uint16_t block_len = telemetry_build_block(block_id, &temp_state, response_buffer, sizeof(response_buffer));

    if (block_len == 0) {
        if (debug_commands) printf("[CMD] APP-TELEM: Invalid block ID or error\n");
        build_nack(result);
        return;
    }

    build_ack_with_data(result, response_buffer, block_len);
    if (debug_commands) printf("[CMD] APP-TELEM: Success, %u bytes\n", block_len);
}

/**
 * @brief APPLICATION-COMMAND handler - ICD compliant
 *
 * ICD Table 12-23: APPLICATION-COMMAND format
 *   Request: [mode:1][setpoint:4] = 5 bytes total
 *     - mode byte: bits 0-2 = control mode (ICD bitmask), bit 7 = debug enable
 *     - setpoint: 32-bit value (little-endian), format depends on mode
 *
 * ICD Table 12-25: Control mode identifiers
 *   0b0001 (0x01): CURRENT control - setpoint is Q14.18 mA
 *   0b0010 (0x02): SPEED control - setpoint is Q14.18 RPM
 *   0b0100 (0x04): TORQUE control - setpoint is Q10.22 mN-m
 *   0b1000 (0x08): PWM control - setpoint is signed 32-bit (LSB 9 bits = duty, sign = dir)
 *   0b0000 (0x00): IDLE - High-Z, no active control
 */
void cmd_application_command(const uint8_t* payload, uint16_t payload_len, cmd_result_t* result) {
    // ICD: [mode:1][setpoint:4] = 5 bytes
    if (payload_len != 5) {
        if (debug_commands) printf("[CMD] APP-CMD: Invalid payload length %u (expected 5)\n", payload_len);
        build_nack(result);
        return;
    }

    uint8_t mode_byte = payload[0];  // ICD mode bitmask + debug flag
    uint32_t setpoint = read_u32_le(&payload[1]);  // Little-endian setpoint

    // Extract mode bits (bits 0-3 per ICD Table 12-25)
    uint8_t mode_bits = mode_byte & 0x0F;
    uint8_t mode_index = icd_mode_to_index(mode_bits);

    if (debug_commands) printf("[CMD] APP-CMD: mode_byte=0x%02X, mode_bits=0x%02X, setpoint=0x%08X\n",
                               mode_byte, mode_bits, setpoint);

    // Handle IDLE mode (0b0000) - no mode change, just acknowledge
    if (mode_bits == ICD_MODE_IDLE) {
        if (debug_commands) printf("[CMD] APP-CMD: IDLE mode - no action\n");
        build_ack(result);
        return;
    }

    // Validate mode
    if (mode_index == 0xFF) {
        if (debug_commands) printf("[CMD] APP-CMD: Invalid mode bits 0x%02X\n", mode_bits);
        build_nack(result);
        return;
    }

    // Convert setpoint based on mode
    // We send both mode and setpoint in a single command to avoid race conditions
    // (the command mailbox can only hold one command at a time)
    float setpoint_converted = 0.0f;

    switch (mode_bits) {
        case ICD_MODE_CURRENT:
            // ICD: Q14.18 current in mA -> convert to A for internal model
            {
                float current_ma = uq14_18_to_float(setpoint);
                setpoint_converted = current_ma / 1000.0f;  // Convert mA to A
                if (debug_commands) printf("[CMD] APP-CMD: CURRENT mode, setpoint=%.3f A\n", setpoint_converted);
            }
            break;

        case ICD_MODE_SPEED:
            // ICD: Q14.18 speed in RPM
            {
                setpoint_converted = uq14_18_to_float(setpoint);
                if (debug_commands) printf("[CMD] APP-CMD: SPEED mode, setpoint=%.1f RPM\n", setpoint_converted);
            }
            break;

        case ICD_MODE_TORQUE:
            // ICD: Q10.22 torque in mN-m
            {
                setpoint_converted = q10_22_to_float(setpoint);
                if (debug_commands) printf("[CMD] APP-CMD: TORQUE mode, setpoint=%.2f mN-m\n", setpoint_converted);
            }
            break;

        case ICD_MODE_PWM:
            // ICD: 32-bit signed, LSB 9 bits = duty cycle, sign = direction
            {
                int32_t signed_setpoint = (int32_t)setpoint;
                setpoint_converted = (float)(abs(signed_setpoint) & 0x1FF) / 5.12f;  // 9-bit duty to 0-100%
                direction_t dir = (signed_setpoint < 0) ? DIRECTION_NEGATIVE : DIRECTION_POSITIVE;

                // TODO: Direction is encoded in setpoint sign, need to send to Core1
                // For now, store locally (not ideal but matches existing behavior)
                wheel_model_set_direction(g_wheel_state, dir);

                if (debug_commands) printf("[CMD] APP-CMD: PWM mode, duty=%.2f%%, dir=%d\n", setpoint_converted, dir);
            }
            break;

        default:
            break;
    }

    // Send mode + setpoint to Core1 via command mailbox as single atomic command
    // param1 = mode_index, param2 = setpoint value (converted to internal units)
    // Use blocking version with retry to ensure delivery (50ms timeout = 5 physics ticks)
    if (!core_sync_send_command_blocking(CMD_SET_MODE, (float)mode_index, setpoint_converted, 0)) {
        printf("[CMD] APP-CMD: Failed to send to Core1 (timeout after retries)\n");
        build_nack(result);
        return;
    }

    build_ack(result);
}

/**
 * @brief CLEAR-FAULT command handler - ICD compliant
 *
 * ICD: Clears latched faults specified by bitmask.
 *   Request: [fault_mask:4] - 32-bit mask (little-endian)
 *   Reply: ACK
 *
 * Note: Does NOT clear LCL trip - requires hardware RESET.
 */
void cmd_clear_fault(const uint8_t* payload, uint16_t payload_len, cmd_result_t* result) {
    if (payload_len != 4) {
        if (debug_commands) printf("[CMD] CLEAR-FAULT: Invalid payload length %u (expected 4)\n", payload_len);
        build_nack(result);
        return;
    }

    // ICD: Little-endian fault mask
    uint32_t fault_mask = read_u32_le(payload);
    if (debug_commands) printf("[CMD] CLEAR-FAULT: mask=0x%08X\n", fault_mask);

    // Send CLEAR_FAULT command to Core1 via command mailbox
    // Pack mask into float for transport (Core1 will unpack)
    // Use blocking version with retry for reliable delivery
    float mask_as_float;
    memcpy(&mask_as_float, &fault_mask, sizeof(float));
    if (!core_sync_send_command_blocking(CMD_CLEAR_FAULT, mask_as_float, 0.0f, 0)) {
        if (debug_commands) printf("[CMD] CLEAR-FAULT: Failed to send to Core1 (timeout after retries)\n");
        build_nack(result);
        return;
    }

    // Check if LCL is still tripped (from local snapshot)
    if (wheel_model_is_lcl_tripped(g_wheel_state)) {
        if (debug_commands) printf("[CMD] CLEAR-FAULT: LCL still tripped (requires hardware RESET)\n");
    }

    build_ack(result);
}

/**
 * @brief CONFIGURE-PROTECTION command handler - ICD compliant
 *
 * ICD Table 12-29: CONFIGURE-PROTECTION format
 *   Request: [disable_mask:4] - 32-bit bitmask (little-endian)
 *     Bit 0: Overspeed-fault disable (1=disabled)
 *     Bit 1: Overspeed-limit disable (1=disabled)
 *     Bit 2: Overcurrent-limit disable (1=disabled)
 *     Bit 3: EDAC scrub disable (1=disabled)
 *     Bit 4: Braking overvoltage-load disable (1=disabled)
 *   Reply: ACK
 *
 * Note: ICD uses "disable" bits (1=off), we internally use "enable" bits (1=on).
 */
void cmd_configure_protection(const uint8_t* payload, uint16_t payload_len, cmd_result_t* result) {
    // ICD: [disable_mask:4] = 4 bytes
    if (payload_len != 4) {
        if (debug_commands) printf("[CMD] CONFIG-PROT: Invalid payload length %u (expected 4)\n", payload_len);
        build_nack(result);
        return;
    }

    // ICD: Little-endian disable bitmask
    uint32_t disable_mask = read_u32_le(payload);
    if (debug_commands) printf("[CMD] CONFIG-PROT: disable_mask=0x%08X\n", disable_mask);

    // Invert to get enable mask (ICD: bit=1 disables, internal: bit=1 enables)
    // Only 5 bits are defined per ICD
    uint32_t enable_mask = ~disable_mask & 0x1F;

    // Send protection config to Core1 via command mailbox
    // Pack enable_mask into float for transport (Core1 will unpack)
    // Use blocking version with retry for reliable delivery
    float enable_mask_as_float;
    memcpy(&enable_mask_as_float, &enable_mask, sizeof(float));
    if (!core_sync_send_command_blocking(CMD_CONFIG_PROTECTION, enable_mask_as_float, 0.0f, 0)) {
        if (debug_commands) printf("[CMD] CONFIG-PROT: Failed to send to Core1 (timeout after retries)\n");
        build_nack(result);
        return;
    }

    if (debug_commands) printf("[CMD] CONFIG-PROT: enable_mask=0x%08X sent to Core1 (inverted from disable)\n", enable_mask);

    build_ack(result);
}

/**
 * @brief TRIP-LCL command handler - ICD compliant
 *
 * ICD Section 12.9.2: Test command to simulate LCL trip.
 *   Request: No payload
 *   Reply: NO REPLY if successfully triggered (LCL cuts power)
 *
 * Per ICD: "If successfully executed, no reply is sent because the
 * LCL has tripped and power rails are disabled."
 */
void cmd_trip_lcl(const uint8_t* payload, uint16_t payload_len, cmd_result_t* result) {
    (void)payload;
    (void)payload_len;

    if (debug_commands) printf("[CMD] TRIP-LCL: Triggering LCL (no reply will be sent)\n");

    // Send TRIP-LCL command to Core1 physics model via command mailbox
    // The Core1 physics loop will execute wheel_model_trip_lcl() on the authoritative state
    // Use blocking version with retry for reliable delivery
    if (!core_sync_send_command_blocking(CMD_TRIP_LCL, 0.0f, 0.0f, 0)) {
        // Timeout after retries - extremely unlikely, but handle gracefully
        if (debug_commands) printf("[CMD] TRIP-LCL: Failed to send to Core1 (timeout after retries)\n");
        // Still suppress reply per ICD (LCL trip is a one-way command)
    }

    // ICD: NO reply if LCL trips successfully
    // The LCL trip cuts power, so no response can be sent
    result->status = CMD_NO_REPLY;
    result->data = NULL;
    result->data_len = 0;
}
