# NRWA-T6 Reaction Wheel Emulator — Implementation Plan

**Project**: Reaction Wheel Emulator for NewSpace Systems NRWA-T6
**Platform**: Raspberry Pi Pico (RP2040)
**Created**: 2025-11-05
**Status**: Planning → Implementation

---

## 1. Executive Summary

This document outlines the implementation plan for a hardware-in-the-loop emulator that replicates the NewSpace Systems NRWA-T6 reaction wheel. The emulator must be **protocol-perfect** and **timing-accurate** to the point where the spacecraft OBC/GNC stack cannot distinguish it from real hardware.

### Key Requirements
- **Protocol Fidelity**: RS-485 with SLIP framing, NSP protocol, CCITT CRC matching ICD spec
- **Real-time Performance**: 100 Hz physics tick with <200µs jitter, <5ms command response
- **Dual-Core Architecture**: Core0 for communications, Core1 for physics simulation
- **Rich Developer UX**: Terminal UI with table/field catalog and command palette
- **Fault Injection**: JSON-driven deterministic error/fault scenarios
- **Single Binary**: UF2 file for drag-and-drop programming

### Estimated Scope
- **Lines of Code**: ~3,000-5,000 LOC (C/C++)
- **Development Phases**: 10 major phases
- **Critical Path**: Drivers → Device Model → Console → Fault Injection

---

## 2. Architecture Overview

### 2.1 Directory Structure

