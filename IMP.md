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
- **Rich Developer UX**: Terminal UI with arrow-key navigation and table/field catalog
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

## 3. Checkpoint-Based Development Strategy

### 3.1 Philosophy

To enable **incremental hardware validation**, each phase is broken into **testable checkpoints**. Every checkpoint:
1. **Builds and flashes** to Pico (produces working `.uf2`)
2. **Demonstrates functionality** via USB console or LED feedback
3. **Can be validated** on real hardware without waiting for phase completion
4. **Commits separately** for safe rollback points

### 3.2 Test Mode Infrastructure

**File**: `firmware/test_mode.c` + `test_mode.h`

Provides checkpoint test functions that can be invoked from `app_main.c`:
- Controlled via `#define CHECKPOINT_X_Y` compile-time flags
- Each checkpoint has dedicated test function (e.g., `test_crc_vectors()`)
- Tests print results to USB console for visual verification
- Can halt execution (`while(1)`) or return to main loop

**Example**:
```c
// test_mode.h
#ifdef CHECKPOINT_3_1
void test_crc_vectors(void);
#endif

// app_main.c
#define CHECKPOINT_3_1  // Enable this checkpoint
#include "test_mode.h"

int main(void) {
    stdio_init_all();
    sleep_ms(2000);

    #ifdef CHECKPOINT_3_1
    test_crc_vectors();
    while(1) { sleep_ms(1000); }  // Stop here for testing
    #endif

    // Normal main loop...
}
```

### 3.3 Checkpoint Progress Tracking

**In PROGRESS.md**: Track sub-phase completion
- Phase 3: 0% → 25% → 50% → 75% → 100%
- Each checkpoint updates the percentage
- Final phase completion includes all checkpoint commits

**Commit Strategy**:
```
Checkpoint 3.1: CRC-CCITT implementation and validation

- Implemented crc_ccitt.c/h with CCITT polynomial
- Added test_mode.c infrastructure
- Test vectors validate LSB-first bit order
- Console output shows all tests passing

Checkpoint acceptance: ✅ CRC matches known test vectors
Phase 3 progress: 25% (1/4 checkpoints)
```

### 3.4 Phase-Specific Checkpoints

Each phase below lists its checkpoints with acceptance criteria.

---

## 4. Implementation Phases

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

**Checkpoints**: 4 (25% each)

---

#### Checkpoint 3.1: CRC-CCITT ✅ (25%)

**Implement**: `drivers/crc_ccitt.c` + `crc_ccitt.h`

