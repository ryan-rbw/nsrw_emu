# Reaction Wheel Emulator — SPEC.md (NRWA-T6 Compatible, RP2040/Pico)

## 1. Goal

Emulate a NewSpace Systems NRWA‑T6 reaction wheel so that the OBC/GNC stack cannot distinguish the emulator from real hardware. The emulator must:
- Speak the same physical/data‑link and NSP protocol over RS‑485 with SLIP framing and CCITT CRC.
- Implement command handling, telemetry, protection and fault semantics compatible with the NRWA‑T6 ICD.
- Simulate realistic dynamics, limits, and timing for current, speed, and torque control modes, including soft/hard protections.
- Provide a local terminal UI on USB‑CDC with arrow-key navigation of tables and fields for live status viewing.
- Support deterministic error/fault injection via JSON scenarios, loadable at runtime from on‑board flash or over the console.
- Build to a single UF2 for Raspberry Pi Pico (RP2040) using the Pico SDK (C/C++).

## 2. Target Platform & Toolchain

- MCU: RP2040 (Pico, not W/W2). Dual‑core Cortex‑M0+ @ 133 MHz.
- SDK: Raspberry Pi Pico SDK (C/C++), CMake‑based.
- Interfaces:
  - USB‑CDC for developer console.
  - RS‑485 transceiver connected to a UART (default UART1) for the data interface.
  - GPIOs for ADDR pins, FAULT, RESET as per harness mapping.
- Timers: hardware timers + PIO (optional) for precise SLIP byte‑stuffing if needed.
- Storage: on‑chip flash for config and error‑injection JSON; optional QSPI XIP.
- Build: `cmake -DPICO_SDK_PATH=... .. && make`, outputs `.uf2`.

## 3. External Interface Compatibility

### 3.1 Physical/Data‑Link (Layer 1–2)

- Dual‑redundant full‑duplex RS‑485 with 8‑N‑1 at ≈460.8 kbps (accept 455.6–465.7 kbps). Transmit drivers tri‑state when idle to allow multi‑drop TXD sharing.
- Addressing pins ADDR[2:0] select device address; FAULT and RESET lines are exposed.
- SLIP framing on both telecommand and reply channels.

### 3.2 Protocol (Layer 3–5)

- NSP with Message Control field [Poll,B,A,5‑bit command]. Commands implemented:
  - 0x00 PING
  - 0x02 PEEK
  - 0x03 POKE
  - 0x07 APPLICATION‑TELEMETRY
  - 0x08 APPLICATION‑COMMAND
  - 0x09 CLEAR‑FAULT
  - 0x0A CONFIGURE‑PROTECTION
  - 0x0B TRIP‑LCL
- CRC: CCITT, init 0xFFFF, LSB first, over Destination..Data bytes.

## 4. Software Architecture

```
reaction-wheel-emulator/
├─ CMakeLists.txt
├─ README.md
├─ firmware/
│  ├─ app_main.c
│  ├─ platform/
│  │  ├─ board_pico.h
│  │  ├─ gpio_map.c
│  │  └─ timebase.c
│  ├─ drivers/
│  │  ├─ rs485_uart.c
│  │  ├─ slip.c
│  │  ├─ nsp.c
│  │  ├─ crc_ccitt.c
│  │  └─ leds.c
│  ├─ device/
│  │  ├─ nss_nrwa_t6_regs.h
│  │  ├─ nss_nrwa_t6_model.c
│  │  ├─ nss_nrwa_t6_commands.c
│  │  ├─ nss_nrwa_t6_telemetry.c
│  │  └─ nss_nrwa_t6_protection.c
│  ├─ console/
│  │  ├─ tui.c
│  │  ├─ tables.c
│  │  └─ json_loader.c
│  ├─ config/
│  │  ├─ defaults.toml
│  │  └─ error_schema.json
│  └─ util/
│     ├─ ringbuf.c
│     └─ fixedpoint.h
├─ tools/
│  ├─ host_tester.py
│  └─ errorgen.py
└─ tests/
   ├─ unit/
   └─ hilsim/
```

### Modules

- `drivers/rs485_uart.c`: UART init, DE/RE pins, tri‑state control, timing, redundancy switching.
- `drivers/slip.c`: SLIP encode/decode with END/ESC and robust state machine.
- `drivers/nsp.c`: Packetize/depacketize NSP, control byte, CRC, routing to command handlers.
- `device/*`: Wheel state machine, control modes, protections, telemetry layout.
- `console/*`: USB‑CDC TUI with table/field navigation, metadata-driven catalog system.
- `util/fixedpoint.h`: Helpers for UQ formats used by PEEK/POKE fields.
- `tools/host_tester.py`: Sends NSP sequences to validate emulator behavior.
- `tools/errorgen.py`: Builds JSON scenarios with timeline/probabilities.

RP2040 core usage: Core0 handles RS‑485 + NSP, Core1 runs physics model at 100 Hz tick and publishes state via lock‑free queue.

## 5. Reaction Wheel Physics & Control Emulation

