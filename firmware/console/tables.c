/**
 * @file tables.c
 * @brief Table/Field Catalog Implementation
 *
 * Modular registration-based catalog system.
 * Tables register themselves at initialization.
 */

#include "tables.h"
#include "table_tests.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>

// ============================================================================
// Catalog Storage
// ============================================================================

// Array of registered tables (pointers to static metadata)
static const table_meta_t* catalog[CATALOG_MAX_TABLES];
static uint8_t catalog_count = 0;

// ============================================================================
// Initialization
// ============================================================================

void catalog_init(void) {
    // Clear catalog
    memset(catalog, 0, sizeof(catalog));
    catalog_count = 0;

    // Register tables (in menu order)
    table_tests_init();

    // TODO: Checkpoint 8.2 will add remaining 6 tables
    // - Serial Interface
    // - NSP Layer
    // - Control Mode
    // - Dynamics
    // - Protections
    // - Telemetry Blocks
    // - Config & JSON

    printf("[CATALOG] Initialized with %d tables\n", catalog_count);
}

// ============================================================================
// Registration
// ============================================================================

bool catalog_register_table(const table_meta_t* table) {
    if (!table) {
        printf("[CATALOG] ERROR: NULL table pointer\n");
        return false;
    }

    if (catalog_count >= CATALOG_MAX_TABLES) {
        printf("[CATALOG] ERROR: Catalog full (%d tables)\n", CATALOG_MAX_TABLES);
        return false;
    }

    catalog[catalog_count++] = table;
    printf("[CATALOG] Registered table: %s (%d fields)\n",
           table->name, table->field_count);
    return true;
}

// ============================================================================
// Query API
// ============================================================================

uint8_t catalog_get_table_count(void) {
    return catalog_count;
}

const table_meta_t* catalog_get_table_by_index(uint8_t index) {
    if (index >= catalog_count) {
        return NULL;
    }
    return catalog[index];
}

const table_meta_t* catalog_get_table_by_name(const char* name) {
    if (!name) {
        return NULL;
    }

    for (uint8_t i = 0; i < catalog_count; i++) {
        if (strcasecmp(catalog[i]->name, name) == 0) {
            return catalog[i];
        }
    }

    return NULL;
}

const field_meta_t* catalog_get_field(const table_meta_t* table, uint8_t field_index) {
    if (!table || field_index >= table->field_count) {
        return NULL;
    }
    return &table->fields[field_index];
}

const field_meta_t* catalog_get_field_by_name(const table_meta_t* table, const char* name) {
    if (!table || !name) {
        return NULL;
    }

    for (uint8_t i = 0; i < table->field_count; i++) {
        if (strcasecmp(table->fields[i].name, name) == 0) {
            return &table->fields[i];
        }
    }

    return NULL;
}

// ============================================================================
// Field Access (Stubs - will be implemented in Checkpoint 8.2)
// ============================================================================

bool catalog_read_field(const field_meta_t* field, float* value) {
    if (!field || !value) {
        return false;
    }

    // TODO: Implement type-specific decoding
    if (field->ptr) {
        *value = (float)(*field->ptr);
        return true;
    }

    return false;
}

bool catalog_write_field(const field_meta_t* field, float value) {
    if (!field) {
        return false;
    }

    // TODO: Implement type-specific encoding and write
    if (field->ptr && field->access != FIELD_ACCESS_RO) {
        *(volatile uint32_t*)field->ptr = (uint32_t)value;
        return true;
    }

    return false;
}

// ============================================================================
// Defaults Tracking (Stubs - will be implemented in Checkpoint 8.2)
// ============================================================================

uint16_t catalog_get_dirty_fields(char* out_buf, size_t buflen) {
    if (!out_buf || buflen == 0) {
        return 0;
    }

    // TODO: Implement dirty tracking
    out_buf[0] = '\0';
    return 0;
}

uint16_t catalog_restore_defaults(const table_meta_t* table, const field_meta_t* field) {
    // TODO: Implement defaults restoration
    return 0;
}

// ============================================================================
// Value Formatting (Stubs - will be enhanced in Checkpoint 8.2)
// ============================================================================

void catalog_format_value(const field_meta_t* field, uint32_t value, char* buf, size_t buflen) {
    if (!field || !buf || buflen == 0) {
        return;
    }

    // Simple formatter (will be enhanced with proper UQ decoding)
    switch (field->type) {
        case FIELD_TYPE_BOOL:
            snprintf(buf, buflen, "%s", value ? "true" : "false");
            break;

        case FIELD_TYPE_HEX:
            snprintf(buf, buflen, "0x%08lX", (unsigned long)value);
            break;

        case FIELD_TYPE_FLOAT:
            // Interpret as IEEE 754 float
            {
                float f;
                memcpy(&f, &value, sizeof(float));
                snprintf(buf, buflen, "%.2f", f);
            }
            break;

        default:
            // Generic integer display
            snprintf(buf, buflen, "%lu", (unsigned long)value);
            break;
    }
}

bool catalog_parse_value(const field_meta_t* field, const char* str, uint32_t* value) {
    if (!field || !str || !value) {
        return false;
    }

    // TODO: Implement type-specific parsing
    *value = 0;
    return false;
}
