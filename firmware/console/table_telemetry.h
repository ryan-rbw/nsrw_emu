/**
 * @file table_telemetry.h
 * @brief Telemetry Blocks Table for Console TUI
 *
 * Table 7: Telemetry Blocks (decoded STANDARD/TEMP/VOLT/CURR/DIAG)
 */

#ifndef TABLE_TELEMETRY_H
#define TABLE_TELEMETRY_H

#include <stdint.h>

/**
 * @brief Initialize Telemetry Blocks table and register with catalog
 */
void table_telemetry_init(void);

#endif // TABLE_TELEMETRY_H
