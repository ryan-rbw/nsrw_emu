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

---

## 15. ICD Reference and Compliance

### 15.1 Source Document Authority

**CRITICAL**: This specification is derived from the NewSpace Systems ICD. When implementing or modifying protocol-related code, the ICD is the **authoritative source of truth**.

| Document | Revision | Sections | Authority |
|----------|----------|----------|-----------|
| NewSpace ICD N2-A2a-DD0021 | Rev 10.02 | All protocol details | **PRIMARY** |
| This SPEC.md | - | Architecture, implementation guidance | Secondary |

**Implementation Rule**: If this SPEC.md and the ICD ever conflict, **the ICD is correct**. Update SPEC.md to match.

### 15.2 How to Use the ICD

When implementing or debugging protocol code:

1. **Before Implementation**: Read the relevant ICD section and tables. Do not assume behavior based on "common patterns" or other protocols.

2. **Required ICD References for Each Command**:
   | Command | ICD Reference |
   |---------|---------------|
   | PING | Table 12-3 |
   | PEEK | Tables 12-5, 12-6 |
   | POKE | Table 12-9 |
   | APP-CMD | Tables 12-23, 12-25 |
   | APP-TELEM | Tables 12-13 through 12-17 |
   | CLEAR-FAULT | Table 12-27 |
   | CONFIG-PROT | Table 12-29 |
   | TRIP-LCL | Section 12.9.2 |

3. **Code Comments Must Cite ICD**: Every command handler and telemetry builder must include the ICD table number:
   ```c
   /**
    * @brief PING command handler
    *
    * ICD Table 12-3: PING reply contains 5 bytes
    */
   ```

4. **Test Vectors from ICD**: Create test cases directly from ICD examples before writing implementation code.

### 15.3 ICD Cross-Reference Checklist

Before marking any protocol implementation complete, verify against this checklist:

| Item | ICD Reference | Verified |
|------|---------------|----------|
| Byte order (all multi-byte) | ICD §3.2 "Little-endian" | ☐ |
| PING response format | Table 12-3 | ☐ |
| PEEK address size | Table 12-5 | ☐ |
| POKE payload format | Table 12-9 | ☐ |
| APP-CMD format | Table 12-23 | ☐ |
| Control mode bitmask | Table 12-25 | ☐ |
| STANDARD telemetry size | Table 12-13 | ☐ |
| TRIP-LCL no-reply behavior | §12.9.2 | ☐ |

---

## 16. Protocol Reference (Byte-Level) — ICD N2-A2a-DD0021 Rev 10.02

This section provides the **authoritative wire-level reference** for all message structures. All values are verified against ICD N2-A2a-DD0021 Rev 10.02.

### 16.1 Global Encoding Rules

**CRITICAL: ALL MULTI-BYTE VALUES ARE LITTLE-ENDIAN (LSB-first)**

This applies to:
- NSP CRC
- All telemetry fields (uint16, uint32, Q-formats)
- All command payloads
- All register values (PEEK/POKE)

| Value | Wire Bytes (hex) | Order |
|-------|------------------|-------|
| `0x12345678` (uint32) | `78 56 34 12` | LSB first |
| `0x1234` (uint16) | `34 12` | LSB first |
| CRC `0xA53F` | `3F A5` | LSB first |

**There are NO exceptions.** The ICD specifies little-endian throughout.

### 16.2 NSP Packet Structure

**Format** (after SLIP decoding):
```
[Dest:1][Src:1][Ctrl:1][Len:1][Data:N][CRC_L:1][CRC_H:1]
```

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 1 | Dest | Destination address (0x00-0x07, 0xFF=broadcast) |
| 1 | 1 | Src | Source address |
| 2 | 1 | Ctrl | Control byte (see below) |
| 3 | 1 | Len | Payload length (0-255) |
| 4 | N | Data | Command/response payload |
| 4+N | 1 | CRC_L | CRC-CCITT low byte |
| 5+N | 1 | CRC_H | CRC-CCITT high byte |

