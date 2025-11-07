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

2. You should see the startup banner:
   ```
   ╔════════════════════════════════════════════════════════════╗
   ║     NRWA-T6 Reaction Wheel Emulator                       ║
   ║     NewSpace Systems NRWA-T6 Compatible                   ║
   ╚════════════════════════════════════════════════════════════╝

   Firmware Version : v0.1.0-7e042b4
   Build Date       : Nov  5 2025 11:55:32
   Target Platform  : RP2040 (Pico)
   Board ID         : E6614103E73F2B34

   Ready. Waiting for commands...
   Type 'help' for console commands.
   ```

### Console Commands (Phase 8)

*Coming soon - full command palette will be available in Phase 8*

Example commands:
- `help` - Show available commands
- `version` - Firmware version and build info
- `tables` - List all configuration tables
- `get dynamics.speed_rpm` - Read current wheel speed
- `set control.mode SPEED` - Change to speed control mode
- `scenario load overspeed_test` - Load fault injection scenario

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

**Current Phase**: Phase 1 Complete ✅

- [x] Phase 1: Project Foundation
- [ ] Phase 2: Platform Layer
- [ ] Phase 3: Core Communication Drivers
- [ ] Phase 4: Utilities Foundation
- [ ] Phase 5: Device Model & Physics
- [ ] Phase 6: Device Commands & Telemetry
- [ ] Phase 7: Protection System
- [ ] Phase 8: Console & TUI
- [ ] Phase 9: Fault Injection System
- [ ] Phase 10: Main Application & Dual-Core

See [IMP.md](IMP.md) for detailed implementation plan.

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
- **[docs/](docs/)** - Design baseline and requirements
  - [NRWA-T6 ICD](docs/NRWA-T6_ICD_v10.02.pdf) - Official hardware datasheet (TO BE ADDED)
  - [Reset/Fault Requirements](docs/RESET_FAULT_REQUIREMENTS.md) - Reset handling specifications

### External References

- **Pico SDK Documentation**: https://www.raspberrypi.com/documentation/pico-sdk/
- **RP2040 Datasheet**: https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf
- **NRWA-T6 ICD**: Version 10.02 (Effective Date: 18 December 2023) - Add to [docs/](docs/) directory

## Contact

*TBD - Add contact information or issue tracker*

---

**Version**: 0.1.0
**Last Updated**: 2025-11-05
**Status**: Phase 1 Complete - Foundation Ready
