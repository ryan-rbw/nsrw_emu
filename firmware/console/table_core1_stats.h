/**
 * @file table_core1_stats.h
 * @brief Table 11: Core1 Physics Statistics - Header
 */

#ifndef TABLE_CORE1_STATS_H
#define TABLE_CORE1_STATS_H

#include "tables.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Get table metadata for Core1 physics stats
 *
 * @return Pointer to table metadata
 */
table_meta_t* table_core1_stats_get(void);

/**
 * @brief Initialize Core1 stats table
 *
 * Call during startup before TUI initialization.
 */
void table_core1_stats_init(void);

/**
 * @brief Update Core1 stats from telemetry
 *
 * Reads latest telemetry snapshot from Core1 and updates local cache.
 * Call this periodically from Core0 main loop (e.g., every 50ms).
 */
void table_core1_stats_update(void);

/**
 * @brief Check if telemetry snapshot is valid
 *
 * @return true if valid telemetry has been received, false otherwise
 */
bool table_core1_stats_is_valid(void);

/**
 * @brief Get age of last telemetry snapshot
 *
 * @return Age in milliseconds, or 0xFFFFFFFF if no valid snapshot
 */
uint32_t table_core1_stats_get_age_ms(void);

#endif // TABLE_CORE1_STATS_H
