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

/**
 * @brief Write register value to wheel state
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
    switch (addr) {
        case REG_CONTROL_MODE:
            printf("[DEBUG] REG_CONTROL_MODE: sending CMD_SET_MODE to Core1\n");
            if (val32 <= CONTROL_MODE_PWM) {
                bool sent = core_sync_send_command(CMD_SET_MODE, (float)val32, 0.0f);
                printf("[DEBUG] CMD_SET_MODE sent=%d\n", sent);
                return sent;
            }
            printf("[DEBUG] Invalid mode value %u\n", val32);
            return false;

        case REG_SPEED_SETPOINT_RPM:
            printf("[DEBUG] REG_SPEED_SETPOINT_RPM: sending CMD_SET_SPEED to Core1\n");
            return core_sync_send_command(CMD_SET_SPEED, uq14_18_to_float(val32), 0.0f);

        case REG_CURRENT_SETPOINT_MA:
            printf("[DEBUG] REG_CURRENT_SETPOINT_MA: sending CMD_SET_CURRENT to Core1\n");
            return core_sync_send_command(CMD_SET_CURRENT, uq18_14_to_float(val32) / 1000.0f, 0.0f);

        case REG_TORQUE_SETPOINT_MNM:
            printf("[DEBUG] REG_TORQUE_SETPOINT_MNM: sending CMD_SET_TORQUE to Core1\n");
            return core_sync_send_command(CMD_SET_TORQUE, uq18_14_to_float(val32), 0.0f);

        case REG_PWM_DUTY_CYCLE:
            printf("[DEBUG] REG_PWM_DUTY_CYCLE: sending CMD_SET_PWM to Core1\n");
            return core_sync_send_command(CMD_SET_PWM, uq16_16_to_float(val32), 0.0f);

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

void cmd_ping(const uint8_t* payload, uint16_t payload_len, cmd_result_t* result) {
    (void)payload;
    (void)payload_len;

    if (debug_commands) printf("[CMD] PING\n");
    build_ack(result);
}

void cmd_peek(const uint8_t* payload, uint16_t payload_len, cmd_result_t* result) {
    if (payload_len != 3) {
        if (debug_commands) printf("[CMD] PEEK: Invalid payload length %u (expected 3)\n", payload_len);
        build_nack(result);
        return;
    }

    uint16_t start_addr = (payload[0] << 8) | payload[1];
    uint8_t count = payload[2];

    if (debug_commands) printf("[CMD] PEEK: addr=0x%04X, count=%u\n", start_addr, count);

    // Validate access
    if (!validate_register_access(start_addr, count, false)) {
        if (debug_commands) printf("[CMD] PEEK: Invalid register access\n");
        build_nack(result);
        return;
    }

    // Read registers into response buffer (registers are 4-byte aligned)
    uint8_t* data = response_buffer;
    for (uint8_t i = 0; i < count; i++) {
        if (!read_register(start_addr + (i * 4), data)) {
            if (debug_commands) printf("[CMD] PEEK: Failed to read register 0x%04X\n", start_addr + (i * 4));
            build_nack(result);
            return;
        }
        data += 4;  // Each register is 4 bytes
    }

    // Build ACK with data
    build_ack_with_data(result, response_buffer, count * 4);
    if (debug_commands) printf("[CMD] PEEK: Success, %u bytes\n", count * 4);
}

void cmd_poke(const uint8_t* payload, uint16_t payload_len, cmd_result_t* result) {
    if (payload_len < 3) {
        if (debug_commands) printf("[CMD] POKE: Invalid payload length %u (expected >=3)\n", payload_len);
        build_nack(result);
        return;
    }

    uint16_t start_addr = (payload[0] << 8) | payload[1];
    uint8_t count = payload[2];

    if (debug_commands) printf("[CMD] POKE: addr=0x%04X, count=%u\n", start_addr, count);

    // Validate payload length
    if (payload_len != (3 + count * 4)) {
        if (debug_commands) printf("[CMD] POKE: Payload length mismatch\n");
        build_nack(result);
        return;
    }

    // Validate access
    if (!validate_register_access(start_addr, count, true)) {
        if (debug_commands) printf("[CMD] POKE: Invalid register access (read-only or out of range)\n");
        build_nack(result);
        return;
    }

    // Write registers (registers are 4-byte aligned)
    const uint8_t* data = &payload[3];
    for (uint8_t i = 0; i < count; i++) {
        if (!write_register(start_addr + (i * 4), data)) {
            if (debug_commands) printf("[CMD] POKE: Failed to write register 0x%04X\n", start_addr + (i * 4));
            build_nack(result);
            return;
        }
        data += 4;
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

    // Build telemetry block (delegated to telemetry module)
    uint16_t block_len = telemetry_build_block(block_id, g_wheel_state, response_buffer, sizeof(response_buffer));

    if (block_len == 0) {
        if (debug_commands) printf("[CMD] APP-TELEM: Invalid block ID or error\n");
        build_nack(result);
        return;
    }

    build_ack_with_data(result, response_buffer, block_len);
    if (debug_commands) printf("[CMD] APP-TELEM: Success, %u bytes\n", block_len);
}

void cmd_application_command(const uint8_t* payload, uint16_t payload_len, cmd_result_t* result) {
    if (payload_len < 1) {
        if (debug_commands) printf("[CMD] APP-CMD: Invalid payload length %u\n", payload_len);
        build_nack(result);
        return;
    }

    uint8_t subcmd = payload[0];
    if (debug_commands) printf("[CMD] APP-CMD: subcmd=0x%02X\n", subcmd);

    switch (subcmd) {
        case 0x00: // SET-MODE
            if (payload_len != 2) {
                build_nack(result);
                return;
            }
            if (payload[1] <= CONTROL_MODE_PWM) {
                wheel_model_set_mode(g_wheel_state, (control_mode_t)payload[1]);
                if (debug_commands) printf("[CMD] APP-CMD: Set mode=%u\n", payload[1]);
                build_ack(result);
            } else {
                if (debug_commands) printf("[CMD] APP-CMD: Invalid mode=%u\n", payload[1]);
                build_nack(result);
            }
            break;

        case 0x01: // SET-SPEED
            if (payload_len != 5) {
                build_nack(result);
                return;
            }
            {
                uint32_t speed_uq = (payload[1] << 24) | (payload[2] << 16) | (payload[3] << 8) | payload[4];
                float speed_rpm = uq14_18_to_float(speed_uq);
                wheel_model_set_speed(g_wheel_state, speed_rpm);
                if (debug_commands) printf("[CMD] APP-CMD: Set speed=%.1f RPM\n", speed_rpm);
                build_ack(result);
            }
            break;

        case 0x02: // SET-CURRENT
            if (payload_len != 5) {
                build_nack(result);
                return;
            }
            {
                uint32_t current_uq = (payload[1] << 24) | (payload[2] << 16) | (payload[3] << 8) | payload[4];
                float current_ma = uq18_14_to_float(current_uq);
                wheel_model_set_current(g_wheel_state, current_ma / 1000.0f);
                if (debug_commands) printf("[CMD] APP-CMD: Set current=%.3f A\n", current_ma / 1000.0f);
                build_ack(result);
            }
            break;

        case 0x03: // SET-TORQUE
            if (payload_len != 5) {
                build_nack(result);
                return;
            }
            {
                uint32_t torque_uq = (payload[1] << 24) | (payload[2] << 16) | (payload[3] << 8) | payload[4];
                float torque_mnm = uq18_14_to_float(torque_uq);
                wheel_model_set_torque(g_wheel_state, torque_mnm);
                if (debug_commands) printf("[CMD] APP-CMD: Set torque=%.1f mN·m\n", torque_mnm);
                build_ack(result);
            }
            break;

        case 0x04: // SET-PWM
            if (payload_len != 3) {
                build_nack(result);
                return;
            }
            {
                uint16_t duty_uq = (payload[1] << 8) | payload[2];
                float duty_pct = uq8_8_to_float(duty_uq);
                wheel_model_set_pwm(g_wheel_state, duty_pct);
                if (debug_commands) printf("[CMD] APP-CMD: Set PWM=%.2f%%\n", duty_pct);
                build_ack(result);
            }
            break;

        case 0x05: // SET-DIRECTION
            if (payload_len != 2) {
                build_nack(result);
                return;
            }
            if (payload[1] <= DIRECTION_NEGATIVE) {
                wheel_model_set_direction(g_wheel_state, (direction_t)payload[1]);
                if (debug_commands) printf("[CMD] APP-CMD: Set direction=%u\n", payload[1]);
                build_ack(result);
            } else {
                if (debug_commands) printf("[CMD] APP-CMD: Invalid direction=%u\n", payload[1]);
                build_nack(result);
            }
            break;

        default:
            if (debug_commands) printf("[CMD] APP-CMD: Unknown subcommand 0x%02X\n", subcmd);
            build_nack(result);
            break;
    }
}

void cmd_clear_fault(const uint8_t* payload, uint16_t payload_len, cmd_result_t* result) {
    if (payload_len != 4) {
        if (debug_commands) printf("[CMD] CLEAR-FAULT: Invalid payload length %u (expected 4)\n", payload_len);
        build_nack(result);
        return;
    }

    uint32_t fault_mask = (payload[0] << 24) | (payload[1] << 16) | (payload[2] << 8) | payload[3];
    if (debug_commands) printf("[CMD] CLEAR-FAULT: mask=0x%08X\n", fault_mask);

    // Clear faults (does NOT clear LCL trip)
    wheel_model_clear_faults(g_wheel_state, fault_mask);

    // Check if LCL is still tripped
    if (wheel_model_is_lcl_tripped(g_wheel_state)) {
        if (debug_commands) printf("[CMD] CLEAR-FAULT: LCL still tripped (requires hardware RESET)\n");
    }

    build_ack(result);
}

void cmd_configure_protection(const uint8_t* payload, uint16_t payload_len, cmd_result_t* result) {
    if (payload_len != 5) {
        if (debug_commands) printf("[CMD] CONFIG-PROT: Invalid payload length %u (expected 5)\n", payload_len);
        build_nack(result);
        return;
    }

    uint8_t param_id = payload[0];
    uint32_t value = (payload[1] << 24) | (payload[2] << 16) | (payload[3] << 8) | payload[4];
    if (debug_commands) printf("[CMD] CONFIG-PROT: param_id=%u, value=0x%08X\n", param_id, value);

    switch (param_id) {
        case 0x00: // Overvoltage threshold (V, UQ16.16)
            g_wheel_state->overvoltage_threshold_v = uq16_16_to_float(value);
            if (debug_commands) printf("[CMD] CONFIG-PROT: Overvoltage threshold = %.2f V\n", g_wheel_state->overvoltage_threshold_v);
            build_ack(result);
            break;

        case 0x01: // Overspeed fault (RPM, UQ14.18)
            g_wheel_state->overspeed_fault_rpm = uq14_18_to_float(value);
            if (debug_commands) printf("[CMD] CONFIG-PROT: Overspeed fault = %.1f RPM\n", g_wheel_state->overspeed_fault_rpm);
            build_ack(result);
            break;

        case 0x02: // Overspeed soft (RPM, UQ14.18)
            g_wheel_state->overspeed_soft_rpm = uq14_18_to_float(value);
            if (debug_commands) printf("[CMD] CONFIG-PROT: Overspeed soft = %.1f RPM\n", g_wheel_state->overspeed_soft_rpm);
            build_ack(result);
            break;

        case 0x03: // Max duty cycle (%, UQ8.8)
            g_wheel_state->max_duty_cycle_pct = uq8_8_to_float(value & 0xFFFF);
            if (debug_commands) printf("[CMD] CONFIG-PROT: Max duty = %.2f%%\n", g_wheel_state->max_duty_cycle_pct);
            build_ack(result);
            break;

        case 0x04: // Motor overpower (W, UQ16.16)
            g_wheel_state->motor_overpower_limit_w = uq16_16_to_float(value);
            if (debug_commands) printf("[CMD] CONFIG-PROT: Motor overpower = %.1f W\n", g_wheel_state->motor_overpower_limit_w);
            build_ack(result);
            break;

        case 0x05: // Soft overcurrent (A, UQ16.16)
            g_wheel_state->soft_overcurrent_a = uq16_16_to_float(value);
            if (debug_commands) printf("[CMD] CONFIG-PROT: Soft overcurrent = %.2f A\n", g_wheel_state->soft_overcurrent_a);
            build_ack(result);
            break;

        default:
            if (debug_commands) printf("[CMD] CONFIG-PROT: Invalid parameter ID %u\n", param_id);
            build_nack(result);
            break;
    }
}

void cmd_trip_lcl(const uint8_t* payload, uint16_t payload_len, cmd_result_t* result) {
    (void)payload;
    (void)payload_len;

    if (debug_commands) printf("[CMD] TRIP-LCL: Triggering LCL\n");
    wheel_model_trip_lcl(g_wheel_state);
    build_ack(result);
}