Per [SPEC.md:49-86](SPEC.md#L49-L86):

```
reaction-wheel-emulator/
├─ CMakeLists.txt              # Top-level build config
├─ README.md                   # Wiring, quickstart, address pins
├─ SPEC.md                     # Specification (reference)
├─ IMP.md                      # This implementation plan
├─ firmware/
│  ├─ app_main.c               # Dual-core orchestration
│  ├─ platform/                # RP2040-specific HAL
│  │  ├─ board_pico.h          # Pin definitions
│  │  ├─ gpio_map.c            # GPIO init, ADDR pins
│  │  └─ timebase.c            # 100 Hz alarm, µs timer
│  ├─ drivers/                 # Communication stack
│  │  ├─ rs485_uart.c          # UART1, DE/RE, tri-state
│  │  ├─ slip.c                # SLIP encode/decode
│  │  ├─ nsp.c                 # NSP packet handling
│  │  ├─ crc_ccitt.c           # CRC-16 CCITT
│  │  └─ leds.c                # Status LEDs
│  ├─ device/                  # Wheel emulation
│  │  ├─ nss_nrwa_t6_regs.h    # Register map
│  │  ├─ nss_nrwa_t6_model.c   # Physics/control
│  │  ├─ nss_nrwa_t6_commands.c # NSP command handlers
│  │  ├─ nss_nrwa_t6_telemetry.c # Telemetry blocks
│  │  └─ nss_nrwa_t6_protection.c # Protections/faults
│  ├─ console/                 # USB-CDC TUI
│  │  ├─ tui.c                 # Menu system
│  │  ├─ tables.c              # Table/field catalog
│  │  └─ json_loader.c         # Scenario parser
│  ├─ config/
│  │  ├─ defaults.toml         # Default parameters
│  │  └─ error_schema.json     # JSON scenario schema
│  └─ util/
│     ├─ ringbuf.c             # Lock-free SPSC queue
│     └─ fixedpoint.h          # UQ format helpers
├─ tools/
│  ├─ host_tester.py           # NSP test harness
│  └─ errorgen.py              # JSON scenario builder
└─ tests/
   ├─ unit/                    # CRC, SLIP, fixedpoint
   └─ hilsim/                  # HIL validation
```

### 2.2 Dual-Core Strategy

| Core | Responsibilities | Priority |
|------|------------------|----------|
| **Core0** | RS-485 RX/TX, NSP protocol, USB-CDC console, command dispatch | Best-effort |
| **Core1** | 100 Hz physics tick, control loops, protection checks | Hard real-time |

**Inter-Core Communication**:
- **Core1 → Core0**: Lock-free SPSC ring buffer (telemetry snapshots)
- **Core0 → Core1**: Spinlock-protected command mailbox (setpoints, mode changes)

### 2.3 Key Interfaces

| Interface | Config | Purpose |
|-----------|--------|---------|
| **UART1** | 460.8 kbps, 8-N-1 | RS-485 telecommand/telemetry |
| **USB-CDC** | Virtual serial | Developer console (TUI) |
| **GPIO** | ADDR[2:0], FAULT, RESET, DE, RE | Address selection, status |
| **Flash** | LittleFS or raw | JSON scenarios, config |

---

## 3. Implementation Phases

### Phase 1: Project Foundation ⚙️

**Goal**: Buildable "Hello World" with Pico SDK

**Tasks**:
1. Create directory structure
2. CMake setup:
   - Find Pico SDK (`PICO_SDK_PATH`)
   - Linker script, UF2 output
   - Version string from `git describe`
3. Minimal `app_main.c`:
   ```c
   int main() {
       stdio_init_all();
       printf("NRWA-T6 Emulator v0.1\n");
       while(1) tight_loop_contents();
   }
   ```
4. Build test: `cmake .. && make` → `.uf2`
5. Flash test: LED blink on Pico

**Deliverables**:
- `CMakeLists.txt`
- Stub `app_main.c`
- `.gitignore` (build/, *.uf2, .vscode/)
- Build validates on host

**Acceptance**: UF2 loads, prints version over USB

---

### Phase 2: Platform Layer 🔌

**Goal**: Hardware abstraction for RP2040

**Tasks**:
1. **board_pico.h**: Pin definitions
   ```c
   #define UART_TX_PIN 4
   #define UART_RX_PIN 5
   #define RS485_DE_PIN 6
   #define RS485_RE_PIN 7
   #define ADDR0_PIN 10
   #define ADDR1_PIN 11
   #define ADDR2_PIN 12
   #define FAULT_PIN 13
   #define RESET_PIN 14
   ```
2. **gpio_map.c**:
   - `gpio_init_all()`: Configure all pins
   - `gpio_read_address()`: Read ADDR[2:0] → device ID
3. **timebase.c**:
   - `timebase_init()`: Setup hardware alarm for 100 Hz
   - `timebase_get_us()`: Microsecond counter
   - Alarm callback stub for Core1 tick

**Deliverables**:
- `platform/board_pico.h`
- `platform/gpio_map.c`
- `platform/timebase.c`

**Acceptance**: Address pins read correctly, alarm fires at 100 Hz

---

### Phase 3: Core Communication Drivers 📡

**Goal**: RS-485, SLIP, NSP, CRC working end-to-end

**Tasks**:

#### 3.1 CRC-CCITT (`crc_ccitt.c`)
- Init: 0xFFFF
- Polynomial: 0x1021
- Bit order: LSB-first per [SPEC.md:44](SPEC.md#L44)
- Test vectors from standard
- `uint16_t crc_ccitt(const uint8_t *data, size_t len)`

#### 3.2 SLIP Codec (`slip.c`)
- Constants: END=0xC0, ESC=0xDB, ESC_END=0xDC, ESC_ESC=0xDD
- State machine encoder/decoder
- Edge cases: leading/trailing END, escaped bytes
- `slip_encode(in, in_len, out, out_max) → out_len`
- `slip_decode(in, in_len, out, out_max) → out_len or -1`

#### 3.3 RS-485 UART (`rs485_uart.c`)
- UART1 config: 460800 baud, 8-N-1
- DE/RE timing: DE high 10µs before TX, low 10µs after
- Tri-state when idle (multi-drop TXD sharing)
- Redundancy: Primary/backup port switching on CRC fail
- RX ring buffer (1KB)
- `rs485_send(data, len)`
- `rs485_recv(buf, max) → len`

#### 3.4 NSP Protocol (`nsp.c`)
- Message Control byte: `[Poll:1|B:1|A:1|Command:5]`
- Packet layout: `[Dest|Src|Ctrl|Len|Data...|CRC_L|CRC_H]`
- Command dispatch table for handlers
- ACK/NACK generation
- `nsp_parse(slip_frame) → command_id, payload`
- `nsp_build_reply(ack, data) → slip_frame`

#### 3.5 LEDs (`leds.c`)
- Status LED: Blink pattern for comms activity
- Fault LED: Solid on fault, blink on warning

**Deliverables**:
- `drivers/crc_ccitt.c` + unit tests
- `drivers/slip.c` + unit tests
- `drivers/rs485_uart.c`
- `drivers/nsp.c`
- `drivers/leds.c`

**Acceptance**:
- PING command (0x00) → ACK reply validated with logic analyzer
- CRC matches test vectors
- SLIP handles escaped bytes correctly

---

### Phase 4: Utilities Foundation 🛠️

**Goal**: Support libraries for device model

**Tasks**:

#### 4.1 Ring Buffer (`ringbuf.c`)
- Lock-free SPSC (single producer, single consumer)
- Power-of-2 size for fast modulo
- Memory barriers for inter-core safety
- `ringbuf_push(rb, item)`
- `ringbuf_pop(rb, item) → bool`

#### 4.2 Fixed-Point Math (`fixedpoint.h`)
- UQ14.18: Speed (rpm)
- UQ16.16: Voltage (V)
- UQ18.14: Torque (mN·m), current (mA), power (mW)
- Conversions: `float_to_uq14_18(f) → uint32_t`
- Arithmetic: Saturating add/sub, multiply with shift

**Deliverables**:
- `util/ringbuf.c` + unit tests
- `util/fixedpoint.h` + unit tests

**Acceptance**:
- Ring buffer survives 1M push/pop cycles
- Fixed-point conversions match reference within 1 LSB

---

### Phase 5: Device Model & Physics ⚙️

**Goal**: Wheel dynamics simulation running on Core1

**Tasks**:

#### 5.1 Register Map (`nss_nrwa_t6_regs.h`)
- Define all PEEK/POKE addresses
- Structures for control/status/config registers
- Example:
  ```c
  #define REG_OVERVOLTAGE_THRESHOLD  0x0100  // UQ16.16
  #define REG_OVERSPEED_FAULT_RPM    0x0104  // UQ14.18
  #define REG_MOTOR_OVERPOWER_LIMIT  0x0108  // UQ18.14 mW
  ```

#### 5.2 Physics Model (`nss_nrwa_t6_model.c`)
- State variables:
  ```c
  struct wheel_state {
      float omega_rad_s;      // Angular velocity
      float momentum_nms;     // H = I·ω
      float current_cmd_a;    // Commanded current
      float current_out_a;    // Actual current
      float torque_cmd_mnm;   // Commanded torque
      float torque_out_mnm;   // Output torque
      float power_w;          // Electrical power
      uint32_t mode;          // CURRENT|SPEED|TORQUE|PWM
  };
  ```
- Dynamics (Δt = 10 ms):
  - τ_motor = k_t · i (k_t ≈ 0.0534 N·m/A)
  - τ_loss = a·ω + b·sign(ω) + c·i²
  - α = (τ_motor - τ_loss) / I
  - ω_new = ω_old + α·Δt
- Control modes:
  - **Current**: Direct i_cmd → τ
  - **Speed**: PI controller with anti-windup
  - **Torque**: ΔH feed-forward
  - **PWM**: Backup duty-cycle mode
- Limits:
  - Power: |τ·ω| ≤ P_lim (100 W)
  - Duty: ≤ 97.85%
  - Current: ≤ 6 A

#### 5.3 Core1 Tick
- Alarm callback at 100 Hz
- Update physics
- Check protections
- Publish state to ring buffer (→ Core0)
- Measure jitter with `time_us_64()`

**Deliverables**:
- `device/nss_nrwa_t6_regs.h`
- `device/nss_nrwa_t6_model.c`
- Core1 loop in `app_main.c`

**Acceptance**:
- Spin-up to 3000 RPM in speed mode
- Jitter < 200 µs (99th percentile)
- Power limit enforced correctly

---

### Phase 6: Device Commands & Telemetry 📊

**Goal**: NSP command handlers and telemetry blocks

**Tasks**:

#### 6.1 Commands (`nss_nrwa_t6_commands.c`)
Implement per [SPEC.md:35-42](SPEC.md#L35-L42):

| Command | Code | Behavior |
|---------|------|----------|
| PING | 0x00 | ACK reply with no data |
| PEEK | 0x02 | Read register(s) → data payload |
| POKE | 0x03 | Write register(s), ACK/NACK |
| APPLICATION-TELEMETRY | 0x07 | Return block (STANDARD/TEMP/VOLT/etc.) |
| APPLICATION-COMMAND | 0x08 | Mode/setpoint changes |
| CLEAR-FAULT | 0x09 | Clear latched faults by mask |
| CONFIGURE-PROTECTION | 0x0A | Update protection thresholds |
| TRIP-LCL | 0x0B | Test fault (local current limit) |

- Parse payload, validate ranges
- Update device state (via Core1 mailbox)
- Build reply with correct CRC

#### 6.2 Telemetry (`nss_nrwa_t6_telemetry.c`)
Blocks per [SPEC.md:129-130](SPEC.md#L129-L130):

- **STANDARD**: Status, faults, mode, speed, current, torque, power
- **TEMPERATURES**: Motor, driver, board sensors
- **VOLTAGES**: DC bus, phase voltages
- **CURRENTS**: Phase currents, bus current
- **DIAGNOSTICS**: Counters, timing stats

Each field encoded in UQ format per ICD

**Deliverables**:
- `device/nss_nrwa_t6_commands.c`
- `device/nss_nrwa_t6_telemetry.c`
- Command dispatch in `nsp.c`

**Acceptance**:
- PEEK/POKE round-trip verified
- Telemetry block sizes match ICD
- APPLICATION-COMMAND changes mode correctly

---

### Phase 7: Protection System 🛡️

**Goal**: Soft/hard protections and fault semantics

**Tasks**:

#### 7.1 Protection Thresholds (`nss_nrwa_t6_protection.c`)

**Hard Protections** (immediate shutdown):
- Overvoltage: 36 V
- Phase overcurrent: 6 A
- Max duty: 97.85%
- Motor overpower: 100 W

**Soft Protections** (warnings → eventual fault):
- Braking load: ≈31 V
- Soft overcurrent: 6 A
- Overspeed soft limit: 5000 RPM
- Overspeed latched fault: 6000 RPM

#### 7.2 Fault Register
- Status bits (live): overspeed, overcurrent, overvoltage, etc.
- Fault bits (latched): Require CLEAR-FAULT to reset
- CLEAR-FAULT mask: Selective clearing

#### 7.3 Integration
- Check protections every physics tick
- Set/clear status bits immediately
- Latch faults on thresholds
- Disable drive outputs on hard fault
- Require CLEAR-FAULT to re-enable

**Deliverables**:
- `device/nss_nrwa_t6_protection.c`
- Protection checks in `nss_nrwa_t6_model.c`

**Acceptance**:
- Overspeed at 6000 RPM latches fault
- CLEAR-FAULT re-enables drive
- Overcurrent shuts down motor

---

### Phase 8: Console & TUI 🖥️

**Goal**: USB-CDC terminal with table/field catalog and command palette

**Tasks**:

#### 8.1 Menu System (`tui.c`)
- USB-CDC stdio
- Navigation: numbers select item, `q` back, `r` refresh, `e` edit
- 7 top-level tables per [SPEC.md:136-143](SPEC.md#L136-L143):
  1. Serial Interface
  2. NSP Layer
  3. Control Mode
  4. Dynamics
  5. Protections
  6. Telemetry Blocks
  7. Config & JSON

#### 8.2 Table/Field Catalog (`tables.c`)
- Metadata structure:
  ```c
  struct field_meta {
      uint16_t id;           // e.g., 4.1
      const char *name;      // "speed_rpm"
      uint8_t type;          // BOOL|U32|Q14_18|ENUM
      const char *units;     // "RPM"
      uint8_t access;        // RO|WO|RW
      uint32_t default_val;  // Compiled default
      uint32_t *current_ptr; // Live value pointer
      bool dirty;            // != default
  };
  ```
- Catalog registry: Array of tables, each with array of fields
- `describe <table>`: Print fields with metadata
- `get <table>.<field>`: Read and format value
- `set <table>.<field> <value>`: Parse and write with validation

#### 8.3 Command Palette
Per [SPEC.md:158-180](SPEC.md#L158-L180):

```
> help
Available commands:
  help [cmd]                    Show help
  version                       Firmware version
  tables                        List all tables
  describe <table>              Show table fields
  get <table>.<field>           Read field
  set <table>.<field> <val>     Write field
  peek <addr> <len>             Raw register read
  poke <addr> <hex>             Raw register write
  defaults list                 Show non-default fields
  defaults restore [scope]      Reset to defaults
  scenario load <name>          Load JSON scenario
  nsp stats                     NSP command counters
  fault clear [mask]            Clear latched faults
```

#### 8.4 Change Tracker
- Track `dirty` bit when value != default
- `defaults list`: Print diff
- `defaults restore`: Reset to compiled defaults

**Deliverables**:
- `console/tui.c`
- `console/tables.c`
- Catalog definitions for all 7 tables

**Acceptance**:
- Navigate to Dynamics table, see live speed
- `set control.mode SPEED` changes mode
- `defaults list` shows non-defaults
- Command palette autocomplete works

---

### Phase 9: Fault Injection System 💉

**Goal**: JSON scenario loader and runtime injection

**Tasks**:

#### 9.1 JSON Schema Validation
- Schema per [SPEC.md:228-278](SPEC.md#L228-L278)
- Required fields: `name`, `schedule`
- Optional: `description`, `version`
- Schedule items: `t_ms`, `action`, optional `condition`

#### 9.2 JSON Parser (`json_loader.c`)
- Use `cJSON` library (or minimal parser)
- Load from flash (LittleFS or raw sectors)
- Parse to internal scenario structure:
  ```c
  struct injection_event {
      uint32_t t_ms;
      struct condition cond;    // mode, rpm range, nsp_cmd
      struct action act;        // inject_crc, drop_frames, set_fault, etc.
  };
  ```

#### 9.3 Scenario Engine
- Activation: `scenario load <name>` sets active scenario
- Tick timer: Check events against `time_since_activation`
- Apply actions:
  - **Transport**: Corrupt CRC before send, drop frames
  - **Device**: Flip status bits, set/clear faults
  - **Physics**: Override limits (power, current, speed)
- Status: `scenario status` shows active events

**Deliverables**:
- `console/json_loader.c`
- Scenario engine in `app_main.c` (Core0)
- Example JSON files in `config/`

**Acceptance**:
- Load scenario: overspeed fault at t=5s
- Verify fault triggered at correct time
- CRC injection causes retries

---

### Phase 10: Main Application & Dual-Core 🚀

**Goal**: Orchestrate Core0/Core1, watchdog, startup

**Tasks**:

#### 10.1 Core0 Main Loop
```c
void core0_main() {
    // Init
    gpio_init_all();
    rs485_init();
    usb_cdc_init();
    tables_init();

    // Launch Core1
    multicore_launch_core1(core1_main);

    while (1) {
        // RS-485 RX
        if (rs485_recv(rx_buf, &len)) {
            slip_decode(rx_buf, len, packet);
            nsp_parse(packet, &cmd_id, &payload);
            // Dispatch command handler
            reply = handle_command(cmd_id, payload);
            slip_encode(reply, tx_buf);
            rs485_send(tx_buf);
        }

        // USB Console
        if (usb_available()) {
            tui_handle_input();
        }

        // Scenario tick
        scenario_update();
    }
}
```

#### 10.2 Core1 Physics Loop
```c
void core1_main() {
    timebase_init_alarm();

    while (1) {
        // Wait for 100 Hz tick
        wait_for_alarm();

        uint32_t t_start = time_us_32();

        // Read commands from Core0
        if (spinlock_try_acquire(&cmd_lock)) {
            apply_commands();
            spinlock_release(&cmd_lock);
        }

        // Update physics
        wheel_model_update(10000); // 10ms tick

        // Check protections
        protection_check();

        // Publish state to Core0
        ringbuf_push(&telem_rb, &wheel_state);

        uint32_t t_end = time_us_32();
        uint32_t jitter = t_end - t_start;
        // Log jitter if > 200µs
    }
}
```

#### 10.3 Synchronization
- **Spinlock**: Protect command mailbox (Core0 writes, Core1 reads)
- **Ring buffer**: Telemetry snapshots (Core1 writes, Core0 reads)
- **Memory barriers**: Ensure visibility across cores

#### 10.4 Watchdog
- Enable WDT with 1s timeout
- Pet from both cores to detect hangs

**Deliverables**:
- Complete `app_main.c`
- Dual-core orchestration
- Watchdog and fault recovery

**Acceptance**:
- Both cores running stable
- Commands from RS-485 change physics state
- Console shows live telemetry
- No crashes after 24h soak test

---

## 4. Testing Strategy

### 4.1 Unit Tests (`tests/unit/`)

| Module | Test Cases | Tools |
|--------|------------|-------|
| CRC-CCITT | Standard vectors, edge cases | Unity framework |
| SLIP | Encode/decode round-trip, escaped bytes | Unity |
| Fixed-point | Conversion accuracy, saturation | Unity |
| Protection | Threshold logic, fault latching | Unity |

**Run**: `cmake -DBUILD_TESTS=ON .. && make test`

### 4.2 Host Integration (`tools/host_tester.py`)

```python
# Example test sequence
def test_ping():
    pkt = build_nsp_packet(cmd=0x00, poll=1)
    send_rs485(pkt)
    reply = recv_rs485()
    assert crc_valid(reply)
    assert reply.ack == True

def test_speed_mode():
    poke(REG_MODE, MODE_SPEED)
    poke(REG_SETPOINT_RPM, 3000)
    time.sleep(1.0)
    telem = request_telemetry(STANDARD)
    assert abs(telem.speed_rpm - 3000) < 50
```

**Coverage**:
- All NSP commands
- Telemetry block integrity
- Timing measurements (response < 5ms)
- CRC/SLIP validation

### 4.3 HIL Simulation (`tests/hilsim/`)

**Scenarios**:
1. **Torque vs. Momentum**: Verify I·α integration
2. **Zero-Crossing**: Sign changes, loss model
3. **Protection Trip**: Overspeed, overcurrent thresholds
4. **Mode Transitions**: Current → Speed → Torque
5. **Fault Recovery**: CLEAR-FAULT re-enables drive

**Tools**: Python scripts driving emulator, logging telemetry to CSV

---

## 5. Critical Design Decisions

### 5.1 Inter-Core Communication

**Decision**: Lock-free SPSC ring buffer (Core1 → Core0) + spinlock mailbox (Core0 → Core1)

**Rationale**:
- Ring buffer: Zero-copy telemetry snapshots, no blocking on Core1
- Spinlock: Commands are rare (< 100 Hz), acceptable to block Core0 briefly
- Memory barriers ensure cache coherency on RP2040

**Alternatives Rejected**:
- FreeRTOS queues: Overkill, adds RTOS overhead
- Shared state with mutex: Risk of priority inversion

### 5.2 Timing Architecture

**Decision**: Hardware alarm for Core1 tick, polling for Core0

**Rationale**:
- Core1 needs deterministic 100 Hz → alarm guarantees this
- Core0 handles bursty I/O (RS-485, USB) → polling is sufficient
- Jitter budget: 200µs allows for protection checks + integration

**Validation**: Oscilloscope on GPIO toggle every tick

### 5.3 SLIP Implementation

**Decision**: Software SLIP in C, consider PIO if needed

**Rationale**:
- Software SLIP: Simple, portable, easy to debug
- PIO state machine: Offload byte-stuffing if CPU-bound
- Threshold: If SLIP encoding takes > 50µs, move to PIO

**Benchmark**: Measure SLIP encode/decode latency in Phase 3

### 5.4 JSON Parsing

**Decision**: Use `cJSON` library

**Rationale**:
- Mature, small footprint (~15 KB)
- Easy integration with Pico SDK
- Flash budget: 2 MB available, code target < 256 KB

**Fallback**: Minimal hand-written parser if flash-constrained

### 5.5 Default Configuration

**Decision**: Compile-time defaults from `defaults.toml`, runtime overrides

**Rationale**:
- TOML → C struct at build time (Python script)
- No flash writes for defaults → faster boot
- Runtime changes tracked in `dirty` bits

**Alternative**: Store defaults in flash (slower, more complexity)

---

## 6. Development Sequence

### Recommended Build Order (Vertical Slices)

1. **Minimal viable path**: Platform → UART → SLIP → NSP → PING → LED
   *Validates end-to-end comms stack*

2. **Add physics**: Core1 tick → simple ω integrator → telemetry readback
   *Proves dual-core + real-time loop*

3. **Add control**: Speed mode → setpoint tracking → PI loop
   *Demonstrates control modes*

4. **Add protections**: Overspeed detection → fault latching → CLEAR-FAULT
   *Validates fault semantics*

5. **Add TUI**: Menu → table display → command palette
   *Developer UX working*

6. **Add injection**: JSON loader → scenario engine
   *Fault injection operational*

**Rationale**: Each slice is testable and adds value. Avoids "big bang" integration.

---

## 7. Risks & Mitigations

| Risk | Likelihood | Impact | Mitigation |
|------|------------|--------|------------|
| **Timing jitter on Core1** | Medium | High | Use hardware alarm, validate with scope, budget 50µs margin |
| **SLIP/CRC protocol bugs** | High | High | Comprehensive test vectors, cross-check with host tester |
| **Flash exhaustion** | Low | Medium | Code size profiling, link-time optimization (LTO), XIP for tables |
| **USB-CDC console latency** | Low | Low | Non-blocking I/O, separate from RT comms, no prints in ISR |
| **Inter-core race conditions** | Medium | High | Memory barriers, lock validator, stress test with injections |
| **ICD misinterpretation** | Medium | Critical | Cross-reference with real hardware (if available), host validation |

### Contingency Plans

- **Jitter exceeds 200µs**: Move protection checks to Core0, slim physics loop
- **Flash too small**: Drop TUI, use minimal command set, store scenarios on host
- **CRC failures**: Add retry logic, dump raw frames for analysis

---

## 8. Milestones & Success Criteria

| Phase | Deliverable | Success Criteria |
|-------|-------------|------------------|
| **1. Foundation** | Buildable UF2 | Pico boots, prints version |
| **2. Platform** | HAL working | Address pins read, 100 Hz alarm fires |
| **3. Drivers** | PING command | Host sends PING, emulator ACKs with valid CRC |
| **4. Utilities** | Fixed-point libs | 1M ring buffer ops, UQ conversions within 1 LSB |
| **5. Physics** | Core1 loop | Spin-up to 3000 RPM, jitter < 200µs |
| **6. Commands** | NSP handlers | All 8 commands respond correctly |
| **7. Protections** | Fault system | Overspeed latches, CLEAR-FAULT resets |
| **8. TUI** | Console working | Navigate tables, set mode, see live telemetry |
| **9. Injection** | JSON loader | Load scenario, faults trigger at correct times |
| **10. Integration** | Complete system | 24h soak test, host tester passes all cases |

**Final Acceptance**: OBC cannot distinguish emulator from real NRWA-T6 hardware

---

## 9. Tools & Dependencies

### Build Tools
- **CMake**: ≥ 3.13
- **ARM GCC**: arm-none-eabi-gcc (Pico SDK toolchain)
- **Pico SDK**: Latest stable (set `PICO_SDK_PATH`)
- **Python**: ≥ 3.8 (for host tools)

### Libraries
- **cJSON**: JSON parsing (MIT license)
- **LittleFS**: Flash filesystem (optional, BSD license)
- **Unity**: Unit test framework (MIT license)

### Host Tools
- **pySerial**: RS-485 communication
- **pytest**: Test harness
- **matplotlib**: Telemetry plotting

### Hardware
- Raspberry Pi Pico (RP2040)
- RS-485 transceiver (e.g., MAX485)
- USB cable (programming + console)
- Logic analyzer (validation)
- Oscilloscope (jitter measurement)

---

## 10. Next Steps

### Immediate Actions
1. **Review this plan** with stakeholders
2. **Obtain NRWA-T6 ICD** (if available) to validate register map
3. **Set up development environment**: Pico SDK, toolchain, IDE
4. **Create Git repository** with initial structure
5. **Begin Phase 1**: Project foundation

### Open Questions
- [ ] Do we have access to real NRWA-T6 hardware for validation?
- [ ] What is the expected test coverage percentage? (Suggest ≥80%)
- [ ] Should we support firmware updates over RS-485 or USB?
- [ ] Are there specific fault scenarios prioritized for Phase 9?
- [ ] Will this emulator be used in closed-loop HITL with real GNC software?

---

## 11. References

- **SPEC.md**: Full specification (this repository)
- **NRWA-T6 ICD**: NewSpace Systems Interface Control Document (external)
- **Pico SDK Docs**: https://www.raspberrypi.com/documentation/pico-sdk/
- **NSP Protocol**: (Reference spec if available)
- **CCITT CRC**: ITU-T Recommendation V.41

---

**Document Version**: 1.0
**Last Updated**: 2025-11-05
**Author**: Implementation Team
**Status**: Ready for Phase 1 kickoff
