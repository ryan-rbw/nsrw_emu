/**
 * @file nss_nrwa_t6_telemetry.c
 * @brief NSS NRWA-T6 Telemetry Block Builder Implementation
 */

#include "nss_nrwa_t6_telemetry.h"
#include "fixedpoint.h"
#include <string.h>
#include <stdio.h>
#include <stdbool.h>

// Debug flag (set to false to disable verbose telemetry logging)
static bool debug_telemetry = false;

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Write uint32_t to buffer (big-endian)
 */
static void write_u32(uint8_t** buf, uint32_t value) {
    (*buf)[0] = (value >> 24) & 0xFF;
    (*buf)[1] = (value >> 16) & 0xFF;
    (*buf)[2] = (value >> 8) & 0xFF;
    (*buf)[3] = value & 0xFF;
    *buf += 4;
}

/**
 * @brief Write uint16_t to buffer (big-endian)
 */
static void write_u16(uint8_t** buf, uint16_t value) {
    (*buf)[0] = (value >> 8) & 0xFF;
    (*buf)[1] = value & 0xFF;
    *buf += 2;
}

/**
 * @brief Write uint8_t to buffer
 */
static void write_u8(uint8_t** buf, uint8_t value) {
    **buf = value;
    (*buf)++;
}

// ============================================================================
// Telemetry API
// ============================================================================

uint16_t telemetry_build_block(uint8_t block_id, const wheel_state_t* state, uint8_t* buffer, uint16_t buffer_size) {
    switch (block_id) {
        case TELEM_BLOCK_STANDARD:
            return telemetry_build_standard(state, buffer, buffer_size);

        case TELEM_BLOCK_TEMPERATURES:
            return telemetry_build_temperatures(state, buffer, buffer_size);

        case TELEM_BLOCK_VOLTAGES:
            return telemetry_build_voltages(state, buffer, buffer_size);

        case TELEM_BLOCK_CURRENTS:
            return telemetry_build_currents(state, buffer, buffer_size);

        case TELEM_BLOCK_DIAGNOSTICS:
            return telemetry_build_diagnostics(state, buffer, buffer_size);

        default:
            if (debug_telemetry) printf("[TELEM] Invalid block ID: %u\n", block_id);
            return 0;
    }
}

// ============================================================================
// Block Builders
// ============================================================================

uint16_t telemetry_build_standard(const wheel_state_t* state, uint8_t* buffer, uint16_t buffer_size) {
    const uint16_t required_size = 38;  // Total block size

    if (buffer_size < required_size) {
        if (debug_telemetry) printf("[TELEM] STANDARD: Buffer too small (%u < %u)\n", buffer_size, required_size);
        return 0;
    }

    uint8_t* buf = buffer;

    // Status register (operational state)
    // For Phase 6: 0x00000001 = Operational
    uint32_t status = 0x00000001;
    if (wheel_model_is_lcl_tripped(state)) {
        status |= 0x80000000;  // LCL trip bit
    }
    write_u32(&buf, status);

    // Fault status (active faults)
    write_u32(&buf, state->fault_status);

    // Fault latch (latched faults)
    write_u32(&buf, state->fault_latch);

    // Warning status
    write_u32(&buf, state->warning_status);

    // Control mode
    write_u8(&buf, (uint8_t)state->mode);

    // Direction
    write_u8(&buf, (uint8_t)state->direction);

    // Speed (RPM, UQ14.18)
    float speed_rpm = wheel_model_get_speed_rpm(state);
    write_u32(&buf, float_to_uq14_18(speed_rpm));

    // Current (mA, UQ18.14)
    float current_ma = state->current_out_a * 1000.0f;
    write_u32(&buf, float_to_uq18_14(current_ma));

    // Torque (mN·m, UQ18.14)
    write_u32(&buf, float_to_uq18_14(state->torque_out_mnm));

    // Power (mW, UQ18.14)
    float power_mw = state->power_w * 1000.0f;
    write_u32(&buf, float_to_uq18_14(power_mw));

    // Momentum (µN·m·s, UQ18.14)
    float momentum_unms = state->momentum_nms * 1e6f;
    write_u32(&buf, float_to_uq18_14(momentum_unms));

    if (debug_telemetry) printf("[TELEM] STANDARD: %u bytes (speed=%.1f RPM, current=%.3f A, power=%.1f W)\n",
           required_size, speed_rpm, state->current_out_a, state->power_w);

    return required_size;
}

