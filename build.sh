#!/bin/bash
#
# Build script for NRWA-T6 Emulator
# Builds firmware and converts to UF2 format for Pico
#

set -e  # Exit on error

# Colors for output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}=== Building NRWA-T6 Emulator ===${NC}"

# Check for Pico SDK
if [ -z "$PICO_SDK_PATH" ]; then
    echo -e "${YELLOW}PICO_SDK_PATH not set, using default: ~/pico-sdk${NC}"
    export PICO_SDK_PATH=~/pico-sdk
fi

if [ ! -d "$PICO_SDK_PATH" ]; then
    echo "ERROR: Pico SDK not found at $PICO_SDK_PATH"
    echo "Please install the Pico SDK or set PICO_SDK_PATH"
    exit 1
fi

echo "Using Pico SDK: $PICO_SDK_PATH"

# Create build directory if it doesn't exist
if [ ! -d "build" ]; then
    echo "Creating build directory..."
    mkdir build
fi

cd build

# Configure if needed
if [ ! -f "Makefile" ]; then
    echo "Configuring with CMake..."
    cmake ..
fi

# Patch the Makefile to skip ARM binary execution (workaround for QEMU crash)
if grep -q "^\s*cd.*&&.*\./nrwa_t6_emulator.elf" firmware/CMakeFiles/nrwa_t6_emulator.dir/build.make 2>/dev/null; then
    echo "Patching Makefile to skip ARM binary execution..."
    sed -i 's|^\(\s*cd.*/firmware && \)\./nrwa_t6_emulator.elf|\1echo "Skipping ARM execution (cross-compiling)"|' \
        firmware/CMakeFiles/nrwa_t6_emulator.dir/build.make
fi

# Build
echo "Building firmware..."
make -j$(nproc)

# Convert to UF2
if [ -f "firmware/nrwa_t6_emulator.elf" ]; then
    echo "Converting ELF to UF2..."
    if [ -f "_deps/picotool/picotool" ]; then
        _deps/picotool/picotool uf2 convert firmware/nrwa_t6_emulator.elf firmware/nrwa_t6_emulator.uf2
    else
        echo "ERROR: picotool not found"
        echo "ELF file created but UF2 conversion failed"
        exit 1
    fi
else
    echo "ERROR: ELF file not found"
    exit 1
fi

# Show results
echo ""
echo -e "${GREEN}=== Build successful ===${NC}"
echo "Output files:"
ls -lh firmware/nrwa_t6_emulator.elf firmware/nrwa_t6_emulator.uf2
echo ""
echo "To flash to Pico:"
echo "  1. Hold BOOTSEL button and connect Pico via USB"
echo "  2. cp build/firmware/nrwa_t6_emulator.uf2 /media/\$USER/RPI-RP2/"
echo ""
