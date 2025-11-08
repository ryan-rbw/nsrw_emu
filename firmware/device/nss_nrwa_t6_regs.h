/**
 * @file nss_nrwa_t6_regs.h
 * @brief NSS NRWA-T6 Reaction Wheel Register Map
 *
 * Memory-mapped registers for PEEK/POKE access via NSP protocol.
 * Organized into logical groups: Control, Status, Config, Protection.
 *
 * Address Space:
 * - 0x0000-0x00FF: Device Information (read-only)
 * - 0x0100-0x01FF: Protection Configuration (read/write)
 * - 0x0200-0x02FF: Control Registers (read/write)
 * - 0x0300-0x03FF: Status Registers (read-only)
 * - 0x0400-0x04FF: Fault & Diagnostic Registers (read/write)
 *
 * All multi-byte values are LITTLE-ENDIAN (LSB first).
 */

#ifndef NSS_NRWA_T6_REGS_H
#define NSS_NRWA_T6_REGS_H

#include <stdint.h>
#include "fixedpoint.h"

// ============================================================================
// Device Information Registers (0x0000-0x00FF) - READ ONLY
// ============================================================================

#define REG_DEVICE_ID               0x0000  // uint32_t: Device ID (always 0x4E525754 "NRWT")
#define REG_FIRMWARE_VERSION        0x0004  // uint32_t: Firmware version (BCD: 0x00010203 = v1.2.3)
#define REG_HARDWARE_REVISION       0x0008  // uint16_t: Hardware revision
#define REG_SERIAL_NUMBER           0x000A  // uint32_t: Serial number
#define REG_BUILD_TIMESTAMP         0x000E  // uint32_t: Unix timestamp of build
#define REG_INERTIA_KGM2            0x0012  // UQ16.16: Wheel inertia (kg·m²)
#define REG_MOTOR_KT_NMA            0x0016  // UQ16.16: Motor torque constant k_t (N·m/A)

// ============================================================================
// Protection Configuration Registers (0x0100-0x01FF) - READ/WRITE
// ============================================================================

// Hard Protections (fault and latch on violation)
#define REG_OVERVOLTAGE_THRESHOLD   0x0100  // UQ16.16: Overvoltage threshold (V), default: 36.0
#define REG_OVERSPEED_FAULT_RPM     0x0104  // UQ14.18: Overspeed fault threshold (RPM), default: 6000.0
#define REG_MAX_DUTY_CYCLE          0x0108  // UQ16.16: Max duty cycle (%), default: 97.85
#define REG_MOTOR_OVERPOWER_LIMIT   0x010C  // UQ18.14: Overpower limit (mW), default: 100000 (100W)

// Soft Protections (warning, but don't latch)
#define REG_BRAKING_LOAD_SETPOINT   0x0110  // UQ16.16: Braking load voltage (V), default: 31.0
#define REG_SOFT_OVERCURRENT_MA     0x0114  // UQ18.14: Soft overcurrent threshold (mA), default: 6000 (6A)
#define REG_SOFT_OVERSPEED_RPM      0x0118  // UQ14.18: Soft overspeed warning (RPM), default: 5000.0

// Protection Enable Flags
#define REG_PROTECTION_ENABLE       0x011C  // uint32_t: Bitmask of enabled protections
  #define PROT_ENABLE_OVERVOLTAGE     (1 << 0)
  #define PROT_ENABLE_OVERSPEED       (1 << 1)
  #define PROT_ENABLE_OVERDUTY        (1 << 2)
  #define PROT_ENABLE_OVERPOWER       (1 << 3)
  #define PROT_ENABLE_SOFT_OVERCURR   (1 << 4)
  #define PROT_ENABLE_SOFT_OVERSPEED  (1 << 5)
  #define PROT_ENABLE_ALL             (PROT_ENABLE_OVERVOLTAGE | PROT_ENABLE_OVERSPEED | \
                                       PROT_ENABLE_OVERDUTY | PROT_ENABLE_OVERPOWER | \
                                       PROT_ENABLE_SOFT_OVERCURR | PROT_ENABLE_SOFT_OVERSPEED)