uint16_t telemetry_build_temperatures(const wheel_state_t* state, uint8_t* buffer, uint16_t buffer_size) {
    (void)state;  // Not used yet (simulated temps)

    const uint16_t required_size = 6;

    if (buffer_size < required_size) {
        if (debug_telemetry) printf("[TELEM] TEMPERATURES: Buffer too small\n");
        return 0;
    }

    uint8_t* buf = buffer;

    // Simulated temperatures (fixed at 25°C for Phase 6)
    float motor_temp = 25.0f;
    float driver_temp = 25.0f;
    float board_temp = 25.0f;

    write_u16(&buf, float_to_uq8_8(motor_temp));
    write_u16(&buf, float_to_uq8_8(driver_temp));
    write_u16(&buf, float_to_uq8_8(board_temp));

    if (debug_telemetry) printf("[TELEM] TEMPERATURES: %u bytes (motor=%.1f°C, driver=%.1f°C, board=%.1f°C)\n",
           required_size, motor_temp, driver_temp, board_temp);

    return required_size;
}

uint16_t telemetry_build_voltages(const wheel_state_t* state, uint8_t* buffer, uint16_t buffer_size) {
    const uint16_t required_size = 12;

    if (buffer_size < required_size) {
        if (debug_telemetry) printf("[TELEM] VOLTAGES: Buffer too small\n");
        return 0;
    }

    uint8_t* buf = buffer;

    // Bus voltage (simulated at 28V)
    float bus_voltage = state->voltage_v;

    // Phase voltages (derived from bus voltage for Phase 6)
    // In real motor driver, these would be PWM-modulated phase voltages
    float phase_a_voltage = bus_voltage / 2.0f;  // Simplified
    float phase_b_voltage = bus_voltage / 2.0f;

    write_u32(&buf, float_to_uq16_16(bus_voltage));
    write_u32(&buf, float_to_uq16_16(phase_a_voltage));
    write_u32(&buf, float_to_uq16_16(phase_b_voltage));

    if (debug_telemetry) printf("[TELEM] VOLTAGES: %u bytes (bus=%.1fV, phase_a=%.1fV, phase_b=%.1fV)\n",
           required_size, bus_voltage, phase_a_voltage, phase_b_voltage);

    return required_size;
}

uint16_t telemetry_build_currents(const wheel_state_t* state, uint8_t* buffer, uint16_t buffer_size) {
    const uint16_t required_size = 12;

    if (buffer_size < required_size) {
        if (debug_telemetry) printf("[TELEM] CURRENTS: Buffer too small\n");
        return 0;
    }

    uint8_t* buf = buffer;

    // Phase currents (derived from motor current for Phase 6)
    // In real motor driver, these would be measured phase currents
    float motor_current_ma = state->current_out_a * 1000.0f;
    float phase_a_current_ma = motor_current_ma / 1.414f;  // Simplified (RMS)
    float phase_b_current_ma = motor_current_ma / 1.414f;
    float bus_current_ma = motor_current_ma;

    write_u32(&buf, float_to_uq18_14(phase_a_current_ma));
    write_u32(&buf, float_to_uq18_14(phase_b_current_ma));
    write_u32(&buf, float_to_uq18_14(bus_current_ma));

    if (debug_telemetry) printf("[TELEM] CURRENTS: %u bytes (phase_a=%.1fmA, phase_b=%.1fmA, bus=%.1fmA)\n",
           required_size, phase_a_current_ma, phase_b_current_ma, bus_current_ma);

    return required_size;
}

uint16_t telemetry_build_diagnostics(const wheel_state_t* state, uint8_t* buffer, uint16_t buffer_size) {
    const uint16_t required_size = 18;

    if (buffer_size < required_size) {
        if (debug_telemetry) printf("[TELEM] DIAGNOSTICS: Buffer too small\n");
        return 0;
    }

    uint8_t* buf = buffer;

    // Tick count
    write_u32(&buf, state->tick_count);

    // Uptime in seconds
    write_u32(&buf, state->uptime_seconds);

    // Fault count (placeholder - not tracked yet)
    write_u32(&buf, 0);

    // Command count (placeholder - not tracked yet)
    write_u32(&buf, 0);

    // Max jitter (placeholder - from timebase in Phase 10)
    write_u16(&buf, 0);

    if (debug_telemetry) printf("[TELEM] DIAGNOSTICS: %u bytes (ticks=%u, uptime=%us)\n",
           required_size, state->tick_count, state->uptime_seconds);

    return required_size;
}
