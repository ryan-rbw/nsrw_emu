/**
 * @brief Fault Injection Control Table for Console TUI
 *
 * Table 10: Fault Injection Control (scenario selection and execution)
 */

#ifndef TABLE_FAULT_INJECTION_H
#define TABLE_FAULT_INJECTION_H

#include <stdint.h>

/**
 * @brief Initialize Fault Injection Control table and register with catalog
 */
void table_fault_injection_init(void);

/**
 * @brief Update fault injection control table values
 *
 * Call periodically to refresh TUI display
 */
void table_fault_injection_update(void);

/**
 * @brief Execute selected scenario (trigger function)
 *
 * Called when user sets trigger field to 1 or presses execute key.
 * Enters live playback mode with scrolling console output.
 */
void fault_injection_execute(void);

#endif // TABLE_FAULT_INJECTION_H
