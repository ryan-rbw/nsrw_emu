/**
 * @file table_nsp.h
 * @brief NSP Layer Table for Console TUI
 *
 * Table 3: NSP Layer (last cmd/reply, poll, ack, stats, timing)
 */

#ifndef TABLE_NSP_H
#define TABLE_NSP_H

#include <stdint.h>

/**
 * @brief Initialize NSP Layer table and register with catalog
 */
void table_nsp_init(void);

#endif // TABLE_NSP_H
