/**
 * @file console_format.h
 * @brief Console Formatting Utilities
 *
 * Helper functions for formatting text to fit console width.
 */

#ifndef CONSOLE_FORMAT_H
#define CONSOLE_FORMAT_H

#include "console_config.h"
#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Print a horizontal line across console width
 *
 * @param ch Character to use for line (e.g., '-', '=', '#')
 */
void console_print_line(char ch);

/**
 * @brief Print a centered string within console width
 *
 * @param str String to center
 */
void console_print_centered(const char* str);

/**
 * @brief Print a bordered box line with left and right borders
 *
 * Format: "| content... |" (padded to console width)
 *
 * @param content Content string (can be empty for blank line)
 */
void console_print_box_line(const char* content);

/**
 * @brief Print top border of box
 */
void console_print_box_top(void);

/**
 * @brief Print bottom border of box
 */
void console_print_box_bottom(void);

/**
 * @brief Calculate padding needed to center text
 *
 * @param text_len Length of text to center
 * @return Number of spaces needed on left side
 */
static inline uint8_t console_center_padding(uint8_t text_len) {
    if (text_len >= CONSOLE_WIDTH) {
        return 0;
    }
    return (CONSOLE_WIDTH - text_len) / 2;
}

#endif // CONSOLE_FORMAT_H
