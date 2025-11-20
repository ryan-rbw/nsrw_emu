/**
 * @file table_serial.h
 * @brief Serial Interface Table for Console TUI
 *
 * Table 2: Serial Interface (RS-485, SLIP statistics)
 */

#ifndef TABLE_SERIAL_H
#define TABLE_SERIAL_H

#include <stdint.h>

/**
 * @brief Initialize Serial Interface table and register with catalog
 */
void table_serial_init(void);

/**
 * @brief Update Serial stats from handler
 *
 * Call this periodically to fetch latest RS-485/SLIP statistics
 */
void table_serial_update(void);

#endif // TABLE_SERIAL_H
