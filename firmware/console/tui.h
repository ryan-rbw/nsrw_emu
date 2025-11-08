/**
 * @file tui.h
 * @brief Text User Interface for NRWA-T6 Emulator Console
 *
 * Provides a live, non-scrolling TUI using ANSI escape sequences.
 * Unlike checkpoint tests which scroll, this interface updates in place
 * like top/htop for a static, refreshing view.
 */

#ifndef TUI_H
#define TUI_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>  // For size_t
#include <stdio.h>   // For printf in inline functions
#include "tables.h"  // For field_type_t

// ============================================================================
// ANSI Escape Sequences (VT100 compatible)
// ============================================================================

// Cursor control
#define ANSI_CLEAR_SCREEN       "\033[2J"
#define ANSI_CURSOR_HOME        "\033[H"
#define ANSI_CURSOR_HIDE        "\033[?25l"
#define ANSI_CURSOR_SHOW        "\033[?25h"
#define ANSI_CURSOR_UP(n)       "\033[" #n "A"
#define ANSI_CURSOR_DOWN(n)     "\033[" #n "B"
#define ANSI_CURSOR_POS(row,col) "\033[" #row ";" #col "H"

// Text attributes
#define ANSI_RESET              "\033[0m"
#define ANSI_BOLD               "\033[1m"
#define ANSI_DIM                "\033[2m"
#define ANSI_UNDERLINE          "\033[4m"
#define ANSI_BLINK              "\033[5m"
#define ANSI_REVERSE            "\033[7m"

// Foreground colors
#define ANSI_FG_BLACK           "\033[30m"
#define ANSI_FG_RED             "\033[31m"
#define ANSI_FG_GREEN           "\033[32m"
#define ANSI_FG_YELLOW          "\033[33m"
#define ANSI_FG_BLUE            "\033[34m"
#define ANSI_FG_MAGENTA         "\033[35m"
#define ANSI_FG_CYAN            "\033[36m"
#define ANSI_FG_WHITE           "\033[37m"

// Background colors
#define ANSI_BG_BLACK           "\033[40m"
#define ANSI_BG_RED             "\033[41m"
#define ANSI_BG_GREEN           "\033[42m"
#define ANSI_BG_YELLOW          "\033[43m"
#define ANSI_BG_BLUE            "\033[44m"
#define ANSI_BG_MAGENTA         "\033[45m"
#define ANSI_BG_CYAN            "\033[46m"
#define ANSI_BG_WHITE           "\033[47m"

// Line operations
#define ANSI_CLEAR_LINE         "\033[2K"
#define ANSI_CLEAR_TO_EOL       "\033[K"

// ============================================================================
// TUI State & Navigation
// ============================================================================

/**
 * @brief TUI display mode
 */
typedef enum {
    TUI_MODE_BROWSE,        // Browse tables/fields with arrow keys
    TUI_MODE_EDIT,          // Edit field value (press Enter on field)
} tui_mode_t;

/**
 * @brief TUI state structure
 */
typedef struct {
    tui_mode_t mode;                // Current display mode
    uint8_t selected_table_idx;     // Currently selected table (0-N)
    uint8_t selected_field_idx;     // Currently selected field within table (0 if table collapsed)
    bool table_expanded[16];        // Expansion state for each table (max 16 tables)
    bool needs_refresh;             // Redraw screen on next update
    char input_buf[128];            // Input buffer for commands/editing
    uint8_t input_len;              // Current input length
    char status_msg[80];            // Status bar message
} tui_state_t;

// ============================================================================
// TUI API
// ============================================================================

/**
 * @brief Initialize TUI system
 *
 * Sets up USB-CDC stdio, clears screen, hides cursor, displays main menu.
 */
void tui_init(void);

/**
 * @brief Update TUI display (call from main loop)
 *
 * Refreshes the current view without scrolling. Uses ANSI cursor positioning
 * to update values in place.
 *
 * @param force_redraw If true, clear and redraw entire screen
 */
void tui_update(bool force_redraw);

/**
 * @brief Handle keyboard input (non-blocking)
 *
 * Processes user input: numbers for navigation, q=back, r=refresh, e=edit.
 * Updates tui_state and sets needs_refresh flag.
 *
 * @return true if input was processed, false if no input available
 */
bool tui_handle_input(void);

/**
 * @brief Shutdown TUI (restore terminal state)
 *
 * Shows cursor, resets colors, clears screen.
 */
void tui_shutdown(void);

// ============================================================================
// Screen Rendering Functions
// ============================================================================

/**
 * @brief Render browse mode (table list with expand/collapse)
 *
 * **NEW DESIGN**: Single unified view with arrow navigation.
 *
 * Layout:
 * ┌─ Header: Title | Uptime | Tests ────────────────┐
 * ├─ Status: ON/OFF | Mode | RPM | Current | Fault ─┤
 * ├───────────────────────────────────────────────────┤
 * │  TABLES                                           │
 * │  ┌────────────────────────────────────────────┐  │
 * │  │ > 1. Built-In Tests     [COLLAPSED]       │  │
 * │  │   2. Serial Interface   [EXPANDED]        │  │
 * │  │      ├─ baud_rate  : 460800  (RO)         │  │
 * │  │      ├─ parity     : NONE    (RO)         │  │
 * │  └────────────────────────────────────────────┘  │
 * ├───────────────────────────────────────────────────┤
 * │ ↑↓: Navigate │ →: Expand │ ←: Collapse │ Q: Quit│
 * └───────────────────────────────────────────────────┘
 *
 * Navigation:
 * - ↑/↓: Move cursor between tables/fields
 * - →: Expand selected table
 * - ←: Collapse expanded table
 * - Enter: Edit selected field
 * - Q: Quit
 */
void tui_render_browse(void);

/**
 * @brief Render field edit dialog
 *
 * Displays:
 * - Field name and current value
 * - Input prompt for new value
 * - Type/range constraints
 * - ESC=cancel, ENTER=save
 *
 * @param table Pointer to table metadata
 * @param field Pointer to field metadata
 */
void tui_render_field_edit(const void* table, const void* field);

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Clear screen and position cursor at home
 */
static inline void tui_clear_screen(void) {
    printf(ANSI_CLEAR_SCREEN ANSI_CURSOR_HOME);
}

/**
 * @brief Position cursor at specific row/column (1-based)
 */
void tui_cursor_pos(uint8_t row, uint8_t col);

/**
 * @brief Print header with firmware version and uptime
 *
 * Format: NRWA-T6 Emulator | Uptime: HH:MM:SS | Tests: N/M ✓
 */
void tui_print_header(void);

/**
 * @brief Print status banner showing wheel state
 *
 * Format: Status: ON | Mode: SPEED | RPM: 3245 | Current: 1.25A | Fault: -
 */
void tui_print_status_banner(void);

/**
 * @brief Print status bar at bottom of screen
 */
void tui_print_status_bar(const char* message);

/**
 * @brief Print navigation hints based on current mode
 */
void tui_print_nav_hints(void);

/**
 * @brief Format and print a field value with units
 *
 * @param field_id Field identifier
 * @param value Raw value (fixed-point or integer)
 * @param buf Output buffer
 * @param buflen Buffer size
 */
void tui_format_field_value(uint16_t field_id, uint32_t value, char* buf, size_t buflen);

/**
 * @brief Get field type name as string
 *
 * @param type Field type enum
 * @return Type name string (e.g., "BOOL", "U32", "Q14.18")
 */
const char* field_type_name(field_type_t type);

#endif // TUI_H
