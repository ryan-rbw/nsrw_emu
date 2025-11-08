/**
 * @file table_dynamics.h
 * @brief Dynamics Table for Console TUI
 *
 * Table 5: Dynamics (speed, momentum, torque, current, power, losses)
 */

#ifndef TABLE_DYNAMICS_H
#define TABLE_DYNAMICS_H

#include <stdint.h>

/**
 * @brief Initialize Dynamics table and register with catalog
 */
void table_dynamics_init(void);

#endif // TABLE_DYNAMICS_H
