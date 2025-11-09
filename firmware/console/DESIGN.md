# Console TUI Design - Modular Architecture

## Philosophy: Zero-Touch Extensibility

The console TUI system is designed to be **completely modular** and **registration-based**. Adding a new table to the TUI requires **zero modifications** to the core TUI or catalog code.

### Key Design Principles

1. **Registration-Based Catalog**: Tables register themselves at initialization
2. **Dynamic Discovery**: TUI discovers tables via catalog API at runtime
3. **Metadata-Driven**: All table/field properties defined declaratively
4. **Type-Safe**: Strong typing with runtime validation
5. **Extensible**: New tables, fields, and types can be added independently
6. **Fixed Console Width**: All output constrained to 80-character width

## Console Configuration

The console enforces a fixed width for all output, defined in `console_config.h`:

```c
#define CONSOLE_WIDTH 80  // Standard VT100 terminal width
```

**Key Features:**
- All text, logos, banners, and UI elements must fit within `CONSOLE_WIDTH`
- Logo is centered within console width
- Header and status banners are padded to exactly 80 characters
- Separator lines span full console width
- Configurable for different terminal sizes (80/100/120 columns)

**Why 80 Characters?**
- Standard VT100/xterm terminal width
- Universal compatibility with serial terminals
- Optimal readability on embedded consoles
- Historical standard for text-based interfaces

**Formatting Utilities** (`console_format.c`):
- `console_print_line(char)` - Draw horizontal line across full width
- `console_print_centered(str)` - Center text within console width
- `console_print_box_line(str)` - Print bordered line with padding
- `console_center_padding(len)` - Calculate centering offset

## Architecture Overview

```
┌─────────────────────────────────────────────────────────┐
│                    TUI Layer (tui.c)                    │
│  ┌────────────┐  ┌────────────┐  ┌────────────┐       │
│  │ Main Menu  │  │ Table View │  │   Command  │       │
│  │  (Dynamic) │  │  (Generic) │  │   Palette  │       │
│  └────────────┘  └────────────┘  └────────────┘       │
└──────────────────────│──────────────────────────────────┘
                       │ catalog_get_table_count()
                       │ catalog_get_table_by_index()
                       │ catalog_get_field()
                       ▼
┌─────────────────────────────────────────────────────────┐
│              Catalog Registry (tables.c)                │
│  ┌─────────────────────────────────────────────────┐   │
│  │ Table Pointer Array [CATALOG_MAX_TABLES]        │   │
│  │ ├─ table_meta_t*  [0]                           │   │
│  │ ├─ table_meta_t*  [1]                           │   │
│  │ ├─ ...                                          │   │
│  │ └─ table_meta_t*  [N-1]                         │   │
│  └─────────────────────────────────────────────────┘   │
└──────────────────────│──────────────────────────────────┘
                       │ catalog_register_table()
                       │
         ┌─────────────┼─────────────┬──────────────┐
         ▼             ▼             ▼              ▼
    ┌─────────┐  ┌─────────┐  ┌──────────┐  ┌─────────┐
    │ Serial  │  │   NSP   │  │ Dynamics │  │  Your   │
    │  Table  │  │  Table  │  │  Table   │  │ Custom  │
    │ (*.c)   │  │  (*.c)  │  │  (*.c)   │  │ Table!  │
    └─────────┘  └─────────┘  └──────────┘  └─────────┘
```

## How to Add a New Table

### Step 1: Create Table Definition File

Create a new file `console/table_<name>.c` (e.g., `table_serial.c`):

#include "tables.h"
#include "nss_nrwa_t6_model.h"  // For live value pointers
#include <string.h>

// ============================================================================
// Field Definitions
// ============================================================================

// External references to live values (from drivers, model, etc.)
extern volatile uint32_t rs485_tx_count;
extern volatile uint32_t rs485_rx_count;
extern volatile wheel_state_t g_wheel_state;

static const field_meta_t serial_fields[] = {
    {
        .id = 1,
        .name = "status",
        .type = FIELD_TYPE_BOOL,
        .units = "",
        .access = FIELD_ACCESS_RO,
        .default_val = 1,  // true = active
        .ptr = NULL,  // Static value, not live
        .dirty = false,
        .enum_values = NULL,
        .enum_count = 0,
    },
    {
        .id = 2,
        .name = "tx_count",
        .type = FIELD_TYPE_U32,
        .units = "packets",
        .access = FIELD_ACCESS_RO,
        .default_val = 0,
        .ptr = &rs485_tx_count,  // Live value from driver
        .dirty = false,
        .enum_values = NULL,
        .enum_count = 0,
    },
    // ... more fields ...
};

// ============================================================================
// Table Metadata
// ============================================================================

static const table_meta_t serial_table = {
    .name = "Serial Interface",
    .description = "RS-485 UART status, TX/RX counts, SLIP, CRC",
    .fields = serial_fields,
    .field_count = sizeof(serial_fields) / sizeof(serial_fields[0]),
};

// ============================================================================
// Registration Function
// ============================================================================

void table_serial_register(void) {
    catalog_register_table(&serial_table);
}
```

### Step 2: Add Header Declaration

In `console/tables.h`, add at the end before `#endif`:

```c
// ============================================================================
// Table Registration Functions (per-table)
// ============================================================================

/**
 * @brief Register Serial Interface table
 * Call during catalog_init()
 */
void table_serial_register(void);

/**
 * @brief Register your custom table
 * Call during catalog_init()
 */
void table_mycustom_register(void);
```

### Step 3: Register in catalog_init()

In `console/tables.c`, modify `catalog_init()`:

