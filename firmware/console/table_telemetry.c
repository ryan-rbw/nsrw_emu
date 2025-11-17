/**
 * @file table_telemetry.c
 * @brief NSP Telemetry Table Implementation
 *
 * Table 7: NSP Telemetry Status (for RS-485 telemetry blocks)
 *
 * This table tracks NSP telemetry block metadata (sequence numbers,
 * timestamps, packet counts) when RS-485 is active.
 *
 * NOTE: For live physics values, see Table 10 (Core1 Physics Stats).
 */

#include "table_telemetry.h"
#include "tables.h"
#include <stdio.h>

// ============================================================================
// NSP Telemetry Metadata (Stubs - will be connected when RS-485 is active)
// ============================================================================

static volatile uint32_t telem_last_block_id = 0;      // Last received NSP block ID
static volatile uint32_t telem_sequence_num = 0;       // Sequence number
static volatile uint32_t telem_rx_count = 0;           // Total blocks received
static volatile uint32_t telem_tx_count = 0;           // Total blocks transmitted
static volatile uint32_t telem_last_rx_time_ms = 0;    // Last RX timestamp (ms)
static volatile uint32_t telem_last_tx_time_ms = 0;    // Last TX timestamp (ms)

// ============================================================================
// Field Definitions
// ============================================================================

static const field_meta_t telemetry_fields[] = {
    {
        .id = 701,
        .name = "last_block_id",
        .type = FIELD_TYPE_HEX,
        .units = "",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&telem_last_block_id,
        .dirty = false,
        .enum_values = NULL,
        .enum_count = 0,
    },
    {
        .id = 702,
        .name = "sequence_num",
        .type = FIELD_TYPE_U32,
        .units = "",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&telem_sequence_num,
        .dirty = false,
        .enum_values = NULL,
        .enum_count = 0,
    },
    {
        .id = 703,
        .name = "rx_count",
        .type = FIELD_TYPE_U32,
        .units = "blocks",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&telem_rx_count,
        .dirty = false,
        .enum_values = NULL,
        .enum_count = 0,
    },
    {
        .id = 704,
        .name = "tx_count",
        .type = FIELD_TYPE_U32,
        .units = "blocks",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&telem_tx_count,
        .dirty = false,
        .enum_values = NULL,
        .enum_count = 0,
    },
    {
        .id = 705,
        .name = "last_rx_time_ms",
        .type = FIELD_TYPE_U32,
        .units = "ms",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&telem_last_rx_time_ms,
        .dirty = false,
        .enum_values = NULL,
        .enum_count = 0,
    },
    {
        .id = 706,
        .name = "last_tx_time_ms",
        .type = FIELD_TYPE_U32,
        .units = "ms",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&telem_last_tx_time_ms,
        .dirty = false,
        .enum_values = NULL,
        .enum_count = 0,
    },
};

// ============================================================================
// Table Definition
// ============================================================================

static const table_meta_t telemetry_table = {
    .id = 7,
    .name = "NSP Telemetry",
    .description = "RS-485 telemetry block metadata (sequence, counts)",
    .fields = telemetry_fields,
    .field_count = sizeof(telemetry_fields) / sizeof(telemetry_fields[0]),
};

// ============================================================================
// Initialization
// ============================================================================

void table_telemetry_init(void) {
    // Register table with catalog
    catalog_register_table(&telemetry_table);
}
