/**
 * @file slip.h
 * @brief SLIP (Serial Line Internet Protocol) Codec - RFC 1055
 *
 * Implements SLIP framing for RS-485 serial communication with the NSP protocol.
 * SLIP provides a simple framing mechanism for transmitting packets over serial lines.
 *
 * Protocol:
 * - END byte (0xC0) marks frame boundaries
 * - ESC byte (0xDB) escapes special characters
 * - 0xC0 in data becomes: 0xDB 0xDC (ESC + ESC_END)
 * - 0xDB in data becomes: 0xDB 0xDD (ESC + ESC_ESC)
 *
 * Usage:
 *   Encoding: slip_encode(data, len, output, &out_len);
 *   Decoding: slip_decode_byte(&decoder, byte, output, &out_len);
 */

#ifndef SLIP_H
#define SLIP_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// ============================================================================
// SLIP Protocol Constants
// ============================================================================

/** @brief Frame delimiter - marks start/end of SLIP frame */
#define SLIP_END     0xC0

/** @brief Escape character - indicates next byte is escaped */
#define SLIP_ESC     0xDB

/** @brief Escaped END byte - replaces END in data */
#define SLIP_ESC_END 0xDC

/** @brief Escaped ESC byte - replaces ESC in data */
#define SLIP_ESC_ESC 0xDD

// ============================================================================
// SLIP Encoder
// ============================================================================

/**
 * @brief Encode data into SLIP frame
 *
 * Encodes raw data into SLIP format with END delimiters and byte stuffing.
 * Output format: [END] [escaped_data] [END]
 *
 * @param data Pointer to input data
 * @param data_len Length of input data
 * @param output Pointer to output buffer (must be large enough)
 * @param output_len Pointer to variable receiving output length
 * @return true on success, false if data is NULL or data_len is 0
 *
 * @note Output buffer must be at least (data_len * 2 + 2) bytes to handle
 *       worst case where every byte needs escaping plus two END bytes
 */
bool slip_encode(const uint8_t *data, size_t data_len,
                 uint8_t *output, size_t *output_len);

// ============================================================================
// SLIP Decoder (Streaming)
// ============================================================================

/**
 * @brief SLIP decoder state machine states
 */
typedef enum {
    SLIP_STATE_IDLE,        /**< Waiting for frame start */
    SLIP_STATE_IN_FRAME,    /**< Receiving frame data */
    SLIP_STATE_ESCAPED      /**< Previous byte was ESC, process escape sequence */
} slip_decoder_state_t;

/**
 * @brief SLIP decoder context
 *
 * Maintains state for streaming SLIP decode. Feed bytes one at a time
 * using slip_decode_byte().
 */
typedef struct {
    slip_decoder_state_t state;  /**< Current decoder state */
    size_t frame_len;            /**< Current frame length */
    bool frame_complete;         /**< Frame is complete and valid */
    bool frame_error;            /**< Frame has an error */
} slip_decoder_t;

/**
 * @brief Initialize SLIP decoder
 *
 * @param decoder Pointer to decoder context
 */
void slip_decoder_init(slip_decoder_t *decoder);

/**
 * @brief Decode a single SLIP byte (streaming)
 *
 * Process one byte through the SLIP decoder state machine. When a complete
 * frame is detected, returns true and sets output_len to the decoded length.
 *
 * @param decoder Pointer to decoder context
 * @param byte Incoming byte
 * @param output Output buffer for decoded data
 * @param output_len Pointer to variable receiving decoded frame length
 * @return true if a complete frame was decoded, false otherwise
 *
 * @note Caller should check decoder->frame_error after receiving a frame
 * @note Output buffer must be large enough for maximum expected frame size
 */
bool slip_decode_byte(slip_decoder_t *decoder, uint8_t byte,
                      uint8_t *output, size_t *output_len);

/**
 * @brief Reset decoder to idle state
 *
 * Call this to abort current frame and reset to waiting for next frame.
 *
 * @param decoder Pointer to decoder context
 */
void slip_decoder_reset(slip_decoder_t *decoder);

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Calculate maximum encoded size for given data length
 *
 * Returns the worst-case output size needed for slip_encode() buffer.
 * Accounts for two END bytes and worst case where every byte is escaped.
 *
 * @param data_len Input data length
 * @return Maximum possible encoded length
 */
static inline size_t slip_max_encoded_size(size_t data_len) {
    return (data_len * 2) + 2;  // Worst case: all bytes escaped + 2 END bytes
}

#endif // SLIP_H
