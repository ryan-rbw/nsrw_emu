# NRWA-T6 Emulator - Build and Flash Guide

Complete instructions for setting up the development environment, building firmware, and flashing to Raspberry Pi Pico.

---

## Table of Contents

1. [Hardware Requirements](#hardware-requirements)
2. [Software Prerequisites](#software-prerequisites)
3. [Pico SDK Installation](#pico-sdk-installation)
4. [Building the Firmware](#building-the-firmware)
5. [Flashing to Pico](#flashing-to-pico)
6. [Connecting to Console](#connecting-to-console)
7. [Expected Output](#expected-output)
8. [Troubleshooting](#troubleshooting)
9. [Quick Reference](#quick-reference)

---

## Hardware Requirements

### Required
- **Raspberry Pi Pico** (RP2040-based, standard Pico - **NOT** Pico W/W2)
- **USB Micro-B cable** (must support data transfer, not power-only)
- **Computer** running Linux (Ubuntu 20.04+ or similar)

### Optional (for full testing)
- Breadboard
- Jumper wires (for address pin configuration)
- MAX485 or compatible RS-485 transceiver (for Phase 3 Checkpoint 3.3+)
- Logic analyzer (for protocol validation)

---

## Software Prerequisites

### Check Current System

First, verify what tools are already installed:

```bash
# Check for required tools
which cmake git arm-none-eabi-gcc picotool

# Check versions
cmake --version      # Need ≥ 3.13
git --version
arm-none-eabi-gcc --version  # If installed
picotool version             # If installed
```

### Install Build Tools

If any tools are missing, install them:

```bash
# Update package lists
sudo apt update

# Install CMake and Git (if needed)
sudo apt install -y cmake git

# Install ARM GCC toolchain (REQUIRED)
sudo apt install -y gcc-arm-none-eabi libnewlib-arm-none-eabi build-essential

# Verify ARM toolchain installation
arm-none-eabi-gcc --version
```

**Expected output** (version may vary):
```
arm-none-eabi-gcc (GNU Arm Embedded Toolchain 10.3-2021.10) 10.3.1 20210824
```

### Install picotool (REQUIRED)

The `picotool` utility is required to convert ELF binaries to UF2 format for flashing to Pico.

**Option 1: Build from source (recommended)**

```bash
# Install dependencies
sudo apt install -y build-essential pkg-config libusb-1.0-0-dev cmake

# Clone picotool repository
cd ~
git clone https://github.com/raspberrypi/picotool.git
cd picotool

# Build
mkdir build && cd build
cmake ..
make -j$(nproc)

# Install system-wide
sudo make install

# Verify installation
picotool version
```

**Option 2: Install from package manager (if available)**

Some distributions have picotool packages:

```bash
# Ubuntu 22.04+ or Debian
sudo apt install picotool

# Verify installation
picotool version
```

**Expected output**:
```
picotool v1.1.2 (...)
```

---

## Pico SDK Installation

### Step 1: Check if SDK Already Exists

```bash
# Check environment variable
echo $PICO_SDK_PATH

# Check common locations
ls ~/pico-sdk 2>/dev/null && echo "Found in home directory"
ls /usr/share/pico-sdk 2>/dev/null && echo "Found in /usr/share"
```

If SDK is already installed and `PICO_SDK_PATH` is set, skip to [Building the Firmware](#building-the-firmware).

### Step 2: Clone Pico SDK

```bash
# Clone to home directory
cd ~
git clone https://github.com/raspberrypi/pico-sdk.git

# Enter SDK directory
cd pico-sdk

# Initialize submodules (IMPORTANT - don't skip this)
git submodule update --init
```

This will download approximately 150 MB and may take 2-5 minutes depending on your connection.

### Step 3: Set Environment Variable

```bash
# Set for current session
export PICO_SDK_PATH=~/pico-sdk

# Make permanent (add to ~/.bashrc)
echo 'export PICO_SDK_PATH=~/pico-sdk' >> ~/.bashrc

# Verify
echo $PICO_SDK_PATH
ls $PICO_SDK_PATH/cmake  # Should show CMake files
```

---

## Building the Firmware

### Method 1: Using build.sh (Recommended)

The easiest way to build is using the provided build script:

```bash
# Navigate to project directory
cd /home/rwhite/src/nsrw_emu

# Run build script
./build.sh
```

The script will:
1. Check for Pico SDK (uses `~/pico-sdk` by default)
2. Create/configure build directory
3. Build the firmware
4. Convert ELF to UF2 format
5. Show flash instructions

**Expected output**:
```
=== Building NRWA-T6 Emulator ===
Using picotool: picotool v1.1.2 (...)
Using Pico SDK: /home/username/pico-sdk
Patching Makefile to skip ARM binary execution...
Building firmware...
[100%] Built target nrwa_t6_emulator
Converting ELF to UF2...

=== Build successful ===
Output files:
  ELF: firmware/nrwa_t6_emulator.elf (729K)
  UF2: nrwa_t6_emulator.uf2 (72K)

To flash to Pico:
  1. Hold BOOTSEL button and connect Pico via USB
  2. cp build/nrwa_t6_emulator.uf2 /media/$USER/RPI-RP2/
```

### Method 2: Manual Build

If you prefer manual control or need to customize the build:

```bash
# Navigate to project
cd /home/rwhite/src/nsrw_emu

# Create and enter build directory
mkdir -p build && cd build

# Configure with CMake
cmake ..

# Build
make -j$(nproc)

# Convert ELF to UF2 using system picotool
picotool uf2 convert firmware/nrwa_t6_emulator.elf firmware/nrwa_t6_emulator.uf2
```

**Note:** The build script (`./build.sh`) automatically patches the Makefile to avoid QEMU crashes during cross-compilation. If building manually and you encounter QEMU segfaults, the build script handles this for you.

### After Code Changes

When you modify source files, rebuild with:

```bash
# Using build script (recommended)
./build.sh

# OR manually
cd build
make -j$(nproc)
picotool uf2 convert firmware/nrwa_t6_emulator.elf firmware/nrwa_t6_emulator.uf2
```

### Clean Rebuild

If you encounter issues or want a fresh build:

```bash
# Using build script with --clean option (recommended)
./build.sh --clean

# OR manually
rm -rf build
mkdir build && cd build
cmake ..
make -j$(nproc)
picotool uf2 convert firmware/nrwa_t6_emulator.elf firmware/nrwa_t6_emulator.uf2
```

### Build Output Files

After a successful build, you'll have:

```bash
build/nrwa_t6_emulator.uf2           #  72KB flashable image for Pico (READY TO FLASH)
build/firmware/nrwa_t6_emulator.elf  # 729KB ARM Cortex-M0+ executable
build/firmware/nrwa_t6_emulator.uf2  #  72KB UF2 (also in firmware/ subdirectory)
```

**The .uf2 file at `build/nrwa_t6_emulator.uf2`** is what you flash to the Pico. It's copied to the build root for easy access.

---

## Flashing to Pico

### Step 1: Enter BOOTSEL Mode

1. **Disconnect** the Pico from USB (if connected)
2. **Hold down** the white **BOOTSEL button** on the Pico (near USB connector)
3. **While holding BOOTSEL**, connect the USB cable to your computer
4. **Release** the BOOTSEL button

The Pico should now mount as a USB drive.

### Step 2: Verify Pico Mounted

```bash
# Check if RPI-RP2 drive appears
ls /media/$USER/RPI-RP2

# Should show:
# INFO_UF2.TXT  INDEX.HTM
```

**Alternative locations** (if not in /media/$USER/):
```bash
ls /media/RPI-RP2
ls /run/media/$USER/RPI-RP2
```

### Step 3: Copy Firmware to Pico

**Method 1: Command Line (Recommended)**

```bash
# From project root - UF2 is in build/ for easy access
cp build/nrwa_t6_emulator.uf2 /media/$USER/RPI-RP2/

# The Pico will automatically reboot after copy completes
```

**Method 2: File Manager**

1. Open your file manager
2. Navigate to `build/` directory
3. Drag `nrwa_t6_emulator.uf2` to the `RPI-RP2` drive
4. The drive will disappear automatically when flashing completes

### Method 3: Using load.sh Script (Recommended for Development)

```bash
# From project root - automatic flash with picotool
./load.sh

# With verification
./load.sh --verify

# Show Pico info without flashing
./load.sh --info
```

The script will:

- Check for Pico in BOOTSEL mode
- Show device information
- Flash the firmware
- Optionally verify the flash
- Reboot the Pico

### Method 4: Using picotool Directly (Manual)

```bash
# Flash directly (Pico must be in BOOTSEL mode)
picotool load build/nrwa_t6_emulator.uf2
picotool reboot
```

### Step 4: Confirm Flashing Success

After flashing:
- The RPI-RP2 drive will **disappear** (this is normal)
- The Pico will **reboot** and run your firmware
- The onboard LED should start **blinking at 1 Hz**

---

## Connecting to Console

The firmware uses USB-CDC (USB serial) for console output.

### Step 1: Find Serial Port

```bash
# List USB serial devices
ls /dev/ttyACM*

# Should show: /dev/ttyACM0 (or /dev/ttyACM1 if others are connected)

# Verify it's the Pico
dmesg | tail -20
```

You should see recent kernel messages about a USB CDC device.

### Step 2: Set Permissions (First Time Only)

```bash
# Add your user to dialout group (allows serial port access)
sudo usermod -a -G dialout $USER

# Log out and log back in for this to take effect
# Or use: newgrp dialout
```

### Step 3: Connect with Terminal Program

**Option A: connect.sh Script (Recommended)**

```bash
# From project root - automatic console connection
./connect.sh

# Connect to specific port
./connect.sh /dev/ttyACM1

# Show help
./connect.sh --help
```

The script will:

- Check for minicom installation
- Verify serial port exists
- Check user permissions (dialout group)
- Connect with proper settings (115200 baud, 8N1)
- Display keyboard shortcuts

**To exit minicom**: Press `Ctrl-A`, then `X`, then confirm with `Enter`

**Option B: screen (simplest manual method)**

```bash
# Connect (115200 baud, 8N1)
screen /dev/ttyACM0 115200

# To exit screen: Press Ctrl-A, then K, then Y
```

**Option C: minicom (manual)**

```bash
# Install if needed
sudo apt install minicom

# Connect
minicom -D /dev/ttyACM0 -b 115200 -8 -o

# To exit: Ctrl-A, then X
```

**Option D: Python serial terminal**

```bash
# Install if needed
pip3 install pyserial

# Connect
python3 -m serial.tools.miniterm /dev/ttyACM0 115200

# To exit: Ctrl-]
```

### Step 4: View Output

If the Pico was already running when you connected, you might not see output immediately.

To restart and see the full boot sequence:
1. Unplug the Pico USB cable
2. Start your terminal program (e.g., `screen /dev/ttyACM0 115200`)
3. Plug the Pico back in
4. You should see the startup banner and test output

---

## Expected Output

### Checkpoint 3.1 (CRC-CCITT Test)

When running with `CHECKPOINT_3_1` enabled (current configuration), you should see:

```
╔════════════════════════════════════════════════════════════╗
║     NRWA-T6 Reaction Wheel Emulator                       ║
║     NewSpace Systems NRWA-T6 Compatible                   ║
╚════════════════════════════════════════════════════════════╝

Firmware Version : v0.1.0-8fe6124
Build Date       : Nov  5 2025 HH:MM:SS
Target Platform  : RP2040 (Pico)
Board ID         : E6614103E73F2B34

Status           : Initializing...

[Core0] Initializing hardware...
[Core0] Device address: 0x00 (from ADDR pins)

╔════════════════════════════════════════════════════════════╗
║  CHECKPOINT 3.1: CRC-CCITT TEST MODE                      ║
╚════════════════════════════════════════════════════════════╝

=== Checkpoint 3.1: CRC-CCITT Test Vectors ===

Test 1: {0x01, 0x02, 0x03}
  Calculated CRC: 0x7E70
  Test 1: ✓ PASS

Test 2: Empty buffer
  Calculated CRC: 0xFFFF
  Test 2 (empty): ✓ PASS

Test 3: {0x00}
  Calculated CRC: 0x1D0F
  Test 3: ✓ PASS

Test 4: {0xFF, 0xFF, 0xFF, 0xFF}
  Calculated CRC: 0x1D00
  Test 4: ✓ PASS

Test 5: ASCII "123456789"
  Calculated CRC: 0x29B1
  Test 5 (ASCII): ✓ PASS

Test 6: Incremental calculation {0x01, 0x02, 0x03}
  Calculated CRC: 0x7E70
  Test 6 (incremental): ✓ PASS

Test 7: NSP PING packet
  Calculated CRC: 0x543D
  Test 7 (NSP PING): ✓ PASS

✓✓✓ ALL CRC TESTS PASSED ✓✓✓

Test complete. Halting in checkpoint mode.
Heartbeat LED will continue blinking.
```

### Physical Indicators

- **Onboard LED**: Blinks at 1 Hz (on for 500ms, off for 500ms)
- **No error messages**: Clean output with all tests passing

---

## Troubleshooting

### Build Issues

#### Error: "PICO_SDK_PATH not found"

**Problem**: CMake can't find the Pico SDK.

**Solution 1** - Set environment variable:
```bash
export PICO_SDK_PATH=~/pico-sdk
cmake ..
```

**Solution 2** - Pass directly to CMake:
```bash
cmake -DPICO_SDK_PATH=~/pico-sdk ..
```

**Solution 3** - Verify SDK installation:
```bash
ls $PICO_SDK_PATH/cmake
# Should show: pico_sdk_init.cmake and other files
```

#### Error: "arm-none-eabi-gcc: command not found"

**Problem**: ARM toolchain not installed.

**Solution**:
```bash
sudo apt install gcc-arm-none-eabi libnewlib-arm-none-eabi
```

#### Error: "pico_sdk_import.cmake not found"

**Problem**: SDK submodules not initialized.

**Solution**:
```bash
cd ~/pico-sdk
git submodule update --init
```

### Flashing Issues

#### Problem: Pico doesn't mount as RPI-RP2

**Solution 1** - Try different USB cable:
- Some cables are power-only and don't carry data
- Use a cable you know works for data transfer

**Solution 2** - Hold BOOTSEL longer:
- Hold the button down while plugging in
- Keep holding for 2-3 seconds after plugging in

**Solution 3** - Check USB connection:
```bash
# Check if Pico is detected at all
lsusb | grep -i "Raspberry\|RP2"

# Check kernel messages
dmesg | tail -30
```

**Solution 4** - Try different USB port:
- Use a USB 2.0 port directly on your computer
- Avoid USB hubs if possible

#### Problem: Permission denied when copying UF2

**Solution**:
```bash
# Check mount point ownership
ls -ld /media/$USER/RPI-RP2

# If needed, remount with write permissions
# (usually not necessary, but worth checking)
```

### Console Issues

#### Problem: No /dev/ttyACM0 device appears

**Solution 1** - Check if device enumerated:
```bash
dmesg | grep -i "cdc\|tty"
# Look for recent messages about USB CDC ACM device
```

**Solution 2** - Check USB serial drivers loaded:
```bash
lsmod | grep cdc_acm
# Should show: cdc_acm
```

**Solution 3** - Check if device has different name:
```bash
ls /dev/ttyACM*  # Might be ttyACM1, ttyACM2, etc.
ls /dev/ttyUSB*  # Some systems use this
```

#### Problem: Permission denied accessing /dev/ttyACM0

**Solution**:
```bash
# Add user to dialout group
sudo usermod -a -G dialout $USER

# Check current groups
groups

# If dialout not listed, log out and log back in
# Or use: newgrp dialout
```

**Quick workaround** (not recommended for permanent use):
```bash
sudo chmod 666 /dev/ttyACM0
screen /dev/ttyACM0 115200
```

#### Problem: Console shows garbage characters

**Solution 1** - Verify baud rate:
```bash
# Make sure you're using 115200 baud
screen /dev/ttyACM0 115200
```

**Solution 2** - Reset the Pico:
- Unplug and replug USB cable
- Or press the RUN button on the Pico (if accessible)

**Solution 3** - Check USB cable quality:
- Try a different cable
- Try a different USB port

#### Problem: No output in console (blank screen)

**Solution 1** - Restart the Pico:
```bash
# Disconnect, then reconnect while terminal is open
```

**Solution 2** - Check if firmware is running:
- The LED should be blinking at 1 Hz
- If not, reflash the firmware

**Solution 3** - Verify USB-CDC is enabled:
```bash
# Check firmware/CMakeLists.txt has:
# pico_enable_stdio_usb(nrwa_t6_emulator 1)
```

### Runtime Issues

#### Problem: CRC tests fail

**Symptoms**: Console shows "✗ FAIL" for one or more tests.

**This should not happen** - if tests fail, it indicates a problem:

1. Check build warnings:
   ```bash
   cd build
   make clean
   make 2>&1 | tee build.log
   # Review build.log for warnings
   ```

2. Verify source code integrity:
   ```bash
   git status  # Should be clean
   git diff    # Should show no uncommitted changes
   ```

3. Rebuild from scratch:
   ```bash
   cd build
   rm -rf *
   cmake ..
   make -j$(nproc)
   ```

#### Problem: LED not blinking

**Possible causes**:
- Firmware not running (stuck in boot or crashed)
- LED hardware issue (rare)

**Solution**:
```bash
# Check console for error messages
# Try reflashing firmware
# Check for panic/crash messages in console
```

---

## Quick Reference

### One-Time Setup

```bash
# Install tools
sudo apt update
sudo apt install -y cmake git gcc-arm-none-eabi libnewlib-arm-none-eabi build-essential

# Install picotool
cd ~
git clone https://github.com/raspberrypi/picotool.git
cd picotool
mkdir build && cd build
cmake .. && make -j$(nproc)
sudo make install

# Clone Pico SDK
cd ~
git clone https://github.com/raspberrypi/pico-sdk.git
cd pico-sdk
git submodule update --init

# Set environment
export PICO_SDK_PATH=~/pico-sdk
echo 'export PICO_SDK_PATH=~/pico-sdk' >> ~/.bashrc

# Add to dialout group
sudo usermod -a -G dialout $USER
# Log out and back in
```

### Build and Flash Workflow

#### Option 1: Using Automation Scripts (Recommended)

```bash
# Navigate to project
cd /home/rwhite/src/nsrw_emu

# Build firmware
./build.sh

# Flash with picotool (Pico in BOOTSEL mode)
./load.sh

# Connect to serial console
./connect.sh
# Exit minicom: Ctrl-A, then X
```

#### Option 2: Using load.sh with Manual Console

```bash
# Navigate to project
cd /home/rwhite/src/nsrw_emu

# Build firmware
./build.sh

# Flash with picotool (Pico in BOOTSEL mode)
./load.sh

# Connect console manually
screen /dev/ttyACM0 115200
# Exit screen: Ctrl-A, then K, then Y
```

#### Option 3: Using USB Mass Storage

```bash
# Navigate to project
cd /home/rwhite/src/nsrw_emu

# Build firmware
./build.sh

# Flash via USB drive (Pico in BOOTSEL mode)
cp build/nrwa_t6_emulator.uf2 /media/$USER/RPI-RP2/

# Connect console
./connect.sh
# Or: screen /dev/ttyACM0 115200
```

### Rebuild After Code Changes

```bash
# Recommended: Use build script
cd /home/rwhite/src/nsrw_emu
./build.sh

# Alternative: Manual rebuild
cd /home/rwhite/src/nsrw_emu/build
make -j$(nproc)
picotool uf2 convert firmware/nrwa_t6_emulator.elf firmware/nrwa_t6_emulator.uf2
```

### Clean Build (if needed)

```bash
# Recommended: Use build script with --clean
cd /home/rwhite/src/nsrw_emu
./build.sh --clean

# Alternative: Manual clean build
cd /home/rwhite/src/nsrw_emu
rm -rf build
mkdir build && cd build
cmake ..
make -j$(nproc)
picotool uf2 convert firmware/nrwa_t6_emulator.elf firmware/nrwa_t6_emulator.uf2
```

---

## Next Steps

After successfully building and testing Checkpoint 3.1:

1. **Verify all tests pass** - All 7 CRC tests should show ✓ PASS
2. **Check LED is blinking** - Confirms hardware initialization worked
3. **Proceed to Checkpoint 3.2** - SLIP codec implementation
4. **See [IMP.md](IMP.md)** for implementation roadmap
5. **See [PROGRESS.md](PROGRESS.md)** for current status

---

## Additional Resources

- **Pico SDK Documentation**: https://www.raspberrypi.com/documentation/pico-sdk/
- **RP2040 Datasheet**: https://datasheets.raspberrypi.com/rp2040/rp2040-datasheet.pdf
- **Getting Started with Pico**: https://datasheets.raspberrypi.com/pico/getting-started-with-pico.pdf
- **This Project's Documentation**:
  - [README.md](README.md) - Project overview
  - [SPEC.md](SPEC.md) - Full specification
  - [IMP.md](IMP.md) - Implementation plan
  - [PROGRESS.md](PROGRESS.md) - Current status

---

**Last Updated**: 2025-11-05
**Current Checkpoint**: 3.1 (CRC-CCITT) ✅ Complete
**Next Checkpoint**: 3.2 (SLIP Codec)
