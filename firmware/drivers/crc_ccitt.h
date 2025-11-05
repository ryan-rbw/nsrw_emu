/**
 * @file crc_ccitt.h
 * @brief CRC-16 CCITT (LSB-first) Implementation
 *
 * This module implements the CCITT CRC-16 algorithm as specified in the
 * NewSpace Systems NSP protocol. Critical parameters:
 *
 * - Polynomial: 0x1021 (x^16 + x^12 + x^5 + 1)
 * - Initial value: 0xFFFF
 * - Bit order: LSB-first (critical for NSP compatibility!)
 * - Final XOR: None
 *
 * Usage:
 *   // One-shot calculation
 *   uint16_t crc = crc_ccitt_calculate(data, len);
 *
 *   // Incremental calculation
 *   uint16_t crc = crc_ccitt_init();
 *   crc = crc_ccitt_update(crc, chunk1, len1);
 *   crc = crc_ccitt_update(crc, chunk2, len2);
 */

#ifndef CRC_CCITT_H
#define CRC_CCITT_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/**
 * @brief CRC-16 CCITT polynomial (0x1021)
 */
#define CRC_CCITT_POLY 0x1021

/**
 * @brief CRC-16 CCITT initial value (0xFFFF)
 */
#define CRC_CCITT_INIT 0xFFFF

/**
 * @brief Initialize CRC-16 CCITT
 *
 * @return Initial CRC value (0xFFFF)
 */
static inline uint16_t crc_ccitt_init(void) {
    return CRC_CCITT_INIT;
}

/**
 * @brief Update CRC-16 CCITT with new data
 *
 * This function can be called multiple times to compute the CRC
 * incrementally. Pass the previous CRC value as the first argument.
 *
 * @param crc Current CRC value (use crc_ccitt_init() for first call)
 * @param data Pointer to data buffer
 * @param len Number of bytes to process
 * @return Updated CRC value
 */
uint16_t crc_ccitt_update(uint16_t crc, const uint8_t *data, size_t len);

/**
 * @brief Calculate CRC-16 CCITT for a buffer (one-shot)
 *
 * Convenience function that initializes and calculates CRC in one call.
 *
 * @param data Pointer to data buffer (can be NULL if len is 0)
 * @param len Number of bytes to process
 * @return Calculated CRC value
 */
uint16_t crc_ccitt_calculate(const uint8_t *data, size_t len);

/**
 * @brief Verify CRC-16 CCITT for a packet
 *
 * Checks if the last 2 bytes of the packet match the CRC of the preceding data.
 * Assumes CRC is stored in LSB-first order (LSB at packet[len-2], MSB at packet[len-1]).
 *
 * @param packet Pointer to packet buffer (data + CRC)
 * @param len Total packet length including CRC (must be >= 2)
 * @return true if CRC is valid, false otherwise
 */
bool crc_ccitt_verify(const uint8_t *packet, size_t len);

/**
 * @brief Append CRC-16 CCITT to a buffer
 *
 * Calculates CRC for the data and appends it to the buffer in LSB-first order.
 *
 * @param buffer Pointer to buffer (must have space for 2 additional bytes)
 * @param data_len Length of data (CRC will be written at buffer[data_len] and buffer[data_len+1])
 * @return Total length (data_len + 2)
 */
size_t crc_ccitt_append(uint8_t *buffer, size_t data_len);

#endif // CRC_CCITT_H