### 5.1 State Variables
- Wheel speed ω [rad/s], momentum H = I·ω [N·m·s]
- Commanded mode ∈ {Current, Speed, Torque, PWM‑backup}
- Current setpoint i_s [A], torque setpoint τ_s [N·m], speed setpoint ω_s
- Motor power limit P_lim [W], DC bus V_d ~ 28 V, current limits
- Temperature proxies T_x for model shaping (simple thermal RC optional)

### 5.2 Dynamics (Δt = 10 ms)
- τ_m = k_t · i, with k_t ≈ 0.0534 N·m/A.
- τ_loss = a·ω + b·sign(ω) + c·i^2.
- α = (τ_m − τ_loss) / I, integrate ω.
- Power and duty clamps: enforce |τ_m·ω| ≤ P_lim and duty ≤ ~97.85%.
- Overspeed: live limit at 5000 RPM, latched fault at 6000 RPM requiring CLEAR‑FAULT.

### 5.3 Control Modes
- Current, Speed (PI with anti‑windup), Torque (ΔH feed‑forward), PWM backup.

### 5.4 Protections
- Hard: overvoltage 36 V, phase overcurrent 6 A, max duty ~97.85%, motor overpower 100 W.
- Soft: braking load ≈31 V, soft overcurrent 6 A, overspeed limit 5000 RPM.
- Latched fault semantics mirror the device; CLEAR‑FAULT is selective.

## 6. NSP Commands & Memory Map
- PING [0x00], PEEK [0x02], POKE [0x03], APPLICATION‑TELEMETRY [0x07], APPLICATION‑COMMAND [0x08], CLEAR‑FAULT [0x09], CONFIGURE‑PROTECTION [0x0A], TRIP‑LCL [0x0B].
- SLIP: END 0xC0, ESC 0xDB; CCITT CRC LSB,MSB.

## 7. Telemetry
Blocks: STANDARD, TEMPERATURES, VOLTAGES, CURRENTS, DIAGNOSTICS; sizes and field order mirror the ICD. Status and Fault registers follow ICD layout. ACK/NACK semantics preserved.

## 8. Console TUI (USB‑CDC)

**Non-scrolling, live-updating interface** (like `top`/`htop`) with arrow-key navigation and expand/collapse tables.

### 8.1 Screen Layout

```text
┌─ NRWA-T6 Emulator ──── Uptime: 00:15:32 ──── Tests: 78/78 ✓ ─────┐
├─ Status: ON │ Mode: SPEED │ RPM: 3245 │ Current: 1.25A │ Fault: -┤
├───────────────────────────────────────────────────────────────────┤
│ TABLES                                                            │
│                                                                   │
│ > 1. ▶ Built-In Tests      [COLLAPSED]                           │
│   2. ▶ Serial Interface    [COLLAPSED]                           │
│   3. ▼ Control Mode        [EXPANDED]                            │
│       ├─ mode          : SPEED       (RW)                         │
│     ► ├─ setpoint_rpm  : 3000        (RW)    ← cursor here       │
│       ├─ actual_rpm    : 3245        (RO)                         │
│       └─ pid_enabled   : true        (RW)                         │
│   4. ▶ Dynamics            [COLLAPSED]                            │
│   5. ▶ Protections         [COLLAPSED]                            │
│   6. ▶ Telemetry Blocks    [COLLAPSED]                            │
│   7. ▶ Config & JSON       [COLLAPSED]                            │
│                                                                   │
├───────────────────────────────────────────────────────────────────┤
│ ↑↓: Navigate │ →: Expand │ ←: Collapse │ R: Refresh │ Q: Quit   │
└───────────────────────────────────────────────────────────────────┘
```

### 8.2 Navigation

- **↑/↓** — Move cursor between tables/fields
- **→** — Expand selected table (show fields)
- **←** — Collapse expanded table
- **R** — Force screen refresh
- **Q** or **ESC** — Quit TUI

### 8.3 Tables

Tables are organized by access type: **Control** (read-write) and **Status** (read-only).

1. **Test Results Status** — Boot-time checkpoint test results (total, passed, failed, duration)
2. **Serial Status** — RS-485 status, tx/rx counts, errors, baud, port, SLIP, CRC
3. **NSP Status** — Last cmd/reply, poll, ack, command stats, timing
4. **Control Setpoints** — Mode, speed, current, torque, PWM, direction (all read-write)
5. **Dynamics Status** — Speed, momentum, torque, current, power, losses, α
6. **Protection Limits** — Configurable thresholds (overvolt, overspeed, overcurrent, etc.)
7. **Protection Status** — Fault flags, warning flags
8. **Telemetry Status** — Decoded STANDARD/TEMP/VOLT/CURR/DIAG blocks
9. **Config Status** — Scenarios, defaults, save/restore

Access type is implicit from table name - no per-field RO/RW labels needed.

## 8A. Table Catalog Architecture

The console uses a metadata-driven table catalog system where all device state, telemetry, and configuration are organized into tables and fields.

### 8A.1 Concepts

