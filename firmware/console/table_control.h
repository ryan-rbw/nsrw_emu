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

#endif // TABLE_CONTROL_H