// ============================================================================
// Control Registers (0x0200-0x02FF) - READ/WRITE
// ============================================================================

// Control Mode
#define REG_CONTROL_MODE            0x0200  // uint8_t: Control mode enum
  typedef enum {
      CONTROL_MODE_CURRENT = 0,  // Direct current control
      CONTROL_MODE_SPEED   = 1,  // Speed (RPM) control with PI controller
      CONTROL_MODE_TORQUE  = 2,  // Torque (momentum change) control
      CONTROL_MODE_PWM     = 3   // PWM duty cycle backup mode
  } control_mode_t;

// Setpoints
#define REG_SPEED_SETPOINT_RPM      0x0204  // UQ14.18: Speed setpoint (RPM)
#define REG_CURRENT_SETPOINT_MA     0x0208  // UQ18.14: Current setpoint (mA)
#define REG_TORQUE_SETPOINT_MNM     0x020C  // UQ18.14: Torque setpoint (mN·m)
#define REG_PWM_DUTY_CYCLE          0x0210  // UQ16.16: PWM duty cycle (%), 0-97.85

// Direction
#define REG_DIRECTION               0x0214  // uint8_t: Rotation direction
  typedef enum {
      DIRECTION_POSITIVE = 0,  // Positive rotation (CCW when viewed from motor shaft)
      DIRECTION_NEGATIVE = 1   // Negative rotation (CW)
  } direction_t;

// PI Controller Tuning (Speed Mode)
#define REG_PI_KP                   0x0218  // UQ16.16: Proportional gain
#define REG_PI_KI                   0x021C  // UQ16.16: Integral gain
#define REG_PI_I_MAX_MA             0x0220  // UQ18.14: Integral windup limit (mA)

// ============================================================================
// Status Registers (0x0300-0x03FF) - READ ONLY
// ============================================================================

// Dynamic State
#define REG_CURRENT_SPEED_RPM       0x0300  // UQ14.18: Current wheel speed (RPM)
#define REG_CURRENT_SPEED_RADS      0x0304  // UQ16.16: Current wheel speed (rad/s)
#define REG_CURRENT_MOMENTUM_NMS    0x0308  // UQ18.14: Angular momentum (N·m·s)
#define REG_CURRENT_TORQUE_MNM      0x030C  // UQ18.14: Output torque (mN·m)
#define REG_CURRENT_CURRENT_MA      0x0310  // UQ18.14: Motor current (mA)
#define REG_CURRENT_POWER_MW        0x0314  // UQ18.14: Electrical power (mW)
#define REG_CURRENT_VOLTAGE_V       0x0318  // UQ16.16: Bus voltage (V)

// Accumulated Values
#define REG_TOTAL_ENERGY_WH         0x031C  // uint32_t: Total energy consumed (mW·h)
#define REG_TOTAL_REVOLUTIONS       0x0320  // uint32_t: Total wheel revolutions
#define REG_UPTIME_SECONDS          0x0324  // uint32_t: Uptime in seconds

// Temperature Sensors
#define REG_TEMP_MOTOR_C            0x0328  // UQ16.16: Motor temperature (°C)
#define REG_TEMP_ELECTRONICS_C      0x032C  // UQ16.16: Electronics temperature (°C)
#define REG_TEMP_BEARING_C          0x0330  // UQ16.16: Bearing temperature (°C)

// ============================================================================
// Fault & Diagnostic Registers (0x0400-0x04FF) - READ/WRITE
// ============================================================================

