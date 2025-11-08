/**
 * @file commands.c
 * @brief Command Palette Parser Implementation
 *
 * Implements partial prefix matching for hierarchical commands.
 */

#include "commands.h"
#include "tables.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Convert string to lowercase in-place
 */
static void str_tolower(char* str) {
    for (char* p = str; *p; p++) {
        *p = tolower((unsigned char)*p);
    }
}

/**
 * @brief Check if str1 starts with str2 (case-insensitive prefix match)
 */
static bool str_prefix_match(const char* str, const char* prefix) {
    while (*prefix) {
        if (tolower((unsigned char)*str) != tolower((unsigned char)*prefix)) {
            return false;
        }
        str++;
        prefix++;
    }
    return true;
}

/**
 * @brief Match token against array of candidates, return index or -1
 *
 * Supports shortest unambiguous prefix matching.
 */
static int match_token(const char* token, const char* const* candidates, int count) {
    int match_idx = -1;
    int match_count = 0;

    for (int i = 0; i < count; i++) {
        if (str_prefix_match(candidates[i], token)) {
            match_idx = i;
            match_count++;
        }
    }

    // Return match only if unambiguous
    return (match_count == 1) ? match_idx : -1;
}

/**
 * @brief Tokenize input string into words (space-separated)
 *
 * Modifies input string in-place by inserting null terminators.
 * Returns number of tokens found.
 */
static int tokenize(char* input, char** tokens, int max_tokens) {
    int count = 0;
    char* p = input;

    while (*p && count < max_tokens) {
        // Skip leading spaces
        while (*p && isspace((unsigned char)*p)) {
            p++;
        }
        if (!*p) break;

        // Mark start of token
        tokens[count++] = p;

        // Find end of token
        while (*p && !isspace((unsigned char)*p)) {
            p++;
        }

        // Null-terminate token
        if (*p) {
            *p++ = '\0';
        }
    }

    return count;
}

// ============================================================================
// Command Handlers
// ============================================================================

/**
 * @brief Handle "help" or "?" command
 */
static cmd_result_t cmd_help(char* output, size_t output_len) {
    cmd_get_help(output, output_len);
    return CMD_OK;
}

/**
 * @brief Handle "version" command
 */
static cmd_result_t cmd_version(char* output, size_t output_len) {
    // FIRMWARE_VERSION is defined by CMake as a string macro
    snprintf(output, output_len,
             "Firmware: " FIRMWARE_VERSION "\n"
             "Build: " __DATE__ " " __TIME__ "\n"
             "Platform: RP2040 (Pico)");
    return CMD_OK;
}

/**
 * @brief Handle "uptime" command
 */
static cmd_result_t cmd_uptime(char* output, size_t output_len) {
    uint64_t uptime_us = time_us_64();
    uint32_t uptime_ms = (uint32_t)(uptime_us / 1000);
    uint32_t uptime_s = uptime_ms / 1000;
    uint32_t hours = uptime_s / 3600;
    uint32_t minutes = (uptime_s % 3600) / 60;
    uint32_t seconds = uptime_s % 60;

    snprintf(output, output_len, "Uptime: %02lu:%02lu:%02lu (%lu ms)",
             (unsigned long)hours, (unsigned long)minutes, (unsigned long)seconds,
             (unsigned long)uptime_ms);
    return CMD_OK;
}

/**
 * @brief Handle "database table list" command
 */
static cmd_result_t cmd_db_table_list(char* output, size_t output_len) {
    uint8_t table_count = catalog_get_table_count();
    char* p = output;
    size_t remaining = output_len;

    int written = snprintf(p, remaining, "Tables (%d):\n", table_count);
    p += written;
    remaining -= written;

    for (uint8_t i = 0; i < table_count; i++) {
        const table_meta_t* table = catalog_get_table_by_index(i);
        if (table && remaining > 0) {
            written = snprintf(p, remaining, "  %d. %s (%d fields)\n",
                             table->id, table->name, table->field_count);
            p += written;
            remaining = (remaining > written) ? (remaining - written) : 0;
        }
    }

    return CMD_OK;
}

/**
 * @brief Handle "database table describe <table>" command
 */
