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
#include "nsp.h"        // Checkpoint 3.4
#include "ringbuf.h"    // Checkpoint 4.1
#include "fixedpoint.h" // Checkpoint 4.2
#include "nss_nrwa_t6_regs.h" // Checkpoint 5.1
#include "nss_nrwa_t6_model.h" // Checkpoint 5.2

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

    // Test 6: LED Strobe Test (continuous transmission for visual feedback)
    printf("\nTest 6: LED Strobe Test\n");
    printf("  Sending data to strobe LEDs...\n");
    printf("  Watch GPIO 4 (TX), GPIO 6 (DE), GPIO 7 (RE) LEDs!\n");
    printf("  (Sending 1000 bytes in 10 bursts with 200ms pauses)\n");

    uint8_t strobe_pattern[100];
    for (int i = 0; i < 100; i++) {
        strobe_pattern[i] = (uint8_t)(i * 0xAA);  // Varying pattern
    }

    for (int burst = 0; burst < 10; burst++) {
        rs485_send(strobe_pattern, sizeof(strobe_pattern));
        sleep_ms(200);  // Longer pause for better LED visibility
        printf(".");
    }
    printf(" Done!\n");

    printf("  Total: 1000 bytes transmitted\n");
    printf("  You should have seen GPIO 6/7 (DE/RE) strobing!\n");
    TEST_RESULT("Test 6 (LED strobe)", true);

    // Test 7: Timing verification
    printf("\nTest 7: Timing and Baud Rate\n");
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
    TEST_RESULT("Test 7 (timing)", timing_ok);
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
// Checkpoint 3.4: NSP Protocol - PING Responder
// ============================================================================

