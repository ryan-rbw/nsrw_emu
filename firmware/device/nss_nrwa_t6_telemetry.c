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
 * @brief Write uint32_t to buffer (little-endian per ICD)
 *
 * ICD N2-A2a-DD0021: All multi-byte values are little-endian (LSB first)
 */
static void write_u32(uint8_t** buf, uint32_t value) {
    (*buf)[0] = value & 0xFF;          // LSB first
    (*buf)[1] = (value >> 8) & 0xFF;
    (*buf)[2] = (value >> 16) & 0xFF;
    (*buf)[3] = (value >> 24) & 0xFF;  // MSB last
    *buf += 4;
}

/**
 * @brief Write uint16_t to buffer (little-endian per ICD)
 */
static void write_u16(uint8_t** buf, uint16_t value) {
    (*buf)[0] = value & 0xFF;          // LSB first
    (*buf)[1] = (value >> 8) & 0xFF;   // MSB last
    *buf += 2;
}

/**
 * @brief Write int16_t to buffer (little-endian per ICD)
 */
static void write_s16(uint8_t** buf, int16_t value) {
    write_u16(buf, (uint16_t)value);
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

/**
 * @brief Build ICD-compliant STANDARD telemetry block (25 bytes)
 *
 * ICD Table 12-13 STANDARD Telemetry Block format:
 *   Bytes 0-3:   Status register (32-bit, LE)
 *   Bytes 4-7:   Fault register (32-bit, LE)
 *   Byte 8:      Control Mode register (8-bit, ICD bitmask)
 *   Bytes 9-12:  Control setpoint (32-bit, LE, mode-dependent Q-format)
 *   Bytes 13-14: PWM duty cycle (16-bit signed, LE)
 *   Bytes 15-16: Current target (Q14.2 mA, LE)
 *   Bytes 17-20: Current measurement (Q20.12 mA, LE)
 *   Bytes 21-24: Speed measurement (Q24.8 RPM, LE)
 */
uint16_t telemetry_build_standard(const wheel_state_t* state, uint8_t* buffer, uint16_t buffer_size) {
    const uint16_t required_size = 25;  // ICD: exactly 25 bytes

    if (buffer_size < required_size) {
        if (debug_telemetry) printf("[TELEM] STANDARD: Buffer too small (%u < %u)\n", buffer_size, required_size);
        return 0;
    }

    uint8_t* buf = buffer;

    // Bytes 0-3: Status register (32-bit, LE)
    // Bit 0: Operational (1 = running)
    // Bit 31: LCL trip flag
    uint32_t status = 0x00000001;  // Operational
    if (wheel_model_is_lcl_tripped(state)) {
        status |= 0x80000000;  // LCL trip bit
    }
    write_u32(&buf, status);

    // Bytes 4-7: Fault register (32-bit, LE) - combined fault_status and fault_latch
    write_u32(&buf, state->fault_status | state->fault_latch);

    // Byte 8: Control mode register (ICD bitmask encoding)
    write_u8(&buf, index_to_icd_mode(state->mode));

    // Bytes 9-12: Control setpoint (32-bit, LE, mode-dependent format)
    // Format depends on active mode per ICD Table 12-23
    uint32_t setpoint = 0;
    switch (state->mode) {
        case CONTROL_MODE_CURRENT:
            // ICD: Q14.18 current in mA (we store A internally)
            setpoint = float_to_uq14_18(state->current_cmd_a * 1000.0f);
            break;
        case CONTROL_MODE_SPEED:
            // ICD: Q14.18 speed in RPM
            setpoint = float_to_uq14_18(state->speed_cmd_rpm);
            break;
        case CONTROL_MODE_TORQUE:
            // ICD: Q10.22 torque in mN-m
            setpoint = float_to_q10_22(state->torque_cmd_mnm);
            break;
        case CONTROL_MODE_PWM:
            // ICD: 32-bit signed, LSB 9 bits = duty, sign = direction
            {
                int16_t duty_raw = (int16_t)(state->pwm_duty_pct * 5.12f);  // Scale 0-100% to 0-512
                if (state->direction == DIRECTION_NEGATIVE) {
                    duty_raw = -duty_raw;
                }
                setpoint = (uint32_t)(int32_t)duty_raw;
            }
            break;
    }
    write_u32(&buf, setpoint);

    // Bytes 13-14: PWM duty cycle (16-bit signed, LE)
    // Bit 15 = direction sign, bits 8:0 = duty cycle magnitude
    int16_t pwm_duty = (int16_t)(state->pwm_duty_pct * 5.12f);  // Scale to 9-bit range
    if (state->direction == DIRECTION_NEGATIVE) {
        pwm_duty = -pwm_duty;
    }
    write_s16(&buf, pwm_duty);

    // Bytes 15-16: Current target (Q14.2 mA, LE)
    float current_target_ma = state->current_cmd_a * 1000.0f;
    write_u16(&buf, float_to_q14_2(current_target_ma));

    // Bytes 17-20: Current measurement (Q20.12 mA, LE)
    float current_meas_ma = state->current_out_a * 1000.0f;
    write_u32(&buf, float_to_q20_12(current_meas_ma));

    // Bytes 21-24: Speed measurement (Q24.8 RPM, LE)
    float speed_rpm = wheel_model_get_speed_rpm(state);
    write_u32(&buf, float_to_q24_8(speed_rpm));

    if (debug_telemetry) printf("[TELEM] STANDARD: %u bytes (speed=%.1f RPM, current=%.3f A, mode=%u)\n",
           required_size, speed_rpm, state->current_out_a, state->mode);

    return required_size;
}

/**
 * @brief Build ICD-compliant TEMPERATURES telemetry block (8 bytes)
 *
 * ICD Table 12-16 TEMPERATURES Telemetry Block format:
 *   Bytes 0-1:   DC-DC Temperature (16-bit unsigned, °C)
 *   Bytes 2-3:   Enclosure Temperature (16-bit unsigned, °C)
 *   Bytes 4-5:   Motor Driver Temperature (16-bit unsigned, °C)
 *   Bytes 6-7:   Motor Temperature (16-bit unsigned, °C)
 */
uint16_t telemetry_build_temperatures(const wheel_state_t* state, uint8_t* buffer, uint16_t buffer_size) {
    (void)state;  // Not used yet (simulated temps)

    const uint16_t required_size = 8;  // ICD Table 12-11: exactly 8 bytes

    if (buffer_size < required_size) {
        if (debug_telemetry) printf("[TELEM] TEMPERATURES: Buffer too small\n");
        return 0;
    }

    uint8_t* buf = buffer;

    // Simulated temperatures (fixed at 25°C until thermal model implemented)
    // ICD: 16-bit unsigned integer °C (not fixed-point)
    uint16_t dcdc_temp = 25;           // DC-DC converter temperature
    uint16_t enclosure_temp = 25;      // Enclosure temperature
    uint16_t motor_driver_temp = 25;   // Motor driver temperature
    uint16_t motor_temp = 25;          // Motor temperature

    write_u16(&buf, dcdc_temp);
    write_u16(&buf, enclosure_temp);
    write_u16(&buf, motor_driver_temp);
    write_u16(&buf, motor_temp);

    if (debug_telemetry) printf("[TELEM] TEMPERATURES: %u bytes (dcdc=%u°C, encl=%u°C, driver=%u°C, motor=%u°C)\n",
           required_size, dcdc_temp, enclosure_temp, motor_driver_temp, motor_temp);

    return required_size;
}

/**
 * @brief Build ICD-compliant VOLTAGES telemetry block (24 bytes)
 *
 * ICD Table 12-17 VOLTAGES Telemetry Block format:
 *   Bytes 0-3:   Monitor 1V5 supply voltage (UQ16.16 V)
 *   Bytes 4-7:   Monitor 3V3 supply voltage (UQ16.16 V)
 *   Bytes 8-11:  Monitor Analog 5V supply voltage (UQ16.16 V)
 *   Bytes 12-15: Monitor 12V supply voltage (UQ16.16 V)
 *   Bytes 16-19: Monitor 30V supply voltage (UQ16.16 V)
 *   Bytes 20-23: Monitor 2V5 reference voltage (UQ16.16 V)
 */
uint16_t telemetry_build_voltages(const wheel_state_t* state, uint8_t* buffer, uint16_t buffer_size) {
    const uint16_t required_size = 24;  // ICD Table 12-11: exactly 24 bytes

    if (buffer_size < required_size) {
        if (debug_telemetry) printf("[TELEM] VOLTAGES: Buffer too small\n");
        return 0;
    }

    uint8_t* buf = buffer;

    // Internal supply rail voltages (simulated nominal values)
    // These rails are derived from the main bus voltage via DC-DC converters
    float voltage_1v5 = 1.5f;           // 1.5V digital supply
    float voltage_3v3 = 3.3f;           // 3.3V digital supply
    float voltage_5va = 5.0f;           // Analog 5V supply
    float voltage_12v = 12.0f;          // 12V motor driver supply
    float voltage_30v = state->voltage_v;  // 30V bus (from bus voltage, nominally 28V)
    float voltage_2v5_ref = 2.5f;       // 2.5V reference voltage

    write_u32(&buf, float_to_uq16_16(voltage_1v5));
    write_u32(&buf, float_to_uq16_16(voltage_3v3));
    write_u32(&buf, float_to_uq16_16(voltage_5va));
    write_u32(&buf, float_to_uq16_16(voltage_12v));
    write_u32(&buf, float_to_uq16_16(voltage_30v));
    write_u32(&buf, float_to_uq16_16(voltage_2v5_ref));

    if (debug_telemetry) printf("[TELEM] VOLTAGES: %u bytes (1V5=%.2f, 3V3=%.2f, 5VA=%.2f, 12V=%.2f, 30V=%.2f, 2V5ref=%.2f)\n",
           required_size, voltage_1v5, voltage_3v3, voltage_5va, voltage_12v, voltage_30v, voltage_2v5_ref);

    return required_size;
}

/**
 * @brief Build ICD-compliant CURRENTS telemetry block (24 bytes)
 *
 * ICD Table 12-18 CURRENTS Telemetry Block format:
 *   Bytes 0-3:   Monitor 1V5 supply current (UQ16.16 mA)
 *   Bytes 4-7:   Monitor 3V3 supply current (UQ16.16 mA)
 *   Bytes 8-11:  Monitor Analog 5V supply current (UQ16.16 mA)
 *   Bytes 12-15: Monitor Digital 5V supply current (UQ16.16 mA)
 *   Bytes 16-19: Monitor 12V supply current (UQ16.16 mA)
 *   Bytes 20-23: Monitor 30V supply current (Q16.16 A) - Note: Q16.16, not UQ16.16
 */
uint16_t telemetry_build_currents(const wheel_state_t* state, uint8_t* buffer, uint16_t buffer_size) {
    const uint16_t required_size = 24;  // ICD Table 12-11: exactly 24 bytes

    if (buffer_size < required_size) {
        if (debug_telemetry) printf("[TELEM] CURRENTS: Buffer too small\n");
        return 0;
    }

    uint8_t* buf = buffer;

    // Derive internal supply rail currents from motor power consumption
    // Motor current flows from 30V bus; internal rails have their own (smaller) loads
    float motor_current_a = state->current_out_a;

    // Internal supply rail currents (simulated based on typical digital/analog loads)
    // These are small quiescent currents for the control electronics
    float current_1v5_ma = 50.0f;    // 1.5V digital logic (~50 mA)
    float current_3v3_ma = 100.0f;   // 3.3V MCU/logic (~100 mA)
    float current_5va_ma = 20.0f;    // Analog 5V sensors (~20 mA)
    float current_5vd_ma = 30.0f;    // Digital 5V drivers (~30 mA)
    float current_12v_ma = 50.0f;    // 12V gate drivers (~50 mA)
    float current_30v_a = motor_current_a;  // 30V bus = motor current (in A, not mA!)

    write_u32(&buf, float_to_uq16_16(current_1v5_ma));
    write_u32(&buf, float_to_uq16_16(current_3v3_ma));
    write_u32(&buf, float_to_uq16_16(current_5va_ma));
    write_u32(&buf, float_to_uq16_16(current_5vd_ma));
    write_u32(&buf, float_to_uq16_16(current_12v_ma));
    write_u32(&buf, float_to_q16_16(current_30v_a));  // Q16.16 A (signed)

    if (debug_telemetry) printf("[TELEM] CURRENTS: %u bytes (1V5=%.0fmA, 3V3=%.0fmA, 5VA=%.0fmA, 5VD=%.0fmA, 12V=%.0fmA, 30V=%.3fA)\n",
           required_size, current_1v5_ma, current_3v3_ma, current_5va_ma, current_5vd_ma, current_12v_ma, current_30v_a);

    return required_size;
}

/**
 * @brief Build ICD-compliant DIAGNOSTICS-GENERAL telemetry block (20 bytes)
 *
 * ICD Table 12-19 DIAGNOSTICS-GENERAL Telemetry Block format:
 *   Bytes 0-3:   Uptime counter (UQ30.2 seconds)
 *   Bytes 4-7:   Revolution count (32-bit unsigned)
 *   Bytes 8-11:  Motor Hall sensor invalid transition count (32-bit unsigned)
 *   Bytes 12-15: Drive-Fault count (32-bit unsigned)
 *   Bytes 16-19: Drive-Overtemperature count (32-bit unsigned)
 */
uint16_t telemetry_build_diagnostics(const wheel_state_t* state, uint8_t* buffer, uint16_t buffer_size) {
    const uint16_t required_size = 20;  // ICD Table 12-11: exactly 20 bytes

    if (buffer_size < required_size) {
        if (debug_telemetry) printf("[TELEM] DIAGNOSTICS: Buffer too small\n");
        return 0;
    }

    uint8_t* buf = buffer;

    // Uptime counter in UQ30.2 format (30 integer bits, 2 fractional bits)
    // UQ30.2 has resolution of 0.25 seconds
    uint32_t uptime_uq30_2 = state->uptime_seconds << 2;  // Shift left by 2 for UQ30.2
    write_u32(&buf, uptime_uq30_2);

    // Revolution count (integrated from wheel speed)
    write_u32(&buf, state->revolution_count);

    // Motor Hall sensor invalid transition count
    write_u32(&buf, state->hall_invalid_count);

    // Drive-Fault count (number of drive faults triggered)
    write_u32(&buf, state->drive_fault_count);

    // Drive-Overtemperature count (number of overtemp events)
    write_u32(&buf, state->drive_overtemp_count);

    if (debug_telemetry) printf("[TELEM] DIAGNOSTICS: %u bytes (uptime=%us, revs=%u, hall_err=%u, drv_flt=%u, overtemp=%u)\n",
           required_size, state->uptime_seconds, state->revolution_count,
           state->hall_invalid_count, state->drive_fault_count, state->drive_overtemp_count);

    return required_size;
}
