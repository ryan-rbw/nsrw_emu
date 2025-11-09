/**
 * @file table_protection_status.c
 * @brief Protection Status Table Implementation
 *
 * Table 7: Protection Status (fault/warning flags - all RO)
 */

#include "table_protection_status.h"
#include "tables.h"
#include "../device/nss_nrwa_t6_regs.h"
#include <stdio.h>

// ============================================================================
// Live Data (Stubs - will be connected to wheel_state in future)
// ============================================================================

static volatile uint32_t prot_flags = 0;                  // Fault flags
static volatile uint32_t prot_warnings = 0;               // Warning flags

// ============================================================================
// Field Definitions
// ============================================================================

static const field_meta_t protection_status_fields[] = {
    {
        .id = 608,
        .name = "fault_flags",
        .type = FIELD_TYPE_HEX,
        .units = "",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&prot_flags,
        .dirty = false,
        .enum_values = NULL,
        .enum_count = 0,
    },
    {
        .id = 609,
        .name = "warning_flags",
        .type = FIELD_TYPE_HEX,
        .units = "",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = (volatile uint32_t*)&prot_warnings,
        .dirty = false,
        .enum_values = NULL,
        .enum_count = 0,
    },
};

// ============================================================================
// Table Definition
// ============================================================================

static const table_meta_t protection_status_table = {
    .id = 7,
    .name = "Protection Status",
    .description = "Fault and warning flags",
    .fields = protection_status_fields,
    .field_count = sizeof(protection_status_fields) / sizeof(protection_status_fields[0]),
};

// ============================================================================
// Initialization
// ============================================================================

void table_protection_status_init(void) {
    // Register table with catalog
    catalog_register_table(&protection_status_table);
}
