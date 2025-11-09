/**
 * @file console_config.h
 * @brief Console Configuration Constants
 *
 * Defines console dimensions and formatting parameters.
 * All TUI elements (logo, banner, tables) must fit within these constraints.
 */

#ifndef CONSOLE_CONFIG_H
#define CONSOLE_CONFIG_H

// ============================================================================
// Console Dimensions
// ============================================================================

/**
 * @brief Default console width in characters
 *
 * All text, logos, banners, and UI elements must fit within this width.
 * Standard terminal width is 80 columns.
 *
 * Can be adjusted for different terminal sizes:
 * - 80:  Standard terminal (VT100 compatible)
 * - 100: Wide terminal
 * - 120: Extra wide terminal
 */
#define CONSOLE_WIDTH 80

/**
 * @brief Console height in lines (informational)
 *
 * The TUI uses a non-scrolling interface, so height is flexible.
 * This value is provided for reference only.
 */
#define CONSOLE_HEIGHT 24

// ============================================================================
// Formatting Constants
// ============================================================================

/**
 * @brief Left margin for table content (spaces)
 */
#define TABLE_INDENT 2

/**
 * @brief Field name column width
 */
#define FIELD_NAME_WIDTH 24

/**
 * @brief Field value column width
 */
#define FIELD_VALUE_WIDTH 12

#endif // CONSOLE_CONFIG_H