- **Table**: A named collection of fields with consistent access type (Control or Status). Corresponds to the 9 tables listed in §8.3.
- **Field**: A typed item with metadata: id, name, type, units, access level, default value, and data pointer.
- **Catalog**: A runtime registry of all tables/fields accessible via arrow-key navigation in the TUI.

### 8A.2 Table Catalog Structure

Each table contains fields with the following metadata:

- `tests` (Table 1) — Test Results Status: total tests, passed, failed, duration
- `serial` (Table 2) — Serial Status: RS-485 status, tx/rx counts, errors, baud rate, SLIP frames, CRC errors
- `nsp` (Table 3) — NSP Status: Last command/reply, poll flag, ACK bit, command statistics
- `control` (Table 4) — Control Setpoints: Mode, speed, current, torque, PWM, direction (all RW)
- `dynamics` (Table 5) — Dynamics Status: Speed (RPM), momentum, torque, current, power, losses, acceleration
- `protection_limits` (Table 6) — Protection Limits: Configurable thresholds (all RW)
- `protection_status` (Table 7) — Protection Status: Fault flags, warning flags (all RO)
- `telemetry` (Table 8) — Telemetry Status: Decoded telemetry block values (temperature, voltage, current, RPM)
- `config` (Table 9) — Config Status: Scenario status, defaults tracking, JSON loaded state

All fields are browse-able via the TUI. Control tables (with RW fields) can be modified via future command interface or NSP POKE commands. Access type is implicit from table name.

## 9. Error/Fault Injection

### 9.1 JSON Schema (excerpt)

```json
{
  "$schema": "https://json-schema.org/draft/2020-12/schema",
  "title": "NRWA-T6 Emulator Error Scenario",
  "type": "object",
  "properties": {
    "name": {"type": "string"},
    "description": {"type": "string"},
    "version": {"type": "string"},
    "schedule": {
      "type": "array",
      "items": {
        "type": "object",
        "properties": {
          "t_ms": {"type": "integer", "minimum": 0},
          "duration_ms": {"type": "integer", "minimum": 0},
          "condition": {
            "type": "object",
            "properties": {
              "mode_in": {"enum": ["CURRENT","SPEED","TORQUE","PWM"]},
              "rpm_gt": {"type": "number"},
              "rpm_lt": {"type": "number"},
              "nsp_cmd_eq": {"type": "string", "pattern": "^0x[0-9A-Fa-f]{2}$"}
            }
          },
          "action": {
            "type": "object",
            "properties": {
              "inject_crc_error": {"type": "boolean"},
              "drop_frames_pct": {"type": "number", "minimum": 0, "maximum": 100},
              "delay_reply_ms": {"type": "integer", "minimum": 0},
              "force_nack": {"type": "boolean"},
              "flip_status_bits": {"type": "integer"},
              "set_fault_bits": {"type": "integer"},
              "clear_fault_bits": {"type": "integer"},
              "limit_power_w": {"type": "number"},
              "limit_current_a": {"type": "number"},
              "limit_speed_rpm": {"type": "number"},
              "override_torque_mNm": {"type": "number"},
              "overspeed_fault": {"type": "boolean"},
              "trip_lcl": {"type": "boolean"}
            },
            "required": []
          }
        },
        "required": ["t_ms", "action"]
      }
    }
  },
  "required": ["name", "schedule"]
}
```

### 9.2 Semantics

- `condition` fields are ANDed; omitted fields are wildcards.
- Time is relative to scenario activation. Repeat by duplicating items.
- Transport errors act before CRC/SLIP; device faults set/clear live and latched bits according to CLEAR‑FAULT rules.
- Limits affect the physics loop immediately.

## 10. Configuration & PEEK/POKE Mapping

Representative addresses with UQ formats:
- Overvoltage threshold (UQ16.16 V)
- Braking load setpoint ≈31 V (UQ16.16 V)
- Overspeed fault 6000 RPM (UQ14.18) and soft limit 5000 RPM (UQ14.18)
- Motor overpower limit 100 W (UQ18.14 mW)
- Soft overcurrent 6 A (UQ18.14 mA)

## 11. Timing & Performance

- Telecommand parse within 2 ms; replies within 5 ms with Poll=1.
- Physics/control tick: 100 Hz; jitter < 200 µs.
- SLIP throughput sustains ≥ 1 kHz STANDARD telemetry requests without overrun.

## 12. Testing

- Host tester validates CRC, SLIP, ACK/NACK, telemetry sizes, and field encodings.
- HIL loopback checks torque vs momentum curves and zero‑cross behavior.
- Unit tests cover CRC, SLIP, NSP parsing, fixed‑point, protections.

## 13. Safety & Defaults

- Defaults: protections enabled, power limit 100 W, replies only on Poll.
- CLEAR‑FAULT required after overspeed fault to re‑enable drive.
- Console provides “defaults list” and “defaults restore”.

## 14. Deliverables

- Source tree per layout above
- SPEC.md and README with wiring and address pins
- Example JSON scenarios
- Host tester and logs
- UF2 binary
