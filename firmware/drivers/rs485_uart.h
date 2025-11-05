/**
 * @file rs485_uart.h
 * @brief RS-485 UART Driver for RP2040
 *
 * Provides half-duplex RS-485 communication with automatic DE/RE control.
 * Configured for 460.8 kbps, 8-N-1, with proper timing for transceiver switching.
 */

#ifndef RS485_UART_H
#define RS485_UART_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// ============================================================================
// Configuration
// ============================================================================

/**
 * @brief RS-485 UART instance (UART1 on RP2040)
 */
#define RS485_UART uart1

/**
 * @brief RS-485 baud rate (460.8 kbps as per NSP specification)
 */
#define RS485_BAUD_RATE 460800

/**
 * @brief UART TX pin (GPIO 4)
 */
#define RS485_TX_PIN 4

/**
 * @brief UART RX pin (GPIO 5)
 */
#define RS485_RX_PIN 5

/**
 * @brief DE/RE switching delay in microseconds
 *
 * The RS-485 transceiver needs time to switch between transmit and receive modes.
 * Typical MAX485-style transceivers need 10-30 µs.
 */
#define RS485_SWITCH_DELAY_US 10

// ============================================================================
// API Functions
// ============================================================================

/**
 * @brief Initialize RS-485 UART
 *
 * Configures UART1 for 460.8 kbps, 8-N-1, and sets up DE/RE control pins.
 * After init, the transceiver is in receive mode.
 *
 * @return true if initialization succeeded, false otherwise
 */
bool rs485_init(void);

/**
 * @brief Send data over RS-485
 *
 * Switches to transmit mode, sends data, waits for transmission to complete,
 * then switches back to receive mode with proper timing.
 *
 * This function blocks until all data is transmitted.
 *
 * @param data Pointer to data buffer to send
 * @param len Number of bytes to send
 * @return true if transmission succeeded, false if data is NULL or len is 0
 */
bool rs485_send(const uint8_t *data, size_t len);

/**
 * @brief Check if data is available to read
 *
 * @return Number of bytes available in RX FIFO
 */
size_t rs485_available(void);

/**
 * @brief Read a single byte from RS-485
 *
 * Non-blocking. Returns immediately if no data available.
 *
 * @param byte Pointer to store received byte
 * @return true if a byte was read, false if no data available
 */
bool rs485_read_byte(uint8_t *byte);

/**
 * @brief Read multiple bytes from RS-485
 *
 * Reads up to len bytes from RX FIFO. Returns actual number of bytes read.
 *
 * @param buffer Buffer to store received bytes
 * @param len Maximum number of bytes to read
 * @return Number of bytes actually read (may be less than len)
 */
size_t rs485_read(uint8_t *buffer, size_t len);

/**
 * @brief Flush TX FIFO and wait for transmission to complete
 *
 * Blocks until all pending TX data has been transmitted.
 */
void rs485_flush_tx(void);

/**
 * @brief Clear RX FIFO
 *
 * Discards all unread data in the receive buffer.
 */
void rs485_clear_rx(void);

/**
 * @brief Get UART error flags
 *
 * Returns bit flags for UART errors:
 * - Bit 0: Overrun error (OE) - RX FIFO overflow
 * - Bit 1: Break error (BE) - Break condition detected
 * - Bit 2: Parity error (PE) - Parity mismatch
 * - Bit 3: Framing error (FE) - Invalid stop bit
 *
 * @return Error flags (0 = no errors)
 */
uint8_t rs485_get_errors(void);

/**
 * @brief Clear UART error flags
 */
void rs485_clear_errors(void);

#endif // RS485_UART_H
