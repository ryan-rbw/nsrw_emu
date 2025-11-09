/**
 * @file table_control.h
 * @brief Control Mode Table for Console TUI
 *
 * Table 4: Control Mode (mode, setpoint, direction, PWM, source)
 */

#ifndef TABLE_CONTROL_H
#define TABLE_CONTROL_H

#include <stdint.h>

/**
 * @brief Initialize Control Mode table and register with catalog
 */
void table_control_init(void);

/**
 * @brief Get current control mode
 * @return Control mode (0=CURRENT, 1=SPEED, 2=TORQUE, 3=PWM)
 */
uint32_t table_control_get_mode(void);

/**
 * @brief Get current direction
 * @return Direction (0=POSITIVE, 1=NEGATIVE)
 */
uint32_t table_control_get_direction(void);

/**
 * @brief Get current speed setpoint in RPM
 * @return Speed setpoint in RPM
 */
uint32_t table_control_get_speed_rpm(void);

/**
 * @brief Get current current setpoint in mA
 * @return Current setpoint in mA
 */
uint32_t table_control_get_current_ma(void);

/**
 * @brief Get current torque setpoint in mN·m
 * @return Torque setpoint in mN·m
 */
uint32_t table_control_get_torque_mnm(void);

/**
 * @brief Get current PWM duty cycle in %
 * @return PWM duty cycle in %
 */
uint32_t table_control_get_pwm_pct(void);

/**
 * @brief Get mode enum string (UPPERCASE)
 * @param mode Mode value (0-3)
 * @return Mode string or "INVALID"
 */
const char* table_control_get_mode_string(uint32_t mode);

/**
 * @brief Get direction enum string (UPPERCASE)
 * @param direction Direction value (0-1)
 * @return Direction string or "INVALID"
 */
const char* table_control_get_direction_string(uint32_t direction);

#endif // TABLE_CONTROL_H
