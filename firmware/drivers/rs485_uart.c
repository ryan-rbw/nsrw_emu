/**
 * @file rs485_uart.c
 * @brief RS-485 UART Driver Implementation
 */

#include "rs485_uart.h"
#include "hardware/uart.h"
#include "hardware/gpio.h"
#include "pico/time.h"
#include "../platform/gpio_map.h"
#include <string.h>

// ============================================================================
// Initialization
// ============================================================================

bool rs485_init(void) {
    // Initialize UART1 at 460.8 kbps, 8-N-1
    uint actual_baud = uart_init(RS485_UART, RS485_BAUD_RATE);

    // Verify baud rate is within acceptable range (±1%)
    uint baud_error = (actual_baud > RS485_BAUD_RATE) ?
                      (actual_baud - RS485_BAUD_RATE) :
                      (RS485_BAUD_RATE - actual_baud);
    uint baud_tolerance = RS485_BAUD_RATE / 100;  // 1%

    if (baud_error > baud_tolerance) {
        return false;  // Baud rate out of tolerance
    }

    // Set data format: 8 bits, no parity, 1 stop bit
    uart_set_format(RS485_UART, 8, 1, UART_PARITY_NONE);

    // Disable hardware flow control
    uart_set_hw_flow(RS485_UART, false, false);

    // Enable UART FIFOs
    uart_set_fifo_enabled(RS485_UART, true);

    // Configure GPIO pins for UART function
    gpio_set_function(RS485_TX_PIN, GPIO_FUNC_UART);
    gpio_set_function(RS485_RX_PIN, GPIO_FUNC_UART);

    // Set RS-485 transceiver to receive mode (default state)
    gpio_rs485_rx_enable();

    return true;
}

// ============================================================================
// Transmit
// ============================================================================

bool rs485_send(const uint8_t *data, size_t len) {
    if (data == NULL || len == 0) {
        return false;
    }

    // Switch to transmit mode
    gpio_rs485_tx_enable();

    // Wait for transceiver to switch to TX mode
    sleep_us(RS485_SWITCH_DELAY_US);

    // Send all bytes
    uart_write_blocking(RS485_UART, data, len);

    // Wait for TX FIFO to drain and transmission to complete
    uart_tx_wait_blocking(RS485_UART);

    // Additional delay to ensure last bit has fully transmitted
    // At 460.8 kbps, 1 byte = ~21.7 µs
    // Add a small safety margin
    sleep_us(RS485_SWITCH_DELAY_US);

    // Switch back to receive mode
    gpio_rs485_rx_enable();

    return true;
}

void rs485_flush_tx(void) {
    uart_tx_wait_blocking(RS485_UART);
}

// ============================================================================
// Receive
// ============================================================================

size_t rs485_available(void) {
    return uart_is_readable(RS485_UART) ? 1 : 0;
}

bool rs485_read_byte(uint8_t *byte) {
    if (byte == NULL) {
        return false;
    }

    if (uart_is_readable(RS485_UART)) {
        *byte = uart_getc(RS485_UART);
        return true;
    }

    return false;
}

size_t rs485_read(uint8_t *buffer, size_t len) {
    if (buffer == NULL || len == 0) {
        return 0;
    }

    size_t count = 0;
    while (count < len && uart_is_readable(RS485_UART)) {
        buffer[count++] = uart_getc(RS485_UART);
    }

    return count;
}

void rs485_clear_rx(void) {
    // Read and discard all bytes in RX FIFO
    while (uart_is_readable(RS485_UART)) {
        uart_getc(RS485_UART);
    }
}

// ============================================================================
// Error Handling
// ============================================================================

uint8_t rs485_get_errors(void) {
    // Read UART error status register
    // This implementation is simplified - the Pico SDK doesn't expose
    // direct error flag access, so we return 0 for now
    // TODO: Access hardware registers directly if detailed error reporting needed
    return 0;
}

void rs485_clear_errors(void) {
    // Clear any pending errors by reading data register
    // On RP2040, reading UARTDR clears error flags
    rs485_clear_rx();
}