static cmd_result_t cmd_db_table_describe(const char* table_name, char* output, size_t output_len) {
    const table_meta_t* table = catalog_get_table_by_name(table_name);
    if (!table) {
        snprintf(output, output_len, "ERR: Table '%s' not found", table_name);
        return CMD_ERR_NOT_FOUND;
    }

    char* p = output;
    size_t remaining = output_len;

    int written = snprintf(p, remaining, "Table: %s\n", table->name);
    p += written;
    remaining -= written;

    for (uint8_t i = 0; i < table->field_count && remaining > 0; i++) {
        const field_meta_t* field = catalog_get_field(table, i);
        if (field) {
            const char* access_str = (field->access == FIELD_ACCESS_RO) ? "RO" :
                                    (field->access == FIELD_ACCESS_WO) ? "WO" : "RW";

            written = snprintf(p, remaining, "  %s (%s) %s\n",
                             field->name, access_str, field->units);
            p += written;
            remaining = (remaining > written) ? (remaining - written) : 0;
        }
    }

    return CMD_OK;
}

/**
 * @brief Handle "database table get <table>.<field>" command
 */
static cmd_result_t cmd_db_table_get(const char* path, char* output, size_t output_len) {
    // Parse "table.field" format
    char path_copy[128];
    strncpy(path_copy, path, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';

    char* dot = strchr(path_copy, '.');
    if (!dot) {
        snprintf(output, output_len, "ERR: Invalid path format (use table.field)");
        return CMD_ERR_INVALID_ARGS;
    }

    *dot = '\0';  // Split into table and field
    const char* table_name = path_copy;
    const char* field_name = dot + 1;

    // Find table
    const table_meta_t* table = catalog_get_table_by_name(table_name);
    if (!table) {
        snprintf(output, output_len, "ERR: Table '%s' not found", table_name);
        return CMD_ERR_NOT_FOUND;
    }

    // Find field
    const field_meta_t* field = catalog_get_field_by_name(table, field_name);
    if (!field) {
        snprintf(output, output_len, "ERR: Field '%s' not found in table '%s'",
                 field_name, table_name);
        return CMD_ERR_NOT_FOUND;
    }

    // Read value
    if (!field->ptr) {
        snprintf(output, output_len, "ERR: Field '%s' has no data pointer", field_name);
        return CMD_ERR_INVALID_ARGS;
    }

    uint32_t value = *field->ptr;
    char value_str[64];
    catalog_format_value(field, value, value_str, sizeof(value_str));

    snprintf(output, output_len, "%s.%s = %s %s",
             table_name, field_name, value_str, field->units);
    return CMD_OK;
}

/**
 * @brief Handle "database table set <table>.<field> <value>" command
 */
static cmd_result_t cmd_db_table_set(const char* path, const char* value_str,
                                     char* output, size_t output_len) {
    // Parse "table.field" format
    char path_copy[128];
    strncpy(path_copy, path, sizeof(path_copy) - 1);
    path_copy[sizeof(path_copy) - 1] = '\0';

    char* dot = strchr(path_copy, '.');
    if (!dot) {
        snprintf(output, output_len, "ERR: Invalid path format (use table.field)");
        return CMD_ERR_INVALID_ARGS;
    }

    *dot = '\0';
    const char* table_name = path_copy;
    const char* field_name = dot + 1;

    // Find table and field
    const table_meta_t* table = catalog_get_table_by_name(table_name);
    if (!table) {
        snprintf(output, output_len, "ERR: Table '%s' not found", table_name);
        return CMD_ERR_NOT_FOUND;
    }

    const field_meta_t* field = catalog_get_field_by_name(table, field_name);
    if (!field) {
        snprintf(output, output_len, "ERR: Field '%s' not found", field_name);
        return CMD_ERR_NOT_FOUND;
    }

    // Check write permission
    if (field->access == FIELD_ACCESS_RO) {
        snprintf(output, output_len, "ERR: Field '%s' is read-only", field_name);
        return CMD_ERR_READ_ONLY;
    }

    // Parse value (simplified - only supports integers for now)
    uint32_t value = 0;
    if (sscanf(value_str, "%lu", (unsigned long*)&value) != 1) {
        snprintf(output, output_len, "ERR: Invalid value '%s'", value_str);
        return CMD_ERR_PARSE_ERROR;
    }

    // Write value
    if (!field->ptr) {
        snprintf(output, output_len, "ERR: Field has no data pointer");
        return CMD_ERR_INVALID_ARGS;
    }

    *(volatile uint32_t*)field->ptr = value;

    snprintf(output, output_len, "OK: %s.%s = %lu", table_name, field_name,
             (unsigned long)value);
    return CMD_OK;
}

// ============================================================================
// Main Command Parser
// ============================================================================

cmd_result_t cmd_execute(const char* input, char* output, size_t output_len) {
    if (!input || !output || output_len == 0) {
        return CMD_ERR_INVALID_ARGS;
    }

    // Copy input to working buffer (tokenize modifies in-place)
    char input_copy[256];
    strncpy(input_copy, input, sizeof(input_copy) - 1);
    input_copy[sizeof(input_copy) - 1] = '\0';

    // Tokenize
    char* tokens[16];
    int token_count = tokenize(input_copy, tokens, 16);

    if (token_count == 0) {
        snprintf(output, output_len, "ERR: Empty command");
        return CMD_ERR_INVALID_ARGS;
    }

    // Match first token against top-level commands
    const char* top_level_cmds[] = {
        "help", "?", "version", "uptime", "quit", "exit", "database", "db", "d"
    };

    int cmd_idx = match_token(tokens[0], top_level_cmds, 9);

    // Handle simple commands
    if (cmd_idx == 0 || cmd_idx == 1) {  // help or ?
        return cmd_help(output, output_len);
    }
    if (cmd_idx == 2) {  // version
        return cmd_version(output, output_len);
    }
    if (cmd_idx == 3) {  // uptime
        return cmd_uptime(output, output_len);
    }
    if (cmd_idx == 4 || cmd_idx == 5) {  // quit or exit
        snprintf(output, output_len, "Use Q or ESC to exit");
        return CMD_OK;
    }

    // Handle database commands (tokens: database, db, or d)
    if (cmd_idx >= 6) {
        if (token_count < 2) {
            snprintf(output, output_len, "ERR: 'database' requires subcommand (table, defaults)");
            return CMD_ERR_INVALID_ARGS;
        }

        const char* db_subcmds[] = {"table", "t", "tab", "defaults", "def"};
        int db_idx = match_token(tokens[1], db_subcmds, 5);

        // Handle "database table" commands
        if (db_idx >= 0 && db_idx <= 2) {  // table, t, tab
            if (token_count < 3) {
                snprintf(output, output_len, "ERR: 'table' requires subcommand (list, get, set, describe)");
                return CMD_ERR_INVALID_ARGS;
            }

            const char* table_subcmds[] = {"list", "ls", "l", "get", "g", "set", "s", "describe", "desc"};
            int table_idx = match_token(tokens[2], table_subcmds, 9);

            if (table_idx >= 0 && table_idx <= 2) {  // list, ls, l
                return cmd_db_table_list(output, output_len);
            }
            if (table_idx >= 3 && table_idx <= 4) {  // get, g
                if (token_count < 4) {
                    snprintf(output, output_len, "ERR: 'get' requires <table>.<field>");
                    return CMD_ERR_INVALID_ARGS;
                }
                return cmd_db_table_get(tokens[3], output, output_len);
            }
            if (table_idx >= 5 && table_idx <= 6) {  // set, s
                if (token_count < 5) {
                    snprintf(output, output_len, "ERR: 'set' requires <table>.<field> <value>");
                    return CMD_ERR_INVALID_ARGS;
                }
                return cmd_db_table_set(tokens[3], tokens[4], output, output_len);
            }
            if (table_idx >= 7) {  // describe, desc
                if (token_count < 4) {
                    snprintf(output, output_len, "ERR: 'describe' requires <table>");
                    return CMD_ERR_INVALID_ARGS;
                }
                return cmd_db_table_describe(tokens[3], output, output_len);
            }
        }

        // Handle "database defaults" commands
        if (db_idx >= 3) {  // defaults, def
            snprintf(output, output_len, "Defaults tracking not yet implemented");
            return CMD_OK;
        }
    }

    snprintf(output, output_len, "ERR: Unknown command '%s'", tokens[0]);
    return CMD_ERR_UNKNOWN_COMMAND;
}

void cmd_get_help(char* output, size_t output_len) {
    snprintf(output, output_len,
             "Commands (prefix matching supported):\n"
             "  help, ?           - Show this help\n"
             "  version           - Firmware version\n"
             "  uptime            - System uptime\n"
             "  quit, exit        - Exit command mode\n"
             "\n"
             "Database commands:\n"
             "  d t l             - List all tables\n"
             "  d t desc <table>  - Describe table fields\n"
             "  d t g <t>.<f>     - Get field value\n"
             "  d t s <t>.<f> <v> - Set field value\n"
             "\n"
             "Full forms also supported:\n"
             "  database table list\n"
             "  database table describe <table>\n"
             "  database table get <table>.<field>\n"
             "  database table set <table>.<field> <value>");
}
