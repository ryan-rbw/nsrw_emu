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

# Build (this may trigger CMake reconfiguration)
echo "Building firmware..."
set +e  # Temporarily allow errors
make -j$(nproc) > build.log 2>&1
BUILD_RESULT=$?
set -e  # Re-enable exit on error

# Check if build failed
if [ $BUILD_RESULT -ne 0 ]; then
    # Show the error
    cat build.log

    # Check if it's the QEMU crash
    if grep -q "qemu.*Segmentation fault" build.log; then
        echo ""
        echo -e "${YELLOW}QEMU crash detected - applying Makefile patch and retrying...${NC}"

        # Patch the Makefile to skip ARM binary execution
        if grep -q "^\s*cd.*&&.*\./nrwa_t6_emulator.elf" firmware/CMakeFiles/nrwa_t6_emulator.dir/build.make 2>/dev/null; then
            sed -i 's|^\(\s*cd.*/firmware && \)\./nrwa_t6_emulator.elf|\1echo "Skipping ARM execution (cross-compiling)"|' \
                firmware/CMakeFiles/nrwa_t6_emulator.dir/build.make

            # Retry build
            echo "Retrying build..."
            make -j$(nproc)
            rm -f build.log
        else
            echo -e "${RED}ERROR: Could not find ARM execution command to patch${NC}"
            rm -f build.log
            exit 1
        fi
    else
        # Different error - exit
        echo -e "${RED}Build failed${NC}"
        rm -f build.log
        exit 1
    fi
else
    # Build succeeded - clean up
    rm -f build.log
fi

# Verify build outputs
echo ""
if [ ! -f "firmware/nrwa_t6_emulator.elf" ]; then
    echo -e "${RED}ERROR: ELF file not found${NC}"
    exit 1
fi

if [ ! -f "firmware/nrwa_t6_emulator.uf2" ]; then
    echo -e "${RED}ERROR: UF2 file not found${NC}"
    echo "Build succeeded but UF2 conversion failed"
    echo "ELF file is available at: build/firmware/nrwa_t6_emulator.elf"
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
