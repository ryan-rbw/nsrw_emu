# NRWA-T6 Reaction Wheel Emulator

High-fidelity emulator for the NewSpace Systems NRWA-T6 reaction wheel, targeting the Raspberry Pi Pico (RP2040). Designed for spacecraft hardware-in-the-loop (HIL) testing.

## Features

- **Protocol Perfect**: RS-485 with SLIP framing, NSP protocol, CCITT CRC matching ICD
- **Real-time Physics**: 100 Hz control loop with reaction wheel dynamics simulation
- **Dual-Core Architecture**: Core0 for comms, Core1 for physics (< 200µs jitter)
- **Rich Console**: USB-CDC terminal with table/field catalog and command palette
- **Fault Injection**: JSON-driven deterministic error scenarios
- **Single Binary**: UF2 file for drag-and-drop programming

## Hardware Requirements

- **MCU**: Raspberry Pi Pico (RP2040), **not** Pico W/W2
- **RS-485 Transceiver**: MAX485 or compatible (connected to UART1)
- **USB Cable**: For programming and console access
- **Optional**: Logic analyzer for protocol validation

### Pin Configuration

| Function | GPIO Pin | Description |
|----------|----------|-------------|
| UART1 TX | GP4 | RS-485 transmit data |
| UART1 RX | GP5 | RS-485 receive data |
| RS485_DE | GP6 | Driver Enable (active high) |
| RS485_RE | GP7 | Receiver Enable (active low) |
| ADDR0 | GP10 | Address bit 0 (pulled high/low) |
| ADDR1 | GP11 | Address bit 1 (pulled high/low) |
| ADDR2 | GP12 | Address bit 2 (pulled high/low) |
| FAULT | GP13 | Fault output (open-drain) |
| RESET | GP14 | Reset input (active low) |
| LED | GP25 | Onboard LED (heartbeat) |

**Address Selection**: ADDR[2:0] pins set the device ID (0-7). Pull high for '1', low for '0'.

## Software Requirements

### Build Tools

- **CMake**: ≥ 3.13
- **ARM GCC Toolchain**: `arm-none-eabi-gcc` (Pico SDK compatible)
- **Pico SDK**: Clone from https://github.com/raspberrypi/pico-sdk
- **Git**: For version tracking

### Host Tools (Optional)

- **Python**: ≥ 3.8 (for host_tester.py and errorgen.py)
- **pySerial**: `pip install pyserial` (for RS-485 testing)

## Building the Firmware

### 1. Install Pico SDK

```bash
# Clone the SDK (if not already installed)
cd ~
git clone https://github.com/raspberrypi/pico-sdk.git
cd pico-sdk
git submodule update --init

# Set environment variable (add to ~/.bashrc for persistence)
export PICO_SDK_PATH=~/pico-sdk
```

### 2. Clone This Repository

```bash
git clone <repository-url>
cd nsrw_emu
```

### 3. Build

```bash
# Create build directory
mkdir build
cd build

# Configure with CMake
cmake ..

# Build the firmware
make -j$(nproc)
```

**Output**: `build/firmware/nrwa_t6_emulator.uf2`

### 4. Flash to Pico

1. Hold down the BOOTSEL button on the Pico
2. Connect USB cable to your computer
3. Release BOOTSEL (Pico mounts as USB drive `RPI-RP2`)
4. Drag `nrwa_t6_emulator.uf2` to the drive
5. Pico automatically reboots and runs the firmware

## Usage

### Console Access

The emulator provides a USB-CDC serial console for monitoring and control.

1. Connect to the Pico's USB serial port:
   ```bash
   # Linux
   screen /dev/ttyACM0 115200

   # macOS
   screen /dev/tty.usbmodem* 115200

   # Windows (use PuTTY or similar)
   # Connect to COM port at 115200 baud
   ```

