/**
 * @file nsp_handler.c
 * @brief Core0 NSP Packet Handler Implementation
 */

#include "nsp_handler.h"
#include "drivers/rs485_uart.h"
#include "drivers/slip.h"
#include "drivers/nsp.h"
#include "device/nss_nrwa_t6_commands.h"
#include <stdio.h>
#include <string.h>

// ============================================================================
// Internal State
// ============================================================================

static slip_decoder_t slip_decoder;
static uint8_t slip_output_buf[NSP_MAX_PACKET_SIZE];
static uint8_t device_addr = 0;

// Statistics
static uint32_t rx_packet_count = 0;
static uint32_t tx_packet_count = 0;
static uint32_t error_count = 0;
static uint32_t rx_byte_count = 0;
static uint32_t slip_error_count = 0;
static uint32_t nsp_parse_error_count = 0;
static uint32_t wrong_addr_count = 0;
static uint32_t cmd_dispatch_error_count = 0;

// Debug flag (set to false to disable verbose logging after initial testing)
static bool debug_rx = true;

// ============================================================================
// Initialization
// ============================================================================

void nsp_handler_init(uint8_t device_address) {
    device_addr = device_address;

    // Initialize RS-485
    if (!rs485_init()) {
        printf("[NSP] ERROR: RS-485 init failed\n");
        return;
    }
    printf("[NSP] RS-485 initialized (460.8 kbps)\n");

    // Initialize SLIP decoder
    slip_decoder_init(&slip_decoder);

    // Initialize NSP subsystem
    nsp_init(device_addr);
    printf("[NSP] NSP handler initialized (addr=0x%02X)\n", device_addr);

    // Reset statistics
    rx_packet_count = 0;
    tx_packet_count = 0;
    error_count = 0;
    rx_byte_count = 0;
    slip_error_count = 0;
    nsp_parse_error_count = 0;
    wrong_addr_count = 0;
    cmd_dispatch_error_count = 0;
}

// ============================================================================
// Packet Handler
// ============================================================================

