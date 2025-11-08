/**
 * @file commands.h
 * @brief Command Palette Parser with Partial Prefix Matching
 *
 * Supports hierarchical command structure with shortest unambiguous prefix matching.
 */

#ifndef COMMANDS_H
#define COMMANDS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// ============================================================================
// Command Result
// ============================================================================

/**
 * @brief Command execution result
 */
typedef enum {
    CMD_OK = 0,                 // Command executed successfully
    CMD_ERR_UNKNOWN_COMMAND,    // Command not recognized
    CMD_ERR_INVALID_ARGS,       // Invalid arguments
    CMD_ERR_NOT_FOUND,          // Table/field not found
    CMD_ERR_READ_ONLY,          // Attempt to write read-only field
    CMD_ERR_PARSE_ERROR,        // Value parsing error
} cmd_result_t;

// ============================================================================
// Command Parser API
// ============================================================================

/**
 * @brief Execute command from user input string
 *
 * Supports partial prefix matching for all command tokens.
 * Output buffer receives result message (success or error).
 *
 * @param input Command string (e.g., "d t l" or "database table list")
 * @param output Output buffer for result message
 * @param output_len Size of output buffer
 * @return Command result code
 */
cmd_result_t cmd_execute(const char* input, char* output, size_t output_len);

/**
 * @brief Get help text for command palette
 *
 * @param output Output buffer for help text
 * @param output_len Size of output buffer
 */
void cmd_get_help(char* output, size_t output_len);

#endif // COMMANDS_H
