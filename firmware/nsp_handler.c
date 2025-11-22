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
static uint32_t tx_byte_count = 0;
static uint32_t slip_frames_ok_count = 0;
static uint32_t slip_error_count = 0;
static uint32_t nsp_parse_error_count = 0;
static uint32_t wrong_addr_count = 0;
static uint32_t cmd_dispatch_error_count = 0;

// Last error details (for debugging)
static uint32_t last_parse_error_code = 0;     // Last NSP parse error code (0=none, 1-4=error)
static uint32_t last_cmd_error_code = 0;       // Last unrecognized command code
static uint8_t last_frame_bytes[32] = {0};     // Last received frame (first 32 bytes)
static uint32_t last_frame_len = 0;            // Length of last frame
static uint8_t last_rx_cmd_bytes[16] = {0};    // Last successfully parsed command (NSP packet)
static uint32_t last_rx_cmd_len = 0;           // Length of last RX command

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
    tx_byte_count = 0;
    slip_frames_ok_count = 0;
    slip_error_count = 0;
    nsp_parse_error_count = 0;
    wrong_addr_count = 0;
    cmd_dispatch_error_count = 0;
    last_parse_error_code = 0;
    last_cmd_error_code = 0;
    last_frame_len = 0;
    last_rx_cmd_len = 0;
    memset(last_frame_bytes, 0, sizeof(last_frame_bytes));
    memset(last_rx_cmd_bytes, 0, sizeof(last_rx_cmd_bytes));
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

            // Valid SLIP frame decoded
            slip_frames_ok_count++;

            // Save last frame for debugging (first 32 bytes)
            last_frame_len = decoded_len;
            size_t copy_len = decoded_len < sizeof(last_frame_bytes) ? decoded_len : sizeof(last_frame_bytes);
            memcpy(last_frame_bytes, slip_output_buf, copy_len);

            if (debug_rx) {
                printf("[NSP] SLIP frame complete (%zu bytes)\n", decoded_len);

                // Hex dump of received frame
                printf("[NSP] Frame hex dump: ");
                for (size_t i = 0; i < decoded_len; i++) {
                    printf("%02X ", slip_output_buf[i]);
                    if ((i + 1) % 16 == 0 && i + 1 < decoded_len) {
                        printf("\n[NSP]                  ");
                    }
                }
                printf("\n");
            }

            // Parse NSP packet
            nsp_packet_t packet;
            nsp_result_t parse_result = nsp_parse(slip_output_buf, decoded_len, &packet);

            if (parse_result != NSP_OK) {
                // NSP parse error (CRC, length, etc.)
                nsp_parse_error_count++;
                error_count++;
                last_parse_error_code = (uint32_t)parse_result;  // Save error code for debugging
                if (debug_rx) {
                    printf("[NSP] Parse error: %d ", parse_result);
                    switch (parse_result) {
                        case 1: printf("(TOO_SHORT: frame < 6 bytes, got %zu)\n", decoded_len); break;
                        case 2: printf("(BAD_LENGTH: len field mismatch)\n"); break;
                        case 3: printf("(BAD_CRC: CRC validation failed)\n"); break;
                        case 4: printf("(NULL_PTR: null pointer)\n"); break;
                        default: printf("(UNKNOWN)\n"); break;
                    }
                }
                slip_decoder_reset(&slip_decoder);
                continue;
            }

            if (debug_rx) {
                printf("[NSP] Packet parsed: dest=0x%02X src=0x%02X ctrl=0x%02X len=%zu\n",
                       packet.dest, packet.src, packet.ctrl, packet.len);
            }

            // Save last successfully parsed command (raw NSP packet, up to 16 bytes)
            last_rx_cmd_len = decoded_len < sizeof(last_rx_cmd_bytes) ? decoded_len : sizeof(last_rx_cmd_bytes);
            memcpy(last_rx_cmd_bytes, slip_output_buf, last_rx_cmd_len);

            // Print last RX command as comma-separated hex
            if (debug_rx && last_rx_cmd_len > 0) {
                printf("[NSP] Last RX cmd: ");
                for (uint32_t i = 0; i < last_rx_cmd_len; i++) {
                    printf("%s%02X", (i == 0) ? "" : ",", last_rx_cmd_bytes[i]);
                }
                printf("\n");
            }

            // Reset error fields on successful parse (don't show stale errors)
            last_parse_error_code = 0;
            last_cmd_error_code = 0;
            last_frame_len = 0;

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
                last_cmd_error_code = (uint32_t)command;  // Save unrecognized command code
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

                // Build NSP reply packet (pass ACK/NACK status from command result)
                bool ack = (result.status == CMD_ACK);
                if (!nsp_build_reply(&packet, ack, result.data, result.data_len,
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
                    tx_byte_count += slip_reply_len;  // Track TX bytes
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

void nsp_handler_get_error_details(uint32_t* last_parse_err, uint32_t* last_cmd_err) {
    if (last_parse_err) *last_parse_err = last_parse_error_code;
    if (last_cmd_err) *last_cmd_err = last_cmd_error_code;
}

void nsp_handler_get_last_frame(uint8_t* frame_buf, size_t buf_size, uint32_t* frame_len) {
    if (frame_len) *frame_len = last_frame_len;
    if (frame_buf && buf_size > 0) {
        size_t copy_len = last_frame_len < buf_size ? last_frame_len : buf_size;
        memcpy(frame_buf, last_frame_bytes, copy_len);
    }
}

void nsp_handler_get_last_rx_cmd(uint8_t* cmd_buf, size_t buf_size, uint32_t* cmd_len) {
    if (cmd_len) *cmd_len = last_rx_cmd_len;
    if (cmd_buf && buf_size > 0) {
        size_t copy_len = last_rx_cmd_len < buf_size ? last_rx_cmd_len : buf_size;
        memcpy(cmd_buf, last_rx_cmd_bytes, copy_len);
    }
}

void nsp_handler_set_debug(bool enable) {
    debug_rx = enable;
}

void nsp_handler_get_serial_stats(uint32_t* rx_bytes, uint32_t* tx_bytes,
                                   uint32_t* slip_frames_ok, uint32_t* slip_errors) {
    if (rx_bytes) *rx_bytes = rx_byte_count;
    if (tx_bytes) *tx_bytes = tx_byte_count;
    if (slip_frames_ok) *slip_frames_ok = slip_frames_ok_count;
    if (slip_errors) *slip_errors = slip_error_count;
}
