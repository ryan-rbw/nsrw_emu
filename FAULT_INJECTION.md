# Fault Injection System - User Guide

**NRWA-T6 Emulator - Timeline-Based Fault Injection**

This guide explains how to create, deploy, and execute fault injection scenarios for hardware-in-the-loop (HIL) testing of the NRWA-T6 reaction wheel emulator.

---

## Table of Contents

1. [Overview](#overview)
2. [Quick Start](#quick-start)
3. [Creating Scenarios](#creating-scenarios)
4. [JSON Schema Reference](#json-schema-reference)
5. [Building Scenarios into Firmware](#building-scenarios-into-firmware)
6. [Executing Scenarios](#executing-scenarios)
7. [Monitoring Scenario Execution](#monitoring-scenario-execution)
8. [Example Scenarios](#example-scenarios)
9. [Best Practices](#best-practices)
10. [Troubleshooting](#troubleshooting)

---

## Overview

The fault injection system allows you to define **timeline-based scenarios** that inject faults into the emulator at specific times or conditions. This enables testing of:

- **OBC/GNC fault recovery** - How does your spacecraft react to communication errors?
- **Protection system validation** - Do safety limits trigger correctly?
- **Protocol robustness** - Can your system handle CRC errors, dropped frames, NACKs?
- **Edge case testing** - Overspeed faults, power limits, torque saturation

### Architecture

The fault injection engine operates in three layers:

1. **Transport Layer** - CRC corruption, frame drops, reply delays, forced NACKs
2. **Device Layer** - Fault bit manipulation, status changes, overspeed faults, LCL trips
3. **Physics Layer** - Power/current/speed limit overrides, torque command overrides

Scenarios are defined in JSON files, compiled into firmware, and triggered interactively from the TUI console.

---

## Quick Start

### 1. Create a Scenario

Create a JSON file in `firmware/config/scenarios/`:

```bash
cd firmware/config/scenarios/
nano my_test.json
```

**Example: Simple CRC error at t=5s**

```json
{
  "name": "CRC Error Test",
  "description": "Inject CRC error at t=5s to test retry logic",
  "version": "1.0",
  "schedule": [
    {
      "t_ms": 5000,
      "action": {
        "inject_crc_error": true
      }
    }
  ]
}
```

### 2. Add Scenario to Build System

Edit `firmware/config/scenario_registry.c` (see [Building Scenarios into Firmware](#building-scenarios-into-firmware) for details):

```c
#include "my_test.json.h"
// Add to scenarios[] array
```

### 3. Build Firmware

```bash
cd build
make -j4
```

### 4. Flash and Execute

1. Flash `nrwa_t6_emulator.uf2` to Pico
2. Connect to USB console (115200 baud)
3. Press any key to enter TUI
4. Navigate to **Table 10: Fault Injection Control**
5. Select scenario from list
6. Press `x` to execute (or use `e` to edit trigger field)
7. Watch live scenario playback on console

---

## Creating Scenarios

### Scenario File Structure

Every scenario JSON file must have:

```json
{
  "name": "Human-readable scenario name (max 63 chars)",
  "description": "What this scenario tests (max 127 chars)",
  "version": "1.0",
  "schedule": [
    { /* event 1 */ },
    { /* event 2 */ },
    ...
  ]
}
```

### Event Structure

Each event in the `schedule` array defines:

```json
{
  "t_ms": 5000,           // Trigger time (ms from scenario activation)
  "duration_ms": 1000,    // Optional: action duration (0 or omit = instant/persistent)
  "condition": {          // Optional: conditional trigger (Phase 10)
    "mode_in": "TORQUE",
    "rpm_gt": 1000.0,
    "rpm_lt": 5000.0,
    "nsp_cmd_eq": 0x08
  },
  "action": {             // Fault injection action (at least one field required)
    // Transport layer
    "inject_crc_error": true,
    "drop_frames_pct": 50,
    "delay_reply_ms": 100,
    "force_nack": true,

    // Device layer
    "flip_status_bits": 0x00000001,
    "set_fault_bits": 0x00000010,
    "clear_fault_bits": 0x00000020,
    "overspeed_fault": true,
    "trip_lcl": true,

    // Physics layer
    "limit_power_w": 50.0,
    "limit_current_a": 3.0,
    "limit_speed_rpm": 3000.0,
    "override_torque_mNm": 100.0
  }
}
```

### Design Guidelines

1. **Keep scenarios focused** - Test one fault type per scenario
2. **Use descriptive names** - Name should indicate what's being tested
3. **Add clear descriptions** - Explain expected behavior / what to verify
4. **Start simple** - Single event first, then build up complexity
5. **Document expected results** - What should happen when this scenario runs?

---

## JSON Schema Reference

### Root Object

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `name` | string | Yes | Scenario name (max 63 chars) - shown in TUI |
| `description` | string | No | What this scenario tests (max 127 chars) |
| `version` | string | No | Scenario version (e.g., "1.0", "2.1") |
| `schedule` | array | Yes | Array of events (max 32 events per scenario) |

### Event Object

| Field | Type | Required | Description |
|-------|------|----------|-------------|
| `t_ms` | uint32 | Yes | Trigger time in milliseconds from scenario activation |
| `duration_ms` | uint32 | No | Action duration (0 or omit = instant/persistent) |
| `condition` | object | No | Conditional trigger (Phase 10 feature) |
| `action` | object | Yes | Fault injection action (must have at least one field) |

### Condition Object (Phase 10)

| Field | Type | Description |
|-------|------|-------------|
| `mode_in` | string | Trigger when control mode matches ("CURRENT", "SPEED", "TORQUE", "PWM") |
| `rpm_gt` | float | Trigger when RPM > threshold |
| `rpm_lt` | float | Trigger when RPM < threshold |
| `nsp_cmd_eq` | uint8 | Trigger when NSP command byte matches (0x00-0x0B) |

### Action Object - Transport Layer

| Field | Type | Description |
|-------|------|-------------|
| `inject_crc_error` | bool | Corrupt CRC of next transmitted frame |
| `drop_frames_pct` | uint8 | Drop N% of frames (0-100) |
| `delay_reply_ms` | uint16 | Delay reply by N milliseconds |
| `force_nack` | bool | Force NACK response (instead of ACK) |

### Action Object - Device Layer

| Field | Type | Description |
|-------|------|-------------|
| `flip_status_bits` | uint32 | XOR status register with bitmask |
| `set_fault_bits` | uint32 | Set fault bits (bitwise OR) |
| `clear_fault_bits` | uint32 | Clear fault bits (bitwise AND NOT) |
| `overspeed_fault` | bool | Trigger overspeed fault (6000 RPM threshold) |
| `trip_lcl` | bool | Trip local current limit fault |

### Action Object - Physics Layer

| Field | Type | Description |
|-------|------|-------------|
| `limit_power_w` | float | Override power limit (Watts) |
| `limit_current_a` | float | Override current limit (Amps) |
| `limit_speed_rpm` | float | Override speed limit (RPM) |
| `override_torque_mNm` | float | Override torque output (milli-Newton-meters) |

---

## Building Scenarios into Firmware

Scenarios are **compiled into firmware** as C string constants. This avoids needing a filesystem and makes scenarios instantly available.

### Step 1: Create Scenario JSON File

Save your scenario in `firmware/config/scenarios/`:

```bash
cd firmware/config/scenarios/
nano crc_burst_test.json
```

### Step 2: Generate C Header (Automated in Build)

The build system automatically converts JSON files to C headers:

```bash
# This happens automatically during build:
# scenarios/my_test.json → scenarios/my_test.json.h
```

**Generated header example:**

```c
// Auto-generated from my_test.json
#ifndef MY_TEST_JSON_H
#define MY_TEST_JSON_H

static const char scenario_my_test[] =
"{\n"
"  \"name\": \"My Test\",\n"
"  \"description\": \"Test description\",\n"
"  ...\n"
"}";

#endif
```

### Step 3: Register Scenario in Scenario Registry

Edit `firmware/config/scenario_registry.c`:

```c
#include "scenario.h"
#include "scenarios/overspeed_fault.json.h"
#include "scenarios/crc_burst.json.h"
#include "scenarios/my_test.json.h"  // ← Add your header

// Scenario registry (shown in TUI)
const scenario_entry_t g_scenario_registry[] = {
    { "Overspeed Fault", scenario_overspeed_fault },
    { "CRC Burst", scenario_crc_burst },
    { "My Test", scenario_my_test },  // ← Add your entry
};

const uint8_t g_scenario_count = sizeof(g_scenario_registry) / sizeof(g_scenario_registry[0]);
```

### Step 4: Rebuild Firmware

```bash
cd build
make -j4
```

Your scenario is now embedded in the firmware and will appear in the TUI scenario list.

---

## Executing Scenarios

### Interactive Execution (TUI)

1. **Enter TUI**: Press any key after boot tests complete
2. **Navigate to Fault Injection Control**: Press `0` (main menu) → `10` (Fault Injection Control)
3. **Select scenario**: Use `e` (edit) on `scenario_index` field, enter index (0-N)
4. **View scenario details**: Table shows scenario name, event count, duration
5. **Execute**: Press `x` (execute) or set `trigger` field to `1` and press Enter
6. **Watch playback**: Console scrolls showing live event triggers
7. **Return to TUI**: Press any key when scenario completes

### Programmatic Execution (C API)

For automated testing or integration:

```c
#include "config/scenario.h"

// Initialize engine (done at boot in table_config_init())
scenario_engine_init();

// Load scenario from registry
const char* json = g_scenario_registry[2].json_data;  // Index 2
bool loaded = scenario_load(json, strlen(json));
if (!loaded) {
    printf("Load failed: %s\n", json_get_last_error());
    return;
}

// Activate timeline
bool activated = scenario_activate();
if (!activated) {
    printf("Activation failed\n");
    return;
}

// Update scenario in main loop
while (scenario_is_active()) {
    scenario_update();  // Check for event triggers
    sleep_ms(10);       // 100 Hz update rate

    // Your test code here...
}

// Deactivate when done
scenario_deactivate();
```

---

## Monitoring Scenario Execution

### Table 9: Fault Injection Status

Real-time scenario monitoring in TUI:

| Field | Description |
|-------|-------------|
| `scenario_name` | Name of currently loaded/active scenario |
| `scenario_active` | 1 = running, 0 = stopped |
| `elapsed_ms` | Time since activation (milliseconds) |
| `events_triggered` | Number of events that have fired |
| `events_total` | Total events in scenario |

### Console Output During Execution

When a scenario is active, the console shows:

```
╔═══════════════════════════════════════════════════════════╗
║  FAULT INJECTION: CRC Burst Test                          ║
╚═══════════════════════════════════════════════════════════╝

Scenario: CRC Burst Test
Description: Multiple CRC errors to test retry logic
Events: 3 events over 5 seconds

[SCENARIO] Scenario activated: CRC Burst Test
[SCENARIO] Event 0 triggered at t=2000 ms
[SCENARIO] CRC corrupted
[SCENARIO] Event 1 triggered at t=3000 ms
[SCENARIO] CRC corrupted
[SCENARIO] Event 2 triggered at t=4000 ms
[SCENARIO] CRC corrupted
[SCENARIO] Deactivated

3/3 events triggered successfully
Press any key to return to TUI...
```

### Debug Logging

Enable verbose logging by defining `DEBUG_SCENARIO` in `scenario.c`:

```c
#define DEBUG_SCENARIO
```

This adds detailed event processing logs:

```
[SCENARIO] Checking event 0: t=2000ms, elapsed=1850ms (skip)
[SCENARIO] Checking event 0: t=2000ms, elapsed=2010ms (trigger!)
[SCENARIO] Applying transport action: inject_crc_error=true
```

---

## Example Scenarios

### Example 1: Single CRC Error

**Use Case**: Test OBC retry logic on CRC failure

```json
{
  "name": "Single CRC Error",
  "description": "Inject one CRC error at t=5s",
  "version": "1.0",
  "schedule": [
    {
      "t_ms": 5000,
      "action": {
        "inject_crc_error": true
      }
    }
  ]
}
```

**Expected Behavior**: Next NSP reply will have corrupted CRC, OBC should retry command.

---

### Example 2: Frame Drop Test

**Use Case**: Test telemetry loss recovery

```json
{
  "name": "Frame Drop 50%",
  "description": "Drop 50% of frames for 5 seconds",
  "version": "1.0",
  "schedule": [
    {
      "t_ms": 2000,
      "duration_ms": 5000,
      "action": {
        "drop_frames_pct": 50
      }
    }
  ]
}
```

**Expected Behavior**: Half of telemetry frames lost between t=2s and t=7s, OBC should handle gaps gracefully.

---

### Example 3: Overspeed Fault

**Use Case**: Test latched fault handling

```json
{
  "name": "Overspeed Fault",
  "description": "Trigger overspeed fault at t=5s (requires CLEAR-FAULT)",
  "version": "1.0",
  "schedule": [
    {
      "t_ms": 5000,
      "action": {
        "overspeed_fault": true
      }
    }
  ]
}
```

**Expected Behavior**: Fault status bit set at t=5s, wheel shuts down. OBC must send `CLEAR-FAULT` command to recover.

---

### Example 4: Power Limit Override

**Use Case**: Test current limiting under reduced power budget

```json
{
  "name": "Power Limit Test",
  "description": "Reduce power limit to 50W for 10 seconds",
  "version": "1.0",
  "schedule": [
    {
      "t_ms": 1000,
      "duration_ms": 10000,
      "action": {
        "limit_power_w": 50.0
      }
    }
  ]
}
```

**Expected Behavior**: Wheel limited to 50W from t=1s to t=11s. Acceleration/torque reduced accordingly.

---

### Example 5: Complex Multi-Event Timeline

**Use Case**: Full HIL test sequence

```json
{
  "name": "Complex Timeline",
  "description": "Multi-phase fault injection test",
  "version": "1.0",
  "schedule": [
    {
      "t_ms": 1000,
      "action": {
        "inject_crc_error": true
      }
    },
    {
      "t_ms": 3000,
      "duration_ms": 2000,
      "action": {
        "drop_frames_pct": 30
      }
    },
    {
      "t_ms": 7000,
      "action": {
        "force_nack": true
      }
    },
    {
      "t_ms": 10000,
      "duration_ms": 5000,
      "action": {
        "limit_current_a": 3.0
      }
    }
  ]
}
```

**Timeline**:
- t=1s: CRC error (tests retry)
- t=3-5s: 30% frame drops (tests telemetry loss)
- t=7s: Force NACK (tests command rejection)
- t=10-15s: Current limit to 3A (tests power management)

---

## Best Practices

### Scenario Design

1. **One fault type per scenario** - Easier to debug, clearer test intent
2. **Realistic timings** - Use actual operational timescales (seconds, not milliseconds)
3. **Document expected results** - What should the OBC do? Add to description field
4. **Version scenarios** - Increment version string when making changes
5. **Test incrementally** - Start with single events, add complexity gradually

### Naming Conventions

- **Descriptive names**: "CRC Error Test", not "Test1"
- **Indicate fault type**: "Overspeed Fault", "Frame Drop 50%", "Power Limit Override"
- **Keep under 60 chars**: Fits nicely in TUI table display

### Development Workflow

1. **Create scenario** in `scenarios/` directory
2. **Test JSON syntax** using online validator (jsonlint.com)
3. **Add to registry** in `scenario_registry.c`
4. **Build and flash** firmware
5. **Dry run** - Execute scenario with no OBC connected, verify console output
6. **HIL test** - Connect OBC, run scenario, verify OBC behavior
7. **Document results** - Add notes to scenario description if needed

### Debugging Tips

1. **Check JSON syntax** - Common issues: missing commas, trailing commas, unescaped quotes
2. **Watch console output** - `[SCENARIO]` prefix shows engine activity
3. **Verify timing** - Scenario update rate is 100 Hz (10ms), so events can fire ±10ms from target
4. **Check event count** - Limit is 32 events per scenario
5. **Use test mode** - Enable `RUN_PHASE9_TESTS` to validate engine before HIL testing

---

## Troubleshooting

### Scenario Won't Load

**Problem**: "Load failed: Expected '{' at root"

**Solution**: JSON syntax error. Check:
- Root object has opening `{` and closing `}`
- All strings in double quotes
- No trailing commas in arrays/objects
- Escape special characters in strings

---

### Events Not Triggering

**Problem**: Scenario active but no events fire

**Solution**:
1. Check `t_ms` values - must be > 0
2. Verify `scenario_update()` is being called in main loop
3. Check console for `[SCENARIO] Event N triggered` messages
4. Enable `DEBUG_SCENARIO` for detailed logs

---

### Duration Not Working

**Problem**: Action continues after `duration_ms` expires

**Solution**:
- Transport layer actions (CRC, drops) use duration correctly
- Device/physics layer actions may persist (Phase 10 integration needed)
- Check that `scenario_update()` is called regularly (100 Hz recommended)

---

### Build Fails After Adding Scenario

**Problem**: "undefined reference to 'scenario_my_test'"

**Solution**: Did you include the `.json.h` header in `scenario_registry.c`?

```c
#include "scenarios/my_test.json.h"  // ← Add this
```

---

### Scenario Not in TUI List

**Problem**: Built successfully but scenario doesn't appear in table

**Solution**: Check `scenario_registry.c`:
1. Header included?
2. Entry added to `g_scenario_registry[]` array?
3. Firmware rebuilt and reflashed?

---

## Future Features (Phase 10)

The following features are planned for Phase 10:

- **Conditional triggers** - Events fire based on wheel state (mode, RPM, NSP commands)
- **Flash storage** - Load scenarios from flash instead of compiling into firmware
- **Scenario editor** - Create/edit scenarios from TUI (no rebuild required)
- **Scenario generator** - Python tool (`errorgen.py`) to generate scenarios from templates
- **Scenario recording** - Record live faults from OBC interactions as scenarios
- **Loop/repeat** - Repeat events or entire scenarios N times
- **Random injection** - Probabilistic fault injection (useful for soak testing)

---

## References

- **JSON Schema**: See `firmware/config/scenarios/README.md`
- **API Reference**: See `firmware/config/scenario.h`
- **Example Scenarios**: `firmware/config/scenarios/*.json`
- **Implementation Plan**: [IMP.md Phase 9](IMP.md#phase-9-fault-injection-system)
- **Progress Tracking**: [PROGRESS.md Phase 9](PROGRESS.md#phase-9-fault-injection-system--complete)

---

**For questions or issues, refer to the [main README](README.md) or check `PROGRESS.md` for current implementation status.**