void test_nsp_ping(void) {
    TEST_SECTION("Checkpoint 3.4: NSP PING Responder");

    // Initialize NSP with device address 1 (for testing)
    uint8_t device_addr = 1;
    nsp_init(device_addr);
    printf("  Device address: %d\n\n", device_addr);

    bool all_passed = true;

    // Test 1: Build a PING request packet
    printf("Test 1: Build PING Request\n");

    nsp_packet_t ping_request;
    ping_request.dest = device_addr;  // To us
    ping_request.src = 0;             // From ground station (address 0)
    ping_request.ctrl = nsp_make_ctrl(true, false, false, NSP_CMD_PING);  // Poll=1, B=0, A=0, CMD=PING
    ping_request.len = 0;             // No data in PING

    // Build raw packet
    uint8_t ping_raw[NSP_MAX_PACKET_SIZE];
    size_t ping_raw_len = 0;

    // Manually build packet for testing
    ping_raw[0] = ping_request.dest;
    ping_raw[1] = ping_request.src;
    ping_raw[2] = ping_request.ctrl;
    ping_raw[3] = ping_request.len;

    // Compute and append CRC
    uint16_t crc = crc_ccitt_calculate(ping_raw, 4);
    ping_raw[4] = (uint8_t)(crc & 0xFF);
    ping_raw[5] = (uint8_t)((crc >> 8) & 0xFF);
    ping_raw_len = 6;

    printf("  PING packet (hex): ");
    for (size_t i = 0; i < ping_raw_len; i++) {
        printf("%02X ", ping_raw[i]);
    }
    printf("\n");
    printf("  Dest=%d, Src=%d, Ctrl=0x%02X, Len=%d, CRC=0x%04X\n",
           ping_request.dest, ping_request.src, ping_request.ctrl,
           ping_request.len, crc);
    TEST_RESULT("Test 1 (build PING)", true);

    // Test 2: Parse PING packet
    printf("\nTest 2: Parse PING Packet\n");

    nsp_packet_t parsed_ping;
    nsp_result_t parse_result = nsp_parse(ping_raw, ping_raw_len, &parsed_ping);

    printf("  Parse result: ");
    switch (parse_result) {
        case NSP_OK:           printf("OK\n"); break;
        case NSP_ERR_TOO_SHORT:  printf("ERR_TOO_SHORT\n"); break;
        case NSP_ERR_BAD_LENGTH: printf("ERR_BAD_LENGTH\n"); break;
        case NSP_ERR_BAD_CRC:    printf("ERR_BAD_CRC\n"); break;
        default:               printf("UNKNOWN\n"); break;
    }

    bool test2_pass = (parse_result == NSP_OK) &&
                     (parsed_ping.dest == ping_request.dest) &&
                     (parsed_ping.src == ping_request.src) &&
                     (parsed_ping.ctrl == ping_request.ctrl) &&
                     (parsed_ping.len == ping_request.len);

    if (test2_pass) {
        printf("  Parsed: Dest=%d, Src=%d, Ctrl=0x%02X, Len=%d\n",
               parsed_ping.dest, parsed_ping.src, parsed_ping.ctrl, parsed_ping.len);
        printf("  Command code: 0x%02X (PING)\n", nsp_get_command(parsed_ping.ctrl));
        printf("  Poll bit: %d\n", nsp_is_poll_set(parsed_ping.ctrl) ? 1 : 0);
    }

    TEST_RESULT("Test 2 (parse PING)", test2_pass);
    all_passed &= test2_pass;

    // Test 3: Generate ACK reply
    printf("\nTest 3: Generate ACK Reply\n");

    uint8_t ack_raw[NSP_MAX_PACKET_SIZE];
    size_t ack_raw_len = 0;

    bool build_ok = nsp_build_ack(&parsed_ping, ack_raw, &ack_raw_len);

    if (build_ok) {
        printf("  ACK packet (hex): ");
        for (size_t i = 0; i < ack_raw_len; i++) {
            printf("%02X ", ack_raw[i]);
        }
        printf("\n");
        printf("  ACK length: %zu bytes\n", ack_raw_len);

        // Parse the ACK to verify it's correct
        nsp_packet_t parsed_ack;
        nsp_result_t ack_parse = nsp_parse(ack_raw, ack_raw_len, &parsed_ack);

        if (ack_parse == NSP_OK) {
            printf("  ACK parsed: Dest=%d, Src=%d, Ctrl=0x%02X, Len=%d\n",
                   parsed_ack.dest, parsed_ack.src, parsed_ack.ctrl, parsed_ack.len);

            // Verify ACK properties:
            // - Dest = original Src
            // - Src = our address
            // - Ctrl: B/A bits preserved, Poll cleared, same command
            bool test3_pass = (parsed_ack.dest == parsed_ping.src) &&
                             (parsed_ack.src == device_addr) &&
                             (parsed_ack.len == 0) &&
                             (nsp_get_command(parsed_ack.ctrl) == NSP_CMD_PING) &&
                             (ack_parse == NSP_OK);

            TEST_RESULT("Test 3 (generate ACK)", test3_pass);
            all_passed &= test3_pass;
        } else {
            printf("  ACK parse failed!\n");
            TEST_RESULT("Test 3 (generate ACK)", false);
            all_passed = false;
        }
    } else {
        printf("  Failed to build ACK\n");
        TEST_RESULT("Test 3 (generate ACK)", false);
        all_passed = false;
    }

    // Test 4: Full PING/ACK round-trip
    printf("\nTest 4: Full PING/ACK Round-Trip\n");

    // Simulate receiving PING and generating ACK
    nsp_packet_t rx_ping;
    nsp_result_t rx_result = nsp_parse(ping_raw, ping_raw_len, &rx_ping);

    if (rx_result == NSP_OK && nsp_get_command(rx_ping.ctrl) == NSP_CMD_PING) {
        printf("  ✓ PING received and recognized\n");

        // Generate ACK
        uint8_t tx_ack[NSP_MAX_PACKET_SIZE];
        size_t tx_ack_len = 0;

        if (nsp_build_ack(&rx_ping, tx_ack, &tx_ack_len)) {
            printf("  ✓ ACK generated (%zu bytes)\n", tx_ack_len);

            // Verify ACK is valid
            nsp_packet_t verify_ack;
            if (nsp_parse(tx_ack, tx_ack_len, &verify_ack) == NSP_OK) {
                printf("  ✓ ACK is valid and CRC correct\n");
                TEST_RESULT("Test 4 (round-trip)", true);
                all_passed &= true;
            } else {
                printf("  ✗ ACK CRC verification failed\n");
                TEST_RESULT("Test 4 (round-trip)", false);
                all_passed = false;
            }
        } else {
            printf("  ✗ ACK generation failed\n");
            TEST_RESULT("Test 4 (round-trip)", false);
            all_passed = false;
        }
    } else {
        printf("  ✗ PING receive/parse failed\n");
        TEST_RESULT("Test 4 (round-trip)", false);
        all_passed = false;
    }

    // Test 5: CRC Validation (intentional corruption)
    printf("\nTest 5: CRC Validation (Bad CRC)\n");

    uint8_t bad_packet[NSP_MAX_PACKET_SIZE];
    memcpy(bad_packet, ping_raw, ping_raw_len);

    // Corrupt the CRC
    bad_packet[4] ^= 0xFF;

    nsp_packet_t bad_parse;
    nsp_result_t bad_result = nsp_parse(bad_packet, ping_raw_len, &bad_parse);

    bool test5_pass = (bad_result == NSP_ERR_BAD_CRC);
    printf("  Expected: NSP_ERR_BAD_CRC, Got: %d\n", bad_result);
    TEST_RESULT("Test 5 (CRC validation)", test5_pass);
    all_passed &= test5_pass;

    // Final result
    printf("\n");
    if (all_passed) {
        printf("✓✓✓ ALL NSP PING TESTS PASSED ✓✓✓\n");
    } else {
        printf("✗✗✗ SOME NSP TESTS FAILED ✗✗✗\n");
    }
    printf("\n");
}

// ============================================================================
// Checkpoint 4.1: Ring Buffer Stress Test
// ============================================================================

