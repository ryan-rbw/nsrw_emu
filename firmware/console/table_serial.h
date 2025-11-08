/**
 * @file table_serial.h
 * @brief Serial Interface Table for Console TUI
 *
 * Table 2: Serial Interface (RS-485, SLIP, CRC statistics)
 */

#ifndef TABLE_SERIAL_H
#define TABLE_SERIAL_H

#include <stdint.h>

/**
 * @brief Initialize Serial Interface table and register with catalog
 */
void table_serial_init(void);

#endif // TABLE_SERIAL_H
