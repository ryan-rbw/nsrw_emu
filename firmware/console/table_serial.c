/**
 * @file table_serial.c
 * @brief Serial Interface Table Implementation
 *
 * Table 2: Serial Interface (RS-485, SLIP, CRC statistics)
 */

#include "table_serial.h"
#include "tables.h"
#include "../nsp_handler.h"
#include <stdio.h>

// ============================================================================
// Live Data (Connected to NSP Handler)
// ============================================================================

static volatile uint32_t serial_status = 1;           // 1 = active, 0 = inactive
static volatile uint32_t serial_tx_count = 0;         // Total TX bytes
static volatile uint32_t serial_rx_count = 0;         // Total RX bytes
static volatile uint32_t serial_slip_frames_ok = 0;   // Valid SLIP frames decoded
static volatile uint32_t serial_slip_errors = 0;      // SLIP framing errors
static volatile uint32_t serial_baud_kbps = 4608;     // 460.8 kbps (× 10)

// ============================================================================
// Field Definitions
// ============================================================================

static const field_meta_t serial_fields[] = {
    {
        .id = 201,
        .name = "status",
        .type = FIELD_TYPE_BOOL,
        .units = "",
        .access = FIELD_ACCESS_RO,
        .default_val = 1,
        .ptr = (volatile uint32_t*)&serial_status,
        .dirty = false,
        .enum_values = NULL,
        .enum_count = 0,
    },
    {
        .id = 202,
        .name = "tx_count",
        .type = FIELD_TYPE_U32,
        .units = "bytes",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&serial_tx_count,
        .dirty = false,
        .enum_values = NULL,
        .enum_count = 0,
    },
    {
        .id = 203,
        .name = "rx_count",
        .type = FIELD_TYPE_U32,
        .units = "bytes",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&serial_rx_count,
        .dirty = false,
        .enum_values = NULL,
        .enum_count = 0,
    },
    {
        .id = 204,
        .name = "slip_frames_ok",
        .type = FIELD_TYPE_U32,
        .units = "frames",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&serial_slip_frames_ok,
        .dirty = false,
        .enum_values = NULL,
        .enum_count = 0,
    },
    {
        .id = 205,
        .name = "slip_errors",
        .type = FIELD_TYPE_U32,
        .units = "errs",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&serial_slip_errors,
        .dirty = false,
        .enum_values = NULL,
        .enum_count = 0,
    },
    {
        .id = 206,
        .name = "baud_kbps",
        .type = FIELD_TYPE_U32,
        .units = "kbps×10",
        .access = FIELD_ACCESS_RO,
        .default_val = 4608,
        .ptr = (volatile uint32_t*)&serial_baud_kbps,
        .dirty = false,
        .enum_values = NULL,
        .enum_count = 0,
    },
};

// ============================================================================
// Table Definition
// ============================================================================

static const table_meta_t serial_table = {
    .id = 2,
    .name = "Serial Status",
    .description = "RS-485, SLIP, CRC statistics",
    .fields = serial_fields,
    .field_count = sizeof(serial_fields) / sizeof(serial_fields[0]),
};

// ============================================================================
// Initialization
// ============================================================================

void table_serial_init(void) {
    // Register table with catalog
    catalog_register_table(&serial_table);
}

// ============================================================================
// Update Function
// ============================================================================

void table_serial_update(void) {
    // Fetch latest stats from NSP handler (serial layer)
    uint32_t rx_b, tx_b, slip_ok, slip_err;

    nsp_handler_get_serial_stats(&rx_b, &tx_b, &slip_ok, &slip_err);

    // Update table fields
    serial_rx_count = rx_b;
    serial_tx_count = tx_b;
    serial_slip_frames_ok = slip_ok;
    serial_slip_errors = slip_err;

    // Status is active if we've received any bytes
    serial_status = (rx_b > 0 || tx_b > 0) ? 1 : 0;
}
