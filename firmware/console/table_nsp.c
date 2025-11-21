/**
 * @file table_nsp.c
 * @brief NSP Layer Table Implementation
 *
 * Table 3: NSP Layer (last cmd/reply, poll, ack, stats, timing)
 */

#include "table_nsp.h"
#include "tables.h"
#include "../nsp_handler.h"
#include <stdio.h>

// ============================================================================
// Live Data (Connected to NSP Handler)
// ============================================================================

static volatile uint32_t nsp_rx_bytes = 0;            // Total bytes received
static volatile uint32_t nsp_rx_packets = 0;          // Valid packets received
static volatile uint32_t nsp_tx_packets = 0;          // Packets transmitted
static volatile uint32_t nsp_slip_errors = 0;         // SLIP framing errors
static volatile uint32_t nsp_parse_errors = 0;        // NSP parse errors
static volatile uint32_t nsp_wrong_addr = 0;          // Wrong address packets
static volatile uint32_t nsp_cmd_errors = 0;          // Command dispatch errors
static volatile uint32_t nsp_total_errors = 0;        // Total errors

// Last error details
static volatile uint32_t nsp_last_parse_error = 0;    // Last parse error code
static volatile uint32_t nsp_last_cmd_error = 0;      // Last command error code
static volatile uint32_t nsp_last_frame_len = 0;      // Last frame length in bytes

// Last RX command (formatted as hex string)
static char last_rx_cmd_str[64] = "-";  // Format: "01,00,82,..." or "-" if none

// Enum string lookup for parse error codes (index = error code)
static const char* parse_error_strings[] = {
    "NONE",         // 0
    "TOO_SHORT",    // 1
    "BAD_LENGTH",   // 2
    "BAD_CRC",      // 3
    "NULL_PTR",     // 4
};

// ============================================================================
// Field Definitions
// ============================================================================

static const field_meta_t nsp_fields[] = {
    {
        .id = 301,
        .name = "rx_bytes",
        .type = FIELD_TYPE_U32,
        .units = "bytes",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&nsp_rx_bytes,
        .dirty = false,
        .enum_values = NULL,
        .enum_count = 0,
    },
    {
        .id = 302,
        .name = "rx_packets",
        .type = FIELD_TYPE_U32,
        .units = "pkts",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&nsp_rx_packets,
        .dirty = false,
        .enum_values = NULL,
        .enum_count = 0,
    },
    {
        .id = 303,
        .name = "tx_packets",
        .type = FIELD_TYPE_U32,
        .units = "pkts",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&nsp_tx_packets,
        .dirty = false,
        .enum_values = NULL,
        .enum_count = 0,
    },
    {
        .id = 304,
        .name = "slip_errors",
        .type = FIELD_TYPE_U32,
        .units = "errs",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&nsp_slip_errors,
        .dirty = false,
        .enum_values = NULL,
        .enum_count = 0,
    },
    {
        .id = 305,
        .name = "parse_errors",
        .type = FIELD_TYPE_U32,
        .units = "errs",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&nsp_parse_errors,
        .dirty = false,
        .enum_values = NULL,
        .enum_count = 0,
    },
    {
        .id = 306,
        .name = "wrong_addr",
        .type = FIELD_TYPE_U32,
        .units = "pkts",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&nsp_wrong_addr,
        .dirty = false,
        .enum_values = NULL,
        .enum_count = 0,
    },
    {
        .id = 307,
        .name = "cmd_errors",
        .type = FIELD_TYPE_U32,
        .units = "errs",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&nsp_cmd_errors,
        .dirty = false,
        .enum_values = NULL,
        .enum_count = 0,
    },
    {
        .id = 308,
        .name = "total_errors",
        .type = FIELD_TYPE_U32,
        .units = "errs",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&nsp_total_errors,
        .dirty = false,
        .enum_values = NULL,
        .enum_count = 0,
    },
    {
        .id = 309,
        .name = "last_parse_err",
        .type = FIELD_TYPE_ENUM,
        .units = "",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&nsp_last_parse_error,
        .dirty = false,
        .enum_values = parse_error_strings,
        .enum_count = sizeof(parse_error_strings) / sizeof(parse_error_strings[0]),
    },
    {
        .id = 310,
        .name = "last_cmd_err",
        .type = FIELD_TYPE_U32,
        .units = "0xNN",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&nsp_last_cmd_error,
        .dirty = false,
        .enum_values = NULL,
        .enum_count = 0,
    },
    {
        .id = 311,
        .name = "last_frame_len",
        .type = FIELD_TYPE_U32,
        .units = "bytes",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&nsp_last_frame_len,
        .dirty = false,
        .enum_values = NULL,
        .enum_count = 0,
    },
};

// ============================================================================
// Table Definition
// ============================================================================

static const table_meta_t nsp_table = {
    .id = 3,
    .name = "NSP Stats",
    .description = "RX/TX packets, errors breakdown",
    .fields = nsp_fields,
    .field_count = sizeof(nsp_fields) / sizeof(nsp_fields[0]),
};

// ============================================================================
// Initialization
// ============================================================================

void table_nsp_init(void) {
    // Register table with catalog
    catalog_register_table(&nsp_table);
}

// ============================================================================
// Update Function
// ============================================================================

void table_nsp_update(void) {
    // Fetch latest stats from NSP handler
    uint32_t rx_b, rx_p, tx_p, slip_e, nsp_e, wrong_a, cmd_e, total_e;

    nsp_handler_get_detailed_stats(&rx_b, &rx_p, &tx_p, &slip_e,
                                    &nsp_e, &wrong_a, &cmd_e, &total_e);

    // Update table fields
    nsp_rx_bytes = rx_b;
    nsp_rx_packets = rx_p;
    nsp_tx_packets = tx_p;
    nsp_slip_errors = slip_e;
    nsp_parse_errors = nsp_e;
    nsp_wrong_addr = wrong_a;
    nsp_cmd_errors = cmd_e;
    nsp_total_errors = total_e;

    // Fetch error details
    uint32_t last_parse, last_cmd, frame_len;
    nsp_handler_get_error_details(&last_parse, &last_cmd);
    nsp_handler_get_last_frame(NULL, 0, &frame_len);  // Just get length

    nsp_last_parse_error = last_parse;
    nsp_last_cmd_error = last_cmd;
    nsp_last_frame_len = frame_len;

    // Fetch and format last RX command
    uint8_t cmd_bytes[16];
    uint32_t cmd_len;
    nsp_handler_get_last_rx_cmd(cmd_bytes, sizeof(cmd_bytes), &cmd_len);

    if (cmd_len == 0) {
        snprintf(last_rx_cmd_str, sizeof(last_rx_cmd_str), "-");
    } else {
        char *ptr = last_rx_cmd_str;
        size_t remaining = sizeof(last_rx_cmd_str);
        for (uint32_t i = 0; i < cmd_len && i < 16; i++) {
            int written = snprintf(ptr, remaining, "%s%02X", (i == 0) ? "" : ",", cmd_bytes[i]);
            if (written < 0 || (size_t)written >= remaining) break;
            ptr += written;
            remaining -= written;
        }
    }
}

const char* table_nsp_get_last_rx_cmd_str(void) {
    return last_rx_cmd_str;
}
