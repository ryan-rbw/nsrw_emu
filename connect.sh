#!/bin/bash
#
# Serial console connection script for NRWA-T6 Emulator
# Connects to Pico USB serial console using minicom
#
# Usage:
#   ./connect.sh           - Connect to /dev/ttyACM0 at 115200 baud
#   ./connect.sh /dev/ttyACM1 - Connect to specific device
#   ./connect.sh --help    - Show help
#

set -e  # Exit on error

# Colors for output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Default settings
DEFAULT_PORT="/dev/ttyACM0"
BAUD_RATE="115200"

# Parse command line arguments
if [ "$1" == "--help" ] || [ "$1" == "-h" ]; then
    echo "Usage: $0 [PORT]"
    echo ""
    echo "Connect to Raspberry Pi Pico serial console"
    echo ""
    echo "Arguments:"
    echo "  PORT       Serial port device (default: /dev/ttyACM0)"
    echo ""
    echo "Options:"
    echo "  --help, -h Show this help message"
    echo ""
    echo "Examples:"
    echo "  $0                    # Connect to /dev/ttyACM0"
    echo "  $0 /dev/ttyACM1       # Connect to /dev/ttyACM1"
    echo "  $0 /dev/ttyUSB0       # Connect to /dev/ttyUSB0"
    echo ""
    echo "Keyboard shortcuts in minicom:"
    echo "  Ctrl-A Z   - Show help menu"
    echo "  Ctrl-A X   - Exit minicom"
    echo "  Ctrl-A Q   - Quit without reset"
    echo "  Ctrl-A C   - Clear screen"
    echo ""
    echo "Note: To exit, press Ctrl-A, then X, then confirm with Enter"
    exit 0
fi

# Determine port
if [ -n "$1" ]; then
    PORT="$1"
else
    PORT="$DEFAULT_PORT"
fi

echo -e "${GREEN}=== NRWA-T6 Serial Console ===${NC}"
echo ""

# Check if minicom is installed
if ! command -v minicom &> /dev/null; then
    echo -e "${RED}ERROR: minicom not found${NC}"
    echo ""
    echo "Please install minicom:"
    echo "  sudo apt install minicom"
    echo ""
    echo "Alternative: Use screen instead:"
    echo "  screen $PORT $BAUD_RATE"
    exit 1
fi

# Check if port exists
if [ ! -e "$PORT" ]; then
    echo -e "${RED}ERROR: Serial port $PORT not found${NC}"
    echo ""
    echo "Available serial ports:"
    ls -1 /dev/ttyACM* /dev/ttyUSB* 2>/dev/null || echo "  (none found)"
    echo ""
    echo "Troubleshooting:"
    echo "  1. Make sure Pico is connected via USB"
    echo "  2. Make sure firmware is running (not in BOOTSEL mode)"
    echo "  3. Check dmesg for USB enumeration:"
    echo "     dmesg | tail -20"
    echo ""
    echo "  4. Try a different USB port or cable"
    exit 1
fi

# Check if user has permission to access serial port
if [ ! -r "$PORT" ] || [ ! -w "$PORT" ]; then
    echo -e "${YELLOW}WARNING: You may not have permission to access $PORT${NC}"
    echo ""
    echo "To fix this, add yourself to the dialout group:"
    echo "  sudo usermod -a -G dialout \$USER"
    echo "  # Then log out and back in"
    echo ""
    echo "Or run this script with sudo (not recommended):"
    echo "  sudo $0 $PORT"
    echo ""
    read -p "Press Enter to try connecting anyway, or Ctrl-C to abort..."
fi

# Show connection info
echo -e "${BLUE}Connecting to Pico serial console${NC}"
echo "  Port: $PORT"
echo "  Baud: $BAUD_RATE"
echo ""
echo -e "${YELLOW}Keyboard shortcuts:${NC}"
echo "  Ctrl-A X  - Exit minicom"
echo "  Ctrl-A Z  - Help menu"
echo "  Ctrl-A C  - Clear screen"
echo ""
echo "Starting minicom..."
sleep 1

# Launch minicom
# -D: Device
# -b: Baud rate
# -8: 8-bit data
# -o: No modem init (prevents sending AT commands)
minicom -D "$PORT" -b "$BAUD_RATE" -8 -o

# After exit
echo ""
echo -e "${GREEN}Disconnected from serial console${NC}"
