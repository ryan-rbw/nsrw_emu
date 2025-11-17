# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Documentation Structure

**IMPORTANT**: This project uses three primary documentation files that must be referenced and maintained:

1. **[SPEC.md](SPEC.md)** - The overarching specification
   - Hardware requirements and pin mappings
   - Communication protocols (RS-485, SLIP, NSP)
   - Physics model and control algorithms
   - Protection system requirements
   - **THIS IS THE SOURCE OF TRUTH** - validate all implementation against this

2. **[IMP.md](IMP.md)** - The implementation plan
   - Based on SPEC.md and the proposed architecture
   - Organized into 10 phases with checkpoints
   - Detailed implementation steps for each checkpoint
   - Acceptance criteria and validation procedures
   - **THIS GUIDES DEVELOPMENT** - follow checkpoint order

3. **[PROGRESS.md](PROGRESS.md)** - Progress tracking
   - Current status of all phases and checkpoints
   - Completed work with commit references
   - Hardware validation results
   - Build metrics and known issues
   - **THIS TRACKS COMPLETION** - update after each checkpoint

**Workflow**: Reference SPEC.md → Follow IMP.md → Update PROGRESS.md

## Project Overview

This is a high-fidelity hardware-in-the-loop (HIL) emulator for the NewSpace Systems NRWA-T6 reaction wheel, targeting the Raspberry Pi Pico (RP2040). The emulator must be **protocol-perfect** and **timing-accurate** so spacecraft OBC/GNC systems cannot distinguish it from real hardware.

**Critical Requirements**:
- RS-485 with SLIP framing, NSP protocol, CCITT CRC matching ICD specification
- 100 Hz physics simulation on Core1 with <200µs jitter
- Dual-core architecture: Core0 (communications) + Core1 (hard real-time physics)
- USB-CDC console with table/field catalog and command palette
- JSON-driven fault injection for HIL testing

## Build System

### Prerequisites
- Raspberry Pi Pico SDK (set `PICO_SDK_PATH` environment variable)
- CMake ≥ 3.13
- ARM GCC toolchain (`arm-none-eabi-gcc`)

### Build Commands
```bash
# First-time setup
export PICO_SDK_PATH=~/pico-sdk  # Or wherever you installed it
mkdir build && cd build

# Configure and build
cmake ..
make -j$(nproc)

# Output: build/firmware/nrwa_t6_emulator.uf2

# Flash to Pico
# 1. Hold BOOTSEL, connect USB, release BOOTSEL
# 2. Copy .uf2 to RPI-RP2 drive
```

### Build Configuration
- **USB-CDC stdio enabled**: Console on USB serial port (115200 baud)
- **UART1 reserved**: For RS-485 (460.8 kbps, 8-N-1)
- **Version from git**: Firmware version = `git describe --tags --always --dirty`

## Architecture

### Dual-Core Strategy (CRITICAL)

| Core | Role | Priority | Communication |
|------|------|----------|---------------|
| **Core0** | RS-485 RX/TX, NSP protocol, USB-CDC console, command dispatch | Best-effort polling | Command mailbox (spinlock) |
| **Core1** | 100 Hz physics tick, control loops, protection checks | Hard real-time | Telemetry snapshots (lock-free ring buffer) |

**Inter-Core Communication**:
- **Core1 → Core0**: Lock-free SPSC ring buffer for telemetry (see `util/ringbuf.c`)
- **Core0 → Core1**: Spinlock-protected command mailbox for setpoints/mode changes
- **NO** FreeRTOS or RTOS - bare metal with dual cores

### Module Hierarchy

