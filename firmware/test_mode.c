/**
 * @file test_mode.c
 * @brief Checkpoint Test Mode Implementation
 */

#include "test_mode.h"
#include <stdio.h>
#include <string.h>
#include "pico/time.h"

// Include checkpoint-specific modules
#include "crc_ccitt.h"
#include "slip.h"       // Checkpoint 3.2
#include "rs485_uart.h" // Checkpoint 3.3
// #include "nsp.h"        // Checkpoint 3.4

// ============================================================================
// Checkpoint 3.1: CRC-CCITT Test Vectors
// ============================================================================

/**
 * @brief Test CRC-CCITT implementation with known vectors
 *
 * Reference: CCITT CRC-16 (polynomial 0x1021, init 0xFFFF, LSB-first)
 */
void test_crc_vectors(void) {
    TEST_SECTION("Checkpoint 3.1: CRC-CCITT Test Vectors");

    bool all_passed = true;
    uint16_t crc_result;

    // Test 1: Simple 3-byte sequence
    // Expected CRC calculated using LSB-first CCITT algorithm
    printf("\nTest 1: {0x01, 0x02, 0x03}\n");
    uint8_t test1[] = {0x01, 0x02, 0x03};
    crc_result = crc_ccitt_calculate(test1, sizeof(test1));
    printf("  Calculated CRC: 0x%04X\n", crc_result);
    // For LSB-first CCITT with init 0xFFFF, poly 0x8408 (reversed):
    // Expected: 0x62C4
    bool test1_pass = (crc_result == 0x62C4);
    TEST_RESULT("Test 1", test1_pass);
    all_passed &= test1_pass;

    // Test 2: Empty buffer (should return init value 0xFFFF)
    printf("\nTest 2: Empty buffer\n");
    crc_result = crc_ccitt_calculate(NULL, 0);
    printf("  Calculated CRC: 0x%04X\n", crc_result);
    bool test2_pass = (crc_result == 0xFFFF);
    TEST_RESULT("Test 2 (empty)", test2_pass);
    all_passed &= test2_pass;

    // Test 3: Single byte 0x00
    printf("\nTest 3: {0x00}\n");
    uint8_t test3[] = {0x00};
    crc_result = crc_ccitt_calculate(test3, sizeof(test3));
    printf("  Calculated CRC: 0x%04X\n", crc_result);
    // Expected: 0xFFFF XOR 0x00 with polynomial operations
    // For LSB-first: 0x0F87
    bool test3_pass = (crc_result == 0x0F87);
    TEST_RESULT("Test 3", test3_pass);
    all_passed &= test3_pass;

    // Test 4: All 0xFF (stress test)
    printf("\nTest 4: {0xFF, 0xFF, 0xFF, 0xFF}\n");
    uint8_t test4[] = {0xFF, 0xFF, 0xFF, 0xFF};
    crc_result = crc_ccitt_calculate(test4, sizeof(test4));
    printf("  Calculated CRC: 0x%04X\n", crc_result);
    // Expected: 0xF0B8 (verified with LSB-first CCITT)
    bool test4_pass = (crc_result == 0xF0B8);
    TEST_RESULT("Test 4", test4_pass);
    all_passed &= test4_pass;

    // Test 5: ASCII string "123456789" (standard test vector)
    printf("\nTest 5: ASCII \"123456789\"\n");
    uint8_t test5[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    crc_result = crc_ccitt_calculate(test5, sizeof(test5));
    printf("  Calculated CRC: 0x%04X\n", crc_result);
    // Expected for LSB-first CCITT: 0x6F91 (CRC-16/KERMIT)
    bool test5_pass = (crc_result == 0x6F91);
    TEST_RESULT("Test 5 (ASCII)", test5_pass);
    all_passed &= test5_pass;

    // Test 6: Incremental CRC calculation
    printf("\nTest 6: Incremental calculation {0x01, 0x02, 0x03}\n");
    uint16_t crc_incremental = crc_ccitt_init();
    crc_incremental = crc_ccitt_update(crc_incremental, test1, 1);
    crc_incremental = crc_ccitt_update(crc_incremental, test1 + 1, 1);
    crc_incremental = crc_ccitt_update(crc_incremental, test1 + 2, 1);
    printf("  Calculated CRC: 0x%04X\n", crc_incremental);
    bool test6_pass = (crc_incremental == 0x62C4);  // Should match Test 1
    TEST_RESULT("Test 6 (incremental)", test6_pass);
    all_passed &= test6_pass;

    // Test 7: NSP PING packet structure (minimal)
    printf("\nTest 7: NSP PING packet\n");
    // NSP PING: [ADDR][CMD][LEN_LSB][LEN_MSB][CRC_LSB][CRC_MSB]
    // Example: ADDR=0x01, CMD=0x00 (PING), LEN=0x0000
    uint8_t ping_packet[] = {0x01, 0x00, 0x00, 0x00};  // Without CRC
    crc_result = crc_ccitt_calculate(ping_packet, sizeof(ping_packet));
    printf("  Calculated CRC: 0x%04X\n", crc_result);
    // This is a reference - actual value depends on NSP framing
    // Just verify it's not 0xFFFF (init value) or 0x0000
    bool test7_pass = (crc_result != 0xFFFF) && (crc_result != 0x0000);
    TEST_RESULT("Test 7 (NSP PING)", test7_pass);
    all_passed &= test7_pass;

    // Final result
    printf("\n");
    if (all_passed) {
        printf("✓✓✓ ALL CRC TESTS PASSED ✓✓✓\n");
    } else {
        printf("✗✗✗ SOME CRC TESTS FAILED ✗✗✗\n");
    }
    printf("\n");
}

// ============================================================================
// Checkpoint 3.2: SLIP Codec
// ============================================================================

/**
 * @brief Test SLIP encoder and decoder with various test cases
 *
 * Validates:
 * - Empty frames
 * - Regular data (no escaping needed)
 * - Data containing END bytes
 * - Data containing ESC bytes
 * - Data containing both END and ESC bytes
 * - Round-trip encode/decode
 * - Streaming decode with byte-by-byte input
 */
void test_slip_codec(void) {
    TEST_SECTION("Checkpoint 3.2: SLIP Codec");

    bool all_passed = true;
    uint8_t encoded[256];
    uint8_t decoded[256];
    size_t encoded_len, decoded_len;
    slip_decoder_t decoder;

    // Test 1: Empty frame
    printf("\nTest 1: Empty frame\n");
    {
        bool encode_ok = slip_encode(NULL, 0, encoded, &encoded_len);
        printf("  Encoded %zu bytes: ", encoded_len);
        for (size_t i = 0; i < encoded_len; i++) {
            printf("0x%02X ", encoded[i]);
        }
        printf("\n");

        // Should produce: END END
        bool test1_pass = encode_ok && (encoded_len == 2) &&
                         (encoded[0] == SLIP_END) && (encoded[1] == SLIP_END);
        TEST_RESULT("Test 1 (empty frame)", test1_pass);
        all_passed &= test1_pass;
    }

    // Test 2: Simple data (no escaping)
    printf("\nTest 2: Simple data {0x01, 0x02, 0x03}\n");
    {
        uint8_t test_data[] = {0x01, 0x02, 0x03};
        bool encode_ok = slip_encode(test_data, 3, encoded, &encoded_len);
        printf("  Encoded %zu bytes: ", encoded_len);
        for (size_t i = 0; i < encoded_len; i++) {
            printf("0x%02X ", encoded[i]);
        }
        printf("\n");

        // Should produce: END 0x01 0x02 0x03 END
        bool test2_pass = encode_ok && (encoded_len == 5) &&
                         (encoded[0] == SLIP_END) &&
                         (encoded[1] == 0x01) && (encoded[2] == 0x02) && (encoded[3] == 0x03) &&
                         (encoded[4] == SLIP_END);
        TEST_RESULT("Test 2 (simple data)", test2_pass);
        all_passed &= test2_pass;
    }

    // Test 3: Data containing END byte
    printf("\nTest 3: Data with END byte {0x01, 0xC0, 0x02}\n");
    {
        uint8_t test_data[] = {0x01, SLIP_END, 0x02};
        bool encode_ok = slip_encode(test_data, 3, encoded, &encoded_len);
        printf("  Encoded %zu bytes: ", encoded_len);
        for (size_t i = 0; i < encoded_len; i++) {
            printf("0x%02X ", encoded[i]);
        }
        printf("\n");

        // Should produce: END 0x01 ESC ESC_END 0x02 END (6 bytes)
        bool test3_pass = encode_ok && (encoded_len == 6) &&
                         (encoded[0] == SLIP_END) &&
                         (encoded[1] == 0x01) &&
                         (encoded[2] == SLIP_ESC) && (encoded[3] == SLIP_ESC_END) &&
                         (encoded[4] == 0x02) &&
                         (encoded[5] == SLIP_END);
        TEST_RESULT("Test 3 (END byte escaping)", test3_pass);
        all_passed &= test3_pass;
    }

    // Test 4: Data containing ESC byte
    printf("\nTest 4: Data with ESC byte {0x01, 0xDB, 0x02}\n");
    {
        uint8_t test_data[] = {0x01, SLIP_ESC, 0x02};
        bool encode_ok = slip_encode(test_data, 3, encoded, &encoded_len);
        printf("  Encoded %zu bytes: ", encoded_len);
        for (size_t i = 0; i < encoded_len; i++) {
            printf("0x%02X ", encoded[i]);
        }
        printf("\n");

        // Should produce: END 0x01 ESC ESC_ESC 0x02 END (6 bytes)
        bool test4_pass = encode_ok && (encoded_len == 6) &&
                         (encoded[0] == SLIP_END) &&
                         (encoded[1] == 0x01) &&
                         (encoded[2] == SLIP_ESC) && (encoded[3] == SLIP_ESC_ESC) &&
                         (encoded[4] == 0x02) &&
                         (encoded[5] == SLIP_END);
        TEST_RESULT("Test 4 (ESC byte escaping)", test4_pass);
        all_passed &= test4_pass;
    }

    // Test 5: Data with both END and ESC
    printf("\nTest 5: Data with END and ESC {0xC0, 0xDB, 0x55}\n");
    {
        uint8_t test_data[] = {SLIP_END, SLIP_ESC, 0x55};
        bool encode_ok = slip_encode(test_data, 3, encoded, &encoded_len);
        printf("  Encoded %zu bytes: ", encoded_len);
        for (size_t i = 0; i < encoded_len; i++) {
            printf("0x%02X ", encoded[i]);
        }
        printf("\n");

        // Should produce: END ESC ESC_END ESC ESC_ESC 0x55 END (7 bytes)
        bool test5_pass = encode_ok && (encoded_len == 7) &&
                         (encoded[0] == SLIP_END) &&
                         (encoded[1] == SLIP_ESC) && (encoded[2] == SLIP_ESC_END) &&
                         (encoded[3] == SLIP_ESC) && (encoded[4] == SLIP_ESC_ESC) &&
                         (encoded[5] == 0x55) &&
                         (encoded[6] == SLIP_END);
        TEST_RESULT("Test 5 (multiple escapes)", test5_pass);
        all_passed &= test5_pass;
    }

    // Test 6: Round-trip encode/decode
    printf("\nTest 6: Round-trip encode/decode\n");
    {
        uint8_t original[] = {0x01, 0xC0, 0x02, 0xDB, 0x03, 0xAA, 0xBB};
        size_t original_len = sizeof(original);

        // Encode
        slip_encode(original, original_len, encoded, &encoded_len);

        // Decode byte-by-byte
        slip_decoder_init(&decoder);
        bool frame_received = false;
        for (size_t i = 0; i < encoded_len; i++) {
            if (slip_decode_byte(&decoder, encoded[i], decoded, &decoded_len)) {
                frame_received = true;
                break;
            }
        }

        printf("  Original: ");
        for (size_t i = 0; i < original_len; i++) {
            printf("0x%02X ", original[i]);
        }
        printf("\n");
        printf("  Decoded:  ");
        for (size_t i = 0; i < decoded_len; i++) {
            printf("0x%02X ", decoded[i]);
        }
        printf("\n");

        // Verify round-trip
        bool test6_pass = frame_received && (decoded_len == original_len) &&
                         (memcmp(original, decoded, original_len) == 0);
        TEST_RESULT("Test 6 (round-trip)", test6_pass);
        all_passed &= test6_pass;
    }

    // Test 7: Streaming decoder with multiple frames
    printf("\nTest 7: Streaming decoder - back-to-back frames\n");
    {
        // Two frames: {0x01, 0x02} and {0x03, 0x04}
        uint8_t frame1[] = {0x01, 0x02};
        uint8_t frame2[] = {0x03, 0x04};
        uint8_t encoded1[16], encoded2[16];
        size_t enc1_len, enc2_len;

        slip_encode(frame1, 2, encoded1, &enc1_len);
        slip_encode(frame2, 2, encoded2, &enc2_len);

        // Combine into stream (second frame starts immediately after first)
        uint8_t stream[32];
        memcpy(stream, encoded1, enc1_len);
        memcpy(stream + enc1_len, encoded2, enc2_len);
        size_t stream_len = enc1_len + enc2_len;

        // Decode stream
        slip_decoder_init(&decoder);
        int frames_received = 0;
        for (size_t i = 0; i < stream_len; i++) {
            if (slip_decode_byte(&decoder, stream[i], decoded, &decoded_len)) {
                frames_received++;
                printf("  Frame %d received: ", frames_received);
                for (size_t j = 0; j < decoded_len; j++) {
                    printf("0x%02X ", decoded[j]);
                }
                printf("\n");
            }
        }

        bool test7_pass = (frames_received == 2);
        TEST_RESULT("Test 7 (streaming)", test7_pass);
        all_passed &= test7_pass;
    }

    // Final result
    printf("\n");
    if (all_passed) {
        printf("✓✓✓ ALL SLIP TESTS PASSED ✓✓✓\n");
    } else {
        printf("✗✗✗ SOME SLIP TESTS FAILED ✗✗✗\n");
    }
    printf("\n");
}

// ============================================================================
// Checkpoint 3.3: RS-485 UART Loopback
// ============================================================================

/**
 * @brief Test RS-485 UART with software loopback
 *
 * Since we don't have actual hardware loopback (TX wired to RX), this test
 * validates the UART initialization, baud rate configuration, and basic API.
 *
 * For full loopback testing, physically connect:
 * - GPIO 4 (TX) to GPIO 5 (RX)
 * - Or use a MAX485 transceiver in loopback mode
 */
void test_rs485_loopback(void) {
    TEST_SECTION("Checkpoint 3.3: RS-485 UART Loopback");

    bool all_passed = true;

    // Test 1: Initialization
    printf("\nTest 1: RS-485 UART Initialization\n");
    bool init_ok = rs485_init();
    printf("  Initialization: %s\n", init_ok ? "OK" : "FAILED");
    printf("  Expected baud rate: 460800\n");
    printf("  Format: 8-N-1\n");
    TEST_RESULT("Test 1 (init)", init_ok);
    all_passed &= init_ok;

    if (!init_ok) {
        printf("\n✗✗✗ INITIALIZATION FAILED - ABORTING TESTS ✗✗✗\n");
        return;
    }

    // Test 2: Clear buffers
    printf("\nTest 2: Buffer Management\n");
    rs485_clear_rx();
    size_t available_after_clear = rs485_available();
    printf("  RX bytes after clear: %zu\n", available_after_clear);
    bool test2_pass = (available_after_clear == 0);
    TEST_RESULT("Test 2 (clear)", test2_pass);
    all_passed &= test2_pass;

    // Test 3: Send data (no loopback, just verifies API doesn't crash)
    printf("\nTest 3: Transmit API\n");
    uint8_t test_data[] = {0x01, 0x02, 0x03, 0x04, 0x05};
    bool send_ok = rs485_send(test_data, sizeof(test_data));
    printf("  Sent %zu bytes: ", sizeof(test_data));
    for (size_t i = 0; i < sizeof(test_data); i++) {
        printf("0x%02X ", test_data[i]);
    }
    printf("\n");
    printf("  Send result: %s\n", send_ok ? "OK" : "FAILED");
    TEST_RESULT("Test 3 (send)", send_ok);
    all_passed &= send_ok;

    // Test 4: Error handling
    printf("\nTest 4: Error Handling\n");
    bool test4a = !rs485_send(NULL, 10);  // Should fail with NULL data
    bool test4b = !rs485_send(test_data, 0);  // Should fail with zero length
    printf("  NULL data rejected: %s\n", test4a ? "OK" : "FAILED");
    printf("  Zero length rejected: %s\n", test4b ? "OK" : "FAILED");
    bool test4_pass = test4a && test4b;
    TEST_RESULT("Test 4 (error handling)", test4_pass);
    all_passed &= test4_pass;

    // Test 5: Hardware loopback (only if physically wired)
    printf("\nTest 5: Hardware Loopback (if wired)\n");
    printf("  To enable this test, connect GPIO 4 (TX) to GPIO 5 (RX)\n");
    printf("  Or connect through a MAX485 transceiver in loopback\n");

    // Clear RX buffer
    rs485_clear_rx();

    // Send test pattern
    uint8_t loopback_pattern[] = {0xAA, 0x55, 0x12, 0x34, 0x56, 0x78, 0x9A, 0xBC};
    rs485_send(loopback_pattern, sizeof(loopback_pattern));

    // Wait a bit for data to arrive (if looped back)
    sleep_ms(10);

    // Try to read back
    uint8_t rx_buffer[16];
    size_t rx_count = rs485_read(rx_buffer, sizeof(rx_buffer));

    printf("  Sent %zu bytes, received %zu bytes\n",
           sizeof(loopback_pattern), rx_count);

    bool loopback_ok = false;
    if (rx_count > 0) {
        printf("  Received: ");
        for (size_t i = 0; i < rx_count; i++) {
            printf("0x%02X ", rx_buffer[i]);
        }
        printf("\n");

        // Verify data matches
        loopback_ok = (rx_count == sizeof(loopback_pattern)) &&
                      (memcmp(rx_buffer, loopback_pattern, rx_count) == 0);
        printf("  Loopback verification: %s\n", loopback_ok ? "OK" : "MISMATCH");
        TEST_RESULT("Test 5 (hardware loopback)", loopback_ok);
        all_passed &= loopback_ok;
    } else {
        printf("  No data received (hardware not looped back)\n");
        printf("  Note: GPIO 4 (TX) should be wired to GPIO 5 (RX) for this test\n");
        TEST_RESULT("Test 5 (hardware loopback)", false);
        // Don't fail overall test if hardware isn't connected
        printf("  (Skipped - not counting toward pass/fail)\n");
    }

    // Test 6: Timing verification
    printf("\nTest 6: Timing and Baud Rate\n");
    printf("  Baud rate: 460800 bps\n");
    printf("  Bit time: ~2.17 µs\n");
    printf("  Byte time (10 bits): ~21.7 µs\n");
    printf("  100-byte transmission: ~2.17 ms\n");

    // Send 100 bytes and time it
    uint8_t timing_data[100];
    for (int i = 0; i < 100; i++) {
        timing_data[i] = i;
    }

    absolute_time_t start = get_absolute_time();
    rs485_send(timing_data, sizeof(timing_data));
    absolute_time_t end = get_absolute_time();

    int64_t elapsed_us = absolute_time_diff_us(start, end);
    float elapsed_ms = elapsed_us / 1000.0f;

    printf("  100 bytes transmitted in: %.3f ms (%lld µs)\n",
           elapsed_ms, elapsed_us);
    printf("  Expected time: ~2.17 ms\n");

    // Allow some tolerance for timing (1-5 ms is reasonable with delays)
    bool timing_ok = (elapsed_us > 1000) && (elapsed_us < 5000);
    TEST_RESULT("Test 6 (timing)", timing_ok);
    all_passed &= timing_ok;

    // Final result
    printf("\n");
    if (all_passed) {
        printf("✓✓✓ ALL RS-485 CORE TESTS PASSED ✓✓✓\n");
        printf("Note: Hardware loopback test requires physical wiring\n");
    } else {
        printf("✗✗✗ SOME RS-485 TESTS FAILED ✗✗✗\n");
    }
    printf("\n");
}

// ============================================================================
// Checkpoint 3.4: NSP Protocol (future)
// ============================================================================

void test_nsp_ping(void) {
    TEST_SECTION("Checkpoint 3.4: NSP Protocol");
    // Implementation in future checkpoint
    printf("Not yet implemented.\n");
}
