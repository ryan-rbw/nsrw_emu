/**
 * @file table_serial.c
 * @brief Serial Interface Table Implementation
 *
 * Table 2: Serial Interface (RS-485, SLIP, CRC statistics)
 */

#include "table_serial.h"
#include "tables.h"
#include <stdio.h>

// ============================================================================
// Live Data (Stubs - will be connected to actual drivers in future)
// ============================================================================

static volatile uint32_t serial_status = 1;           // 1 = active, 0 = inactive
static volatile uint32_t serial_tx_count = 0;         // Total TX bytes
static volatile uint32_t serial_rx_count = 0;         // Total RX bytes
static volatile uint32_t serial_tx_errors = 0;        // TX error count
static volatile uint32_t serial_rx_errors = 0;        // RX error count
static volatile uint32_t serial_baud_kbps = 4608;     // 460.8 kbps (× 10)
static volatile uint32_t serial_slip_frames_ok = 0;   // Valid SLIP frames decoded
static volatile uint32_t serial_crc_errors = 0;       // CRC mismatches

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
    },
    {
        .id = 204,
        .name = "tx_errors",
        .type = FIELD_TYPE_U32,
        .units = "errors",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&serial_tx_errors,
        .dirty = false,
    },
    {
        .id = 205,
        .name = "rx_errors",
        .type = FIELD_TYPE_U32,
        .units = "errors",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&serial_rx_errors,
        .dirty = false,
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
    },
    {
        .id = 207,
        .name = "slip_frames_ok",
        .type = FIELD_TYPE_U32,
        .units = "frames",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&serial_slip_frames_ok,
        .dirty = false,
    },
    {
        .id = 208,
        .name = "crc_errors",
        .type = FIELD_TYPE_U32,
        .units = "errors",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&serial_crc_errors,
        .dirty = false,
    },
};

// ============================================================================
// Table Definition
// ============================================================================

static const table_meta_t serial_table = {
    .id = 2,
    .name = "Serial Interface",
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
