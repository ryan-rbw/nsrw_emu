/**
 * @file nss_nrwa_t6_regs.c
 * @brief NSS NRWA-T6 Register Map Implementation
 */

#include "nss_nrwa_t6_regs.h"
#include <stddef.h>

/**
 * @brief Get human-readable name for register address
 *
 * @param addr Register address
 * @return Register name string, or "UNKNOWN" if not found
 */
const char* reg_get_name(uint16_t addr) {
    switch (addr) {
        // Device Information
        case REG_DEVICE_ID:             return "DEVICE_ID";
        case REG_FIRMWARE_VERSION:      return "FIRMWARE_VERSION";
        case REG_HARDWARE_REVISION:     return "HARDWARE_REVISION";
        case REG_SERIAL_NUMBER:         return "SERIAL_NUMBER";
        case REG_BUILD_TIMESTAMP:       return "BUILD_TIMESTAMP";
        case REG_INERTIA_KGM2:          return "INERTIA_KGM2";
        case REG_MOTOR_KT_NMA:          return "MOTOR_KT_NMA";

        // Protection Configuration
        case REG_OVERVOLTAGE_THRESHOLD: return "OVERVOLTAGE_THRESHOLD";
        case REG_OVERSPEED_FAULT_RPM:   return "OVERSPEED_FAULT_RPM";
        case REG_MAX_DUTY_CYCLE:        return "MAX_DUTY_CYCLE";
        case REG_MOTOR_OVERPOWER_LIMIT: return "MOTOR_OVERPOWER_LIMIT";
        case REG_BRAKING_LOAD_SETPOINT: return "BRAKING_LOAD_SETPOINT";
        case REG_SOFT_OVERCURRENT_MA:   return "SOFT_OVERCURRENT_MA";
        case REG_SOFT_OVERSPEED_RPM:    return "SOFT_OVERSPEED_RPM";
        case REG_PROTECTION_ENABLE:     return "PROTECTION_ENABLE";

        // Control Registers
        case REG_CONTROL_MODE:          return "CONTROL_MODE";
        case REG_SPEED_SETPOINT_RPM:    return "SPEED_SETPOINT_RPM";
        case REG_CURRENT_SETPOINT_MA:   return "CURRENT_SETPOINT_MA";
        case REG_TORQUE_SETPOINT_MNM:   return "TORQUE_SETPOINT_MNM";
        case REG_PWM_DUTY_CYCLE:        return "PWM_DUTY_CYCLE";
        case REG_DIRECTION:             return "DIRECTION";
        case REG_PI_KP:                 return "PI_KP";
        case REG_PI_KI:                 return "PI_KI";
        case REG_PI_I_MAX_MA:           return "PI_I_MAX_MA";

        // Status Registers
        case REG_CURRENT_SPEED_RPM:     return "CURRENT_SPEED_RPM";
        case REG_CURRENT_SPEED_RADS:    return "CURRENT_SPEED_RADS";
        case REG_CURRENT_MOMENTUM_NMS:  return "CURRENT_MOMENTUM_NMS";
        case REG_CURRENT_TORQUE_MNM:    return "CURRENT_TORQUE_MNM";
        case REG_CURRENT_CURRENT_MA:    return "CURRENT_CURRENT_MA";
        case REG_CURRENT_POWER_MW:      return "CURRENT_POWER_MW";
        case REG_CURRENT_VOLTAGE_V:     return "CURRENT_VOLTAGE_V";
        case REG_TOTAL_ENERGY_WH:       return "TOTAL_ENERGY_WH";
        case REG_TOTAL_REVOLUTIONS:     return "TOTAL_REVOLUTIONS";
        case REG_UPTIME_SECONDS:        return "UPTIME_SECONDS";
        case REG_TEMP_MOTOR_C:          return "TEMP_MOTOR_C";
        case REG_TEMP_ELECTRONICS_C:    return "TEMP_ELECTRONICS_C";
        case REG_TEMP_BEARING_C:        return "TEMP_BEARING_C";

        // Fault & Diagnostic Registers
        case REG_FAULT_STATUS:          return "FAULT_STATUS";
        case REG_FAULT_LATCH:           return "FAULT_LATCH";
        case REG_WARNING_STATUS:        return "WARNING_STATUS";
        case REG_COMM_ERRORS_CRC:       return "COMM_ERRORS_CRC";
        case REG_COMM_ERRORS_FRAMING:   return "COMM_ERRORS_FRAMING";
        case REG_COMM_ERRORS_OVERRUN:   return "COMM_ERRORS_OVERRUN";
        case REG_COMM_PACKETS_RX:       return "COMM_PACKETS_RX";
        case REG_COMM_PACKETS_TX:       return "COMM_PACKETS_TX";
        case REG_TICK_JITTER_MAX_US:    return "TICK_JITTER_MAX_US";
        case REG_TICK_JITTER_AVG_US:    return "TICK_JITTER_AVG_US";
        case REG_LAST_COMMAND_CODE:     return "LAST_COMMAND_CODE";
        case REG_LAST_COMMAND_TIMESTAMP: return "LAST_COMMAND_TIMESTAMP";

        default:                        return "UNKNOWN";
    }
}
