/**
 * @file table_test_modes.h
 * @brief Table 11: Test Modes - Header
 */

#ifndef TABLE_TEST_MODES_H
#define TABLE_TEST_MODES_H

#include "tables.h"
#include <stddef.h>
#include <stdbool.h>

/**
 * @brief Initialize test modes table
 */
void table_test_modes_init(void);

/**
 * @brief Update test modes table status
 */
void table_test_modes_update(void);

/**
 * @brief Print detailed test mode status
 */
void table_test_modes_print_status(void);

/**
 * @brief Activate a test mode by ID
 *
 * @param mode_id Test mode ID (1-7, or 0 to deactivate)
 * @return true if successful
 */
bool table_test_modes_activate(int mode_id);

/**
 * @brief Deactivate current test mode
 *
 * @return true if successful
 */
bool table_test_modes_deactivate(void);

/**
 * @brief List all available test modes
 */
void table_test_modes_list(void);

#endif // TABLE_TEST_MODES_H