2. You should see the startup banner followed by built-in test results:
   ```
   ╔════════════════════════════════════════════════════════════╗
   ║     NRWA-T6 Reaction Wheel Emulator                       ║
   ║     NewSpace Systems NRWA-T6 Compatible                   ║
   ╚════════════════════════════════════════════════════════════╝

   Firmware Version : v0.1.0-7e042b4
   Build Date       : Nov  8 2025 14:23:15
   Target Platform  : RP2040 (Pico)
   Board ID         : E6614103E73F2B34

   [Core0] Running Built-In Test Suite...

   ╔════════════════════════════════════════════════════════════╗
   ║  CHECKPOINT 3.1: CRC-CCITT                                ║
   ╚════════════════════════════════════════════════════════════╝

   [CRC] Empty buffer: PASS
   [CRC] Single byte (0x12): PASS
   ...

   ✓✓✓ ALL TESTS PASSED (46/46) ✓✓✓

   Waiting for keypress...
   ```

3. Press any key to enter the interactive TUI

### TUI Interface (Phase 8+)

The TUI is a **non-scrolling, live-updating interface** like `top` or `htop` with arrow-key navigation:

```text
┌─ NRWA-T6 Emulator ──── Uptime: 00:15:32 ──── Tests: 78/78 ✓ ─────┐
├─ Status: ON │ Mode: SPEED │ RPM: 3245 │ Current: 1.25A │ Fault: -┤
├───────────────────────────────────────────────────────────────────┤
│ TABLES                                                            │
│                                                                   │
│ > 1. ▶ Built-In Tests      [COLLAPSED]                           │
│   2. ▼ Control Mode        [EXPANDED]                            │
│       ├─ mode          : SPEED       (RW)                         │
│     ► ├─ setpoint_rpm  : 3000        (RW)    ← cursor            │
│       ├─ actual_rpm    : 3245        (RO)                         │
│       └─ pid_enabled   : true        (RW)                         │
│   3. ▶ Dynamics            [COLLAPSED]                            │
│                                                                   │
├───────────────────────────────────────────────────────────────────┤
│ ↑↓: Navigate │ →: Expand │ ←: Collapse │ Enter: Edit │ C: Command│
└───────────────────────────────────────────────────────────────────┘
```

**Navigation:**

- **↑/↓** - Move cursor between tables and fields
- **→** - Expand selected table to show fields
- **←** - Collapse expanded table
- **Enter** - Edit selected field value (Phase 8.3)
- **C** - Enter command mode
- **R** - Force refresh
- **Q** or **ESC** - Quit

### Command Mode (Phase 8.3)

Press **C** to enter command mode. Commands support **partial prefix matching**:

**Examples:**
```
> d t l                              → database table list
> db tab get control.mode            → get field value
> d t s control.setpoint_rpm 5000    → set field value
> def                                → show non-defaults
> ?                                  → help
```

**Available Commands:**

- `help`, `?` - Show available commands
- `database` (`db`, `d`)
  - `table list` - List all tables
  - `table get <table>.<field>` - Read field value
  - `table set <table>.<field> <value>` - Write field value
  - `defaults` - Show non-default values
- `version` - Firmware version and build info
- `quit` - Exit TUI

### RS-485 Communication

Connect the RS-485 transceiver to your OBC/test harness:

- **Baud Rate**: 460.8 kbps (accepts 455.6 - 465.7 kbps)
- **Format**: 8-N-1 (8 data bits, no parity, 1 stop bit)
- **Framing**: SLIP (END=0xC0, ESC=0xDB)
- **Protocol**: NSP with CCITT CRC-16

Supported NSP commands (see [SPEC.md](SPEC.md) for details):
- 0x00 PING
- 0x02 PEEK
- 0x03 POKE
- 0x07 APPLICATION-TELEMETRY
- 0x08 APPLICATION-COMMAND
- 0x09 CLEAR-FAULT
- 0x0A CONFIGURE-PROTECTION
- 0x0B TRIP-LCL

## Testing

### Host Tester (Python)

*Coming soon - Phase 10*

```bash
cd tools
python host_tester.py --port /dev/ttyUSB0 --baud 460800
```

### Unit Tests

*Coming soon - Phase 4-7*

```bash
cd build
cmake -DBUILD_TESTS=ON ..
make test
```

## Project Structure

