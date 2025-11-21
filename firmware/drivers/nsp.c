/**
 * @file nsp.c
 * @brief NSP (NewSpace Protocol) Implementation
 */

#include "nsp.h"
#include "crc_ccitt.h"
#include <string.h>

// ============================================================================
// Global State
// ============================================================================

static uint8_t g_device_address = 0;  /**< Our device address (0-7) */

// ============================================================================
// Initialization
// ============================================================================

void nsp_init(uint8_t device_address) {
    g_device_address = device_address & 0x07;  // Ensure 0-7 range
}

// ============================================================================
// Packet Parsing
// ============================================================================

nsp_result_t nsp_parse(const uint8_t *raw_data, size_t raw_len, nsp_packet_t *packet) {
    // Validate inputs
    if (raw_data == NULL || packet == NULL) {
        return NSP_ERR_NULL_PTR;
    }

    // Check minimum packet size: Dest + Src + Ctrl + CRC (2 bytes) = 5 bytes
    if (raw_len < NSP_MIN_PACKET_SIZE) {
        return NSP_ERR_TOO_SHORT;
    }

    // Parse header fields
    packet->dest = raw_data[0];
    packet->src  = raw_data[1];
    packet->ctrl = raw_data[2];

    // Calculate data length from packet size
    // Packet layout: [Dest | Src | Ctrl | Data... | CRC_L | CRC_H]
    // Data length = total_len - 3 (header) - 2 (CRC)
    size_t data_len = raw_len - 5;

    // Validate data length fits in packet
    if (data_len > NSP_MAX_DATA_SIZE) {
        return NSP_ERR_BAD_LENGTH;
    }

    packet->len = (uint8_t)data_len;

    // Extract data payload (if any)
    if (packet->len > 0) {
        memcpy(packet->data, &raw_data[3], packet->len);
    }

    // Extract received CRC (LSB-first: CRC_L at [3+len], CRC_H at [3+len+1])
    size_t crc_offset = 3 + packet->len;
    uint16_t received_crc = (uint16_t)raw_data[crc_offset] |
                           ((uint16_t)raw_data[crc_offset + 1] << 8);
    packet->crc = received_crc;

    // Compute CRC over: Dest + Src + Ctrl + Data
    // (Length field is NOT part of NSP protocol per ICD Table 11-1)
    uint16_t computed_crc = crc_ccitt_calculate(raw_data, 3 + packet->len);

    // Verify CRC
    if (computed_crc != received_crc) {
        return NSP_ERR_BAD_CRC;
    }

    return NSP_OK;
}

// ============================================================================
// Packet Building
// ============================================================================

bool nsp_build_reply(const nsp_packet_t *request,
                     const uint8_t *data, size_t data_len,
                     uint8_t *output, size_t *output_len) {
    // Validate inputs
    if (request == NULL || output == NULL || output_len == NULL) {
        return false;
    }

    // Validate data length
    if (data_len > NSP_MAX_DATA_SIZE) {
        return false;
    }

    // If data_len > 0, data pointer must be valid
    if (data_len > 0 && data == NULL) {
        return false;
    }

    // Build reply packet
    size_t idx = 0;

    // Destination = original source
    output[idx++] = request->src;

    // Source = our address
    output[idx++] = g_device_address;

    // Control byte: preserve B/A bits from request, keep same command
    // Poll bit is typically cleared in replies (0 = no reply expected to a reply)
    uint8_t ctrl_b = (request->ctrl & NSP_CTRL_B_BIT);
    uint8_t ctrl_a = (request->ctrl & NSP_CTRL_A_BIT);
    uint8_t cmd = nsp_get_command(request->ctrl);
    output[idx++] = ctrl_b | ctrl_a | cmd;  // Poll=0 for replies

    // Data payload (NO Length field per ICD Table 11-1)
    if (data_len > 0) {
        memcpy(&output[idx], data, data_len);
        idx += data_len;
    }

    // Compute CRC over: Dest + Src + Ctrl + Data
    // (Length field is NOT part of NSP protocol per ICD Table 11-1)
    uint16_t crc = crc_ccitt_calculate(output, idx);

    // Append CRC (LSB-first)
    output[idx++] = (uint8_t)(crc & 0xFF);        // CRC_L
    output[idx++] = (uint8_t)((crc >> 8) & 0xFF); // CRC_H

    *output_len = idx;
    return true;
}

bool nsp_build_ack(const nsp_packet_t *request, uint8_t *output, size_t *output_len) {
    // ACK is a reply with no data payload
    return nsp_build_reply(request, NULL, 0, output, output_len);
}
