/**
 * @file tables.c
 * @brief Table/Field Catalog Implementation
 *
 * Modular registration-based catalog system.
 * Tables register themselves at initialization.
 */

#include "tables.h"
#include "table_tests.h"
#include "table_serial.h"
#include "table_nsp.h"
#include "table_control.h"
#include "table_protection_limits.h"
#include "table_protection_status.h"
#include "table_telemetry.h"
#include "table_config.h"
#include "table_fault_injection.h"
#include "table_core1_stats.h"
#include "table_test_modes.h"
#include <string.h>
#include <math.h>
#include <strings.h>  // For strcasecmp
#include <stdio.h>
#include <stdlib.h>   // For atoi
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
    table_serial_init();
    table_nsp_init();
    table_control_init();
    table_protection_limits_init();
    table_protection_status_init();
    table_telemetry_init();
    table_config_init();
    table_fault_injection_init();
    table_core1_stats_init();
    table_test_modes_init();

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

    // Basic implementation - reads uint32_t as float
    // Type-specific decoding not needed for current phase
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

    // Basic implementation - writes float as uint32_t
    // Type-specific encoding delegated to TUI field edit handler
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

    // Dirty tracking not implemented - not required for Phase 10
    // Future: Track which fields differ from defaults
    out_buf[0] = '\0';
    return 0;
}

uint16_t catalog_restore_defaults(const table_meta_t* table, const field_meta_t* field) {
    // Defaults restoration not implemented - not required for Phase 10
    // Future: Reset fields to default_val from metadata
    (void)table;
    (void)field;
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
            snprintf(buf, buflen, "%s", value ? "TRUE" : "FALSE");
            break;

        case FIELD_TYPE_HEX:
            snprintf(buf, buflen, "0x%08lX", (unsigned long)value);
            break;

        case FIELD_TYPE_ENUM:
            // Display enum as UPPERCASE string
            if (field->enum_values && value < field->enum_count) {
                snprintf(buf, buflen, "%s", field->enum_values[value]);
            } else {
                snprintf(buf, buflen, "INVALID(%lu)", (unsigned long)value);
            }
            break;

        case FIELD_TYPE_STRING:
            // String ptr is cast to uint32_t - cast back to char*
            {
                const char* str = (const char*)(uintptr_t)value;
                if (str) {
                    snprintf(buf, buflen, "%s", str);
                } else {
                    snprintf(buf, buflen, "(null)");
                }
            }
            break;

        case FIELD_TYPE_FLOAT:
            // Interpret as IEEE 754 float
            {
                float f;
                memcpy(&f, &value, sizeof(float));
                // Check for NaN/Inf to prevent printf lockup
                if (isnan(f)) {
                    snprintf(buf, buflen, "NaN");
                } else if (isinf(f)) {
                    snprintf(buf, buflen, f > 0 ? "+Inf" : "-Inf");
                } else {
                    snprintf(buf, buflen, "%.2f", f);
                }
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

    switch (field->type) {
        case FIELD_TYPE_BOOL:
            // Accept: "true", "false", "1", "0", "yes", "no"
            if (strcasecmp(str, "true") == 0 || strcasecmp(str, "yes") == 0 || strcmp(str, "1") == 0) {
                *value = 1;
                return true;
            } else if (strcasecmp(str, "false") == 0 || strcasecmp(str, "no") == 0 || strcmp(str, "0") == 0) {
                *value = 0;
                return true;
            }
            return false;

        case FIELD_TYPE_ENUM:
            // Special case: "?" returns false to trigger help display
            if (strcmp(str, "?") == 0) {
                return false;  // Caller should detect this and show help
            }

            // Try case-insensitive string match
            if (field->enum_values) {
                for (uint8_t i = 0; i < field->enum_count; i++) {
                    if (strcasecmp(str, field->enum_values[i]) == 0) {
                        *value = i;
                        return true;
                    }
                }
            }

            // Fall back to numeric input (backward compatible)
            char* endptr;
            long num = strtol(str, &endptr, 10);
            if (*endptr == '\0' && num >= 0 && num < field->enum_count) {
                *value = (uint32_t)num;
                return true;
            }

            return false;  // Invalid enum value

        case FIELD_TYPE_U8:
        case FIELD_TYPE_U16:
        case FIELD_TYPE_U32:
            {
                char* endptr;
                long num = strtol(str, &endptr, 10);
                if (*endptr == '\0' && num >= 0) {
                    *value = (uint32_t)num;
                    return true;
                }
            }
            return false;

        case FIELD_TYPE_HEX:
            {
                char* endptr;
                long num = strtol(str, &endptr, 16);
                if (*endptr == '\0') {
                    *value = (uint32_t)num;
                    return true;
                }
            }
            return false;

        default:
            // Unsupported type
            return false;
    }
}
