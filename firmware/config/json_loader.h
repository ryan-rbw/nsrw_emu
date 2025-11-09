/**
 * @file json_loader.h
 * @brief JSON Scenario File Parser
 *
 * Minimal JSON parser for fault injection scenarios.
 * Supports the schema defined in SPEC.md §9.
 */

#ifndef JSON_LOADER_H
#define JSON_LOADER_H

#include "scenario.h"
#include <stddef.h>
#include <stdbool.h>

/**
 * @brief Parse JSON scenario string into scenario structure
 *
 * @param json_str JSON string (null-terminated)
 * @param json_len Length of JSON string
 * @param scenario Output scenario structure
 * @return true if parsed successfully, false on error
 */
bool json_parse_scenario(const char* json_str, size_t json_len, scenario_t* scenario);

/**
 * @brief Get last parse error message
 *
 * @return Error string or NULL if no error
 */
const char* json_get_last_error(void);

#endif // JSON_LOADER_H