```
nsrw_emu/
├── CMakeLists.txt          # Top-level build config
├── README.md               # This file
├── SPEC.md                 # Full specification
├── IMP.md                  # Implementation plan
├── PROGRESS.md             # Development progress tracker
├── docs/                   # Design baseline documents
│   ├── README.md           # Documentation guide
│   ├── NRWA-T6_ICD_v10.02.pdf           # Hardware datasheet (TO BE ADDED)
│   └── RESET_FAULT_REQUIREMENTS.md      # Reset/fault handling specs
├── firmware/               # Embedded firmware
│   ├── app_main.c          # Main application
│   ├── platform/           # RP2040 HAL
│   ├── drivers/            # RS-485, SLIP, NSP
│   ├── device/             # Wheel model & commands
│   ├── console/            # USB-CDC TUI
│   └── util/               # Helpers
├── tools/                  # Host-side tools
└── tests/                  # Unit and HIL tests
```

## Development Status

**Current Phase**: Phase 8 (Console & TUI) - Checkpoint 8.1 Complete (33%) 🔄

- [x] Phase 1: Project Foundation
- [x] Phase 2: Platform Layer
- [x] Phase 3: Core Communication Drivers
- [x] Phase 4: Utilities Foundation
- [x] Phase 5: Device Model & Physics
- [x] Phase 6: Device Commands & Telemetry
- [x] Phase 7: Protection System
- [x] Phase 8.1: TUI Core & Test System Refactor
- [ ] Phase 8.2: Table Catalog (7 base tables)
- [ ] Phase 8.3: Command Palette
- [ ] Phase 9: Fault Injection System
- [ ] Phase 10: Main Application & Dual-Core

**Metrics**:
- **Lines of Code**: ~10,520 (210% of target)
- **Checkpoints Complete**: 15/~19 (79%)
- **Unit Tests**: 46 tests (all passing, cached at boot)
- **Flash Usage**: 175 KB / 256 KB (68%)

See [PROGRESS.md](PROGRESS.md) for detailed status and [IMP.md](IMP.md) for implementation plan.

## Troubleshooting

### Build Issues

**Problem**: `CMake Error: PICO_SDK_PATH not found`
- **Solution**: Set `PICO_SDK_PATH` environment variable or pass `-DPICO_SDK_PATH=/path/to/sdk`

**Problem**: `arm-none-eabi-gcc: command not found`
- **Solution**: Install ARM toolchain:
  ```bash
  # Debian/Ubuntu
  sudo apt install gcc-arm-none-eabi

  # macOS
  brew install --cask gcc-arm-embedded
  ```

### Runtime Issues

**Problem**: Pico doesn't appear as USB serial port
- **Solution**: Check that `pico_enable_stdio_usb` is set to 1 in CMakeLists.txt
- **Solution**: Try a different USB cable (some are power-only)

**Problem**: RS-485 communication not working
- **Solution**: Verify DE/RE pin connections and transceiver power
- **Solution**: Check baud rate matches (460.8 kbps ±1%)
- **Solution**: Use logic analyzer to inspect UART signals

## Contributing

This project follows the implementation plan in [IMP.md](IMP.md). Each phase has clear acceptance criteria. When contributing:

1. Follow the existing code style (see existing files)
2. Add unit tests for new functionality
3. Update IMP.md progress as phases complete
4. Document any deviations from SPEC.md

## License

*TBD - Specify license here*

## Documentation

### Key Documents

- **[SPEC.md](SPEC.md)** - High-level emulator specification
- **[IMP.md](IMP.md)** - Phased implementation plan (10 phases)
- **[PROGRESS.md](PROGRESS.md)** - Development status and completed work
- **[firmware/console/DESIGN.md](firmware/console/DESIGN.md)** - Table catalog architecture guide
- **[docs/](docs/)** - Design baseline and requirements
  - [NRWA-T6 ICD](docs/NRWA-T6_ICD_v10.02.pdf) - Official hardware datasheet (TO BE ADDED)

### External References

- **Pico SDK Documentation**: https://www.raspberrypi.com/documentation/pico-sdk/
- **RP2040 Datasheet**: https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf
- **NRWA-T6 ICD**: Version 10.02 (Effective Date: 18 December 2023) - Add to [docs/](docs/) directory

## Contact

*TBD - Add contact information or issue tracker*

---

**Version**: 0.1.0
**Last Updated**: 2025-11-08
**Status**: Phase 8 Checkpoint 8.1 Complete - TUI Core & Test System Refactor
