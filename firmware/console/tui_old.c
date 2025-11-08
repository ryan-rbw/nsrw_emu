/**
 * @file tui.c
 * @brief Text User Interface Implementation
 *
 * Live, non-scrolling TUI using ANSI escape sequences.
 * Updates in place like top/htop - no scrolling like checkpoint tests.
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
// Initialization
// ============================================================================

void tui_init(void) {
    memset(&g_tui_state, 0, sizeof(g_tui_state));
    g_tui_state.mode = TUI_MODE_MENU;
    g_tui_state.needs_refresh = true;
    g_boot_time_ms = to_ms_since_boot(get_absolute_time());

    // Clear screen and hide cursor
    printf(ANSI_CLEAR_SCREEN ANSI_CURSOR_HOME ANSI_CURSOR_HIDE);

    // Show initial menu
    tui_render_menu();
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
        case TUI_MODE_MENU:
            tui_render_menu();
            break;

        case TUI_MODE_TABLE:
            if (g_tui_state.current_table) {
                tui_render_table(g_tui_state.current_table);
            }
            break;

        case TUI_MODE_COMMAND:
            tui_render_command_palette();
            break;

        case TUI_MODE_EDIT:
            // Field editing not implemented yet (Checkpoint 8.3)
            break;
    }

    g_tui_state.needs_refresh = false;
}

// ============================================================================
// Input Handling (Forward Declarations)
// ============================================================================

static bool tui_handle_menu_input(int c);
static bool tui_handle_table_input(int c);
static bool tui_handle_command_input(int c);

bool tui_handle_input(void) {
    int c = getchar_timeout_us(0);  // Non-blocking
    if (c == PICO_ERROR_TIMEOUT) {
        return false;  // No input
    }

    // Handle based on current mode
    switch (g_tui_state.mode) {
        case TUI_MODE_MENU:
            return tui_handle_menu_input(c);

        case TUI_MODE_TABLE:
            return tui_handle_table_input(c);

        case TUI_MODE_COMMAND:
            return tui_handle_command_input(c);

        default:
            return false;
    }
}

static bool tui_handle_menu_input(int c) {
    // Number keys: select table
    if (c >= '1' && c <= '9') {
        uint8_t selection = c - '0';
        uint8_t table_count = catalog_get_table_count();

        if (selection > 0 && selection <= table_count) {
            const table_meta_t* table = catalog_get_table_by_index(selection - 1);
            if (table) {
                g_tui_state.mode = TUI_MODE_TABLE;
                g_tui_state.current_table = table;
                g_tui_state.current_field_index = 0;
                g_tui_state.cursor_pos = 0;
                g_tui_state.needs_refresh = true;
                return true;
            }
        }
    }

    // Special keys
    switch (c) {
        case 'r':
        case 'R':
            // Refresh
            g_tui_state.needs_refresh = true;
            return true;

        case ':':
            // Enter command palette
            g_tui_state.mode = TUI_MODE_COMMAND;
            g_tui_state.input_len = 0;
            memset(g_tui_state.input_buf, 0, sizeof(g_tui_state.input_buf));
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

static bool tui_handle_table_input(int c) {
    // Number keys: select field (for future field detail view)
    if (c >= '1' && c <= '9') {
        // Field selection not implemented yet
        return false;
    }

    // Special keys
    switch (c) {
        case 'q':
        case 'Q':
        case 27:  // ESC
            // Back to menu
            g_tui_state.mode = TUI_MODE_MENU;
            g_tui_state.current_table = NULL;
            g_tui_state.needs_refresh = true;
            return true;

        case 'r':
        case 'R':
            // Refresh
            g_tui_state.needs_refresh = true;
            return true;

        case ':':
            // Enter command palette
            g_tui_state.mode = TUI_MODE_COMMAND;
            g_tui_state.input_len = 0;
            memset(g_tui_state.input_buf, 0, sizeof(g_tui_state.input_buf));
            g_tui_state.needs_refresh = true;
            return true;
    }

    return false;
}

static bool tui_handle_command_input(int c) {
    switch (c) {
        case 27:  // ESC
            // Cancel command input
            g_tui_state.mode = TUI_MODE_MENU;
            g_tui_state.needs_refresh = true;
            return true;

        case '\r':
        case '\n':
            // Execute command (not implemented yet - Checkpoint 8.3)
            snprintf(g_tui_state.status_msg, sizeof(g_tui_state.status_msg),
                     "Command execution not implemented yet");
            g_tui_state.mode = TUI_MODE_MENU;
            g_tui_state.needs_refresh = true;
            return true;

        case '\b':
        case 127:  // Backspace/Delete
            if (g_tui_state.input_len > 0) {
                g_tui_state.input_len--;
                g_tui_state.input_buf[g_tui_state.input_len] = '\0';
                g_tui_state.needs_refresh = true;
            }
            return true;

        default:
            // Add character to input buffer
            if (isprint(c) && g_tui_state.input_len < sizeof(g_tui_state.input_buf) - 1) {
                g_tui_state.input_buf[g_tui_state.input_len++] = c;
                g_tui_state.input_buf[g_tui_state.input_len] = '\0';
                g_tui_state.needs_refresh = true;
                return true;
            }
            break;
    }

    return false;
}

// ============================================================================
// Rendering Functions
// ============================================================================

void tui_render_menu(void) {
    tui_clear_screen();

    // Header
    tui_print_header();

    // Table list
    printf("\n");
    printf(ANSI_BOLD "Available Tables:" ANSI_RESET "\n");
    printf("\n");

    uint8_t table_count = catalog_get_table_count();
    for (uint8_t i = 0; i < table_count; i++) {
        const table_meta_t* table = catalog_get_table_by_index(i);
        if (table) {
            printf("  " ANSI_FG_CYAN "%d" ANSI_RESET ". %s\n",
                   i + 1, table->name);
            printf("     %s\n", table->description);
        }
    }

    // Navigation hints
    printf("\n");
    tui_print_nav_hints();

    // Status bar
    tui_print_status_bar(g_tui_state.status_msg);
}

void tui_render_table(const void* table_ptr) {
    const table_meta_t* table = (const table_meta_t*)table_ptr;
    if (!table) {
        return;
    }

    tui_clear_screen();

    // Header
    tui_print_header();

    // Table name and description
    printf("\n");
    printf(ANSI_BOLD ANSI_FG_CYAN "%s" ANSI_RESET "\n", table->name);
    printf("%s\n", table->description);
    printf("\n");

    // Field list header
    printf(ANSI_BOLD "%-4s %-20s %-10s %-8s %-20s" ANSI_RESET "\n",
           "ID", "Name", "Type", "Units", "Value");
    printf("───────────────────────────────────────────────────────────────────\n");

    // Fields
    for (uint8_t i = 0; i < table->field_count; i++) {
        const field_meta_t* field = catalog_get_field(table, i);
        if (field) {
            // Format value
            char value_str[32];
            if (field->ptr) {
                catalog_format_value(field, *field->ptr, value_str, sizeof(value_str));
            } else {
                snprintf(value_str, sizeof(value_str), "N/A");
            }

            // Print field
            printf("%-4d %-20s %-10s %-8s %s%s" ANSI_RESET "\n",
                   field->id,
                   field->name,
                   field_type_name(field->type),
                   field->units,
                   field->dirty ? ANSI_FG_YELLOW : "",
                   value_str);
        }
    }

    // Navigation hints
    printf("\n");
    tui_print_nav_hints();

    // Status bar
    tui_print_status_bar(g_tui_state.status_msg);
}

void tui_render_command_palette(void) {
    tui_clear_screen();

    // Header
    tui_print_header();

    // Command prompt
    printf("\n");
    printf(ANSI_BOLD "Command Palette" ANSI_RESET "\n");
    printf("\n");
    printf(ANSI_FG_GREEN "> " ANSI_RESET "%s" ANSI_CURSOR_SHOW, g_tui_state.input_buf);

    // Hints
    printf("\n\n");
    printf(ANSI_DIM "Available commands: help, tables, describe, get, set, quit" ANSI_RESET "\n");
    printf(ANSI_DIM "ESC to cancel, ENTER to execute" ANSI_RESET "\n");

    printf(ANSI_CURSOR_HIDE);  // Hide cursor again after showing it in prompt
}

void tui_render_field_edit(const void* table, const void* field) {
    // Not implemented yet (Checkpoint 8.3)
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

    printf(ANSI_BOLD ANSI_FG_CYAN "NRWA-T6 Emulator Console" ANSI_RESET);
    printf("  |  ");
    printf("Uptime: %02lu:%02lu:%02lu",
           uptime_sec / 3600,
           (uptime_sec % 3600) / 60,
           uptime_sec % 60);
    printf("  |  ");

    // Test summary
    char summary[64];
    snprintf(summary, sizeof(summary), "Tests: %d/%d passed",
             g_test_results.total_passed, g_test_results.total_tests);
    printf("%s", summary);

    if (g_test_results.all_passed) {
        printf(" " ANSI_FG_GREEN "✓" ANSI_RESET);
    } else {
        printf(" " ANSI_FG_RED "✗" ANSI_RESET);
    }

    printf("\n");
    printf("═══════════════════════════════════════════════════════════════════\n");
}

void tui_print_status_bar(const char* message) {
    printf("\n");
    printf("───────────────────────────────────────────────────────────────────\n");
    if (message && message[0]) {
        printf(ANSI_FG_YELLOW "%s" ANSI_RESET "\n", message);
    }
}

void tui_print_nav_hints(void) {
    switch (g_tui_state.mode) {
        case TUI_MODE_MENU:
            printf(ANSI_DIM "1-9: Select table  |  R: Refresh  |  :: Command  |  Q: Quit" ANSI_RESET "\n");
            break;

        case TUI_MODE_TABLE:
            printf(ANSI_DIM "Q: Back  |  R: Refresh  |  :: Command" ANSI_RESET "\n");
            break;

        case TUI_MODE_COMMAND:
            printf(ANSI_DIM "ESC: Cancel  |  ENTER: Execute" ANSI_RESET "\n");
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
