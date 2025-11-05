#!/bin/bash
#
# Flash script for NRWA-T6 Emulator
# Loads firmware to Pico using picotool
#
# Usage:
#   ./load.sh           - Flash and reboot
#   ./load.sh --verify  - Flash, verify, and reboot
#   ./load.sh --info    - Show Pico info without flashing
#

set -e  # Exit on error

# Colors for output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

UF2_FILE="build/nrwa_t6_emulator.uf2"

# Parse command line arguments
VERIFY=false
INFO_ONLY=false

if [ "$1" == "--verify" ] || [ "$1" == "-v" ]; then
    VERIFY=true
elif [ "$1" == "--info" ] || [ "$1" == "-i" ]; then
    INFO_ONLY=true
elif [ "$1" == "--help" ] || [ "$1" == "-h" ]; then
    echo "Usage: $0 [OPTIONS]"
    echo ""
    echo "Flash firmware to Raspberry Pi Pico using picotool"
    echo ""
    echo "Options:"
    echo "  --verify, -v   Verify flash after loading"
    echo "  --info, -i     Show Pico info without flashing"
    echo "  --help, -h     Show this help message"
    echo ""
    echo "Prerequisites:"
    echo "  1. Pico must be in BOOTSEL mode (hold BOOTSEL button while connecting USB)"
    echo "  2. Firmware must be built (run ./build.sh first)"
    echo ""
    echo "Examples:"
    echo "  $0             # Flash and reboot"
    echo "  $0 --verify    # Flash with verification"
    echo "  $0 --info      # Show Pico information"
    exit 0
fi

echo -e "${GREEN}=== NRWA-T6 Emulator Flash Tool ===${NC}"
echo ""

# Check for picotool
if ! command -v picotool &> /dev/null; then
    echo -e "${RED}ERROR: picotool not found${NC}"
    echo ""
    echo "Please install picotool:"
    echo "  sudo apt install picotool"
    echo ""
    echo "Or build from source:"
    echo "  cd ~"
    echo "  git clone https://github.com/raspberrypi/picotool.git"
    echo "  cd picotool && mkdir build && cd build"
    echo "  cmake .. && make -j\$(nproc)"
    echo "  sudo make install"
    exit 1
fi

# Check if Pico is connected in BOOTSEL mode
echo -e "${BLUE}Checking for Pico in BOOTSEL mode...${NC}"
if ! picotool info &> /dev/null; then
    echo -e "${RED}ERROR: No Pico found in BOOTSEL mode${NC}"
    echo ""
    echo "To enter BOOTSEL mode:"
    echo "  1. Disconnect Pico from USB"
    echo "  2. Hold down the BOOTSEL button (white button near USB)"
    echo "  3. While holding BOOTSEL, connect Pico via USB"
    echo "  4. Release BOOTSEL button"
    echo "  5. Run this script again"
    echo ""
    echo "Alternative: If Pico is already connected, run:"
    echo "  picotool reboot -f -u"
    exit 1
fi

# Show Pico info
echo -e "${GREEN}Pico detected!${NC}"
picotool info

# If info-only mode, exit here
if [ "$INFO_ONLY" = true ]; then
    echo ""
    echo "Info displayed. Use './load.sh' to flash firmware."
    exit 0
fi

echo ""

# Check if firmware exists
if [ ! -f "$UF2_FILE" ]; then
    echo -e "${RED}ERROR: Firmware not found at $UF2_FILE${NC}"
    echo ""
    echo "Please build firmware first:"
    echo "  ./build.sh"
    exit 1
fi

# Show firmware size
FILESIZE=$(ls -lh "$UF2_FILE" | awk '{print $5}')
echo -e "${BLUE}Firmware: $UF2_FILE ($FILESIZE)${NC}"
echo ""

# Load firmware
echo -e "${YELLOW}Flashing firmware to Pico...${NC}"
picotool load "$UF2_FILE"

# Verify if requested
if [ "$VERIFY" = true ]; then
    echo ""
    echo -e "${YELLOW}Verifying flash...${NC}"
    picotool verify "$UF2_FILE"
fi

# Reboot
echo ""
echo -e "${YELLOW}Rebooting Pico...${NC}"
picotool reboot

echo ""
echo -e "${GREEN}=== Flash successful ===${NC}"
echo ""
echo "Next steps:"
echo "  1. Connect to serial console:"
echo "     screen /dev/ttyACM0 115200"
echo ""
echo "  2. Or use minicom:"
echo "     minicom -D /dev/ttyACM0 -b 115200"
echo ""
echo "  3. Exit screen: Ctrl-A, then K, then Y"
echo ""
