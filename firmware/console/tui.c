/**
 * @file tui.c
 * @brief Text User Interface Implementation (Redesigned)
 *
 * Arrow-key navigation with expand/collapse tables.
 */

#include "tui.h"
#include "tables.h"
#include "test_results.h"
#include "logo.h"
#include "console_config.h"
#include "console_format.h"
#include "table_control.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "pico/stdlib.h"
#include "pico/time.h"

// ============================================================================
// Global State
// ============================================================================

static tui_state_t g_tui_state;
static uint32_t g_boot_time_ms;

// ============================================================================
// ANSI Escape Helpers (for arrow keys)
// ============================================================================

#define KEY_ARROW_UP    0x1000
#define KEY_ARROW_DOWN  0x1001
#define KEY_ARROW_RIGHT 0x1002
#define KEY_ARROW_LEFT  0x1003

/**
 * @brief Read key with arrow key support
 * @return Character code or KEY_ARROW_* constants
 */
static int tui_getkey(void) {
    int c = getchar_timeout_us(0);
    if (c == PICO_ERROR_TIMEOUT) {
        return PICO_ERROR_TIMEOUT;
    }

    // Check for ESC sequence (arrow keys)
    if (c == 27) {  // ESC
        int c2 = getchar_timeout_us(1000);  // Wait 1ms for '['
        if (c2 == '[') {
            int c3 = getchar_timeout_us(1000);  // Arrow key code
            switch (c3) {
                case 'A': return KEY_ARROW_UP;
                case 'B': return KEY_ARROW_DOWN;
                case 'C': return KEY_ARROW_RIGHT;
                case 'D': return KEY_ARROW_LEFT;
                default: return 27;  // Just ESC
            }
        }
        return 27;  // Just ESC
    }

    return c;
}

// ============================================================================
// Initialization
// ============================================================================

void tui_init(void) {
    memset(&g_tui_state, 0, sizeof(g_tui_state));
    g_tui_state.mode = TUI_MODE_BROWSE;
    g_tui_state.selected_table_idx = 0;
    g_tui_state.selected_field_idx = 0;
    g_tui_state.needs_refresh = true;
    g_boot_time_ms = to_ms_since_boot(get_absolute_time());

    // Clear screen and hide cursor
    printf(ANSI_CLEAR_SCREEN ANSI_CURSOR_HOME ANSI_CURSOR_HIDE);

    // Show initial browse view
    tui_render_browse();
}

void tui_shutdown(void) {
    // Restore terminal state
    printf(ANSI_CURSOR_SHOW ANSI_RESET);
    printf(ANSI_CLEAR_SCREEN ANSI_CURSOR_HOME);
    printf("TUI closed. Goodbye!\n");
}

// ============================================================================
// Main Update Loop
// ============================================================================

void tui_update(bool force_redraw) {
    if (!force_redraw && !g_tui_state.needs_refresh) {
        return;  // Nothing to do
    }

    // Render current mode
    switch (g_tui_state.mode) {
        case TUI_MODE_BROWSE:
            tui_render_browse();
            break;

        case TUI_MODE_EDIT:
            // In edit mode, re-render browse view with input shown in status bar
            tui_render_browse();
            break;
    }

    g_tui_state.needs_refresh = false;
}

// ============================================================================
// Input Handling
// ============================================================================

static bool tui_handle_browse_input(int key);
static bool tui_handle_edit_input(int key);

bool tui_handle_input(void) {
    int key = tui_getkey();
    if (key == PICO_ERROR_TIMEOUT) {
        return false;  // No input
    }

    // Handle based on current mode
    switch (g_tui_state.mode) {
        case TUI_MODE_BROWSE:
            return tui_handle_browse_input(key);

        case TUI_MODE_EDIT:
            return tui_handle_edit_input(key);

        default:
            return false;
    }
}

