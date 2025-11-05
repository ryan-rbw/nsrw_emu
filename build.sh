#!/bin/bash
#
# Build script for NRWA-T6 Emulator
# Builds firmware and converts to UF2 format for Pico
#
# Usage:
#   ./build.sh          - Normal build (incremental)
#   ./build.sh --clean  - Clean rebuild (removes build directory)
#

set -e  # Exit on error

# Colors for output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# Parse command line arguments
CLEAN_BUILD=false
if [ "$1" == "--clean" ] || [ "$1" == "-c" ]; then
    CLEAN_BUILD=true
fi

# Show usage if help requested
if [ "$1" == "--help" ] || [ "$1" == "-h" ]; then
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Options:"
    echo "  --clean, -c    Clean rebuild (removes build directory)"
    echo "  --help, -h     Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0             # Normal incremental build"
    echo "  $0 --clean     # Clean rebuild from scratch"
    exit 0
fi

echo -e "${GREEN}=== Building NRWA-T6 Emulator ===${NC}"

# Handle clean build
if [ "$CLEAN_BUILD" = true ]; then
    if [ -d "build" ]; then
        echo -e "${YELLOW}Removing build directory for clean rebuild...${NC}"
        rm -rf build
    fi
fi

# Check for Pico SDK
if [ -z "$PICO_SDK_PATH" ]; then
    echo -e "${YELLOW}PICO_SDK_PATH not set, using default: ~/pico-sdk${NC}"
    export PICO_SDK_PATH=~/pico-sdk
fi

if [ ! -d "$PICO_SDK_PATH" ]; then
    echo -e "${RED}ERROR: Pico SDK not found at $PICO_SDK_PATH${NC}"
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