```
Platform Layer (firmware/platform/)
├─ board_pico.h      # Pin definitions, timing constants (RS-485: GP4-7, ADDR: GP10-12)
├─ gpio_map.c        # GPIO init, RS-485 DE/RE control, address reading (0-7)
└─ timebase.c        # 100 Hz alarm (RP2040 timer), jitter tracking, µs timing

Drivers (firmware/drivers/)
├─ crc_ccitt.c       # CCITT CRC-16 (init 0xFFFF, LSB-first)
├─ slip.c            # SLIP encoder/decoder (END=0xC0, ESC=0xDB)
├─ rs485_uart.c      # UART1 driver, DE/RE timing (10µs setup/hold), tri-state
├─ nsp.c             # NSP protocol parser, command dispatch, ACK/NACK
└─ leds.c            # Status LED control

Device (firmware/device/)
├─ nss_nrwa_t6_regs.h       # Register map (PEEK/POKE addresses, UQ formats)
├─ nss_nrwa_t6_model.c      # Physics: ω, H, τ, I, control modes, Δt=10ms
├─ nss_nrwa_t6_commands.c   # NSP command handlers (8 commands: PING, PEEK, POKE, etc.)
├─ nss_nrwa_t6_telemetry.c  # Telemetry blocks (STANDARD, TEMP, VOLT, CURR, DIAG)
└─ nss_nrwa_t6_protection.c # Soft/hard protections, fault latching, CLEAR-FAULT

Console (firmware/console/)
├─ tui.c             # Menu system (navigate with numbers, q=back, r=refresh, e=edit)
├─ tables.c          # Table/field catalog (7 tables: Serial, NSP, Control, Dynamics, ...)
└─ json_loader.c     # Fault injection scenario parser (from flash)

Utilities (firmware/util/)
├─ ringbuf.c         # Lock-free SPSC queue (power-of-2 size, memory barriers)
└─ fixedpoint.h      # UQ14.18, UQ16.16, UQ18.14 conversions
```

### Pin Configuration (See board_pico.h)

| Function | GPIO | Notes |
|----------|------|-------|
| UART1_TX | GP4  | RS-485 transmit |
| UART1_RX | GP5  | RS-485 receive |
| RS485_DE | GP6  | Driver Enable (high = TX) |
| RS485_RE | GP7  | Receiver Enable (low = RX) |
| ADDR0    | GP10 | Address bit 0 (pull high/low) |
| ADDR1    | GP11 | Address bit 1 |
| ADDR2    | GP12 | Address bit 2 (→ device ID 0-7) |
| FAULT    | GP13 | Fault output (active low) |
| RESET    | GP14 | Reset input (active low) |
| LED      | GP25 | Onboard LED (heartbeat) |

## Development Workflow

### Checkpoint-Based Development (CRITICAL)

**Every phase is broken into testable checkpoints** that can be validated on hardware incrementally.

**Checkpoint Requirements**:
1. **Builds to .uf2**: Produces flashable binary
2. **Validates on hardware**: USB console shows test results or LED feedback
3. **Commits separately**: Each checkpoint is a safe rollback point
4. **Updates PROGRESS.md**: Track sub-phase progress (0% → 25% → 50% → etc.)

**Test Mode Infrastructure**: `firmware/test_mode.c` + `test_mode.h`
- Controlled via `#define CHECKPOINT_X_Y` in `app_main.c`
- Each checkpoint has test function (e.g., `test_crc_vectors()`)
- Tests print to USB console for visual verification
- Can halt with `while(1)` or return to main loop

**Example Checkpoint Flow**:
```c
// app_main.c
#define CHECKPOINT_3_1  // Enable CRC test mode

#include "test_mode.h"

int main(void) {
    stdio_init_all();
    sleep_ms(2000);

    #ifdef CHECKPOINT_3_1
    test_crc_vectors();  // Run CRC tests
    while(1) { sleep_ms(1000); }  // Stop here
    #endif

    // Normal main loop...
}
```

**Flash, Test, Commit**:
1. Enable checkpoint define in `app_main.c`
2. Build: `make -j$(nproc)`
3. Flash `.uf2` to Pico
4. Connect console: `screen /dev/ttyACM0 115200`
5. Verify test output (e.g., "All CRC tests PASSED")
6. Commit: "Checkpoint 3.1: CRC-CCITT implementation and validation"
7. Disable checkpoint, enable next one

### Phased Implementation (See IMP.md and PROGRESS.md)

The project follows a 10-phase plan with strict acceptance criteria. **Always check PROGRESS.md** for current status.

**Completed Phases**:
- ✅ Phase 1: Project Foundation (CMake, git, docs)
- ✅ Phase 2: Platform Layer (GPIO, timebase, board config)

**In Progress/Next**:
- 🔄 Phase 3: Core Drivers (4 checkpoints: CRC, SLIP, RS-485, NSP)

**Important**: Each phase has acceptance criteria in IMP.md. Update PROGRESS.md when completing a phase with:
- Files created/modified
- Line counts
- Acceptance criteria checklist
- Commit hashes

### Code Style and Patterns

**File Organization**:
- Each module has `.c` and `.h` pairs
- Headers in corresponding subdirectory (e.g., `drivers/*.h`, `platform/*.h`)
- `app_main.c` includes headers: `board_pico.h`, `gpio_map.h`, `timebase.h`, etc.