**Minimum packet size**: 6 bytes (Dest + Src + Ctrl + Len + CRC_L + CRC_H)

**Control Byte Format**:
```
Bit 7     Bit 6  Bit 5  Bits 4-0
[Poll] | [B] | [A] | [Command]
```

- **Poll** (bit 7): 1 = reply expected
- **B** (bit 6): Reserved
- **A** (bit 5): 1 = ACK, 0 = NACK (in replies)
- **Command** (bits 4-0): 5-bit command code

**CRC Calculation**:
- Polynomial: 0x1021 (CCITT)
- Initial value: 0xFFFF
- Bit order: LSB-first (reflect input/output)
- Covers: Dest + Src + Ctrl + Len + Data bytes
- **NOT** over CRC bytes themselves

### 16.3 Command Payload Formats

#### 0x00: PING (ICD Table 12-3)

**Request Payload**: None (0 bytes)

**Response Payload**: 5 bytes
| Offset | Size | Field | Value |
|--------|------|-------|-------|
| 0 | 1 | Device Type | 0x06 (NRWA-T6) |
| 1 | 1 | Serial Number | Device-specific |
| 2 | 1 | FW Major | e.g., 0x01 |
| 3 | 1 | FW Minor | e.g., 0x00 |
| 4 | 1 | FW Patch | e.g., 0x00 |

#### 0x02: PEEK (ICD Tables 12-5, 12-6)

**Request Payload**: 1 byte
| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 1 | Address | ICD RAM address (0x00-0x30) |

**Response Payload**: 4 bytes (little-endian uint32)

**ICD Address Map** (8-bit addresses, NOT 16-bit):
| ICD Addr | Description |
|----------|-------------|
| 0x00 | Device ID |
| 0x04 | Firmware Version |
| 0x08 | Control Mode (ICD bitmask) |
| 0x0C | Speed Setpoint (Q14.18 RPM) |
| 0x10 | Current Setpoint (Q14.18 mA) |
| 0x14 | Torque Setpoint (Q10.22 mN-m) |
| 0x18 | PWM Duty Cycle |
| 0x1C | Fault Status |
| 0x20 | Warning Status |
| 0x24 | Current Measurement (Q20.12 mA) |
| 0x28 | Speed Measurement (Q24.8 RPM) |
| 0x2C | Protection Enable Mask |
| 0x30 | LCL Status |

#### 0x03: POKE (ICD Table 12-9)

**Request Payload**: 5 bytes
| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 1 | Address | ICD RAM address (0x00-0x30) |
| 1-4 | 4 | Value | 32-bit value (little-endian) |

**Response Payload**: None (ACK/NACK only)

#### 0x07: APPLICATION-TELEMETRY (ICD Tables 12-13 to 12-17)

**Request Payload**: 1 byte
| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 1 | Block ID | Telemetry block selector |

**Response Payload**: Variable (see §16.4)

#### 0x08: APPLICATION-COMMAND (ICD Tables 12-23, 12-25)

**Request Payload**: 5 bytes
| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 1 | Mode | Control mode (ICD bitmask) |
| 1-4 | 4 | Setpoint | Mode-dependent value (LE) |

**Mode Byte Format** (ICD Table 12-25):
| Value | Mode | Setpoint Format |
|-------|------|-----------------|
| 0b0000 (0x00) | IDLE | Ignored |
| 0b0001 (0x01) | CURRENT | Q14.18 mA |
| 0b0010 (0x02) | SPEED | Q14.18 RPM |
| 0b0100 (0x04) | TORQUE | Q10.22 mN-m |
| 0b1000 (0x08) | PWM | Signed 32-bit (9-bit duty + sign) |

**PWM Setpoint Encoding**:
- Bits 8:0 = Duty cycle (0-512 maps to 0-100%)
- Bit 31 (sign) = Direction (negative = reverse)

**Response Payload**: None (ACK/NACK only)