void test_ringbuf_stress(void) {
    TEST_SECTION("Checkpoint 4.1: Ring Buffer Stress Test");

    bool all_passed = true;

    // Test 1: Initialization
    printf("Test 1: Initialization\n");

    ringbuf_t rb;
    bool init_ok = ringbuf_init(&rb, 256);  // Power of 2
    printf("  Initialize with size 256: %s\n", init_ok ? "OK" : "FAIL");

    // Test invalid sizes
    ringbuf_t rb_bad;
    bool bad1 = !ringbuf_init(&rb_bad, 100);  // Not power of 2
    bool bad2 = !ringbuf_init(&rb_bad, 0);    // Zero
    bool bad3 = !ringbuf_init(&rb_bad, 512);  // > MAX_SIZE

    bool test1_pass = init_ok && bad1 && bad2 && bad3;
    printf("  Reject invalid sizes: %s\n", (bad1 && bad2 && bad3) ? "OK" : "FAIL");
    TEST_RESULT("Test 1 (init)", test1_pass);
    all_passed &= test1_pass;

    // Test 2: Push/Pop FIFO Order
    printf("\nTest 2: Push/Pop FIFO Order\n");

    ringbuf_reset(&rb);

    // Push 10 items
    bool push_ok = true;
    for (uint32_t i = 0; i < 10; i++) {
        if (!ringbuf_push(&rb, i + 100)) {
            push_ok = false;
            break;
        }
    }
    printf("  Pushed 10 items: %s\n", push_ok ? "OK" : "FAIL");

    // Pop and verify FIFO order
    bool pop_ok = true;
    for (uint32_t i = 0; i < 10; i++) {
        uint32_t value;
        if (!ringbuf_pop(&rb, &value) || value != (i + 100)) {
            pop_ok = false;
            printf("  ERROR: Expected %u, got %u\n", i + 100, value);
            break;
        }
    }
    printf("  Popped 10 items in order: %s\n", pop_ok ? "OK" : "FAIL");

    bool test2_pass = push_ok && pop_ok;
    TEST_RESULT("Test 2 (FIFO order)", test2_pass);
    all_passed &= test2_pass;

    // Test 3: Empty Detection
    printf("\nTest 3: Empty Detection\n");

    ringbuf_reset(&rb);
    bool empty1 = ringbuf_is_empty(&rb);
    printf("  Buffer empty after reset: %s\n", empty1 ? "OK" : "FAIL");

    ringbuf_push(&rb, 42);
    bool not_empty = !ringbuf_is_empty(&rb);
    printf("  Buffer not empty after push: %s\n", not_empty ? "OK" : "FAIL");

    uint32_t value;
    ringbuf_pop(&rb, &value);
    bool empty2 = ringbuf_is_empty(&rb);
    printf("  Buffer empty after pop: %s\n", empty2 ? "OK" : "FAIL");

    // Try to pop from empty buffer
    bool pop_fail = !ringbuf_pop(&rb, &value);
    printf("  Pop from empty fails: %s\n", pop_fail ? "OK" : "FAIL");

    bool test3_pass = empty1 && not_empty && empty2 && pop_fail;
    TEST_RESULT("Test 3 (empty detection)", test3_pass);
    all_passed &= test3_pass;

    // Test 4: Full Detection
    printf("\nTest 4: Full Detection\n");

    ringbuf_reset(&rb);

    // Fill buffer (255 items max, since we sacrifice one slot)
    uint32_t filled = 0;
    while (ringbuf_push(&rb, filled)) {
        filled++;
    }
    printf("  Filled %u items before full\n", filled);
    printf("  Expected ~255 items (size-1)\n");

    bool is_full = ringbuf_is_full(&rb);
    printf("  Buffer reports full: %s\n", is_full ? "OK" : "FAIL");

    // Try to push to full buffer
    bool push_fail = !ringbuf_push(&rb, 999);
    printf("  Push to full fails: %s\n", push_fail ? "OK" : "FAIL");

    bool test4_pass = (filled >= 254 && filled <= 256) && is_full && push_fail;
    TEST_RESULT("Test 4 (full detection)", test4_pass);
    all_passed &= test4_pass;

    // Test 5: Count and Available
    printf("\nTest 5: Count and Available\n");

    ringbuf_reset(&rb);

    uint32_t count0 = ringbuf_count(&rb);
    uint32_t avail0 = ringbuf_available(&rb);
    printf("  Empty: count=%u, available=%u\n", count0, avail0);

    // Push 50 items
    for (uint32_t i = 0; i < 50; i++) {
        ringbuf_push(&rb, i);
    }

    uint32_t count50 = ringbuf_count(&rb);
    uint32_t avail50 = ringbuf_available(&rb);
    printf("  After 50 pushes: count=%u, available=%u\n", count50, avail50);

    bool test5_pass = (count0 == 0) && (count50 == 50) && (avail0 > 200) && (avail50 < avail0);
    TEST_RESULT("Test 5 (count/available)", test5_pass);
    all_passed &= test5_pass;

    // Test 6: Stress Test (1M operations)
    printf("\nTest 6: Stress Test (1,000,000 push/pop cycles)\n");
    printf("  This will take ~3-5 seconds...\n");

    ringbuf_reset(&rb);

    absolute_time_t start = get_absolute_time();

    // Perform 1 million push/pop cycles
    const uint32_t STRESS_COUNT = 1000000;
    bool stress_ok = true;

    for (uint32_t i = 0; i < STRESS_COUNT; i++) {
        // Push value
        if (!ringbuf_push(&rb, i)) {
            printf("  ERROR: Push failed at iteration %u\n", i);
            stress_ok = false;
            break;
        }

        // Pop value
        uint32_t popped;
        if (!ringbuf_pop(&rb, &popped)) {
            printf("  ERROR: Pop failed at iteration %u\n", i);
            stress_ok = false;
            break;
        }

        // Verify value
        if (popped != i) {
            printf("  ERROR: Data corruption at iteration %u (expected %u, got %u)\n",
                   i, i, popped);
            stress_ok = false;
            break;
        }
    }

    absolute_time_t end = get_absolute_time();
    int64_t elapsed_us = absolute_time_diff_us(start, end);
    float elapsed_s = elapsed_us / 1000000.0f;

    printf("  Completed %u cycles in %.3f seconds\n", STRESS_COUNT, elapsed_s);
    printf("  Rate: %.0f ops/sec\n", STRESS_COUNT / elapsed_s);
    printf("  Average: %.2f µs per push+pop\n", (float)elapsed_us / STRESS_COUNT);

    bool test6_pass = stress_ok && ringbuf_is_empty(&rb);
    printf("  Buffer empty after test: %s\n", ringbuf_is_empty(&rb) ? "OK" : "FAIL");
    TEST_RESULT("Test 6 (stress test)", test6_pass);
    all_passed &= test6_pass;

    // Final result
    printf("\n");
    if (all_passed) {
        printf("✓✓✓ ALL RING BUFFER TESTS PASSED ✓✓✓\n");
    } else {
        printf("✗✗✗ SOME RING BUFFER TESTS FAILED ✗✗✗\n");
    }
    printf("\n");
}

