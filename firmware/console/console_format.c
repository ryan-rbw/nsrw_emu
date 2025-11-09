/**
 * @file console_format.c
 * @brief Console Formatting Utilities Implementation
 */

#include "console_format.h"
#include <stdio.h>
#include <string.h>

void console_print_line(char ch) {
    for (uint8_t i = 0; i < CONSOLE_WIDTH; i++) {
        putchar(ch);
    }
    putchar('\n');
}

void console_print_centered(const char* str) {
    uint8_t len = strlen(str);
    uint8_t padding = console_center_padding(len);

    // Print left padding
    for (uint8_t i = 0; i < padding; i++) {
        putchar(' ');
    }

    // Print string
    printf("%s", str);

    // Print right padding (fill to console width)
    uint8_t remaining = CONSOLE_WIDTH - padding - len;
    for (uint8_t i = 0; i < remaining; i++) {
        putchar(' ');
    }

    putchar('\n');
}

void console_print_box_line(const char* content) {
    uint8_t content_len = strlen(content);
    uint8_t content_width = CONSOLE_WIDTH - 4;  // 2 for borders, 2 for spaces

    printf("| ");

    // Print content
    printf("%s", content);

    // Pad remaining space
    uint8_t padding = (content_len < content_width) ? (content_width - content_len) : 0;
    for (uint8_t i = 0; i < padding; i++) {
        putchar(' ');
    }

    printf(" |\n");
}

void console_print_box_top(void) {
    putchar('+');
    for (uint8_t i = 0; i < CONSOLE_WIDTH - 2; i++) {
        putchar('-');
    }
    putchar('+');
    putchar('\n');
}

void console_print_box_bottom(void) {
    putchar('+');
    for (uint8_t i = 0; i < CONSOLE_WIDTH - 2; i++) {
        putchar('-');
    }
    putchar('+');
    putchar('\n');
}