#### 0x09: CLEAR-FAULT (ICD Table 12-27)

**Request Payload**: 4 bytes
| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0-3 | 4 | Fault Mask | Faults to clear (LE, 0xFFFFFFFF = all) |

**Response Payload**: None (ACK only)

**Note**: Does NOT clear LCL trip flag (requires hardware reset)

#### 0x0A: CONFIGURE-PROTECTION (ICD Table 12-29)

**Request Payload**: 4 bytes
| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0-3 | 4 | Disable Mask | Protection disable bits (LE) |

**Disable Mask Bits** (1 = DISABLED):
| Bit | Protection |
|-----|------------|
| 0 | Overspeed-fault |
| 1 | Overspeed-limit |
| 2 | Overcurrent-limit |
| 3 | EDAC scrub |
| 4 | Braking overvoltage-load |

**Response Payload**: None (ACK only)

#### 0x0B: TRIP-LCL (ICD Section 12.9.2)

**Request Payload**: None (0 bytes)

**Response**: **NO REPLY** if successful

Per ICD: "If successfully executed, no reply is sent because the LCL has tripped and power rails are disabled."

### 16.4 Telemetry Block Formats

**ALL FIELDS ARE LITTLE-ENDIAN**

#### Block 0x00: STANDARD (25 bytes) — ICD Table 12-13

| Offset | Size | Field | Type | Description |
|--------|------|-------|------|-------------|
| 0-3 | 4 | Status | uint32 LE | Bit 0: Operational, Bit 31: LCL Trip |
| 4-7 | 4 | Faults | uint32 LE | Combined fault_status \| fault_latch |
| 8 | 1 | Mode | uint8 | ICD bitmask (0x01/0x02/0x04/0x08) |
| 9-12 | 4 | Setpoint | uint32 LE | Mode-dependent Q-format |
| 13-14 | 2 | PWM Duty | int16 LE | Signed (bit 15=dir, bits 8:0=duty) |
| 15-16 | 2 | Current Target | Q14.2 LE | mA |
| 17-20 | 4 | Current Meas | Q20.12 LE | mA |
| 21-24 | 4 | Speed Meas | Q24.8 LE | RPM |

**Setpoint Field Format** (depends on Mode byte):
| Mode | Setpoint Format |
|------|-----------------|
| CURRENT (0x01) | Q14.18 mA |
| SPEED (0x02) | Q14.18 RPM |
| TORQUE (0x04) | Q10.22 mN-m |
| PWM (0x08) | Signed 32-bit duty |

#### Block 0x01: TEMPERATURES (6 bytes)

| Offset | Size | Field | Type |
|--------|------|-------|------|
| 0-1 | 2 | Motor Temp | UQ8.8 °C, LE |
| 2-3 | 2 | Driver Temp | UQ8.8 °C, LE |
| 4-5 | 2 | Board Temp | UQ8.8 °C, LE |

#### Block 0x02: VOLTAGES (12 bytes)

| Offset | Size | Field | Type |
|--------|------|-------|------|
| 0-3 | 4 | Bus Voltage | UQ16.16 V, LE |
| 4-7 | 4 | Phase A Voltage | UQ16.16 V, LE |
| 8-11 | 4 | Phase B Voltage | UQ16.16 V, LE |

#### Block 0x03: CURRENTS (12 bytes)

| Offset | Size | Field | Type |
|--------|------|-------|------|
| 0-3 | 4 | Phase A Current | UQ18.14 mA, LE |
| 4-7 | 4 | Phase B Current | UQ18.14 mA, LE |
| 8-11 | 4 | Bus Current | UQ18.14 mA, LE |

#### Block 0x04: DIAGNOSTICS (18 bytes)

| Offset | Size | Field | Type |
|--------|------|-------|------|
| 0-3 | 4 | Tick Count | uint32 LE |
| 4-7 | 4 | Uptime | uint32 LE (seconds) |
| 8-11 | 4 | Fault Count | uint32 LE |
| 12-15 | 4 | Command Count | uint32 LE |
| 16-17 | 2 | Max Jitter | uint16 LE (µs) |