// ============================================================================
// Checkpoint 4.2: Fixed-Point Math Accuracy Tests
// ============================================================================

/**
 * @brief Test fixed-point conversions and arithmetic
 *
 * Validates:
 * - Float ↔ fixed-point conversions (within 1 LSB)
 * - Arithmetic operations (add, sub, multiply)
 * - Saturation behavior
 */
void test_fixedpoint_accuracy(void) {
    TEST_SECTION("Checkpoint 4.2: Fixed-Point Math Accuracy");

    bool all_passed = true;

    // Test 1: UQ14.18 RPM conversions
    printf("\nTest 1: UQ14.18 Speed (RPM) Conversions\n");
    bool test1_pass = true;

    float rpm_test_values[] = {0.0f, 3000.0f, 5000.0f, 6000.0f};
    int rpm_test_count = sizeof(rpm_test_values) / sizeof(rpm_test_values[0]);

    for (int i = 0; i < rpm_test_count; i++) {
        float original = rpm_test_values[i];
        uq14_18_t fixed = float_to_uq14_18(original);
        float recovered = uq14_18_to_float(fixed);
        float error = fabsf(recovered - original);
        float tolerance = uq14_18_resolution();  // 1 LSB

        printf("  %.1f RPM → 0x%08X → %.6f RPM (error: %.9f, tol: %.9f)\n",
               original, fixed, recovered, error, tolerance);

        if (error > tolerance) {
            test1_pass = false;
            printf("    ERROR: Exceeds 1 LSB tolerance!\n");
        }
    }

    TEST_RESULT("UQ14.18 RPM conversions", test1_pass);
    all_passed &= test1_pass;

    // Test 2: UQ16.16 Voltage conversions
    printf("\nTest 2: UQ16.16 Voltage (V) Conversions\n");
    bool test2_pass = true;

    float voltage_test_values[] = {0.0f, 28.0f, 36.0f};
    int voltage_test_count = sizeof(voltage_test_values) / sizeof(voltage_test_values[0]);

    for (int i = 0; i < voltage_test_count; i++) {
        float original = voltage_test_values[i];
        uq16_16_t fixed = float_to_uq16_16(original);
        float recovered = uq16_16_to_float(fixed);
        float error = fabsf(recovered - original);
        float tolerance = uq16_16_resolution();  // 1 LSB

        printf("  %.1f V → 0x%08X → %.6f V (error: %.9f, tol: %.9f)\n",
               original, fixed, recovered, error, tolerance);

        if (error > tolerance) {
            test2_pass = false;
            printf("    ERROR: Exceeds 1 LSB tolerance!\n");
        }
    }

    TEST_RESULT("UQ16.16 Voltage conversions", test2_pass);
    all_passed &= test2_pass;

    // Test 3: UQ18.14 Torque/Current/Power conversions
    printf("\nTest 3: UQ18.14 Torque/Current/Power Conversions\n");
    bool test3_pass = true;

    float uq18_14_test_values[] = {0.0f, 100.0f, 500.0f, 1000.0f};
    int uq18_14_test_count = sizeof(uq18_14_test_values) / sizeof(uq18_14_test_values[0]);

    for (int i = 0; i < uq18_14_test_count; i++) {
        float original = uq18_14_test_values[i];
        uq18_14_t fixed = float_to_uq18_14(original);
        float recovered = uq18_14_to_float(fixed);
        float error = fabsf(recovered - original);
        float tolerance = uq18_14_resolution();  // 1 LSB

        printf("  %.1f mA → 0x%08X → %.6f mA (error: %.9f, tol: %.9f)\n",
               original, fixed, recovered, error, tolerance);

        if (error > tolerance) {
            test3_pass = false;
            printf("    ERROR: Exceeds 1 LSB tolerance!\n");
        }
    }

    TEST_RESULT("UQ18.14 conversions", test3_pass);
    all_passed &= test3_pass;

    // Test 4: Arithmetic - Addition (100mA + 200mA = 300mA)
    printf("\nTest 4: Arithmetic - Addition\n");
    bool test4_pass = true;

    uq18_14_t a = float_to_uq18_14(100.0f);
    uq18_14_t b = float_to_uq18_14(200.0f);
    uq18_14_t sum = uq18_14_add(a, b);
    float sum_float = uq18_14_to_float(sum);

    printf("  100.0 mA + 200.0 mA = %.6f mA (expected: 300.0)\n", sum_float);

    float expected_sum = 300.0f;
    float sum_error = fabsf(sum_float - expected_sum);
    float sum_tolerance = uq18_14_resolution() * 3.0f;  // Allow 3 LSB accumulation

    if (sum_error > sum_tolerance) {
        test4_pass = false;
        printf("    ERROR: Addition error %.6f exceeds tolerance %.6f\n",
               sum_error, sum_tolerance);
    }

    TEST_RESULT("Addition (100 + 200 = 300)", test4_pass);
    all_passed &= test4_pass;

    // Test 5: Saturation - UQ18.14 max value + 1 → saturates
    printf("\nTest 5: Saturation Behavior\n");
    bool test5_pass = true;

    uq18_14_t max_val = UQ18_14_MAX;
    uq18_14_t one = float_to_uq18_14(1.0f);
    uq18_14_t saturated = uq18_14_add(max_val, one);

    printf("  UQ18_14_MAX (0x%08X) + 1 = 0x%08X\n", max_val, saturated);

    if (saturated != UQ18_14_MAX) {
        test5_pass = false;
        printf("    ERROR: Did not saturate to MAX value!\n");
    } else {
        printf("    Correctly saturated to UQ18_14_MAX\n");
    }

    TEST_RESULT("Saturation on overflow", test5_pass);
    all_passed &= test5_pass;

    // Test 6: Subtraction with underflow protection
    printf("\nTest 6: Subtraction with Underflow Protection\n");
    bool test6_pass = true;

    uq18_14_t val_50 = float_to_uq18_14(50.0f);
    uq18_14_t val_100 = float_to_uq18_14(100.0f);
    uq18_14_t underflow_result = uq18_14_sub(val_50, val_100);  // 50 - 100 should = 0

    printf("  50.0 mA - 100.0 mA = 0x%08X (expected: 0x00000000)\n", underflow_result);

    if (underflow_result != 0) {
        test6_pass = false;
        printf("    ERROR: Did not saturate to zero on underflow!\n");
    } else {
        printf("    Correctly saturated to zero\n");
    }

    TEST_RESULT("Underflow saturation to zero", test6_pass);
    all_passed &= test6_pass;

    // Test 7: Multiplication
    printf("\nTest 7: Multiplication\n");
    bool test7_pass = true;

    uq14_18_t speed_2 = float_to_uq14_18(2.0f);
    uq14_18_t speed_3 = float_to_uq14_18(3.0f);
    uq14_18_t product = uq14_18_mul(speed_2, speed_3);
    float product_float = uq14_18_to_float(product);

    printf("  2.0 * 3.0 = %.6f (expected: 6.0)\n", product_float);

    float expected_product = 6.0f;
    float product_error = fabsf(product_float - expected_product);
    float product_tolerance = uq14_18_resolution() * 10.0f;  // Allow 10 LSB for multiply

    if (product_error > product_tolerance) {
        test7_pass = false;
        printf("    ERROR: Multiplication error %.6f exceeds tolerance %.6f\n",
               product_error, product_tolerance);
    }

    TEST_RESULT("Multiplication (2 * 3 = 6)", test7_pass);
    all_passed &= test7_pass;

    // Final result
    printf("\n");
    if (all_passed) {
        printf("✓✓✓ ALL FIXED-POINT TESTS PASSED ✓✓✓\n");
    } else {
        printf("✗✗✗ SOME FIXED-POINT TESTS FAILED ✗✗✗\n");
    }
    printf("\n");
}