static bool tui_handle_browse_input(int key) {
    uint8_t table_count = catalog_get_table_count();

    switch (key) {
        case KEY_ARROW_UP:
            // Move cursor up
            if (g_tui_state.table_expanded[g_tui_state.selected_table_idx]) {
                // Inside expanded table - move through fields
                if (g_tui_state.selected_field_idx > 0) {
                    g_tui_state.selected_field_idx--;
                } else {
                    // At top of fields, collapse and select table header
                    g_tui_state.table_expanded[g_tui_state.selected_table_idx] = false;
                }
            } else {
                // Navigate between tables
                if (g_tui_state.selected_table_idx > 0) {
                    g_tui_state.selected_table_idx--;
                }
            }
            g_tui_state.needs_refresh = true;
            return true;

        case KEY_ARROW_DOWN:
            // Move cursor down
            if (g_tui_state.table_expanded[g_tui_state.selected_table_idx]) {
                // Inside expanded table - move through fields
                const table_meta_t* table = catalog_get_table_by_index(g_tui_state.selected_table_idx);
                if (table && g_tui_state.selected_field_idx < table->field_count - 1) {
                    g_tui_state.selected_field_idx++;
                }
            } else {
                // Navigate between tables
                if (g_tui_state.selected_table_idx < table_count - 1) {
                    g_tui_state.selected_table_idx++;
                }
            }
            g_tui_state.needs_refresh = true;
            return true;

        case KEY_ARROW_RIGHT:
            // Expand selected table
            g_tui_state.table_expanded[g_tui_state.selected_table_idx] = true;
            g_tui_state.selected_field_idx = 0;
            g_tui_state.needs_refresh = true;
            return true;

        case KEY_ARROW_LEFT:
            // Collapse expanded table
            g_tui_state.table_expanded[g_tui_state.selected_table_idx] = false;
            g_tui_state.selected_field_idx = 0;
            g_tui_state.needs_refresh = true;
            return true;

        case '\r':
        case '\n':
            // Enter: Edit selected field
            if (g_tui_state.table_expanded[g_tui_state.selected_table_idx]) {
                // Get selected field
                const table_meta_t* table = catalog_get_table_by_index(g_tui_state.selected_table_idx);
                if (table && g_tui_state.selected_field_idx < table->field_count) {
                    const field_meta_t* field = catalog_get_field(table, g_tui_state.selected_field_idx);

                    if (field && field->access != FIELD_ACCESS_RO) {
                        // Enter edit mode
                        g_tui_state.mode = TUI_MODE_EDIT;
                        g_tui_state.input_len = 0;
                        g_tui_state.input_buf[0] = '\0';

                        // Show hint for ENUM and BOOL fields that "?" is available
                        if (field->type == FIELD_TYPE_ENUM || field->type == FIELD_TYPE_BOOL) {
                            snprintf(g_tui_state.status_msg, sizeof(g_tui_state.status_msg),
                                     "Enter new value (? for help, ESC to cancel): ");
                        } else {
                            snprintf(g_tui_state.status_msg, sizeof(g_tui_state.status_msg),
                                     "Enter new value (ESC to cancel): ");
                        }
                        g_tui_state.needs_refresh = true;
                    } else {
                        snprintf(g_tui_state.status_msg, sizeof(g_tui_state.status_msg),
                                 "Field is read-only");
                        g_tui_state.needs_refresh = true;
                    }
                }
            }
            return true;

        case 'r':
        case 'R':
            // Refresh
            g_tui_state.needs_refresh = true;
            return true;

        case 'q':
        case 'Q':
        case 27:  // ESC
            // Quit TUI
            tui_shutdown();
            return true;
    }

    return false;
}

