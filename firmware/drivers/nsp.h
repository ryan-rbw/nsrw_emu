/**
 * @file nsp.h
 * @brief NSP (NewSpace Protocol) Implementation
 *
 * NSP packet structure per ICD Table 11-1 (after SLIP decoding):
 * [Dest | Src | Ctrl | Data... | CRC_L | CRC_H]
 *
 * Note: There is NO Length field. Packet boundaries are determined by SLIP framing.
 * Data length is calculated as: packet_size - 3 (header) - 2 (CRC)
 *
 * Message Control byte (Ctrl):
 * [Poll:1 | B:1 | A:1 | Command:5]
 * - Poll: 1 = reply expected, 0 = no reply
 * - B, A: ACK bits (preserved in reply)
 * - Command: 5-bit command code (0x00-0x1F)
 *
 * CRC: CCITT CRC-16 (init 0xFFFF, LSB-first) covers [Dest | Src | Ctrl | Data...]
 * Transmitted as [CRC_L, CRC_H] (little-endian)
 */

#ifndef NSP_H
#define NSP_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// ============================================================================
// NSP Protocol Constants
// ============================================================================

/** @brief Minimum NSP packet size: Dest + Src + Ctrl + CRC (2 bytes) */
#define NSP_MIN_PACKET_SIZE 5

/** @brief Maximum NSP data payload size */
#define NSP_MAX_DATA_SIZE 255

/** @brief Maximum NSP packet size (including CRC) */
#define NSP_MAX_PACKET_SIZE (3 + NSP_MAX_DATA_SIZE + 2)  // Header (Dest+Src+Ctrl) + Data + CRC

// ============================================================================
// NSP Command Codes
// ============================================================================

#define NSP_CMD_PING                    0x00  /**< Ping (connectivity test) */
#define NSP_CMD_PEEK                    0x02  /**< Read memory/registers */
#define NSP_CMD_POKE                    0x03  /**< Write memory/registers */
#define NSP_CMD_APPLICATION_TELEMETRY   0x07  /**< Request telemetry block */
#define NSP_CMD_APPLICATION_COMMAND     0x08  /**< Send application command */
#define NSP_CMD_CLEAR_FAULT             0x09  /**< Clear latched faults */
#define NSP_CMD_CONFIGURE_PROTECTION    0x0A  /**< Configure protection thresholds */
#define NSP_CMD_TRIP_LCL                0x0B  /**< Trip local current limit */

// ============================================================================
// Control Byte Bit Masks
// ============================================================================

#define NSP_CTRL_POLL_BIT    0x80  /**< Poll bit (1 = reply expected) */
#define NSP_CTRL_B_BIT       0x40  /**< B bit (ACK semantics) */
#define NSP_CTRL_A_BIT       0x20  /**< A bit (ACK semantics) */
#define NSP_CTRL_CMD_MASK    0x1F  /**< Command code mask (5 bits) */

// ============================================================================
// NSP Packet Structure
// ============================================================================

/**
 * @brief NSP packet structure (parsed representation)
 */
typedef struct {
    uint8_t dest;         /**< Destination address (0-7) */
    uint8_t src;          /**< Source address (0-7) */
    uint8_t ctrl;         /**< Control byte [Poll|B|A|Cmd] */
    uint8_t len;          /**< Data length (0-255) */
    uint8_t data[NSP_MAX_DATA_SIZE];  /**< Data payload */
    uint16_t crc;         /**< CRC-16 CCITT (computed or received) */
} nsp_packet_t;

/**
 * @brief NSP parse result codes
 */
typedef enum {
    NSP_OK = 0,           /**< Packet parsed successfully */
    NSP_ERR_TOO_SHORT,    /**< Packet too short */
    NSP_ERR_BAD_LENGTH,   /**< Length field inconsistent */
    NSP_ERR_BAD_CRC,      /**< CRC mismatch */
    NSP_ERR_NULL_PTR      /**< NULL pointer argument */
} nsp_result_t;

// ============================================================================
// NSP API Functions
// ============================================================================

/**
 * @brief Initialize NSP subsystem
 *
 * @param device_address Our device address (0-7)
 */
void nsp_init(uint8_t device_address);

/**
 * @brief Parse NSP packet from raw bytes (SLIP-decoded)
 *
 * @param raw_data Pointer to raw packet data (after SLIP decode)
 * @param raw_len Length of raw packet data
 * @param packet Pointer to nsp_packet_t structure to fill
 * @return nsp_result_t Parse result
 */
nsp_result_t nsp_parse(const uint8_t *raw_data, size_t raw_len, nsp_packet_t *packet);

/**
 * @brief Build NSP reply packet
 *
 * Creates a reply packet with appropriate ACK/NACK bit set, preserves B bit
 * from request, and optionally adds data payload. CRC is automatically computed and appended.
 *
 * @param request Pointer to original request packet
 * @param ack true for ACK (A=1), false for NACK (A=0)
 * @param data Pointer to reply data (can be NULL if data_len = 0)
 * @param data_len Length of reply data
 * @param output Pointer to output buffer
 * @param output_len Pointer to variable receiving output length
 * @return true on success, false on error
 */
bool nsp_build_reply(const nsp_packet_t *request,
                     bool ack,
                     const uint8_t *data, size_t data_len,
                     uint8_t *output, size_t *output_len);

/**
 * @brief Build NSP ACK reply (PING response)
 *
 * Creates an ACK reply with no data payload.
 *
 * @param request Pointer to original request packet
 * @param output Pointer to output buffer
 * @param output_len Pointer to variable receiving output length
 * @return true on success, false on error
 */
bool nsp_build_ack(const nsp_packet_t *request, uint8_t *output, size_t *output_len);

/**
 * @brief Get command code from control byte
 *
 * @param ctrl Control byte
 * @return Command code (0x00-0x1F)
 */
static inline uint8_t nsp_get_command(uint8_t ctrl) {
    return ctrl & NSP_CTRL_CMD_MASK;
}

/**
 * @brief Check if Poll bit is set
 *
 * @param ctrl Control byte
 * @return true if Poll=1 (reply expected), false otherwise
 */
static inline bool nsp_is_poll_set(uint8_t ctrl) {
    return (ctrl & NSP_CTRL_POLL_BIT) != 0;
}

/**
 * @brief Build control byte from components
 *
 * @param poll Poll bit (1 = reply expected)
 * @param b B bit
 * @param a A bit
 * @param command Command code (0x00-0x1F)
 * @return Control byte
 */
static inline uint8_t nsp_make_ctrl(bool poll, bool b, bool a, uint8_t command) {
    uint8_t ctrl = command & NSP_CTRL_CMD_MASK;
    if (poll) ctrl |= NSP_CTRL_POLL_BIT;
    if (b)    ctrl |= NSP_CTRL_B_BIT;
    if (a)    ctrl |= NSP_CTRL_A_BIT;
    return ctrl;
}

#endif // NSP_H
