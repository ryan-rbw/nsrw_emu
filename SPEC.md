# Reaction Wheel Emulator — SPEC.md (NRWA-T6 Compatible, RP2040/Pico)

## 1. Goal

Emulate a NewSpace Systems NRWA‑T6 reaction wheel so that the OBC/GNC stack cannot distinguish the emulator from real hardware. The emulator must:
- Speak the same physical/data‑link and NSP protocol over RS‑485 with SLIP framing and CCITT CRC.
- Implement command handling, telemetry, protection and fault semantics compatible with the NRWA‑T6 ICD.
- Simulate realistic dynamics, limits, and timing for current, speed, and torque control modes, including soft/hard protections.
- Provide a local terminal UI on USB‑CDC with drill‑down “tables” and fields for live status and control.
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
- `console/*`: USB‑CDC TUI with table/field navigation, command palette, JSON loader.
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
│ ↑↓: Navigate │ →: Expand │ ←: Collapse │ Enter: Edit │ C: Command│
└───────────────────────────────────────────────────────────────────┘
```

### 8.2 Navigation

- **↑/↓** — Move cursor between tables/fields
- **→** — Expand selected table (show fields)
- **←** — Collapse expanded table
- **Enter** — Edit selected field (prompts for new value)
- **C** — Enter command mode (see §8A)
- **R** — Force screen refresh
- **Q** or **ESC** — Quit TUI

### 8.3 Tables

1. **Built-In Tests** — Boot-time checkpoint test results (total, passed, failed, duration)
2. **Serial Interface** — Status, tx/rx counts, errors, baud, port, SLIP, CRC
3. **NSP Layer** — Last cmd/reply, poll, ack, command stats, timing
4. **Control Mode** — Active mode, setpoint, direction, PWM, source
5. **Dynamics** — Speed, momentum, torque, current, power, losses, α
6. **Protections** — Thresholds, power limit, flags, clear faults
7. **Telemetry Blocks** — Decoded STANDARD/TEMP/VOLT/CURR/DIAG
8. **Config & JSON** — Scenarios, defaults, save/restore

## 8A. Terminal Command Palette & “Database” Tables

The console exposes both a menu UI and a command palette for power users. The emulator maintains an internal “database” of tables and fields that map 1:1 to device registers, telemetry, driver stats, and emulator-only controls. All tables and fields are addressable by IDs and by stable string names.

### 8A.1 Concepts

- **Table**: A named collection of fields (read-only, write-only, or read-write). Mirrors the sections in §8.
- **Field**: A typed item with metadata: id, name, type, units, access, default, encoder/decoder.
- **Catalog**: A runtime registry of all tables/fields. Used by the TUI and command palette.
- **Change Tracker**: Tracks any field whose current value differs from its compiled default. Persists until “restore-defaults” or power cycle.

### 8A.2 Command Mode

Press **C** in browse mode to enter command mode. Commands support **partial prefix matching**:

**Examples:**

- `d t l` → `database table list`
- `da t list` → `database table list`
- `db tab get control.mode` → `database table get control.mode`
- `d t s control.setpoint_rpm 5000` → `database table set control.setpoint_rpm 5000`
- `def` → `database defaults`
- `?` → `help`

### 8A.3 Built-in Commands

Commands are **case-insensitive** with **prefix matching**. Aliases shown in parentheses.

**General:**

- `help` or `?` — Show help (or `help <command>` for details)
- `quit` (`q`, `exit`) — Exit TUI
- `version` — Firmware version, build time, git describe
- `uptime` — Milliseconds since boot

**Database Commands:**

- `database` (`db`, `d`)
  - `table` (`t`, `tab`)
    - `list` (`ls`, `l`) — List all tables with ID, name, brief
    - `get <table>.<field>` (`g`) — Read field value (decoded units)
    - `set <table>.<field> <value>` (`s`) — Write field (type validated)
    - `describe <table>` (`desc`) — Show all fields in table
  - `defaults` (`def`)
    - `list` — Show non-default fields (change tracker)
    - `restore [<table>|<table>.<field>]` — Reset to compiled defaults

**Protocol Commands:**

- `peek <addr> <len>` — Raw register read (hex output)
- `poke <addr> <hex-bytes>` — Raw register write
- `nsp stats` — NSP command counters
- `serial stats` — UART/RS-485/SLIP stats

**Scenario Commands:**

- `scenario list` — List JSON scenarios in flash
- `scenario load <name>` — Load and activate scenario
- `scenario status` — Active scenario timers

**Protection Commands:**

- `protect enable <name>` — Enable soft protection
- `protect disable <name>` — Disable soft protection
- `fault clear [all|mask]` — Clear latched faults

All commands return: `OK` or `ERR <code> <message>`.

### 8A.4 Database Catalog Structure (Examples)

- `serial` (Table 1)
  - `status` (1.1, bool)
  - `tx_count` (1.2, u32), `rx_count` (1.3, u32), `tx_errors` (1.4, u32), `rx_errors` (1.5, u32)
  - `baud_kbps` (1.6, u16), `port_active` (1.7, enum A|B), `slip_ok` (1.8, u32), `crc_err` (1.9, u32)

- `nsp` (Table 2)
  - `last_cmd` (2.1, hex[...]), `last_reply` (2.2, hex[...])
  - `poll_seen` (2.3, bool), `ack_bit` (2.4, bool), `cmd_stats` (2.5, struct), `seq_timing_ms` (2.6, u32)

- `control` (Table 3)
  - `mode` (3.1, enum), `setpoint` (3.2, fixed), `direction` (3.3, enum), `pwm_duty` (3.4, q8.8), `source` (3.5, enum)

- `dynamics` (Table 4)
  - `speed_rpm` (4.1, q14.18), `momentum_nms` (4.2, q18.14), `torque_mNm_cmd` (4.3, q18.14), `torque_mNm_out` (4.3b, q18.14)
  - `current_a_cmd` (4.4, q18.14), `current_a_out` (4.4b, q18.14), `power_w` (4.5, q18.14)
  - `loss_a` (4.6a, q18.14), `loss_b` (4.6b, q18.14), `loss_c` (4.6c, q18.14), `alpha_rad_s2` (4.7, q18.14)

- `protections` (Table 5)
  - thresholds and flags per §5.4

- `telemetry` (Table 6)
  - sub-blocks with decoded views

- `config` (Table 7)
  - scenario and defaults management

### 8A.4 Non-default Tracking

- Each field stores `compiled_default`, `current_value`, and a `dirty` bit.
- On any successful `set`, the emulator updates `current_value` and toggles `dirty` when `current_value != compiled_default`.
- `defaults list` prints a compact diff: `<table>.<field>: default=<X> current=<Y>`.
- `defaults restore` clears `dirty`, writes `compiled_default` back to the active map, and emits a change event to NSP (if applicable).
- The change tracker ignores read-only fields and transient counters unless explicitly opted-in.

### 8A.5 Leaving the UI

- `exit` / `quit` closes the console task gracefully, deinitializes USB-CDC, and returns to the run loop.
- If `auto-reopen` is enabled (default), the device will re-enumerate the console on the next USB connect.

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