static bool tui_handle_edit_input(int key) {
    // Get currently selected field
    const table_meta_t* table = catalog_get_table_by_index(g_tui_state.selected_table_idx);
    if (!table) {
        g_tui_state.mode = TUI_MODE_BROWSE;
        return false;
    }

    const field_meta_t* field = catalog_get_field(table, g_tui_state.selected_field_idx);
    if (!field) {
        g_tui_state.mode = TUI_MODE_BROWSE;
        return false;
    }

    switch (key) {
        case 27:  // ESC - cancel editing
            g_tui_state.mode = TUI_MODE_BROWSE;
            snprintf(g_tui_state.status_msg, sizeof(g_tui_state.status_msg),
                     "Edit cancelled");
            g_tui_state.needs_refresh = true;
            return true;

        case '\r':
        case '\n':  // Enter - save value
            if (g_tui_state.input_len > 0) {
                // Parse and write value
                g_tui_state.input_buf[g_tui_state.input_len] = '\0';

                // Special case: "?" for ENUM and BOOL fields shows help
                if (strcmp(g_tui_state.input_buf, "?") == 0) {
                    if (field->type == FIELD_TYPE_ENUM) {
                        // Display enum help
                        printf("\nAvailable values for %s:\n", field->name);
                        if (field->enum_values) {
                            for (uint8_t i = 0; i < field->enum_count; i++) {
                                printf("  %d: %s\n", i, field->enum_values[i]);
                            }
                        }
                    } else if (field->type == FIELD_TYPE_BOOL) {
                        // Display bool help
                        printf("\nAvailable values for %s:\n", field->name);
                        printf("  0: FALSE (or false, no)\n");
                        printf("  1: TRUE (or true, yes)\n");
                    }
                    printf("\nPress any key to continue editing...");
                    getchar_timeout_us(5000000);  // Wait for keypress
                    g_tui_state.input_len = 0;
                    g_tui_state.input_buf[0] = '\0';
                    g_tui_state.needs_refresh = true;
                    return true;
                }

                // Use type-aware parsing
                uint32_t value;
                if (catalog_parse_value(field, g_tui_state.input_buf, &value)) {
                    // Valid value - write to field
                    if (field->ptr) {
                        *(volatile uint32_t*)field->ptr = value;

                        // Format value for display
                        char value_str[32];
                        catalog_format_value(field, value, value_str, sizeof(value_str));

                        snprintf(g_tui_state.status_msg, sizeof(g_tui_state.status_msg),
                                 "Saved: %s = %s", field->name, value_str);
                    } else {
                        snprintf(g_tui_state.status_msg, sizeof(g_tui_state.status_msg),
                                 "Error: No pointer for field");
                    }
                } else {
                    snprintf(g_tui_state.status_msg, sizeof(g_tui_state.status_msg),
                             "Error: Invalid value. Type '?' for help.");
                }
            }
            g_tui_state.mode = TUI_MODE_BROWSE;
            g_tui_state.needs_refresh = true;
            return true;

        case 127:   // Backspace
        case '\b':
            if (g_tui_state.input_len > 0) {
                g_tui_state.input_len--;
                g_tui_state.input_buf[g_tui_state.input_len] = '\0';
                g_tui_state.needs_refresh = true;
            }
            return true;

        default:
            // Accept printable characters
            // For ENUM/BOOL fields: letters, digits, underscore, question mark
            // For numeric fields: digits, +, -, .
            if (isprint(key)) {
                bool accept = false;

                if (field->type == FIELD_TYPE_ENUM || field->type == FIELD_TYPE_BOOL) {
                    // ENUM/BOOL: Accept letters (for string names), digits, underscore, ?
                    accept = isalnum(key) || key == '_' || key == '?';
                } else {
                    // Numeric: Accept digits, +, -, .
                    accept = isdigit(key) || key == '-' || key == '+' || key == '.';
                }

                if (accept && g_tui_state.input_len < sizeof(g_tui_state.input_buf) - 1) {
                    g_tui_state.input_buf[g_tui_state.input_len++] = (char)key;
                    g_tui_state.input_buf[g_tui_state.input_len] = '\0';
                    g_tui_state.needs_refresh = true;
                    return true;
                }
            }
            break;
    }

    return false;
}

// ============================================================================
// Rendering Functions
// ============================================================================

// Forward declaration
static void format_field_display_name(const char* var_name, char* buf, size_t buflen);

