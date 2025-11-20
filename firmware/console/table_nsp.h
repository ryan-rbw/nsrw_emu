/**
 * @file table_nsp.h
 * @brief NSP Layer Table for Console TUI
 *
 * Table 3: NSP Stats (RX/TX packets, error breakdown)
 */

#ifndef TABLE_NSP_H
#define TABLE_NSP_H

#include <stdint.h>

/**
 * @brief Initialize NSP Layer table and register with catalog
 */
void table_nsp_init(void);

/**
 * @brief Update NSP stats from handler
 *
 * Call this periodically to fetch latest statistics from nsp_handler
 */
void table_nsp_update(void);

#endif // TABLE_NSP_H
