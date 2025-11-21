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

/**
 * @brief Get detailed statistics for debugging
 *
 * @param rx_bytes Total bytes received
 * @param rx_packets Valid packets received
 * @param tx_packets Packets transmitted
 * @param slip_errors SLIP framing errors
 * @param nsp_errors NSP parse errors (CRC, length, etc.)
 * @param wrong_addr Packets with wrong destination address
 * @param cmd_errors Command dispatch errors
 * @param total_errors Total error count
 */
void nsp_handler_get_detailed_stats(uint32_t* rx_bytes, uint32_t* rx_packets,
                                     uint32_t* tx_packets, uint32_t* slip_errors,
                                     uint32_t* nsp_errors, uint32_t* wrong_addr,
                                     uint32_t* cmd_errors, uint32_t* total_errors);

/**
 * @brief Get last error details for debugging
 *
 * @param last_parse_err Last NSP parse error code (0=none, 1=TOO_SHORT, 2=BAD_LENGTH, 3=BAD_CRC, 4=NULL_PTR)
 * @param last_cmd_err Last unrecognized command code
 */
void nsp_handler_get_error_details(uint32_t* last_parse_err, uint32_t* last_cmd_err);

/**
 * @brief Enable or disable debug RX logging
 *
 * When enabled, prints detailed information about received bytes and packet processing.
 * Default: enabled
 *
 * @param enable true to enable debug logging, false to disable
 */
void nsp_handler_set_debug(bool enable);

/**
 * @brief Get serial layer statistics (RS-485 and SLIP)
 *
 * @param rx_bytes Total bytes received on RS-485
 * @param tx_bytes Total bytes transmitted on RS-485
 * @param slip_frames_ok Successfully decoded SLIP frames
 * @param slip_errors SLIP framing errors
 */
void nsp_handler_get_serial_stats(uint32_t* rx_bytes, uint32_t* tx_bytes,
                                   uint32_t* slip_frames_ok, uint32_t* slip_errors);

#endif // NSP_HANDLER_H
