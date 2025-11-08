/**
 * @file tables.h
 * @brief Modular Table/Field Catalog for NRWA-T6 Emulator Console
 *
 * **DESIGN PHILOSOPHY: FULLY MODULAR & EXTENSIBLE**
 *
 * This catalog system uses a registration-based architecture that allows
 * new tables to be added without modifying core TUI code:
 *
 * 1. Define table metadata (see table_meta_t)
 * 2. Define field metadata array (see field_meta_t)
 * 3. Call catalog_register_table() during initialization
 *
 * The TUI and command palette dynamically discover tables via the catalog API.
 *
 * **CURRENT TABLES** (per SPEC.md §8):
 * 1. Serial Interface
 * 2. NSP Layer
 * 3. Control Mode
 * 4. Dynamics
 * 5. Protections
 * 6. Telemetry Blocks
 * 7. Config & JSON
 *
 * **ADDING A NEW TABLE**:
 * See example in tables.c:catalog_init() and table_*.c files.
 */

#ifndef TABLES_H
#define TABLES_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>  // For size_t

// ============================================================================
// Configuration
// ============================================================================

/**
 * @brief Maximum number of tables that can be registered
 *
 * Increase this if you need more than 16 tables in the catalog.
 */
#define CATALOG_MAX_TABLES  16

/**
 * @brief Maximum number of fields per table
 *
 * Increase this if a single table needs more than 32 fields.
 */
#define CATALOG_MAX_FIELDS_PER_TABLE  32

/**
 * @brief Field type identifiers
 */
typedef enum {
    FIELD_TYPE_BOOL,        // Boolean (true/false)
    FIELD_TYPE_U8,          // Unsigned 8-bit
    FIELD_TYPE_U16,         // Unsigned 16-bit
    FIELD_TYPE_U32,         // Unsigned 32-bit
    FIELD_TYPE_I32,         // Signed 32-bit
    FIELD_TYPE_HEX,         // Hexadecimal display
    FIELD_TYPE_ENUM,        // Enumeration (string lookup)
    FIELD_TYPE_FLOAT,       // Floating-point
    FIELD_TYPE_Q14_18,      // UQ14.18 fixed-point (RPM)
    FIELD_TYPE_Q16_16,      // UQ16.16 fixed-point (voltage, %)
    FIELD_TYPE_Q18_14,      // UQ18.14 fixed-point (torque, current, power)
    FIELD_TYPE_STRING,      // String (read-only)
} field_type_t;

/**
 * @brief Field access permissions
 */
typedef enum {
    FIELD_ACCESS_RO,        // Read-only
    FIELD_ACCESS_WO,        // Write-only
    FIELD_ACCESS_RW,        // Read-write
} field_access_t;

// ============================================================================
// Metadata Structures
// ============================================================================

/**
 * @brief Field metadata
 */
typedef struct {
    uint16_t id;                // Field ID (e.g., 4.1 = 401)
    const char* name;           // Field name (e.g., "speed_rpm")
    field_type_t type;          // Data type
    const char* units;          // Units string (e.g., "RPM", "A", "V")
    field_access_t access;      // Read/Write permissions
    uint32_t default_val;       // Compiled default value
    volatile uint32_t* ptr;     // Pointer to live value (NULL if not applicable)
    bool dirty;                 // True if value != default
    const char** enum_values;   // Enum string lookup (NULL if not enum)
    uint8_t enum_count;         // Number of enum values
} field_meta_t;

/**
 * @brief Table metadata
 */
typedef struct {
    uint8_t id;                 // Table ID (1-7)
    const char* name;           // Table name (e.g., "Serial Interface")
    const char* description;    // Brief description
    const field_meta_t* fields; // Array of fields
    uint8_t field_count;        // Number of fields
} table_meta_t;

// ============================================================================
// Catalog API - Core Functions
// ============================================================================

/**
 * @brief Initialize table/field catalog (empty)
 *
 * Clears all registered tables. Call this first, then register tables.
 * Must be called before any table registration.
 */
void catalog_init(void);

