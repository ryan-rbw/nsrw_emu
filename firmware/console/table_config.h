/**
 * @brief Config & JSON Table for Console TUI
 *
 * Table 9: Config Status (scenarios, defaults, save/restore)
 */

#ifndef TABLE_CONFIG_H
#define TABLE_CONFIG_H

#include <stdint.h>

/**
 * @brief Initialize Config & JSON table and register with catalog
 */
void table_config_init(void);

/**
 * @brief Update config table values from scenario engine
 *
 * Call periodically to refresh TUI display
 */
void table_config_update(void);

#endif // TABLE_CONFIG_H
