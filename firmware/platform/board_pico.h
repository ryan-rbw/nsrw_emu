/**
 * @file board_pico.h
 * @brief Raspberry Pi Pico (RP2040) Board Configuration for NRWA-T6 Emulator
 *
 * Pin definitions and hardware configuration for the reaction wheel emulator.
 * Matches the pin mapping documented in README.md.
 */

#ifndef BOARD_PICO_H
#define BOARD_PICO_H

#include "pico/stdlib.h"

// ============================================================================
// UART Configuration (RS-485 Interface)
// ============================================================================

/** UART instance for RS-485 communication (UART1) */
#define RS485_UART          uart1

/** UART1 TX pin (transmit data to RS-485 transceiver) */
#define RS485_UART_TX_PIN   4

/** UART1 RX pin (receive data from RS-485 transceiver) */
#define RS485_UART_RX_PIN   5

/** RS-485 Driver Enable (DE) - active high to enable transmit */
#define RS485_DE_PIN        6

/** RS-485 Receiver Enable (RE) - active low to enable receive (inverted DE) */
#define RS485_RE_PIN        7

/** RS-485 baud rate (460.8 kbps nominal, tolerance ±1%) */
#define RS485_BAUD_RATE     460800

/** RS-485 data format: 8-N-1 */
#define RS485_DATA_BITS     8
#define RS485_STOP_BITS     1
#define RS485_PARITY        UART_PARITY_NONE

// ============================================================================
// Address Configuration Pins
// ============================================================================

/** Address bit 0 (LSB) - pulled high or low to set device address */
#define ADDR0_PIN           10

/** Address bit 1 - pulled high or low to set device address */
#define ADDR1_PIN           11

/** Address bit 2 (MSB) - pulled high or low to set device address */
#define ADDR2_PIN           12

/** Number of address bits (supports 8 devices: 0-7) */
#define ADDR_PIN_COUNT      3

// ============================================================================
// Status and Control Pins
// ============================================================================

/** Fault output pin (open-drain, active low) */
#define FAULT_PIN           13

/** Reset input pin (active low) */
#define RESET_PIN           14

/** Onboard LED (GP25 on standard Pico, used for heartbeat) */
#define LED_HEARTBEAT_PIN   PICO_DEFAULT_LED_PIN

// ============================================================================
// Optional Status LEDs (if external LEDs are added)
// ============================================================================

/** Optional: RS-485 activity LED */
#define LED_RS485_ACTIVE_PIN    15  // Optional external LED

/** Optional: Fault indicator LED */
#define LED_FAULT_PIN           16  // Optional external LED

/** Optional: Mode indicator LED */
#define LED_MODE_PIN            17  // Optional external LED

// ============================================================================
// Hardware Timing Configuration
// ============================================================================

/** Physics loop tick rate (Hz) */
#define PHYSICS_TICK_RATE_HZ    100

/** Physics loop tick period (microseconds) */
#define PHYSICS_TICK_PERIOD_US  (1000000 / PHYSICS_TICK_RATE_HZ)

/** Maximum allowed jitter in physics tick (microseconds) */
#define MAX_TICK_JITTER_US      200

/** RS-485 DE/RE timing: microseconds to assert DE before TX */
#define RS485_DE_SETUP_US       10

/** RS-485 DE/RE timing: microseconds to hold DE after TX */
#define RS485_DE_HOLD_US        10

// ============================================================================
// Memory and Buffer Configuration
// ============================================================================

/** RS-485 RX ring buffer size (must be power of 2) */
#define RS485_RX_BUFFER_SIZE    1024

/** RS-485 TX ring buffer size (must be power of 2) */
#define RS485_TX_BUFFER_SIZE    1024

/** Maximum SLIP frame size (bytes) */
#define SLIP_MAX_FRAME_SIZE     256

/** Telemetry ring buffer size (Core1 → Core0, must be power of 2) */
#define TELEMETRY_RINGBUF_SIZE  16

// ============================================================================
// USB-CDC Console Configuration
// ============================================================================

/** USB-CDC RX buffer size */
#define USB_CDC_RX_BUFFER_SIZE  512

/** Console command line buffer size */
#define CONSOLE_LINE_BUFFER_SIZE 128

// ============================================================================
// Flash Configuration (for JSON scenarios)
// ============================================================================

/** Flash sector size for RP2040 (4 KB) */
#define FLASH_SECTOR_SIZE       4096

/** Reserved flash size for scenarios (64 KB = 16 sectors) */
#define FLASH_SCENARIO_SIZE     (16 * FLASH_SECTOR_SIZE)

/** Flash offset for scenario storage (starts at 1.5 MB from flash base) */
#define FLASH_SCENARIO_OFFSET   0x00180000

// ============================================================================
// Core Assignment
// ============================================================================

/** Core 0: Communications, console, command dispatch */
#define CORE_COMMS              0

/** Core 1: Physics simulation, control loops, protections */
#define CORE_PHYSICS            1

// ============================================================================
// Hardware Feature Flags
// ============================================================================

/** Enable watchdog timer (1s timeout) */
#define ENABLE_WATCHDOG         1

/** Enable external status LEDs (0 = onboard LED only) */
#define ENABLE_EXTERNAL_LEDS    0

/** Enable UART hardware flow control (0 = no flow control for RS-485) */
#define ENABLE_UART_FLOW_CTRL   0

// ============================================================================
// Pin Validation Macros
// ============================================================================

/** Check if pin number is valid for RP2040 (GP0-GP29) */
#define IS_VALID_GPIO(pin)      ((pin) <= 29)

/** Static assertion for pin configuration validity (compile-time check) */
#define BOARD_STATIC_ASSERT(cond, msg) \
    _Static_assert(cond, msg)

// Compile-time checks for pin conflicts
BOARD_STATIC_ASSERT(IS_VALID_GPIO(RS485_UART_TX_PIN), "RS485 TX pin invalid");
BOARD_STATIC_ASSERT(IS_VALID_GPIO(RS485_UART_RX_PIN), "RS485 RX pin invalid");
BOARD_STATIC_ASSERT(RS485_UART_TX_PIN != RS485_UART_RX_PIN, "TX/RX pins must differ");

#endif // BOARD_PICO_H