void tui_render_browse(void) {
    tui_clear_screen();

    // Header
    tui_print_header();

    // Status banner (wheel state)
    tui_print_status_banner();

    // Table list with expand/collapse
    printf("\n");
    printf(ANSI_BOLD "TABLES" ANSI_RESET "\n");
    printf("\n");

    uint8_t table_count = catalog_get_table_count();

    if (table_count == 0) {
        printf("  " ANSI_DIM "(No tables registered)" ANSI_RESET "\n");
    } else {
        for (uint8_t i = 0; i < table_count; i++) {
            const table_meta_t* table = catalog_get_table_by_index(i);
            if (!table) continue;

            // Table header
            bool is_selected = (i == g_tui_state.selected_table_idx) && !g_tui_state.table_expanded[i];
            const char* cursor = is_selected ? ANSI_REVERSE ">" ANSI_RESET : " ";
            const char* expand_icon = g_tui_state.table_expanded[i] ? "▼" : "▶";

            printf("%s %2d. %s %s\n",
                   cursor, i + 1, expand_icon, table->name);

            // Fields (if expanded)
            if (g_tui_state.table_expanded[i]) {
                for (uint8_t j = 0; j < table->field_count; j++) {
                    const field_meta_t* field = catalog_get_field(table, j);
                    if (!field) continue;

                    // Format display name and value
                    char display_name[32];
                    char value_str[32];

                    format_field_display_name(field->name, display_name, sizeof(display_name));

                    if (field->ptr) {
                        // For STRING type, pass the pointer value itself (address of string)
                        // For other types, pass the dereferenced value
                        uint32_t value = (field->type == FIELD_TYPE_STRING)
                            ? (uint32_t)field->ptr
                            : *field->ptr;
                        catalog_format_value(field, value, value_str, sizeof(value_str));
                    } else {
                        snprintf(value_str, sizeof(value_str), "N/A");
                    }

                    // Highlight selected field
                    bool is_field_selected = (i == g_tui_state.selected_table_idx) &&
                                             (j == g_tui_state.selected_field_idx);
                    const char* field_cursor = is_field_selected ? ANSI_REVERSE "►" ANSI_RESET : " ";

                    // Field line: "Display Name (var_name) : value"
                    printf("  %s   ├─ %s " ANSI_DIM "(%s)" ANSI_RESET " : %s\n",
                           field_cursor, display_name, field->name, value_str);
                }
            }
        }
    }

    // Navigation hints
    printf("\n");
    tui_print_nav_hints();

    // Status bar
    tui_print_status_bar(g_tui_state.status_msg);
}

void tui_render_field_edit(const void* table, const void* field) {
    // Not implemented yet (future enhancement)
}

// ============================================================================
// Helper Functions
// ============================================================================

void tui_cursor_pos(uint8_t row, uint8_t col) {
    printf("\033[%d;%dH", row, col);
}

void tui_print_header(void) {
    uint32_t uptime_ms = to_ms_since_boot(get_absolute_time()) - g_boot_time_ms;
    uint32_t uptime_sec = uptime_ms / 1000;
    char header_buf[CONSOLE_WIDTH + 1];

    // Print logo (centered)
    printf("%s", LOGO_ART);

    // Format header info to fit exactly 80 characters
    // Format: "NRWA-T6 Emulator    |    Uptime: HH:MM:SS    |    Tests: N/M ✓/✗"
    int written = snprintf(header_buf, sizeof(header_buf),
                           ANSI_BOLD ANSI_FG_CYAN "NRWA-T6 Emulator" ANSI_RESET
                           "    |    "
                           "Uptime: %02lu:%02lu:%02lu"
                           "    |    "
                           "Tests: %d/%d %s",
                           uptime_sec / 3600,
                           (uptime_sec % 3600) / 60,
                           uptime_sec % 60,
                           g_test_results.total_passed,
                           g_test_results.total_tests,
                           g_test_results.all_passed ? ANSI_FG_GREEN "✓" ANSI_RESET : ANSI_FG_RED "✗" ANSI_RESET);

    // Print header with padding to fill 80 characters
    printf("%s", header_buf);

    // Calculate visible length (excluding ANSI codes)
    // Approximate: count only printable characters
    int visible_len = 16 + 8 + 23 + 8 + 10;  // Approximate without ANSI codes
    int padding = CONSOLE_WIDTH - visible_len;
    if (padding > 0) {
        for (int i = 0; i < padding; i++) {
            putchar(' ');
        }
    }

    printf("\n");
}