// Fault Status
#define REG_FAULT_STATUS            0x0400  // uint32_t: Active fault bitmask
  #define FAULT_OVERVOLTAGE           (1 << 0)
  #define FAULT_OVERSPEED             (1 << 1)
  #define FAULT_OVERDUTY              (1 << 2)
  #define FAULT_OVERPOWER             (1 << 3)
  #define FAULT_MOTOR_OVERTEMP        (1 << 4)
  #define FAULT_ELECTRONICS_OVERTEMP  (1 << 5)
  #define FAULT_BEARING_OVERTEMP      (1 << 6)
  #define FAULT_COMMS_TIMEOUT         (1 << 7)
  #define FAULT_ENCODER_ERROR         (1 << 8)

// Fault Latch (write 1 to clear)
#define REG_FAULT_LATCH             0x0404  // uint32_t: Latched fault bitmask (same bits as FAULT_STATUS)

// Warning Status (non-latching)
#define REG_WARNING_STATUS          0x0408  // uint32_t: Warning bitmask
  #define WARN_SOFT_OVERCURRENT       (1 << 0)
  #define WARN_SOFT_OVERSPEED         (1 << 1)
  #define WARN_HIGH_TEMP_MOTOR        (1 << 2)
  #define WARN_HIGH_TEMP_ELECTRONICS  (1 << 3)

// Diagnostic Counters
#define REG_COMM_ERRORS_CRC         0x040C  // uint32_t: CRC error count
#define REG_COMM_ERRORS_FRAMING     0x0410  // uint32_t: SLIP framing error count
#define REG_COMM_ERRORS_OVERRUN     0x0414  // uint32_t: UART overrun count
#define REG_COMM_PACKETS_RX         0x0418  // uint32_t: Total packets received
#define REG_COMM_PACKETS_TX         0x041C  // uint32_t: Total packets transmitted

// Jitter Statistics
#define REG_TICK_JITTER_MAX_US      0x0420  // uint32_t: Maximum tick jitter (µs)
#define REG_TICK_JITTER_AVG_US      0x0424  // uint32_t: Average tick jitter (µs)

// Last Command
#define REG_LAST_COMMAND_CODE       0x0428  // uint8_t: Last NSP command code received
#define REG_LAST_COMMAND_TIMESTAMP  0x042C  // uint32_t: Timestamp of last command (ms)

// ============================================================================
// Register Access Helpers
// ============================================================================

/**
 * @brief Check if register address is valid
 */
static inline bool reg_is_valid_address(uint16_t addr) {
    // Must be 4-byte aligned for 32-bit registers, 2-byte for 16-bit, 1-byte for 8-bit
    // For simplicity, we'll allow any address in our defined ranges
    return (addr >= 0x0000 && addr <= 0x0430);
}

/**
 * @brief Check if register is read-only
 */
static inline bool reg_is_readonly(uint16_t addr) {
    // Device info (0x0000-0x00FF) is read-only
    if (addr >= 0x0000 && addr <= 0x00FF) return true;

    // Status registers (0x0300-0x03FF) are read-only
    if (addr >= 0x0300 && addr <= 0x03FF) return true;

    // Some diagnostic counters are read-only, but most fault/diag are RW
    if (addr >= 0x040C && addr <= 0x0427) return true;  // Counters and jitter

    return false;
}

/**
 * @brief Check if register is write-only
 */
static inline bool reg_is_writeonly(uint16_t addr) {
    // No write-only registers in this design
    return false;
}

/**
 * @brief Get register size in bytes
 */
static inline uint8_t reg_get_size(uint16_t addr) {
    // Single-byte registers
    if (addr == REG_CONTROL_MODE || addr == REG_DIRECTION || addr == REG_LAST_COMMAND_CODE) {
        return 1;
    }

    // Two-byte registers
    if (addr == REG_HARDWARE_REVISION) {
        return 2;
    }

    // All others are 4-byte (uint32_t or UQ formats)
    return 4;
}

/**
 * @brief Register name lookup (for debug/console)
 */
const char* reg_get_name(uint16_t addr);

#endif // NSS_NRWA_T6_REGS_H
