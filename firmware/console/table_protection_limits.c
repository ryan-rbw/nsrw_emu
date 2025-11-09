/**
 * @file table_protection_limits.c
 * @brief Protection Limits Table Implementation
 *
 * Table 6: Protection Limits (configurable thresholds - all RW)
 */

#include "table_protection_limits.h"
#include "tables.h"
#include "../device/nss_nrwa_t6_regs.h"
#include <stdio.h>

// ============================================================================
// Live Data (Stubs - will be connected to wheel_state in future)
// ============================================================================

static volatile uint32_t prot_overvolt_v = 36000;         // 36V (mV)
static volatile uint32_t prot_overspeed_rpm = 6000;       // 6000 RPM (latched)
static volatile uint32_t prot_soft_overspeed_rpm = 5000;  // 5000 RPM (soft)
static volatile uint32_t prot_overcurr_a = 6000;          // 6A (mA)
static volatile uint32_t prot_soft_overcurr_a = 5000;     // 5A (mA, soft)
static volatile uint32_t prot_overpower_w = 100000;       // 100W (mW)
static volatile uint32_t prot_max_duty_pct = 9785;        // 97.85% (× 100)

// ============================================================================
// Field Definitions
// ============================================================================

static const field_meta_t protection_limits_fields[] = {
    {
        .id = 601,
        .name = "overvolt_v",
        .type = FIELD_TYPE_U32,
        .units = "mV",
        .access = FIELD_ACCESS_RW,
        .default_val = 36000,
        .ptr = (volatile uint32_t*)&prot_overvolt_v,
        .dirty = false,
    },
    {
        .id = 602,
        .name = "overspeed_rpm",
        .type = FIELD_TYPE_U32,
        .units = "RPM",
        .access = FIELD_ACCESS_RW,
        .default_val = 6000,
        .ptr = (volatile uint32_t*)&prot_overspeed_rpm,
        .dirty = false,
    },
    {
        .id = 603,
        .name = "soft_overspeed_rpm",
        .type = FIELD_TYPE_U32,
        .units = "RPM",
        .access = FIELD_ACCESS_RW,
        .default_val = 5000,
        .ptr = (volatile uint32_t*)&prot_soft_overspeed_rpm,
        .dirty = false,
    },
    {
        .id = 604,
        .name = "overcurr_a",
        .type = FIELD_TYPE_U32,
        .units = "mA",
        .access = FIELD_ACCESS_RW,
        .default_val = 6000,
        .ptr = (volatile uint32_t*)&prot_overcurr_a,
        .dirty = false,
    },
    {
        .id = 605,
        .name = "soft_overcurr_a",
        .type = FIELD_TYPE_U32,
        .units = "mA",
        .access = FIELD_ACCESS_RW,
        .default_val = 5000,
        .ptr = (volatile uint32_t*)&prot_soft_overcurr_a,
        .dirty = false,
    },
    {
        .id = 606,
        .name = "overpower_w",
        .type = FIELD_TYPE_U32,
        .units = "mW",
        .access = FIELD_ACCESS_RW,
        .default_val = 100000,
        .ptr = (volatile uint32_t*)&prot_overpower_w,
        .dirty = false,
    },
    {
        .id = 607,
        .name = "max_duty_pct",
        .type = FIELD_TYPE_U32,
        .units = "%×100",
        .access = FIELD_ACCESS_RW,
        .default_val = 9785,
        .ptr = (volatile uint32_t*)&prot_max_duty_pct,
        .dirty = false,
    },
};

// ============================================================================
// Table Definition
// ============================================================================

static const table_meta_t protection_limits_table = {
    .id = 6,
    .name = "Protection Limits",
    .description = "Configurable thresholds",
    .fields = protection_limits_fields,
    .field_count = sizeof(protection_limits_fields) / sizeof(protection_limits_fields[0]),
};

// ============================================================================
// Initialization
// ============================================================================

void table_protection_limits_init(void) {
    // Register table with catalog
    catalog_register_table(&protection_limits_table);
}