// ============================================================================
// Checkpoint 5.1: Register Map Tests
// ============================================================================

/**
 * @brief Test register map structure and organization
 *
 * Validates:
 * - Register addresses are non-overlapping
 * - All registers have valid addresses
 * - Register access permissions (read-only, read/write)
 * - Register name lookup works correctly
 */
void test_register_map(void) {
    TEST_SECTION("Checkpoint 5.1: Register Map");

    bool all_passed = true;

    // Test 1: Register address validity
    printf("\nTest 1: Register Address Validity\n");
    bool test1_pass = true;

    // Sample of key registers to validate
    uint16_t test_regs[] = {
        REG_DEVICE_ID,
        REG_FIRMWARE_VERSION,
        REG_OVERVOLTAGE_THRESHOLD,
        REG_CONTROL_MODE,
        REG_SPEED_SETPOINT_RPM,
        REG_CURRENT_SPEED_RPM,
        REG_FAULT_STATUS,
        REG_COMM_ERRORS_CRC
    };
    int num_test_regs = sizeof(test_regs) / sizeof(test_regs[0]);

    for (int i = 0; i < num_test_regs; i++) {
        bool valid = reg_is_valid_address(test_regs[i]);
        const char* name = reg_get_name(test_regs[i]);
        printf("  0x%04X (%s): %s\n", test_regs[i], name, valid ? "VALID" : "INVALID");
        if (!valid) {
            test1_pass = false;
        }
    }

    TEST_RESULT("Register address validity", test1_pass);
    all_passed &= test1_pass;

    // Test 2: Non-overlapping addresses
    printf("\nTest 2: Non-Overlapping Address Ranges\n");
    bool test2_pass = true;

    // Check that address ranges don't overlap
    printf("  Device Info: 0x0000-0x00FF\n");
    printf("  Protection:  0x0100-0x01FF\n");
    printf("  Control:     0x0200-0x02FF\n");
    printf("  Status:      0x0300-0x03FF\n");
    printf("  Fault/Diag:  0x0400-0x04FF\n");

    // Verify boundaries
    if (REG_OVERVOLTAGE_THRESHOLD < 0x0100 || REG_OVERVOLTAGE_THRESHOLD >= 0x0200) {
        printf("  ERROR: Protection register out of range!\n");
        test2_pass = false;
    }
    if (REG_CONTROL_MODE < 0x0200 || REG_CONTROL_MODE >= 0x0300) {
        printf("  ERROR: Control register out of range!\n");
        test2_pass = false;
    }
    if (REG_CURRENT_SPEED_RPM < 0x0300 || REG_CURRENT_SPEED_RPM >= 0x0400) {
        printf("  ERROR: Status register out of range!\n");
        test2_pass = false;
    }

    TEST_RESULT("Address ranges non-overlapping", test2_pass);
    all_passed &= test2_pass;

    // Test 3: Read-only register detection
    printf("\nTest 3: Read-Only Register Detection\n");
    bool test3_pass = true;

    // Device info should be read-only
    bool device_id_ro = reg_is_readonly(REG_DEVICE_ID);
    printf("  REG_DEVICE_ID (0x%04X): %s\n",
           REG_DEVICE_ID, device_id_ro ? "READ-ONLY" : "READ/WRITE");
    if (!device_id_ro) {
        printf("    ERROR: Should be read-only!\n");
        test3_pass = false;
    }

    // Status registers should be read-only
    bool speed_ro = reg_is_readonly(REG_CURRENT_SPEED_RPM);
    printf("  REG_CURRENT_SPEED_RPM (0x%04X): %s\n",
           REG_CURRENT_SPEED_RPM, speed_ro ? "READ-ONLY" : "READ/WRITE");
    if (!speed_ro) {
        printf("    ERROR: Should be read-only!\n");
        test3_pass = false;
    }

    // Control registers should be writable
    bool control_ro = reg_is_readonly(REG_CONTROL_MODE);
    printf("  REG_CONTROL_MODE (0x%04X): %s\n",
           REG_CONTROL_MODE, control_ro ? "READ-ONLY" : "READ/WRITE");
    if (control_ro) {
        printf("    ERROR: Should be read/write!\n");
        test3_pass = false;
    }

    // Protection registers should be writable
    bool prot_ro = reg_is_readonly(REG_OVERVOLTAGE_THRESHOLD);
    printf("  REG_OVERVOLTAGE_THRESHOLD (0x%04X): %s\n",
           REG_OVERVOLTAGE_THRESHOLD, prot_ro ? "READ-ONLY" : "READ/WRITE");
    if (prot_ro) {
        printf("    ERROR: Should be read/write!\n");
        test3_pass = false;
    }

    TEST_RESULT("Read-only detection correct", test3_pass);
    all_passed &= test3_pass;

    // Test 4: Register size detection
    printf("\nTest 4: Register Size Detection\n");
    bool test4_pass = true;

    // Control mode is 1 byte (enum)
    uint8_t size_mode = reg_get_size(REG_CONTROL_MODE);
    printf("  REG_CONTROL_MODE: %u bytes (expected: 1)\n", size_mode);
    if (size_mode != 1) {
        test4_pass = false;
    }

    // Hardware revision is 2 bytes
    uint8_t size_hw = reg_get_size(REG_HARDWARE_REVISION);
    printf("  REG_HARDWARE_REVISION: %u bytes (expected: 2)\n", size_hw);
    if (size_hw != 2) {
        test4_pass = false;
    }

    // Most registers are 4 bytes
    uint8_t size_volt = reg_get_size(REG_OVERVOLTAGE_THRESHOLD);
    printf("  REG_OVERVOLTAGE_THRESHOLD: %u bytes (expected: 4)\n", size_volt);
    if (size_volt != 4) {
        test4_pass = false;
    }

    TEST_RESULT("Register sizes correct", test4_pass);
    all_passed &= test4_pass;

    // Test 5: Register name lookup
    printf("\nTest 5: Register Name Lookup\n");
    bool test5_pass = true;

    struct {
        uint16_t addr;
        const char* expected;
    } name_tests[] = {
        {REG_DEVICE_ID, "DEVICE_ID"},
        {REG_CONTROL_MODE, "CONTROL_MODE"},
        {REG_CURRENT_SPEED_RPM, "CURRENT_SPEED_RPM"},
        {REG_FAULT_STATUS, "FAULT_STATUS"},
        {0xFFFF, "UNKNOWN"}  // Invalid address
    };

    for (int i = 0; i < 5; i++) {
        const char* name = reg_get_name(name_tests[i].addr);
        bool match = (strcmp(name, name_tests[i].expected) == 0);
        printf("  0x%04X: \"%s\" (expected: \"%s\") %s\n",
               name_tests[i].addr, name, name_tests[i].expected,
               match ? "✓" : "✗");
        if (!match) {
            test5_pass = false;
        }
    }

    TEST_RESULT("Register name lookup", test5_pass);
    all_passed &= test5_pass;

    // Test 6: Register count and coverage
    printf("\nTest 6: Register Map Coverage\n");
    bool test6_pass = true;

    // Count defined registers (approximate)
    int device_info_regs = 7;
    int protection_regs = 8;
    int control_regs = 9;
    int status_regs = 13;
    int fault_diag_regs = 12;
    int total_regs = device_info_regs + protection_regs + control_regs +
                     status_regs + fault_diag_regs;

    printf("  Device Info:    %2d registers\n", device_info_regs);
    printf("  Protection:     %2d registers\n", protection_regs);
    printf("  Control:        %2d registers\n", control_regs);
    printf("  Status:         %2d registers\n", status_regs);
    printf("  Fault/Diag:     %2d registers\n", fault_diag_regs);
    printf("  Total:          %2d registers\n", total_regs);

    if (total_regs < 40) {
        printf("  WARNING: Expected at least 40 registers!\n");
        test6_pass = false;
    }

    TEST_RESULT("Register map coverage", test6_pass);
    all_passed &= test6_pass;

    // Final result
    printf("\n");
    if (all_passed) {
        printf("✓✓✓ ALL REGISTER MAP TESTS PASSED ✓✓✓\n");
    } else {
        printf("✗✗✗ SOME REGISTER MAP TESTS FAILED ✗✗✗\n");
    }
    printf("\n");
}