**Naming Conventions**:
- Functions: `module_action()` (e.g., `gpio_init_all()`, `timebase_get_us()`)
- Globals/statics: Avoid except for ISR state (use `volatile` for inter-core)
- Constants: `UPPERCASE_WITH_UNDERSCORES` (e.g., `RS485_BAUD_RATE`)

**Platform-Specific**:
- Use Pico SDK types: `uint8_t`, `uint32_t`, `bool` (not `int` for flags)
- Timing: `time_us_64()` for µs, hardware alarms for periodic tasks
- GPIO: Always via `gpio_map.h` API (never direct `gpio_put()` in app code)

**Real-Time Constraints**:
- Core1 ISR must complete in <200µs (jitter spec)
- No `printf()` in Core1 tick ISR (use counters, debug in Core0)
- No blocking calls on Core1 (no `sleep_ms()`, `busy_wait_us()` only if <10µs)

### Adding New Modules

When implementing new driver/device modules:

1. **Add to CMakeLists.txt**: `firmware/CMakeLists.txt` → `add_executable()` sources
2. **Header in correct directory**: e.g., `drivers/crc_ccitt.h`
3. **Include in app_main.c**: Add include after platform headers
4. **Update PROGRESS.md**: Add file to phase completion section
5. **Follow module pattern**:
   ```c
   // module_name.h
   #ifndef MODULE_NAME_H
   #define MODULE_NAME_H

   void module_init(void);
   int module_do_thing(uint8_t param);

   #endif

   // module_name.c
   #include "module_name.h"
   #include "board_pico.h"  // For constants

   void module_init(void) {
       // Init code with printf("[Module] Initialized\n");
   }
   ```

## Protocol Details (Critical for Phase 3+)

### NSP Commands (8 required)
- `0x00` PING - ACK with no data
- `0x02` PEEK - Read register(s), return data
- `0x03` POKE - Write register(s), ACK/NACK
- `0x07` APPLICATION-TELEMETRY - Return telemetry block
- `0x08` APPLICATION-COMMAND - Mode/setpoint changes
- `0x09` CLEAR-FAULT - Clear latched faults by mask
- `0x0A` CONFIGURE-PROTECTION - Update protection thresholds
- `0x0B` TRIP-LCL - Test local current limit fault

### SLIP Framing
- `END = 0xC0` (frame delimiter)
- `ESC = 0xDB` (escape character)
- `ESC_END = 0xDC` (escaped END)
- `ESC_ESC = 0xDD` (escaped ESC)
- State machine: Encode on TX, decode on RX
- Handle leading/trailing END, mid-frame escapes

### CRC-CCITT
- Polynomial: 0x1021
- Initial value: 0xFFFF
- **Bit order: LSB-first** (critical!)
- Covers: Destination + Source + Control + Length + Data bytes
- Transmitted: [CRC_L, CRC_H]

### RS-485 Timing
- Baud rate: 460800 (tolerance ±1% = 455.6-465.7 kbps)
- DE/RE timing:
  - Assert DE 10µs **before** first UART TX byte
  - Hold DE 10µs **after** last UART TX byte
  - Tri-state when idle (multi-drop bus sharing)

## Physics Model (Phase 5)

### State Variables
- `ω` (omega): Angular velocity [rad/s]
- `H`: Momentum = I·ω [N·m·s]
- `τ_m`: Motor torque [N·m]
- `i`: Current [A]
- `P`: Power [W]

### Dynamics (Δt = 10 ms on Core1)
```
τ_m = k_t · i          (k_t ≈ 0.0534 N·m/A)
τ_loss = a·ω + b·sign(ω) + c·i²
α = (τ_m - τ_loss) / I
ω_new = ω_old + α·Δt
```

### Control Modes
- **Current**: Direct `i_cmd` → `τ`
- **Speed**: PI controller with anti-windup (target ω)
- **Torque**: ΔH feed-forward
- **PWM**: Backup duty-cycle mode

### Protection Limits
- Hard faults (immediate shutdown): 36V overvoltage, 6A overcurrent, 97.85% max duty, 100W overpower
- Soft limits (warnings → fault): 31V braking load, 5000 RPM overspeed
- Latched faults: 6000 RPM overspeed (requires `CLEAR-FAULT` to reset)

## Testing Strategy

### Unit Tests (Phase 4-7)
- CRC-CCITT: Test vectors from standard
- SLIP: Encode/decode round-trip, edge cases (leading END, escaped bytes)
- Fixed-point: Conversion accuracy within 1 LSB
- Protection: Threshold logic, fault latching

