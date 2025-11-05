/**
 * @file slip.c
 * @brief SLIP (Serial Line Internet Protocol) Codec Implementation
 *
 * Reference: RFC 1055 - A Nonstandard for Transmission of IP Datagrams over Serial Lines
 */

#include "slip.h"
#include <string.h>

// ============================================================================
// SLIP Encoder
// ============================================================================

bool slip_encode(const uint8_t *data, size_t data_len,
                 uint8_t *output, size_t *output_len) {
    // Validate inputs
    if (output == NULL || output_len == NULL) {
        return false;
    }

    // Handle empty data (valid - produces just two END bytes)
    // Note: data can be NULL if data_len is 0
    if (data_len == 0) {
        output[0] = SLIP_END;
        output[1] = SLIP_END;
        *output_len = 2;
        return true;
    }

    // For non-empty data, validate data pointer
    if (data == NULL) {
        return false;
    }

    size_t out_idx = 0;

    // Start frame with END byte
    output[out_idx++] = SLIP_END;

    // Encode data with byte stuffing
    for (size_t i = 0; i < data_len; i++) {
        uint8_t byte = data[i];

        if (byte == SLIP_END) {
            // Replace END with ESC + ESC_END
            output[out_idx++] = SLIP_ESC;
            output[out_idx++] = SLIP_ESC_END;
        } else if (byte == SLIP_ESC) {
            // Replace ESC with ESC + ESC_ESC
            output[out_idx++] = SLIP_ESC;
            output[out_idx++] = SLIP_ESC_ESC;
        } else {
            // Regular byte - copy as-is
            output[out_idx++] = byte;
        }
    }

    // End frame with END byte
    output[out_idx++] = SLIP_END;

    *output_len = out_idx;
    return true;
}

// ============================================================================
// SLIP Decoder (Streaming State Machine)
// ============================================================================

void slip_decoder_init(slip_decoder_t *decoder) {
    if (decoder == NULL) {
        return;
    }

    decoder->state = SLIP_STATE_IDLE;
    decoder->frame_len = 0;
    decoder->frame_complete = false;
    decoder->frame_error = false;
}

void slip_decoder_reset(slip_decoder_t *decoder) {
    slip_decoder_init(decoder);
}

bool slip_decode_byte(slip_decoder_t *decoder, uint8_t byte,
                      uint8_t *output, size_t *output_len) {
    // Validate inputs
    if (decoder == NULL || output == NULL || output_len == NULL) {
        return false;
    }

    // Reset frame completion flags at start of each call
    decoder->frame_complete = false;

    switch (decoder->state) {
        case SLIP_STATE_IDLE:
            // Waiting for frame start
            if (byte == SLIP_END) {
                // Start of new frame
                decoder->state = SLIP_STATE_IN_FRAME;
                decoder->frame_len = 0;
                decoder->frame_error = false;
            }
            // Ignore all other bytes while idle
            break;

        case SLIP_STATE_IN_FRAME:
            if (byte == SLIP_END) {
                // End of frame
                if (decoder->frame_len > 0) {
                    // Valid frame - report it
                    *output_len = decoder->frame_len;
                    decoder->frame_complete = true;
                    decoder->state = SLIP_STATE_IDLE;
                    decoder->frame_len = 0;
                    return true;  // Frame complete!
                } else {
                    // Empty frame (two consecutive END bytes) - ignore and stay in frame mode
                    // This handles the case where frames are sent back-to-back
                    decoder->frame_len = 0;
                }
            } else if (byte == SLIP_ESC) {
                // Start of escape sequence
                decoder->state = SLIP_STATE_ESCAPED;
            } else {
                // Regular data byte
                output[decoder->frame_len++] = byte;
            }
            break;

        case SLIP_STATE_ESCAPED:
            // Previous byte was ESC - decode escape sequence
            if (byte == SLIP_ESC_END) {
                // ESC + ESC_END -> END byte in data
                output[decoder->frame_len++] = SLIP_END;
                decoder->state = SLIP_STATE_IN_FRAME;
            } else if (byte == SLIP_ESC_ESC) {
                // ESC + ESC_ESC -> ESC byte in data
                output[decoder->frame_len++] = SLIP_ESC;
                decoder->state = SLIP_STATE_IN_FRAME;
            } else if (byte == SLIP_END) {
                // ESC followed by END - protocol error, but treat as end of frame
                decoder->frame_error = true;
                decoder->state = SLIP_STATE_IDLE;
                decoder->frame_len = 0;
            } else {
                // Invalid escape sequence - protocol error
                // Discard frame and reset
                decoder->frame_error = true;
                decoder->state = SLIP_STATE_IDLE;
                decoder->frame_len = 0;
            }
            break;

        default:
            // Should never reach here - reset to safe state
            slip_decoder_init(decoder);
            break;
    }

    return false;  // No complete frame yet
}
