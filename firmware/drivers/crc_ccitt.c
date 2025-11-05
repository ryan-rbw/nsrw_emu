/**
 * @file crc_ccitt.c
 * @brief CRC-16 CCITT (LSB-first) Implementation
 *
 * CRITICAL: This implementation uses LSB-first bit order to match the
 * NewSpace Systems NSP protocol specification. Do not confuse with the
 * more common MSB-first variant (also called "CRC-CCITT").
 *
 * Algorithm:
 * 1. Initialize CRC to 0xFFFF
 * 2. For each byte:
 *    a. XOR byte with low byte of CRC
 *    b. For 8 bits:
 *       - If LSB is 1: shift right and XOR with 0x8408 (reversed poly)
 *       - If LSB is 0: shift right
 * 3. Return final CRC
 *
 * The polynomial 0x1021 becomes 0x8408 when reversed for LSB-first processing.
 */

#include "crc_ccitt.h"
#include <stdbool.h>

/**
 * @brief Reversed polynomial for LSB-first processing
 *
 * 0x1021 reversed = 0x8408
 * (bit reversal: 0001 0000 0010 0001 -> 1000 0100 0000 1000)
 */
#define CRC_CCITT_POLY_REVERSED 0x8408

/**
 * @brief Update CRC-16 CCITT with new data (LSB-first)
 *
 * @param crc Current CRC value
 * @param data Pointer to data buffer
 * @param len Number of bytes to process
 * @return Updated CRC value
 */
uint16_t crc_ccitt_update(uint16_t crc, const uint8_t *data, size_t len) {
    // Handle NULL pointer or zero length
    if (data == NULL || len == 0) {
        return crc;
    }

    // Process each byte
    for (size_t i = 0; i < len; i++) {
        // XOR byte into low byte of CRC
        crc ^= data[i];

        // Process 8 bits (LSB-first)
        for (int bit = 0; bit < 8; bit++) {
            if (crc & 0x0001) {
                // LSB is 1: shift right and XOR with reversed polynomial
                crc = (crc >> 1) ^ CRC_CCITT_POLY_REVERSED;
            } else {
                // LSB is 0: just shift right
                crc = crc >> 1;
            }
        }
    }

    return crc;
}

/**
 * @brief Calculate CRC-16 CCITT for a buffer (one-shot)
 *
 * @param data Pointer to data buffer
 * @param len Number of bytes to process
 * @return Calculated CRC value
 */
uint16_t crc_ccitt_calculate(const uint8_t *data, size_t len) {
    uint16_t crc = crc_ccitt_init();
    return crc_ccitt_update(crc, data, len);
}

/**
 * @brief Verify CRC-16 CCITT for a packet
 *
 * The packet should contain data followed by 2-byte CRC in LSB-first order.
 *
 * @param packet Pointer to packet buffer (data + CRC)
 * @param len Total packet length including CRC (must be >= 2)
 * @return true if CRC is valid, false otherwise
 */
bool crc_ccitt_verify(const uint8_t *packet, size_t len) {
    // Need at least 2 bytes for CRC
    if (packet == NULL || len < 2) {
        return false;
    }

    // Calculate CRC of data portion (everything except last 2 bytes)
    size_t data_len = len - 2;
    uint16_t calculated_crc = crc_ccitt_calculate(packet, data_len);

    // Extract CRC from packet (LSB-first: LSB at [len-2], MSB at [len-1])
    uint16_t packet_crc = (uint16_t)packet[len - 2] | ((uint16_t)packet[len - 1] << 8);

    return (calculated_crc == packet_crc);
}

/**
 * @brief Append CRC-16 CCITT to a buffer
 *
 * Calculates CRC for the data and appends it in LSB-first order.
 *
 * @param buffer Pointer to buffer (must have space for 2 additional bytes)
 * @param data_len Length of data already in buffer
 * @return Total length (data_len + 2)
 */
size_t crc_ccitt_append(uint8_t *buffer, size_t data_len) {
    if (buffer == NULL) {
        return 0;
    }

    // Calculate CRC
    uint16_t crc = crc_ccitt_calculate(buffer, data_len);

    // Append in LSB-first order
    buffer[data_len]     = (uint8_t)(crc & 0xFF);        // LSB
    buffer[data_len + 1] = (uint8_t)((crc >> 8) & 0xFF); // MSB

    return data_len + 2;
}