Build with: `cmake -DBUILD_TESTS=ON .. && make test`

### Host Integration (tools/host_tester.py)
- Send NSP command sequences over RS-485
- Validate CRC, SLIP framing, ACK/NACK responses
- Telemetry block size/content verification
- Timing measurements (response <5ms with Poll=1)

### Console Access
```bash
# Linux
screen /dev/ttyACM0 115200

# View startup banner, device address, jitter stats
# Future: Command palette (help, tables, get/set, scenario load)
```

## Common Pitfalls

1. **CRC byte order**: Must be LSB-first (not big-endian!)
2. **DE/RE timing**: 10µs delays required, use `timebase_delay_us()`
3. **Core1 jitter**: No printf, sleep, or blocking calls in ISR
4. **Ring buffer size**: Must be power-of-2 for fast modulo
5. **SLIP edge cases**: Handle leading END, trailing END, consecutive ESC
6. **Fixed-point formats**: UQ14.18 for speed (RPM), UQ18.14 for torque/current/power
7. **GPIO init order**: Call `gpio_init_all()` before any GPIO operations
8. **Timebase init**: Call `timebase_init()` before `timebase_start()`
9. **Float printf MUST be enabled**: Add `PICO_PRINTF_SUPPORT_FLOAT=1` to CMakeLists.txt or printf("%.2f") causes lockup
10. **Static struct initialization**: Use `= {0}` not just `{}` - C doesn't guarantee zero-init without explicit `{0}`
11. **Watchdog timing**: Enable watchdog AFTER boot tests complete (tests can exceed timeout)
12. **Qemu segfault on large binaries**: Comment out ARM execution line in build.make after cmake reconfigure (known SDK issue)
13. **Memory alignment on ARM Cortex-M0+**: NEVER cast float/enum pointers to `(uint32_t*)` and dereference - causes hard fault. Use `memcpy()` for all non-uint32_t field reads from shared structs
14. **ENUM size assumptions**: C enums may be 1-4 bytes. Read only actual size (1 byte for values 0-255) then zero-extend to uint32_t. Reading 4 bytes from 1-byte enum reads garbage padding
15. **Alignment bug symptoms**: System freeze (LED stops), no error message, specific table fields trigger lockup, ENUM shows `INVALID(0xBDADADA0)`. Test table rendering on hardware - QEMU doesn't catch alignment faults

## Documentation Workflow (CRITICAL)

This project uses **four key documents** that work together. You MUST understand and maintain all of them:

### 1. SPEC.md - The Source of Truth
**Purpose**: Full specification defining what to build
**When to reference**:
- Before starting any phase (understand requirements)
- When implementing protocols (NSP commands, SLIP, CRC)
- When implementing physics (equations, control modes, protections)
- When implementing console (table structure, command palette)

**Key sections**:
- §3: Protocol details (RS-485, SLIP, NSP, CRC)
- §5: Physics & control emulation (dynamics, modes, protections)
- §6: NSP commands (8 commands with specific behaviors)
- §8: Console TUI tables and command palette
- §9: Error/fault injection JSON schema

**Example**: Implementing CRC? Check SPEC.md:44 for "CCITT, init 0xFFFF, LSB first"

### 2. IMP.md - The Implementation Roadmap
**Purpose**: Detailed implementation plan with 10 phases
**When to reference**:
- Starting a new phase (understand tasks and deliverables)
- Making design decisions (see §5 Critical Design Decisions)
- Understanding acceptance criteria for current phase
- Planning development sequence (§6 Development Sequence)

**Key sections**:
- §3: Implementation Phases (Phase 1-10 with tasks, deliverables, acceptance)
- §4: Testing Strategy (unit tests, host integration, HIL)
- §5: Critical Design Decisions (inter-core comms, timing, SLIP, JSON)
- §8: Milestones & Success Criteria

**Example**: Starting Phase 3? Check IMP.md:166-216 for CRC/SLIP/RS-485/NSP tasks

### 3. PROGRESS.md - The Live Status Tracker
**Purpose**: Real-time implementation status and completed work log
**When to update**: After EVERY phase completion and major milestone

**MANDATORY updates when completing a phase**:
1. **Overall Progress table**: Change phase status from ⏸️ Pending → ✅ Complete
2. **Phase section**: Add complete "What We Built" writeup:
   - List all files created with line counts
   - Describe features implemented
   - Check off all acceptance criteria with ✅
   - Add commit hash(es)
3. **Metrics section**: Update:
   - Lines of Code (C): Current / Target / Percentage
   - Phases Complete: N / 10 / Percentage
