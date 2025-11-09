# TUI Enum Field Improvements

**Problem**: Many fields use raw numbers where text enums would be more user-friendly.

**Solution**: Convert numeric fields to ENUM type with string lookups and add `?` help to show available values.

---

## Fields That Need Enum Conversion

### **Table 4: Control Setpoints**

| Field | Current Type | Should Be | Values |
|-------|-------------|-----------|--------|
| `mode` | U32 | **ENUM** | `CURRENT`, `SPEED`, `TORQUE`, `PWM` |
| `direction` | U32 | **ENUM** | `POSITIVE`, `NEGATIVE` |

**Why**: Users shouldn't need to remember that mode 0=CURRENT, 1=SPEED, etc.

**Example**:
```
Current: mode = 1
Better:  mode = SPEED
```

---

### **Table 10: Fault Injection Control**

| Field | Current Type | Should Be | Values |
|-------|-------------|-----------|--------|
| `scenario_index` | U8 | **ENUM** | Scenario names from registry |

**Why**: Users want to select by name, not index.

**Example**:
```
Current: scenario_index = 2
Better:  scenario = "Frame Drop 50%"
```

---

### **Table 7: Protection Status** (Future)

| Field | Current Type | Should Be | Values |
|-------|-------------|-----------|--------|
| `fault_active` | BOOL | **ENUM** | `NONE`, `OVERVOLTAGE`, `OVERCURRENT`, `OVERSPEED`, etc. |

**Why**: Shows which fault is active by name.

---

### **Table 3: NSP Protocol** (Future)

| Field | Current Type | Should Be | Values |
|-------|-------------|-----------|--------|
| `last_command` | U32 (hex) | **ENUM** | `PING`, `PEEK`, `POKE`, `APP_TELEMETRY`, etc. |

**Why**: Command names are more meaningful than hex codes.

---

## Implementation Plan

### 1. **Extend Field Metadata for Enums**

Add enum string table to `field_meta_t`:

```c
typedef struct {
    uint8_t id;
    const char* name;
    field_type_t type;
    const char* units;
    field_access_t access;
    uint32_t default_val;
    volatile uint32_t* ptr;
    bool dirty;

    // NEW: Enum support
    const char** enum_strings;  // Array of string values
    uint8_t enum_count;          // Number of enum values
} field_meta_t;
```

### 2. **Define Enum String Tables**

For each enum field, define a string table:

```c
// Control mode enum
static const char* control_mode_strings[] = {
    "CURRENT",
    "SPEED",
    "TORQUE",
    "PWM"
};

// Direction enum
static const char* direction_strings[] = {
    "POSITIVE",
    "NEGATIVE"
};
```

### 3. **Update Field Definitions**

```c
{
    .id = 401,
    .name = "mode",
    .type = FIELD_TYPE_ENUM,  // Changed from U32
    .units = "",
    .access = FIELD_ACCESS_RW,
    .default_val = 0,
    .ptr = (volatile uint32_t*)&control_mode,
    .dirty = false,
    .enum_strings = control_mode_strings,  // NEW
    .enum_count = 4                         // NEW
},
```

### 4. **Update TUI Display Logic**

Modify `console_format.c` to show enum strings:

```c
if (field->type == FIELD_TYPE_ENUM) {
    uint32_t value = *field->ptr;
    if (value < field->enum_count) {
        snprintf(buf, buflen, "%s", field->enum_strings[value]);
    } else {
        snprintf(buf, buflen, "INVALID(%d)", value);
    }
}
```

### 5. **Update TUI Edit Logic**

Modify `tui.c` edit function to accept both string and numeric input:

```c
// When editing an ENUM field:
// Option 1: User types string name (case-insensitive match)
// Option 2: User types number (backward compatible)
// Option 3: User types "?" to show available values

if (field->type == FIELD_TYPE_ENUM) {
    if (strcmp(input, "?") == 0) {
        // Show available values
        printf("Available values:\n");
        for (int i = 0; i < field->enum_count; i++) {
            printf("  %d: %s\n", i, field->enum_strings[i]);
        }
        return;
    }

    // Try to match string
    for (int i = 0; i < field->enum_count; i++) {
        if (strcasecmp(input, field->enum_strings[i]) == 0) {
            *field->ptr = i;
            return;
        }
    }

    // Fall back to numeric input
    uint32_t value = atoi(input);
    if (value < field->enum_count) {
        *field->ptr = value;
    } else {
        printf("Invalid value. Type '?' for help.\n");
    }
}
```

### 6. **Add Help System**

When user types `?` in edit mode:

```
Edit mode (ENUM): Type value or '?' for help
Available values:
  0: CURRENT
  1: SPEED
  2: TORQUE
  3: PWM
Enter value (or 'q' to cancel):
```

---

## Priority Fields to Convert

### **High Priority** (User confusion likely)

1. ✅ **Control mode** - `CURRENT` vs 0 is much clearer
2. ✅ **Control direction** - `POSITIVE` vs 0 is intuitive
3. ✅ **Scenario selection** - Names vs indexes

### **Medium Priority** (Nice to have)

4. **NSP commands** - Command names vs hex codes
5. **Fault types** - Fault names vs bitmasks

### **Low Priority** (Already clear enough)

6. Status flags (booleans are fine)
7. Counters (numbers make sense)
8. Physical quantities (RPM, mA, etc. should stay numeric)

---

## Example User Experience

### **Before** (Current - Confusing):
```
Table 4: Control Setpoints
  401: mode            = 1
  406: direction       = 0
```

User must remember: 0=CURRENT, 1=SPEED, 2=TORQUE, 3=PWM

### **After** (With Enums - Clear):
```
Table 4: Control Setpoints
  401: mode            = SPEED
  406: direction       = POSITIVE
```

### **Editing** (Interactive Help):
```
> Press 'e' on mode field
Edit field 'mode' (current: SPEED)
Type value or '?' for help: ?

Available values:
  0: CURRENT
  1: SPEED
  2: TORQUE
  3: PWM

Enter value: TORQUE
✓ Field updated: mode = TORQUE
```

---

## Implementation Steps

1. **Phase 1**: Extend field_meta_t with enum support
2. **Phase 2**: Update console_format.c to display enum strings
3. **Phase 3**: Update tui.c edit logic to accept strings and "?" help
4. **Phase 4**: Convert high-priority fields (mode, direction)
5. **Phase 5**: Convert medium-priority fields (scenario, NSP commands)
6. **Phase 6**: Add unit tests for enum string matching

---

## Backward Compatibility

- ✅ Numeric input still works (user can type `1` instead of `SPEED`)
- ✅ Existing code uses numeric values internally
- ✅ Only display and input parsing changes
- ✅ No protocol changes required

---

## Code Size Impact

- **Enum string tables**: ~20 bytes per enum × 10 fields = 200 bytes
- **Display logic**: ~50 bytes
- **Edit logic**: ~150 bytes
- **Total**: ~400 bytes (negligible - 0.15% of flash)

---

## Benefits

✅ **User-Friendly** - Text is self-documenting
✅ **Fewer Errors** - Can't accidentally set mode=99
✅ **Interactive Help** - `?` shows available values
✅ **Faster Learning** - No need to memorize numeric codes
✅ **Professional** - Industry-standard enum UI pattern

---

## Next Steps

1. Review this proposal with user
2. Implement enum infrastructure (field_meta_t extension)
3. Update console_format.c and tui.c
4. Convert control mode and direction first (proof of concept)
5. Convert remaining high-priority fields
6. Document enum usage in TUI help text