### 16.5 Fixed-Point Q-Format Reference

| Format | Total Bits | Int Bits | Frac Bits | Range | Resolution | Usage |
|--------|------------|----------|-----------|-------|------------|-------|
| Q24.8 | 32 | 24 | 8 | 0-16M | 0.00391 | Speed measurement (RPM) |
| Q14.2 | 16 | 14 | 2 | 0-16383 | 0.25 | Current target (mA) |
| Q20.12 | 32 | 20 | 12 | 0-1M | 0.000244 | Current measurement (mA) |
| Q10.22 | 32 | 10 | 22 | 0-1023 | 0.000000238 | Torque (mN-m) |
| UQ14.18 | 32 | 14 | 18 | 0-16383 | 0.0000038 | Speed setpoint (RPM) |
| UQ16.16 | 32 | 16 | 16 | 0-65535 | 0.0000153 | Voltage (V) |
| UQ18.14 | 32 | 18 | 14 | 0-262143 | 0.0000610 | Torque/Current/Power |
| UQ8.8 | 16 | 8 | 8 | 0-255 | 0.00391 | Temperature (°C) |

**Conversion**:
```c
// Float to Q-format
uint32_t q_value = (uint32_t)(float_value * (1 << FRAC_BITS) + 0.5f);

// Q-format to float
float float_value = (float)q_value / (float)(1 << FRAC_BITS);
```

### 16.6 Control Mode Translation

The emulator uses **internal ordinal indices** (0-3) for array indexing in the physics model. The ICD uses **bitmask values** on the wire.

| Internal Index | ICD Bitmask | Mode Name |
|----------------|-------------|-----------|
| 0 | 0b0001 (0x01) | CURRENT |
| 1 | 0b0010 (0x02) | SPEED |
| 2 | 0b0100 (0x04) | TORQUE |
| 3 | 0b1000 (0x08) | PWM |
| N/A | 0b0000 (0x00) | IDLE (High-Z) |

**Translation Functions** (in `nss_nrwa_t6_regs.h`):
```c
uint8_t icd_mode_to_index(uint8_t icd_mode);  // Wire → Internal
uint8_t index_to_icd_mode(uint8_t index);     // Internal → Wire
```

### 16.7 Common Implementation Errors

These errors were identified during ICD compliance review. **Do not repeat them**:

| Error | Wrong | Correct | ICD Reference |
|-------|-------|---------|---------------|
| Byte order | Big-endian | **Little-endian everywhere** | §3.2 |
| PING response | Empty ACK | **5-byte device info** | Table 12-3 |
| PEEK address | 2-byte address | **1-byte address** | Table 12-5 |
| POKE format | [addr:2][count:1][data] | **[addr:1][data:4]** | Table 12-9 |
| APP-CMD format | Subcommand dispatch | **[mode:1][setpoint:4]** | Table 12-23 |
| Control mode encoding | Ordinal (0,1,2,3) | **Bitmask (0x01,0x02,0x04,0x08)** | Table 12-25 |
| STANDARD telemetry | 38 bytes | **25 bytes** | Table 12-13 |
| TRIP-LCL response | ACK | **No reply on success** | §12.9.2 |
| CONFIG-PROT format | [param_id:1][value:4] | **[disable_mask:4]** | Table 12-29 |

### 16.8 Wire Protocol Quick Reference

