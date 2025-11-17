/**
 * @file nsp_handler.h
 * @brief Core0 NSP Packet Handler
 *
 * Manages RS-485 receive/transmit for NSP protocol on Core0.
 * Handles SLIP framing, NSP parsing, command dispatch, and reply transmission.
 */

#ifndef NSP_HANDLER_H
#define NSP_HANDLER_H

#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// API Functions
// ============================================================================

/**
 * @brief Initialize NSP handler
 *
 * Initializes RS-485, SLIP decoder, NSP subsystem, and command dispatch.
 *
 * @param device_address Device address (0-7, from ADDR pins)
 */
void nsp_handler_init(uint8_t device_address);

/**
 * @brief Poll RS-485 for incoming NSP packets and handle them
 *
 * This function should be called periodically from the Core0 main loop.
 * It performs the following:
 * 1. Reads bytes from RS-485 RX FIFO
 * 2. Feeds bytes through SLIP decoder
 * 3. When complete frame received, parses NSP packet
 * 4. Dispatches command to handler
 * 5. Sends reply (if Poll bit set)
 *
 * Non-blocking - returns immediately if no data available.
 */
void nsp_handler_poll(void);

/**
 * @brief Get statistics
 *
 * @param rx_count Pointer to receive count
 * @param tx_count Pointer to transmit count
 * @param error_count Pointer to error count
 */
void nsp_handler_get_stats(uint32_t* rx_count, uint32_t* tx_count, uint32_t* error_count);

#endif // NSP_HANDLER_H
