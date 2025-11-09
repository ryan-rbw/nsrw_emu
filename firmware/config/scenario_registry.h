/**
 * @file scenario_registry.h
 * @brief Scenario Registry for Fault Injection System
 *
 * Registry of all embedded scenarios (compiled into firmware).
 * Used by TUI to list and select scenarios for execution.
 */

#ifndef SCENARIO_REGISTRY_H
#define SCENARIO_REGISTRY_H

#include <stdint.h>
#include <stddef.h>

// ============================================================================
// Scenario Registry Entry
// ============================================================================

/**
 * @brief Scenario registry entry
 *
 * Links scenario name to JSON data (compiled as string constant)
 */
typedef struct {
    const char* name;       // Display name (shown in TUI)
    const char* json_data;  // JSON scenario data (string constant)
    size_t json_len;        // JSON data length
} scenario_entry_t;

// ============================================================================
// Global Registry
// ============================================================================

/**
 * @brief Global scenario registry
 *
 * Array of all available scenarios compiled into firmware.
 */
extern const scenario_entry_t g_scenario_registry[];

/**
 * @brief Number of scenarios in registry
 */
extern const uint8_t g_scenario_count;

// ============================================================================
// Registry API
// ============================================================================

/**
 * @brief Get scenario by index
 *
 * @param index Scenario index (0 to g_scenario_count-1)
 * @return Pointer to scenario entry, or NULL if invalid index
 */
const scenario_entry_t* scenario_registry_get(uint8_t index);

/**
 * @brief Get scenario count
 *
 * @return Number of scenarios available
 */
uint8_t scenario_registry_count(void);

/**
 * @brief Find scenario by name
 *
 * @param name Scenario name to search for
 * @return Scenario index, or 0xFF if not found
 */
uint8_t scenario_registry_find(const char* name);

#endif // SCENARIO_REGISTRY_H