```
┌─────────────────────────────────────────────────────────────┐
│                    UNIVERSAL RULES                          │
├─────────────────────────────────────────────────────────────┤
│ • ALL multi-byte values: LITTLE-ENDIAN (LSB first)         │
│ • CRC: CCITT 0x1021, init 0xFFFF, LSB-first bit order      │
│ • SLIP: FEND=0xC0, FESC=0xDB, TFEND=0xDC, TFESC=0xDD       │
│ • Control modes on wire: bitmask (0x01, 0x02, 0x04, 0x08)  │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│                    COMMAND PAYLOADS                         │
├─────────────────────────────────────────────────────────────┤
│ PING:        (empty)           → 5 bytes device info       │
│ PEEK:        [addr:1]          → [value:4 LE]              │
│ POKE:        [addr:1][value:4 LE] → ACK                    │
│ APP-CMD:     [mode:1][setpoint:4 LE] → ACK                 │
│ APP-TELEM:   [block:1]         → (block data, all LE)      │
│ CLEAR-FAULT: [mask:4 LE]       → ACK                       │
│ CONFIG-PROT: [disable:4 LE]    → ACK                       │
│ TRIP-LCL:    (empty)           → NO REPLY                  │
└─────────────────────────────────────────────────────────────┘

┌─────────────────────────────────────────────────────────────┐
│                 TELEMETRY BLOCK SIZES                       │
├─────────────────────────────────────────────────────────────┤
│ STANDARD (0x00):     25 bytes                              │
│ TEMPERATURES (0x01):  6 bytes                              │
│ VOLTAGES (0x02):     12 bytes                              │
│ CURRENTS (0x03):     12 bytes                              │
│ DIAGNOSTICS (0x04):  18 bytes                              │
└─────────────────────────────────────────────────────────────┘
```

---

## 17. Dual-Core Architecture and State Synchronization

### 17.1 Core Responsibilities

| Core | Role | Priority | State Access |
|------|------|----------|--------------|
| **Core0** | Protocol (RS-485, NSP, Console) | Best-effort | Read telemetry snapshot, send commands |
| **Core1** | Physics (100 Hz tick, control loops) | Hard real-time | **Authoritative** state owner |

### 17.2 State Ownership Rules

**CRITICAL**: Core1 owns the authoritative `wheel_state_t`. Core0 must NEVER directly modify physics state.

| Operation | Core0 Action | Core1 Action |
|-----------|--------------|--------------|
| Mode change | `core_sync_send_command(CMD_SET_MODE, ...)` | Apply to `g_wheel_state` |
| Setpoint change | `core_sync_send_command(CMD_SET_SPEED, ...)` | Apply to `g_wheel_state` |
| Read telemetry | `core_sync_read_telemetry(&snapshot)` | Publish snapshots at 100 Hz |
| Clear faults | `core_sync_send_command(CMD_CLEAR_FAULT, ...)` | Clear fault bits |
| Trip LCL | `core_sync_send_command(CMD_TRIP_LCL, ...)` | Execute `wheel_model_trip_lcl()` |

### 17.3 Command Handler Compliance

Every NSP command handler that affects physics state MUST use `core_sync_send_command()`:

```c
// CORRECT: Send to Core1
void cmd_application_command(...) {
    core_sync_send_command(CMD_SET_MODE, (float)mode_index, 0.0f);
    core_sync_send_command(CMD_SET_SPEED, speed_rpm, 0.0f);
    build_ack(result);
}

// WRONG: Direct modification bypasses Core1
void cmd_application_command(...) {
    wheel_model_set_mode(g_wheel_state, mode);  // DON'T DO THIS
    build_ack(result);
}
```

### 17.4 Command Types (core_sync.h)

| Command | Description | param1 |
|---------|-------------|--------|
| `CMD_SET_MODE` | Change control mode | Mode index (0-3) |
| `CMD_SET_SPEED` | Speed setpoint | RPM (float) |
| `CMD_SET_CURRENT` | Current setpoint | Amps (float) |
| `CMD_SET_TORQUE` | Torque setpoint | mN-m (float) |
| `CMD_SET_PWM` | PWM duty cycle | Percent (float) |
| `CMD_CLEAR_FAULT` | Clear fault bits | Mask (as float) |
| `CMD_TRIP_LCL` | Test LCL trip | Unused |
| `CMD_CONFIG_PROTECTION` | Protection enable | Mask (as float) |
| `CMD_RESET` | Soft reset | Unused |