4. **Overall Completion percentage**: Update at top (N/10 phases)

**Example Phase 2 completion**:
```markdown
## Phase 2: Platform Layer ✅ COMPLETE

**Status**: Complete
**Completed**: 2025-11-05
**Commits**: `acee592`

### What We Built

#### 1. Board Configuration ✅
**File**: [firmware/platform/board_pico.h](firmware/platform/board_pico.h) (155 lines)

Complete pin mapping and hardware configuration:
- RS-485 UART pins (TX=GP4, RX=GP5, DE=GP6, RE=GP7)
- Address selection pins (ADDR0=GP10, ADDR1=GP11, ADDR2=GP12)
...

### Acceptance Criteria

| Criteria | Status | Evidence |
|----------|--------|----------|
| Address pins ADDR[2:0] read correctly | ✅ | `gpio_read_address()` implemented |
| Hardware alarm fires at 100 Hz | ✅ | `timebase.c` ISR configured |
...

### Files Created

| File | Lines | Purpose |
|------|-------|---------|
| platform/board_pico.h | 155 | Pin definitions, constants |
...

**Total**: 695 lines of platform code
```

### 4. README.md - The User Guide
**Purpose**: Build instructions, usage, troubleshooting for end users
**When to update**: When build process changes or new user-facing features added
**Generally stable**: Most updates happen during Phase 1 and Phase 8 (console)

## Document Reference Examples

**Scenario 1: Starting Phase 3 (Core Drivers)**
1. **Read SPEC.md §3**: Understand RS-485, SLIP, NSP, CRC requirements
2. **Read IMP.md §3.3 (Phase 3)**: Get detailed implementation tasks
3. **Read PROGRESS.md**: Check current status, understand what's already done
4. **Implement** the drivers
5. **Update PROGRESS.md**: Mark Phase 3 complete with full writeup
6. **Commit** with reference to phase completion

**Scenario 2: Implementing NSP Protocol**
1. **Read SPEC.md §3.2**: NSP protocol structure, Message Control byte
2. **Read SPEC.md §6**: NSP commands (0x00-0x0B) and behaviors
3. **Read IMP.md:195-203**: NSP implementation details (packet layout, dispatch)
4. **Implement** `nsp.c` following the spec
5. **Update PROGRESS.md**: Add to Phase 3 completion section

**Scenario 3: Understanding Physics Model**
1. **Read SPEC.md §5**: Complete physics equations and control modes
2. **Read IMP.md:250-297**: Implementation approach for device model
3. **Check PROGRESS.md**: See if Phase 5 is current phase
4. **Implement** `nss_nrwa_t6_model.c`
5. **Update PROGRESS.md**: Complete Phase 5 section with details

## PROGRESS.md Update Checklist

Before committing a phase completion, verify PROGRESS.md has:

- [ ] Overall Progress table: Phase marked ✅ Complete
- [ ] Overall Completion percentage: Updated (e.g., 20% → 30%)
- [ ] New phase section with "✅ COMPLETE" in title
- [ ] "What We Built" subsections for each major component
- [ ] All files listed with line counts in table format
- [ ] All acceptance criteria checked off with ✅
- [ ] Commit hash(es) listed
- [ ] "Next Steps" section pointing to next phase
- [ ] Metrics table: LOC and Phases Complete updated
- [ ] Next phase marked 🔄 Next (not ⏸️ Pending)

## Git Workflow

**Checkpoint Commit Pattern**:
```
Checkpoint N.M: <Component> implementation and validation

- Implemented <files>
- Test mode: <test_function_name>
- Hardware validation: <what was tested>
- Console output: <test results summary>

Checkpoint acceptance: ✅ <criteria met>
Phase N progress: X% (M/K checkpoints)
```

**Phase Completion Commit Pattern**:
```
Phase N complete: <Title>

<Multi-line description of what was implemented>

Files created:
- file1.c (X lines)
- file2.h (Y lines)

Phase N acceptance criteria met:
✅ Criterion 1
✅ Criterion 2

Total LOC: X (Y% of target)
Overall completion: Z% (N/10 phases)

Next: Phase N+1 (<brief description>)
```

**Branch strategy**: Main branch only (no feature branches currently)

## Current Status (Check PROGRESS.md for latest)

**Phases Complete**: 2/10 (20%)
**Lines of Code**: 835 / 3000-5000 target (17%)

Next implementation: Phase 3 (Core Communication Drivers - CRC, SLIP, RS-485, NSP)
