/**
 * @file tui.c
 * @brief Text User Interface Implementation (Redesigned)
 *
 * Arrow-key navigation with expand/collapse tables.
 */

#include "tui.h"
#include "tables.h"
#include "test_results.h"
#include <stdio.h>
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
            // Field editing (future enhancement)
            break;
    }

    g_tui_state.needs_refresh = false;
}

// ============================================================================
// Input Handling
// ============================================================================

static bool tui_handle_browse_input(int key);

bool tui_handle_input(void) {
    int key = tui_getkey();
    if (key == PICO_ERROR_TIMEOUT) {
        return false;  // No input
    }

    // Handle based on current mode
    switch (g_tui_state.mode) {
        case TUI_MODE_BROWSE:
            return tui_handle_browse_input(key);

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
            // Enter: Edit selected field (future enhancement)
            if (g_tui_state.table_expanded[g_tui_state.selected_table_idx]) {
                // Get selected field
                const table_meta_t* table = catalog_get_table_by_index(g_tui_state.selected_table_idx);
                if (table && g_tui_state.selected_field_idx < table->field_count) {
                    const field_meta_t* field = catalog_get_field(table, g_tui_state.selected_field_idx);

                    if (field && field->access != FIELD_ACCESS_RO) {
                        snprintf(g_tui_state.status_msg, sizeof(g_tui_state.status_msg),
                                 "Editing not yet implemented");
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

            printf("%s %d. %s %s\n",
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
                        catalog_format_value(field, *field->ptr, value_str, sizeof(value_str));
                    } else {
                        snprintf(value_str, sizeof(value_str), "N/A");
                    }

                    // Highlight selected field
                    bool is_field_selected = (i == g_tui_state.selected_table_idx) &&
                                             (j == g_tui_state.selected_field_idx);
                    const char* field_cursor = is_field_selected ? ANSI_REVERSE "►" ANSI_RESET : " ";

                    // Field line: "Display Name (var_name) : value (RO)"
                    printf("  %s   ├─ %s " ANSI_DIM "(%s)" ANSI_RESET " : %-12s " ANSI_DIM "(%s)" ANSI_RESET "\n",
                           field_cursor, display_name, field->name, value_str,
                           field->access == FIELD_ACCESS_RO ? "RO" :
                           field->access == FIELD_ACCESS_WO ? "WO" : "RW");
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

    printf(ANSI_BOLD ANSI_FG_CYAN "NRWA-T6 Emulator" ANSI_RESET);
    printf("  |  ");
    printf("Uptime: %02lu:%02lu:%02lu",
           uptime_sec / 3600,
           (uptime_sec % 3600) / 60,
           uptime_sec % 60);
    printf("  |  ");

    // Test summary
    printf("Tests: %d/%d",
           g_test_results.total_passed, g_test_results.total_tests);

    if (g_test_results.all_passed) {
        printf(" " ANSI_FG_GREEN "✓" ANSI_RESET);
    } else {
        printf(" " ANSI_FG_RED "✗" ANSI_RESET);
    }

    printf("\n");
}

void tui_print_status_banner(void) {
    // TODO: Checkpoint 8.2 - get live values from wheel model
    // For now, show placeholder values
    printf("Status: " ANSI_FG_GREEN "IDLE" ANSI_RESET);
    printf(" │ Mode: " ANSI_DIM "OFF" ANSI_RESET);
    printf(" │ RPM: " ANSI_DIM "0" ANSI_RESET);
    printf(" │ Current: " ANSI_DIM "0.00A" ANSI_RESET);
    printf(" │ Fault: " ANSI_DIM "-" ANSI_RESET "\n");
    printf("---\n");
}

void tui_print_status_bar(const char* message) {
    printf("\n---\n");
    if (message && message[0]) {
        printf(ANSI_FG_YELLOW "%s" ANSI_RESET "\n", message);
    }
}

void tui_print_nav_hints(void) {
    switch (g_tui_state.mode) {
        case TUI_MODE_BROWSE:
            printf(ANSI_DIM "↑↓: Navigate | →: Expand | ←: Collapse | R: Refresh | Q: Quit" ANSI_RESET "\n");
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