void nsp_handler_poll(void) {
    // Check if data available on RS-485
    size_t available = rs485_available();
    if (available == 0) {
        return;  // No data - return immediately
    }

    // Read and process bytes through SLIP decoder
    uint8_t byte;
    while (rs485_read_byte(&byte)) {
        rx_byte_count++;

        if (debug_rx) {
            printf("[RX] Byte: 0x%02X\n", byte);
        }

        size_t decoded_len;

        // Feed byte to SLIP decoder
        if (slip_decode_byte(&slip_decoder, byte, slip_output_buf, &decoded_len)) {
            // Complete SLIP frame received
            if (slip_decoder.frame_error) {
                // SLIP decode error
                slip_error_count++;
                error_count++;
                if (debug_rx) {
                    printf("[NSP] SLIP decode error (frame corrupted)\n");
                }
                slip_decoder_reset(&slip_decoder);
                continue;
            }

            if (debug_rx) {
                printf("[NSP] SLIP frame complete (%zu bytes)\n", decoded_len);
            }

            // Parse NSP packet
            nsp_packet_t packet;
            nsp_result_t parse_result = nsp_parse(slip_output_buf, decoded_len, &packet);

            if (parse_result != NSP_OK) {
                // NSP parse error (CRC, length, etc.)
                nsp_parse_error_count++;
                error_count++;
                if (debug_rx) {
                    printf("[NSP] Parse error: %d (CRC/length/format)\n", parse_result);
                }
                slip_decoder_reset(&slip_decoder);
                continue;
            }

            if (debug_rx) {
                printf("[NSP] Packet parsed: dest=0x%02X src=0x%02X ctrl=0x%02X len=%zu\n",
                       packet.dest, packet.src, packet.ctrl, packet.len);
            }

            // Check if packet is addressed to us
            if (packet.dest != device_addr && packet.dest != 0xFF) {
                // Not for us - ignore (multi-drop bus)
                wrong_addr_count++;
                if (debug_rx) {
                    printf("[NSP] Wrong address (dest=0x%02X, our_addr=0x%02X)\n",
                           packet.dest, device_addr);
                }
                slip_decoder_reset(&slip_decoder);
                continue;
            }

            rx_packet_count++;

            // Dispatch command to handler
            uint8_t command = nsp_get_command(packet.ctrl);
            cmd_result_t result;

            if (debug_rx) {
                printf("[NSP] Dispatching command: 0x%02X\n", command);
            }

            if (!commands_dispatch(command, packet.data, packet.len, &result)) {
                // Unrecognized command
                cmd_dispatch_error_count++;
                error_count++;
                if (debug_rx) {
                    printf("[NSP] Command dispatch failed: 0x%02X (unrecognized)\n", command);
                }
                slip_decoder_reset(&slip_decoder);
                continue;
            }

            if (debug_rx) {
                printf("[NSP] Command executed successfully\n");
            }

            // If Poll bit set, send reply
            if (nsp_is_poll_set(packet.ctrl)) {
                uint8_t nsp_reply[NSP_MAX_PACKET_SIZE];
                size_t nsp_reply_len;

                if (debug_rx) {
                    printf("[NSP] Poll bit set, building reply...\n");
                }

                // Build NSP reply packet
                if (!nsp_build_reply(&packet, result.data, result.data_len,
                                     nsp_reply, &nsp_reply_len)) {
                    error_count++;
                    if (debug_rx) {
                        printf("[NSP] Failed to build reply packet\n");
                    }
                    slip_decoder_reset(&slip_decoder);
                    continue;
                }

                // SLIP encode the reply
                uint8_t slip_reply[NSP_MAX_PACKET_SIZE * 2 + 2];
                size_t slip_reply_len;

                if (!slip_encode(nsp_reply, nsp_reply_len, slip_reply, &slip_reply_len)) {
                    error_count++;
                    if (debug_rx) {
                        printf("[NSP] Failed to SLIP encode reply\n");
                    }
                    slip_decoder_reset(&slip_decoder);
                    continue;
                }

                // Send SLIP-encoded reply over RS-485
                if (rs485_send(slip_reply, slip_reply_len)) {
                    tx_packet_count++;
                    if (debug_rx) {
                        printf("[NSP] Reply sent (%zu bytes)\n", slip_reply_len);
                    }
                } else {
                    error_count++;
                    if (debug_rx) {
                        printf("[NSP] Failed to send reply over RS-485\n");
                    }
                }
            }

            // Reset decoder for next frame
            slip_decoder_reset(&slip_decoder);
        }
    }
}

// ============================================================================
// Statistics
// ============================================================================

void nsp_handler_get_stats(uint32_t* rx_count, uint32_t* tx_count, uint32_t* err_count) {
    if (rx_count) *rx_count = rx_packet_count;
    if (tx_count) *tx_count = tx_packet_count;
    if (err_count) *err_count = error_count;
}

void nsp_handler_get_detailed_stats(uint32_t* rx_bytes, uint32_t* rx_packets,
                                     uint32_t* tx_packets, uint32_t* slip_errors,
                                     uint32_t* nsp_errors, uint32_t* wrong_addr,
                                     uint32_t* cmd_errors, uint32_t* total_errors) {
    if (rx_bytes) *rx_bytes = rx_byte_count;
    if (rx_packets) *rx_packets = rx_packet_count;
    if (tx_packets) *tx_packets = tx_packet_count;
    if (slip_errors) *slip_errors = slip_error_count;
    if (nsp_errors) *nsp_errors = nsp_parse_error_count;
    if (wrong_addr) *wrong_addr = wrong_addr_count;
    if (cmd_errors) *cmd_errors = cmd_dispatch_error_count;
    if (total_errors) *total_errors = error_count;
}

void nsp_handler_set_debug(bool enable) {
    debug_rx = enable;
}