**Details**:
- Init: 0xFFFF
- Polynomial: 0x1021
- **Bit order: LSB-first** per [SPEC.md:44](SPEC.md#L44) (CRITICAL!)
- `uint16_t crc_ccitt(const uint8_t *data, size_t len)`

**Test Mode**: `test_crc_vectors()` in `test_mode.c`
- Test vector 1: `{0x01, 0x02, 0x03}` → Known CRC
- Test vector 2: Empty buffer → 0xFFFF
- Test vector 3: NSP PING packet → Validate against spec
- Test vector 4: Long buffer (256 bytes) → Performance check

**Hardware Validation**:
1. Flash to Pico
2. Connect USB console (`screen /dev/ttyACM0 115200`)
3. See test results printed:
   ```
   === Checkpoint 3.1: CRC-CCITT Test ===
   Test 1: {0x01, 0x02, 0x03} → CRC=0xXXXX ✓
   Test 2: Empty buffer → CRC=0xFFFF ✓
   Test 3: PING packet → CRC=0xYYYY ✓
   All tests PASSED
   ```

**Acceptance**: ✅ All CRC test vectors match expected values

---

#### Checkpoint 3.2: SLIP Codec ✅ (50%)

**Implement**: `drivers/slip.c` + `slip.h`

**Details**:
- Constants: END=0xC0, ESC=0xDB, ESC_END=0xDC, ESC_ESC=0xDD
- State machine encoder/decoder
- Edge cases: leading/trailing END, escaped bytes, consecutive ESC
- `slip_encode(in, in_len, out, out_max) → out_len`
- `slip_decode(in, in_len, out, out_max) → out_len or -1`

**Test Mode**: `test_slip_codec()` in `test_mode.c`
- Test 1: Simple packet `{0x01, 0x02}` → `{END, 0x01, 0x02, END}`
- Test 2: Packet with END `{0xC0, 0x01}` → `{END, ESC, ESC_END, 0x01, END}`
- Test 3: Packet with ESC `{0xDB, 0x01}` → `{END, ESC, ESC_ESC, 0x01, END}`
- Test 4: Round-trip: encode → decode → verify original
- Test 5: Malformed frame (missing END) → Error handling

**Hardware Validation**:
1. Flash to Pico
2. Console shows encode/decode tests with hex dumps
3. LED blinks to indicate test progress

**Acceptance**: ✅ SLIP round-trip preserves data, edge cases handled

---

#### Checkpoint 3.3: RS-485 UART (Software Loopback) ✅ (75%)

**Implement**: `drivers/rs485_uart.c` + `rs485_uart.h`

**Details**:
- UART1 config: 460800 baud, 8-N-1
- DE/RE timing: DE high 10µs before TX, low 10µs after (use `timebase_delay_us()`)
- **Software loopback mode** for testing (UART TX → RX internally via Pico SDK)
- Optional: RS-232 mode (no DE/RE control) for simpler validation
- RX ring buffer (1KB)
- `rs485_init()`, `rs485_send(data, len)`, `rs485_recv(buf, max) → len`

**Test Mode**: `test_uart_loopback()` in `test_mode.c`
- Test 1: Send "HELLO" → Receive "HELLO" (loopback)
- Test 2: Send 256-byte buffer → Verify received
- Test 3: DE/RE timing: Print timestamps, verify 10µs delays
- Test 4: Baud rate: Measure with `timebase_get_us()`

**Hardware Validation**:
1. Flash to Pico
2. Console shows TX/RX data matching
3. LED toggles on each TX/RX byte
4. Optional: Scope on GP6 (DE) and GP7 (RE) to verify timing

**Acceptance**: ✅ UART loopback works, DE/RE timing correct (±2µs)

---

#### Checkpoint 3.4: NSP Protocol (PING Responder) ✅ (100%)

**Implement**: `drivers/nsp.c` + `nsp.h`, `drivers/leds.c` + `leds.h`

**Details**:
- Message Control byte: `[Poll:1|B:1|A:1|Command:5]`
- Packet layout: `[Dest|Src|Ctrl|Len|Data...|CRC_L|CRC_H]`
- Command dispatch: Implement PING (0x00) handler only
- ACK generation with correct CRC
- `nsp_parse(slip_frame) → command_id, payload`
- `nsp_build_reply(ack, data) → slip_frame`

**Test Mode**: `test_nsp_ping()` in `test_mode.c`
- Test 1: Build PING packet (0x00), compute CRC, encode SLIP
- Test 2: Parse received PING, generate ACK reply
- Test 3: Full round-trip: PING → parse → ACK → verify
- LED patterns: Blink on PING received, solid for 1s on ACK sent

**Hardware Validation**:
1. Flash to Pico
2. Run `tools/host_tester.py --ping` (or manual packet via console)
3. See PING request logged, ACK reply sent
4. LED feedback confirms packet processing

**Acceptance**: ✅ PING command generates valid ACK with correct CRC

---

**Phase 3 Final Deliverables**:
- `drivers/crc_ccitt.c/h`
- `drivers/slip.c/h`
- `drivers/rs485_uart.c/h`
- `drivers/nsp.c/h`
- `drivers/leds.c/h`
- `test_mode.c/h` (checkpoint infrastructure)

**Phase 3 Final Acceptance**:
- All 4 checkpoints passing on hardware
- End-to-end: Host sends SLIP-encoded NSP PING → Emulator replies with ACK
- CRC validated, SLIP framing correct, UART timing within spec

---

### Phase 4: Utilities Foundation 🛠️

**Goal**: Support libraries for device model

**Checkpoints**: 2 (50% each)

---

#### Checkpoint 4.1: Ring Buffer ✅ (50%)

**Implement**: `util/ringbuf.c` + `ringbuf.h`

**Details**:
- Lock-free SPSC (single producer, single consumer)
- Power-of-2 size for fast modulo
- Memory barriers for inter-core safety
- `ringbuf_init(rb, size)`, `ringbuf_push(rb, item)`, `ringbuf_pop(rb, item) → bool`

**Test Mode**: `test_ringbuf_stress()` in `test_mode.c`
- Test 1: Push/pop 1000 items, verify FIFO order
- Test 2: Fill buffer, verify full detection
- Test 3: Empty buffer, verify empty detection
- Test 4: Stress test: 1M push/pop cycles, measure time

**Hardware Validation**:
1. Flash to Pico
2. Console shows test progress and results
3. LED blinks at different rates for test phases

**Acceptance**: ✅ Ring buffer survives 1M cycles, no data corruption

---

#### Checkpoint 4.2: Fixed-Point Math ✅ (100%)

**Implement**: `util/fixedpoint.h` (header-only library)

**Details**:
- UQ14.18: Speed (RPM) - `uint32_t`
- UQ16.16: Voltage (V) - `uint32_t`
- UQ18.14: Torque (mN·m), current (mA), power (mW) - `uint32_t`
- Conversions: `float_to_uqX_Y()`, `uqX_Y_to_float()`
- Arithmetic: Saturating add/sub, multiply with shift

**Test Mode**: `test_fixedpoint_accuracy()` in `test_mode.c`
- Test 1: RPM conversions (0, 3000, 5000, 6000 RPM)
- Test 2: Voltage conversions (0.0, 28.0, 36.0 V)
- Test 3: Torque/current/power conversions
- Test 4: Arithmetic: Add 100mA + 200mA = 300mA (verify)
- Test 5: Saturation: UQ18.14 max value + 1 → saturates

**Hardware Validation**:
1. Flash to Pico
2. Console shows float → fixed → float round-trip errors
3. All errors < 1 LSB tolerance

**Acceptance**: ✅ Fixed-point conversions within 1 LSB of reference

---

**Phase 4 Final Deliverables**:
- `util/ringbuf.c/h`
- `util/fixedpoint.h`

**Phase 4 Final Acceptance**:
- Ring buffer ready for Core1 ↔ Core0 telemetry
- Fixed-point math ready for physics calculations

---

### Phase 5: Device Model & Physics ⚙️

**Goal**: Wheel dynamics simulation running on Core1

**Checkpoints**: 3 (33% each)

---

#### Checkpoint 5.1: Register Map (33%)

**Implement**: `device/nss_nrwa_t6_regs.h`

**Details**:
- Define all PEEK/POKE addresses per ICD
- Control registers: mode, setpoint, direction
- Status registers: speed, current, torque, power
- Config registers: protection thresholds
- Example:
  ```c
  #define REG_OVERVOLTAGE_THRESHOLD  0x0100  // UQ16.16
  #define REG_OVERSPEED_FAULT_RPM    0x0104  // UQ14.18
  #define REG_MOTOR_OVERPOWER_LIMIT  0x0108  // UQ18.14 mW
  #define REG_CONTROL_MODE           0x0200  // enum
  #define REG_SPEED_SETPOINT_RPM     0x0204  // UQ14.18
  ```

**Test Mode**: `test_register_map()` in `test_mode.c`
- Test 1: Verify all register addresses are non-overlapping
- Test 2: Read default values from register table
- Test 3: Validate UQ format assignments match spec
- Test 4: Test register grouping (control/status/config)

**Hardware Validation**:
1. Flash to Pico
2. Console shows register map table
3. Display all registers with addresses, types, and defaults

**Acceptance**: ✅ Register map complete with all ICD addresses defined

---

#### Checkpoint 5.2: Physics Model & Control Modes (66%)

**Implement**: `device/nss_nrwa_t6_model.c` + `nss_nrwa_t6_model.h`

**Details**:
- State variables:
  ```c
  typedef struct {
      float omega_rad_s;      // Angular velocity
      float momentum_nms;     // H = I·ω
      float current_cmd_a;    // Commanded current
      float current_out_a;    // Actual current
      float torque_cmd_mnm;   // Commanded torque
      float torque_out_mnm;   // Output torque
      float power_w;          // Electrical power
      control_mode_t mode;    // CURRENT|SPEED|TORQUE|PWM
  } wheel_state_t;
  ```
- Dynamics (Δt = 10 ms):
  - τ_motor = k_t · i (k_t ≈ 0.0534 N·m/A)
  - τ_loss = a·ω + b·sign(ω) + c·i²
  - α = (τ_motor - τ_loss) / I
  - ω_new = ω_old + α·Δt
- Control modes:
  - **Current**: Direct i_cmd → τ
  - **Speed**: PI controller with anti-windup (Kp, Ki, I_max)
  - **Torque**: ΔH feed-forward with current limiting
  - **PWM**: Backup duty-cycle mode (0-97.85%)
- Limits:
  - Power: |τ·ω| ≤ 100 W
  - Duty: ≤ 97.85%
  - Current: ≤ 6 A

**Test Mode**: `test_wheel_physics()` in `test_mode.c`
- Test 1: Initialize model, verify zero state
- Test 2: Current mode: Command 1A, verify torque = k_t * 1A
- Test 3: Speed mode: Command 1000 RPM, verify ramp-up
- Test 4: Torque mode: Command 10 mN·m, verify current response
- Test 5: Power limiting: Command exceeds 100W, verify clamp
- Test 6: Loss model: Spin at 3000 RPM, verify deceleration

**Hardware Validation**:
1. Flash to Pico
2. Console shows physics tick at 10 Hz (visible for testing)
3. Display state variables updating in real-time
4. Speed ramp test: 0 → 3000 RPM in ~5 seconds

**Acceptance**: ✅ Physics model runs, all control modes functional, limits enforced

---

#### Checkpoint 5.3: Core1 Integration & 100 Hz Tick (100%)

**Implement**: Core1 entry point in `app_main.c`, integrate timebase

**Details**:
- Launch Core1 with `multicore_launch_core1(core1_main)`
- Core1 main loop:
  - Setup 100 Hz alarm using `timebase_init()` with callback
  - In callback:
    - Call `wheel_model_tick()`
    - Check protections
    - Publish state snapshot to ring buffer
    - Measure jitter: `time_us_64()` deltas
- Inter-core communication:
  - Core0 → Core1: Command mailbox (spinlock-protected)
  - Core1 → Core0: Telemetry ring buffer (lock-free SPSC)

**Test Mode**: `test_core1_timing()` in `test_mode.c`
- Test 1: Launch Core1, verify it starts
- Test 2: Measure tick rate over 1000 ticks
- Test 3: Jitter analysis: min/max/avg/p99 tick timing
- Test 4: Inter-core comm: Core0 sends command, Core1 acknowledges
- Test 5: Ring buffer: Core1 publishes 100 samples, Core0 reads all

**Hardware Validation**:
1. Flash to Pico
2. Console shows Core1 launch message
3. Display tick rate (should show ~100.0 Hz)
4. Display jitter stats (p99 < 200 µs)
5. Toggle GPIO on each tick, verify with scope/logic analyzer

**Acceptance**: ✅ Core1 running at 100 Hz, jitter < 200 µs, inter-core comm working

---

**Phase 5 Final Deliverables**:
- `device/nss_nrwa_t6_regs.h`
- `device/nss_nrwa_t6_model.c/h`
- Core1 integration in `app_main.c`

**Phase 5 Final Acceptance**:
- Register map complete
- Physics model validated for all 4 control modes
- Core1 running at 100 Hz with acceptable jitter
- Inter-core communication operational

---

### Phase 6: Device Commands & Telemetry 📊

**Goal**: NSP command handlers and telemetry blocks

**Checkpoints**: 2 (Commands 50%, Telemetry 100%)
- **6.1**: Implement all 8 NSP command handlers (PEEK/POKE/etc.)
- **6.2**: Implement 5 telemetry blocks (STANDARD/TEMP/VOLT/CURR/DIAG)
- **Test**: Use host_tester.py to request telemetry, verify block sizes/formats

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

**Checkpoints**: 2 (Protections 50%, Fault logic 100%)
- **7.1**: Implement hard/soft protections (overvoltage, overcurrent, overspeed)
- **7.2**: Implement fault latching and CLEAR-FAULT selective clearing
- **Test**: Trigger overspeed fault at 6000 RPM, verify CLEAR-FAULT resets

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

**Goal**: USB-CDC terminal with arrow-key navigation and expand/collapse tables

**Checkpoints**: 2 (TUI Core 50%, Table Catalog 100%)

- **8.1**: Implement TUI core with arrow navigation and status banner
- **8.2**: Implement table/field catalog with 7 base tables
- **Test**: Navigate with arrows, expand/collapse tables, view field values

**Tasks**:

#### 8.1 TUI Core (`tui.c`) ✅ COMPLETE

**Implementation Completed:**

- **Non-scrolling interface** using ANSI escape sequences (VT100)
- **Arrow-key navigation**: ↑/↓ to navigate, →/← to expand/collapse
- **Status banner**: Shows wheel state (ON/OFF, Mode, RPM, Current, Fault)
- **Single browse mode**: Unified view with expandable tables
- **Refresh**: Press **R** to force screen refresh
- **Test result caching**: Boot tests run once, results displayed in TUI

**Screen Layout:**

```text
┌─ NRWA-T6 Emulator ──── Uptime: 00:15:32 ──── Tests: 78/78 ✓ ─────┐
├─ Status: ON │ Mode: SPEED │ RPM: 3245 │ Current: 1.25A │ Fault: -┤
├───────────────────────────────────────────────────────────────────┤
│ TABLES                                                            │
│                                                                   │
│ > 1. ▶ Built-In Tests      [COLLAPSED]                           │
│   2. ▼ Control Mode        [EXPANDED]                            │
│       ├─ mode          : SPEED       (RW)                         │
│     ► ├─ setpoint_rpm  : 3000        (RW)                         │
│       ├─ actual_rpm    : 3245        (RO)                         │
│   3. ▶ Dynamics            [COLLAPSED]                            │
│                                                                   │
├───────────────────────────────────────────────────────────────────┤
│ ↑↓: Navigate │ →: Expand │ ←: Collapse │ Enter: Edit │ C: Command│
└───────────────────────────────────────────────────────────────────┘
```

**Navigation:**

- ↑/↓: Move cursor
- →: Expand table
- ←: Collapse table
- Enter: Edit field (Checkpoint 8.3)
- C: Command mode
- Q/ESC: Quit
- R: Refresh

**Files Created:**

- `console/tui.c` - Complete rewrite with arrow navigation (570 lines)
- `console/tui.h` - Updated state structure for browse mode
- `console/table_tests.c` - Built-In Tests table (120 lines)
- `console/table_tests.h` - Header
- `test_results.c/h` - Test result caching system (245 lines)

**Hardware Validated:** Pending (build successful, ready to flash)

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

#### 8.3 Field Value Display

- All field values displayed with units
- Read-only fields clearly marked (RO)
- Read-write fields marked (RW)
- Values formatted according to type (hex, decimal, fixed-point)

**Deliverables**:
- `console/tui.c`
- `console/tables.c`
- Catalog definitions for all 7 tables

**Acceptance**:
- Navigate to Dynamics table with arrow keys
- Expand/collapse tables with →/← keys
- All 8 tables visible with field counts
- Field values display with proper units and RO/RW indicators
- TUI refreshes without flicker

---

### Phase 9: Fault Injection System 💉

**Goal**: JSON scenario loader and runtime injection

**Checkpoints**: 2 (JSON parser 50%, Scenario engine 100%)
- **9.1**: Implement JSON parser for scenario files
- **9.2**: Implement scenario engine with timeline and injection actions
- **Test**: Load overspeed scenario, verify fault triggers at correct time

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

**Checkpoints**: 3 (Core integration 33%, Host tools 66%, Final integration 100%)
- **10.1**: Complete Core0/Core1 integration with multicore launch
- **10.2**: Implement host_tester.py and errorgen.py tools
- **10.3**: Full system integration test and 24h soak test
- **Test**: End-to-end validation with OBC sending NSP commands

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

5. **Add TUI**: Arrow-key navigation → table display → field viewing
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