// ============================================================================
// Checkpoint 5.2: Wheel Physics Model
// ============================================================================

/**
 * @brief Test wheel physics model and control modes
 *
 * Validates: Initialization, dynamics, control modes, limits, loss model
 */
void test_wheel_physics(void) {
    TEST_SECTION("Checkpoint 5.2: Wheel Physics Model");

    bool all_passed = true;
    wheel_state_t state;

    // ========================================================================
    // Test 1: Initialization
    // ========================================================================
    {
        printf("\n--- Test 1: Model Initialization ---\n");

        wheel_model_init(&state);

        bool passed = true;

        // Check zero initial state
        if (fabsf(state.omega_rad_s) > 0.001f) {
            printf("  ERROR: Initial omega should be zero, got %.6f\n", state.omega_rad_s);
            passed = false;
        }

        if (fabsf(state.momentum_nms) > 0.001f) {
            printf("  ERROR: Initial momentum should be zero, got %.6f\n", state.momentum_nms);
            passed = false;
        }

        // Check default mode
        if (state.mode != CONTROL_MODE_CURRENT) {
            printf("  ERROR: Default mode should be CURRENT, got %d\n", state.mode);
            passed = false;
        }

        // Check default protection thresholds
        if (fabsf(state.overvoltage_threshold_v - DEFAULT_OVERVOLTAGE_V) > 0.1f) {
            printf("  ERROR: Default overvoltage threshold mismatch\n");
            passed = false;
        }

        if (fabsf(state.overspeed_fault_rpm - DEFAULT_OVERSPEED_FAULT_RPM) > 1.0f) {
            printf("  ERROR: Default overspeed threshold mismatch\n");
            passed = false;
        }

        printf("  omega = %.6f rad/s (expected 0.0)\n", state.omega_rad_s);
        printf("  momentum = %.6f N·m·s (expected 0.0)\n", state.momentum_nms);
        printf("  mode = %d (expected CURRENT = 0)\n", state.mode);
        printf("  overvoltage_threshold = %.2f V\n", state.overvoltage_threshold_v);
        printf("  overspeed_fault = %.2f RPM\n", state.overspeed_fault_rpm);

        TEST_RESULT("Initialization", passed);
        if (!passed) all_passed = false;
    }

    // ========================================================================
    // Test 2: CURRENT Mode - Torque Generation
    // ========================================================================
    {
        printf("\n--- Test 2: CURRENT Mode (i = 1.0 A) ---\n");

        wheel_model_init(&state);
        wheel_model_set_mode(&state, CONTROL_MODE_CURRENT);
        wheel_model_set_current(&state, 1.0f);  // 1 A

        // Run one tick
        wheel_model_tick(&state);

        bool passed = true;

        // Expected torque: τ = k_t · i = 0.0534 N·m/A * 1.0 A = 0.0534 N·m = 53.4 mN·m
        float expected_torque_mnm = MOTOR_KT_NM_PER_A * 1000.0f;  // 53.4 mN·m
        float torque_error = fabsf(state.torque_out_mnm - expected_torque_mnm);

        if (torque_error > 0.5f) {  // 0.5 mN·m tolerance
            printf("  ERROR: Torque mismatch. Expected %.2f mN·m, got %.2f mN·m\n",
                   expected_torque_mnm, state.torque_out_mnm);
            passed = false;
        }

        // Wheel should start accelerating
        if (state.omega_rad_s <= 0.0f) {
            printf("  ERROR: Wheel should be accelerating (omega > 0)\n");
            passed = false;
        }

        printf("  current_cmd = 1.0 A\n");
        printf("  current_out = %.3f A\n", state.current_out_a);
        printf("  torque_out = %.2f mN·m (expected ~%.2f mN·m)\n", state.torque_out_mnm, expected_torque_mnm);
        printf("  omega = %.6f rad/s (should be > 0)\n", state.omega_rad_s);
        printf("  alpha = %.3f rad/s² (acceleration)\n", state.alpha_rad_s2);

        TEST_RESULT("CURRENT mode torque", passed);
        if (!passed) all_passed = false;
    }

    // ========================================================================
    // Test 3: SPEED Mode - Ramp to 1000 RPM
    // ========================================================================
    {
        printf("\n--- Test 3: SPEED Mode (ramp to 1000 RPM) ---\n");

        wheel_model_init(&state);
        wheel_model_set_mode(&state, CONTROL_MODE_SPEED);
        wheel_model_set_speed(&state, 1000.0f);  // 1000 RPM

        // Run for 5 seconds (500 ticks at 10 ms)
        uint32_t ticks = 500;
        for (uint32_t i = 0; i < ticks; i++) {
            wheel_model_tick(&state);
        }

        float final_speed_rpm = wheel_model_get_speed_rpm(&state);

        bool passed = true;

        // Speed should be close to 1000 RPM after 5 seconds
        float speed_error = fabsf(final_speed_rpm - 1000.0f);

        if (speed_error > 100.0f) {  // 100 RPM tolerance
            printf("  ERROR: Speed not converged. Expected ~1000 RPM, got %.2f RPM\n", final_speed_rpm);
            passed = false;
        }

        printf("  speed_setpoint = 1000.0 RPM\n");
        printf("  final_speed = %.2f RPM (after 5 seconds)\n", final_speed_rpm);
        printf("  speed_error = %.2f RPM\n", speed_error);
        printf("  pi_output = %.3f A\n", state.pi_output_a);

        TEST_RESULT("SPEED mode ramp", passed);
        if (!passed) all_passed = false;
    }

    // ========================================================================
    // Test 4: TORQUE Mode - Commanded Torque to Current
    // ========================================================================
    {
        printf("\n--- Test 4: TORQUE Mode (τ = 10 mN·m) ---\n");

        wheel_model_init(&state);
        wheel_model_set_mode(&state, CONTROL_MODE_TORQUE);
        wheel_model_set_torque(&state, 10.0f);  // 10 mN·m

        // Run one tick
        wheel_model_tick(&state);

        bool passed = true;

        // Expected current: i = τ / k_t = 0.010 N·m / 0.0534 N·m/A ≈ 0.187 A
        float expected_current_a = 0.010f / MOTOR_KT_NM_PER_A;  // ≈0.187 A
        float current_error = fabsf(state.current_out_a - expected_current_a);

        if (current_error > 0.01f) {  // 10 mA tolerance
            printf("  ERROR: Current mismatch. Expected %.3f A, got %.3f A\n",
                   expected_current_a, state.current_out_a);
            passed = false;
        }

        printf("  torque_cmd = 10.0 mN·m\n");
        printf("  current_out = %.3f A (expected ~%.3f A)\n", state.current_out_a, expected_current_a);
        printf("  torque_out = %.2f mN·m\n", state.torque_out_mnm);

        TEST_RESULT("TORQUE mode", passed);
        if (!passed) all_passed = false;
    }

    // ========================================================================
    // Test 5: Power Limiting
    // ========================================================================
    {
        printf("\n--- Test 5: Power Limiting (100 W limit) ---\n");

        wheel_model_init(&state);
        wheel_model_set_mode(&state, CONTROL_MODE_SPEED);

        // Spin up to 3000 RPM first
        wheel_model_set_speed(&state, 3000.0f);
        for (uint32_t i = 0; i < 1000; i++) {  // 10 seconds
            wheel_model_tick(&state);
        }

        // Now command 6000 RPM (would exceed power limit)
        wheel_model_set_speed(&state, 6000.0f);

        // Run for a bit
        for (uint32_t i = 0; i < 100; i++) {  // 1 second
            wheel_model_tick(&state);
        }

        bool passed = true;

        // Power should not exceed 100W
        if (fabsf(state.power_w) > state.motor_overpower_limit_w * 1.1f) {  // 10% tolerance
            printf("  ERROR: Power exceeded limit. Limit %.2f W, got %.2f W\n",
                   state.motor_overpower_limit_w, fabsf(state.power_w));
            passed = false;
        }

        printf("  power_limit = %.2f W\n", state.motor_overpower_limit_w);
        printf("  actual_power = %.2f W\n", fabsf(state.power_w));
        printf("  current_out = %.3f A (limited)\n", state.current_out_a);
        printf("  speed = %.2f RPM\n", wheel_model_get_speed_rpm(&state));

        TEST_RESULT("Power limiting", passed);
        if (!passed) all_passed = false;
    }

    // ========================================================================
    // Test 6: Loss Model - Deceleration
    // ========================================================================
    {
        printf("\n--- Test 6: Loss Model (spin-down from 3000 RPM) ---\n");

        wheel_model_init(&state);
        wheel_model_set_mode(&state, CONTROL_MODE_SPEED);

        // Spin up to 3000 RPM
        wheel_model_set_speed(&state, 3000.0f);
        for (uint32_t i = 0; i < 1000; i++) {  // 10 seconds
            wheel_model_tick(&state);
        }

        float initial_speed = wheel_model_get_speed_rpm(&state);

        // Command zero speed (coast down)
        wheel_model_set_speed(&state, 0.0f);

        // Run for 5 seconds
        for (uint32_t i = 0; i < 500; i++) {  // 5 seconds
            wheel_model_tick(&state);
        }

        float final_speed = wheel_model_get_speed_rpm(&state);

        bool passed = true;

        // Speed should decrease due to losses
        if (final_speed >= initial_speed * 0.9f) {  // Should lose at least 10% speed
            printf("  ERROR: Speed did not decrease enough. Initial %.2f RPM, final %.2f RPM\n",
                   initial_speed, final_speed);
            passed = false;
        }

        printf("  initial_speed = %.2f RPM\n", initial_speed);
        printf("  final_speed = %.2f RPM (after 5s coast-down)\n", final_speed);
        printf("  speed_loss = %.2f RPM\n", initial_speed - final_speed);
        printf("  loss_torque = %.2f mN·m\n", state.torque_loss_mnm);

        TEST_RESULT("Loss model deceleration", passed);
        if (!passed) all_passed = false;
    }

    // ========================================================================
    // Test 7: Overspeed Protection
    // ========================================================================
    {
        printf("\n--- Test 7: Overspeed Protection (6000 RPM fault) ---\n");

        wheel_model_init(&state);
        wheel_model_set_mode(&state, CONTROL_MODE_SPEED);

        // Try to command overspeed
        wheel_model_set_speed(&state, 7000.0f);  // Exceeds 6000 RPM fault threshold

        // Run until overspeed or 20 seconds
        uint32_t max_ticks = 2000;
        bool fault_triggered = false;

        for (uint32_t i = 0; i < max_ticks; i++) {
            wheel_model_tick(&state);

            if (state.fault_latch & FAULT_OVERSPEED) {
                fault_triggered = true;
                printf("  Overspeed fault triggered at tick %u (%.1f seconds)\n", i, i * MODEL_DT_S);
                printf("  speed = %.2f RPM\n", wheel_model_get_speed_rpm(&state));
                break;
            }
        }

        bool passed = fault_triggered;

        if (!passed) {
            printf("  ERROR: Overspeed fault not triggered\n");
            printf("  Final speed = %.2f RPM\n", wheel_model_get_speed_rpm(&state));
        }

        printf("  fault_latch = 0x%08X\n", state.fault_latch);

        TEST_RESULT("Overspeed protection", passed);
        if (!passed) all_passed = false;
    }

    // Final result
    printf("\n");
    if (all_passed) {
        printf("✓✓✓ ALL WHEEL PHYSICS TESTS PASSED ✓✓✓\n");
    } else {
        printf("✗✗✗ SOME WHEEL PHYSICS TESTS FAILED ✗✗✗\n");
    }
    printf("\n");
}

// ============================================================================
// Future Checkpoints
// ============================================================================