/**
 * @brief Register a table in the catalog
 *
 * **HOW TO ADD A NEW TABLE**:
 * 1. Create table metadata structure (table_meta_t)
 * 2. Create field metadata array (field_meta_t[])
 * 3. Call this function during initialization
 *
 * Example:
 * ```c
 * static const field_meta_t my_fields[] = {
 *     {.id = 1, .name = "field1", .type = FIELD_TYPE_U32, ...},
 *     {.id = 2, .name = "field2", .type = FIELD_TYPE_FLOAT, ...},
 * };
 *
 * static const table_meta_t my_table = {
 *     .name = "My Custom Table",
 *     .description = "Does something cool",
 *     .fields = my_fields,
 *     .field_count = 2,
 * };
 *
 * catalog_register_table(&my_table);
 * ```
 *
 * @param table Pointer to table metadata (must remain valid)
 * @return true if registered successfully, false if catalog full
 */
bool catalog_register_table(const table_meta_t* table);

/**
 * @brief Get number of registered tables
 *
 * @return Number of tables currently in catalog
 */
uint8_t catalog_get_table_count(void);

/**
 * @brief Get table metadata by index (0-based)
 *
 * Useful for iterating over all tables:
 * ```c
 * for (uint8_t i = 0; i < catalog_get_table_count(); i++) {
 *     const table_meta_t* table = catalog_get_table_by_index(i);
 *     printf("%s\n", table->name);
 * }
 * ```
 *
 * @param index Table index (0 to catalog_get_table_count()-1)
 * @return Pointer to table metadata, or NULL if index out of range
 */
const table_meta_t* catalog_get_table_by_index(uint8_t index);

/**
 * @brief Get table metadata by name
 *
 * @param name Table name (case-insensitive)
 * @return Pointer to table metadata, or NULL if not found
 */
const table_meta_t* catalog_get_table_by_name(const char* name);

/**
 * @brief Get field metadata by index within table
 *
 * @param table Table metadata pointer
 * @param field_index Field index (0 to table->field_count-1)
 * @return Pointer to field metadata, or NULL if invalid
 */
const field_meta_t* catalog_get_field(const table_meta_t* table, uint8_t field_index);

/**
 * @brief Get field metadata by name within table
 *
 * @param table Table metadata pointer
 * @param name Field name (case-insensitive)
 * @return Pointer to field metadata, or NULL if not found
 */
const field_meta_t* catalog_get_field_by_name(const table_meta_t* table, const char* name);

// ============================================================================
// Catalog API - Field Access Functions
// ============================================================================

/**
 * @brief Read field value (decoded to float)
 *
 * @param field Field metadata pointer
 * @param value Output: decoded value
 * @return true if successful, false if not readable or NULL pointer
 */
bool catalog_read_field(const field_meta_t* field, float* value);

/**
 * @brief Write field value (encode from float)
 *
 * @param field Field metadata pointer
 * @param value Input value to encode and write
 * @return true if successful, false if not writable or NULL pointer
 */
bool catalog_write_field(const field_meta_t* field, float value);

// ============================================================================
// Catalog API - Defaults Tracking
// ============================================================================

/**
 * @brief Get list of non-default fields (for "defaults list" command)
 *
 * Iterates through all registered tables and finds fields where current != default.
 *
 * @param out_buf Output buffer for formatted string
 * @param buflen Buffer size
 * @return Number of non-default fields found
 */
uint16_t catalog_get_dirty_fields(char* out_buf, size_t buflen);

/**
 * @brief Restore defaults for a table or specific field
 *
 * @param table Table metadata pointer, or NULL for all tables
 * @param field Field metadata pointer, or NULL for all fields in table
 * @return Number of fields restored
 */
uint16_t catalog_restore_defaults(const table_meta_t* table, const field_meta_t* field);

/**
 * @brief Format field value to string with units
 *
 * @param field Field metadata
 * @param value Raw value (fixed-point or integer)
 * @param buf Output buffer
 * @param buflen Buffer size
 */
void catalog_format_value(const field_meta_t* field, uint32_t value, char* buf, size_t buflen);

/**
 * @brief Parse string to field value (with type validation)
 *
 * @param field Field metadata
 * @param str Input string (e.g., "1234.5", "true", "SPEED")
 * @param value Output: parsed value (fixed-point or integer)
 * @return true if parse successful and within valid range
 */
bool catalog_parse_value(const field_meta_t* field, const char* str, uint32_t* value);

#endif // TABLES_H