```c
void catalog_init(void) {
    // Clear catalog
    memset(catalog, 0, sizeof(catalog));
    catalog_count = 0;

    // Register all tables (ORDER MATTERS - this is menu order)
    table_serial_register();      // Table 1
    table_nsp_register();         // Table 2
    table_control_register();     // Table 3
    table_dynamics_register();    // Table 4
    table_protections_register(); // Table 5
    table_telemetry_register();   // Table 6
    table_config_register();      // Table 7

    // ADD YOUR NEW TABLE HERE:
    table_mycustom_register();    // Table 8 (automatically!)
}
```

### Step 4: Add to Build System

In `firmware/CMakeLists.txt`, add your new file:

```cmake
add_executable(nrwa_t6_emulator
    # ... existing files ...
    # Console (Phase 8)
    console/tui.c
    console/tables.c
    console/table_serial.c
    console/table_nsp.c
    console/table_dynamics.c
    console/table_mycustom.c      # <-- ADD HERE
)
```

### That's It!

No changes needed to:
- ❌ `tui.c` - automatically discovers tables via catalog
- ❌ `tui.h` - already generic, works with any table
- ❌ Command palette - uses catalog API
- ❌ Main loop - TUI handles everything

## Live Value Updates

The TUI updates live values **in place** without scrolling by:

1. **ANSI Cursor Positioning**: Position cursor at specific row/col
2. **Selective Updates**: Only redraw changed values
3. **Double Buffering**: Track previous values to detect changes
4. **Refresh Rate**: Main loop calls `tui_update()` at ~10-20 Hz

Example refresh cycle:

```c
void main_loop() {
    while (1) {
        // Process RS-485, NSP, etc.
        rs485_poll();
        nsp_poll();

        // Update TUI (non-blocking, only redraws if needed)
        tui_update(false);  // false = incremental update

        // Handle keyboard input
        if (tui_handle_input()) {
            tui_update(true);  // true = force full redraw
        }

        sleep_ms(50);  // 20 Hz update rate
    }
}
```

## Field Types & Custom Types

The system supports these built-in types:
- `FIELD_TYPE_BOOL` - true/false
- `FIELD_TYPE_U8/U16/U32` - Unsigned integers
- `FIELD_TYPE_I32` - Signed integer
- `FIELD_TYPE_HEX` - Hexadecimal display
- `FIELD_TYPE_ENUM` - String enumeration
- `FIELD_TYPE_FLOAT` - Floating-point
- `FIELD_TYPE_Q14_18/Q16_16/Q18_14` - Fixed-point formats

### Adding a New Field Type

1. Add enum value to `field_type_t` in `tables.h`
2. Implement encoder/decoder in `catalog_format_value()` in `tables.c`
3. Implement parser in `catalog_parse_value()` in `tables.c`

Example:

```c
// In tables.h
typedef enum {
    // ... existing types ...
    FIELD_TYPE_IP_ADDR,  // Custom: IPv4 address
} field_type_t;

// In tables.c
void catalog_format_value(const field_meta_t* field, uint32_t value,
                          char* buf, size_t buflen) {
    switch (field->type) {
        // ... existing cases ...

        case FIELD_TYPE_IP_ADDR:
            snprintf(buf, buflen, "%d.%d.%d.%d",
                     (value >> 24) & 0xFF,
                     (value >> 16) & 0xFF,
                     (value >> 8) & 0xFF,
                     value & 0xFF);
            break;
    }
}
```

## Command Palette Integration

The command palette automatically supports all registered tables:

```
> tables
Available tables:
  1. Serial Interface - RS-485 UART status, TX/RX counts, SLIP, CRC
  2. NSP Layer - Last cmd/reply, poll, ack, command stats, timing
  3. Control Mode - Active mode, setpoint, direction, PWM, source
  4. Dynamics - Speed, momentum, torque, current, power, losses
  5. Protections - Thresholds, power limit, flags, clear faults
  6. Telemetry Blocks - Decoded STANDARD/TEMP/VOLT/CURR/DIAG
  7. Config & JSON - Scenarios, defaults, save/restore
  8. Your Custom Table - Does something amazing!  <-- AUTOMATICALLY APPEARS

> describe dynamics
Table: Dynamics - Speed, momentum, torque, current, power, losses

Fields:
  ID   Name              Type      Units  Access  Default    Current
  ───────────────────────────────────────────────────────────────────
  1    speed_rpm         Q14.18    RPM    RO      0.00       3245.67
  2    momentum_nms      Q18.14    N·m·s  RO      0.00       0.15
  3    torque_cmd_mnm    Q18.14    mN·m   RW      0.00       52.30 *
  4    current_a         FLOAT     A      RO      0.00       2.45

> get dynamics.speed_rpm
3245.67 RPM

> set dynamics.torque_cmd_mnm 100
OK - torque_cmd_mnm = 100.00 mN·m
```

## Memory Considerations

- **Static Allocation**: All table/field metadata is `const static` (in flash)
- **Catalog Array**: Small array of pointers (16 × 4 bytes = 64 bytes RAM)
- **TUI State**: Single structure (~200 bytes RAM)
- **No Dynamic Allocation**: Zero `malloc()` calls

## Testing New Tables

1. Flash firmware with new table registered
2. Connect to USB-CDC console
3. Press `r` to refresh - new table appears in menu
4. Navigate with number keys
5. Test field reads with `get <table>.<field>`
6. Test field writes with `set <table>.<field> <value>`

## Summary

**Adding a new table is a 4-step process**:
1. Create `table_<name>.c` with field/table metadata
2. Add registration function declaration to `tables.h`
3. Call `table_<name>_register()` in `catalog_init()`
4. Add `.c` file to `CMakeLists.txt`

**Zero changes needed to**:
- TUI rendering code
- Command palette
- Navigation logic
- Menu display

The system is **fully modular** and **future-proof**.