void tui_print_status_banner(void) {
    // Draw separator line
    console_print_line('-');

    // Get live values from control table
    uint32_t mode = table_control_get_mode();
    const char* mode_str = table_control_get_mode_string(mode);
    uint32_t speed_rpm = table_control_get_speed_rpm();
    uint32_t current_ma = table_control_get_current_ma();

    // Convert current from mA to A for display
    float current_a = current_ma / 1000.0f;

    // Format status banner with live values
    // Status is IDLE if speed is 0, otherwise ACTIVE
    const char* status = (speed_rpm == 0) ? "IDLE" : "ACTIVE";
    const char* status_color = (speed_rpm == 0) ? ANSI_FG_GREEN : ANSI_FG_CYAN;

    // Build the status string
    printf("Status: %s%s" ANSI_RESET " │ Mode: %s%s" ANSI_RESET " │ RPM: %s%lu" ANSI_RESET " │ Current: %s%.2fA" ANSI_RESET " │ Fault: " ANSI_DIM "-" ANSI_RESET,
           status_color, status,
           (speed_rpm == 0) ? ANSI_DIM : "", mode_str,
           (speed_rpm == 0) ? ANSI_DIM : ANSI_FG_CYAN, (unsigned long)speed_rpm,
           (current_ma == 0) ? ANSI_DIM : ANSI_FG_YELLOW, current_a);

    // Pad to end of line
    printf("\n");

    // Draw separator line
    console_print_line('-');
}

void tui_print_status_bar(const char* message) {
    printf("\n");
    console_print_line('-');
    if (message && message[0]) {
        printf(ANSI_FG_YELLOW "%s" ANSI_RESET, message);

        // In edit mode, show input buffer
        if (g_tui_state.mode == TUI_MODE_EDIT) {
            printf(ANSI_BOLD "%s" ANSI_RESET "_", g_tui_state.input_buf);
        }
        printf("\n");
    }
}

void tui_print_nav_hints(void) {
    switch (g_tui_state.mode) {
        case TUI_MODE_BROWSE:
            printf(ANSI_DIM "↑↓ : Navigate | → : Expand | ← : Collapse | R : Refresh | Q : Quit" ANSI_RESET "\n");
            break;

        default:
            break;
    }
}

void tui_format_field_value(uint16_t field_id, uint32_t value, char* buf, size_t buflen) {
    // Simple formatter (will be enhanced in Checkpoint 8.2)
    snprintf(buf, buflen, "%lu", (unsigned long)value);
}

// ============================================================================
// Field Name Formatting
// ============================================================================

/**
 * @brief Convert snake_case to Title Case for display
 *
 * Examples:
 *   "tx_count" → "TX Count"
 *   "speed_rpm" → "Speed RPM"
 *   "pid_kp" → "PID Kp"
 */
static void format_field_display_name(const char* var_name, char* buf, size_t buflen) {
    size_t i = 0, j = 0;
    bool new_word = true;

    while (var_name[i] && j < buflen - 1) {
        if (var_name[i] == '_') {
            buf[j++] = ' ';
            new_word = true;
            i++;
        } else {
            if (new_word) {
                buf[j++] = toupper(var_name[i]);
                new_word = false;
            } else {
                buf[j++] = var_name[i];
            }
            i++;
        }
    }
    buf[j] = '\0';
}

// ============================================================================
// Type Name Lookup
// ============================================================================

const char* field_type_name(field_type_t type) {
    switch (type) {
        case FIELD_TYPE_BOOL:    return "BOOL";
        case FIELD_TYPE_U8:      return "U8";
        case FIELD_TYPE_U16:     return "U16";
        case FIELD_TYPE_U32:     return "U32";
        case FIELD_TYPE_I32:     return "I32";
        case FIELD_TYPE_HEX:     return "HEX";
        case FIELD_TYPE_ENUM:    return "ENUM";
        case FIELD_TYPE_FLOAT:   return "FLOAT";
        case FIELD_TYPE_Q14_18:  return "Q14.18";
        case FIELD_TYPE_Q16_16:  return "Q16.16";
        case FIELD_TYPE_Q18_14:  return "Q18.14";
        case FIELD_TYPE_STRING:  return "STRING";
        default:                 return "UNKNOWN";
    }
}
