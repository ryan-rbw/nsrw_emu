/**
 * @file table_nsp.c
 * @brief NSP Layer Table Implementation
 *
 * Table 3: NSP Layer (last cmd/reply, poll, ack, stats, timing)
 */

#include "table_nsp.h"
#include "tables.h"
#include <stdio.h>

// ============================================================================
// Live Data (Stubs - will be connected to actual NSP driver in future)
// ============================================================================

static volatile uint32_t nsp_last_cmd = 0x00;         // Last command received
static volatile uint32_t nsp_poll_seen = 0;           // Poll bit seen (bool)
static volatile uint32_t nsp_ack_bit = 0;             // ACK bit status (bool)
static volatile uint32_t nsp_cmd_count = 0;           // Total commands processed
static volatile uint32_t nsp_reply_count = 0;         // Total replies sent
static volatile uint32_t nsp_last_timing_ms = 0;      // Last cmd processing time (ms)

// ============================================================================
// Field Definitions
// ============================================================================

static const field_meta_t nsp_fields[] = {
    {
        .id = 301,
        .name = "last_cmd",
        .type = FIELD_TYPE_HEX,
        .units = "",
        .access = FIELD_ACCESS_RO,
        .default_val = 0x00,
        .ptr = (volatile uint32_t*)&nsp_last_cmd,
        .dirty = false,
        .enum_values = NULL,
        .enum_count = 0,
    },
    {
        .id = 302,
        .name = "poll_seen",
        .type = FIELD_TYPE_BOOL,
        .units = "",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&nsp_poll_seen,
        .dirty = false,
        .enum_values = NULL,
        .enum_count = 0,
    },
    {
        .id = 303,
        .name = "ack_bit",
        .type = FIELD_TYPE_BOOL,
        .units = "",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&nsp_ack_bit,
        .dirty = false,
        .enum_values = NULL,
        .enum_count = 0,
    },
    {
        .id = 304,
        .name = "cmd_count",
        .type = FIELD_TYPE_U32,
        .units = "cmds",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&nsp_cmd_count,
        .dirty = false,
        .enum_values = NULL,
        .enum_count = 0,
    },
    {
        .id = 305,
        .name = "reply_count",
        .type = FIELD_TYPE_U32,
        .units = "replies",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&nsp_reply_count,
        .dirty = false,
        .enum_values = NULL,
        .enum_count = 0,
    },
    {
        .id = 306,
        .name = "last_timing_ms",
        .type = FIELD_TYPE_U32,
        .units = "ms",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&nsp_last_timing_ms,
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
    .name = "NSP Status",
    .description = "Last cmd/reply, poll, ack, stats",
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
