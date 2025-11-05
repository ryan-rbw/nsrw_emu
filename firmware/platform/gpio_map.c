/**
 * @file gpio_map.c
 * @brief GPIO Initialization and Management for NRWA-T6 Emulator
 *
 * Configures all GPIOs for RS-485, address selection, fault/reset signals,
 * and status LEDs.
 */

#include "board_pico.h"
#include "hardware/gpio.h"
#include <stdio.h>

/**
 * @brief Initialize RS-485 control pins (DE/RE)
 *
 * DE (Driver Enable): High to transmit, low to tri-state
 * RE (Receiver Enable): Low to receive, high to disable
 */
static void gpio_init_rs485_control(void) {
    // Initialize DE pin (output, initially low = receive mode)
    gpio_init(RS485_DE_PIN);
    gpio_set_dir(RS485_DE_PIN, GPIO_OUT);
    gpio_put(RS485_DE_PIN, 0);  // Tri-state transmitter

    // Initialize RE pin (output, initially low = receive enabled)
    gpio_init(RS485_RE_PIN);
    gpio_set_dir(RS485_RE_PIN, GPIO_OUT);
    gpio_put(RS485_RE_PIN, 0);  // Receiver enabled

    printf("[GPIO] RS-485 control pins initialized (DE=%d, RE=%d)\n",
           RS485_DE_PIN, RS485_RE_PIN);
}

/**
 * @brief Initialize address selection pins (ADDR[2:0])
 *
 * These pins are configured as inputs with pull-ups. External resistors
 * can pull them high (1) or low (0) to set the device address.
 */
static void gpio_init_address_pins(void) {
    gpio_init(ADDR0_PIN);
    gpio_set_dir(ADDR0_PIN, GPIO_IN);
    gpio_pull_up(ADDR0_PIN);  // Default to high if not externally pulled

    gpio_init(ADDR1_PIN);
    gpio_set_dir(ADDR1_PIN, GPIO_IN);
    gpio_pull_up(ADDR1_PIN);

    gpio_init(ADDR2_PIN);
    gpio_set_dir(ADDR2_PIN, GPIO_IN);
    gpio_pull_up(ADDR2_PIN);

    printf("[GPIO] Address pins initialized (ADDR0=%d, ADDR1=%d, ADDR2=%d)\n",
           ADDR0_PIN, ADDR1_PIN, ADDR2_PIN);
}

/**
 * @brief Initialize fault and reset pins
 *
 * FAULT: Open-drain output (active low), indicates fault condition
 * RESET: Input with pull-up (active low), allows external reset
 */
static void gpio_init_fault_reset(void) {
    // FAULT pin: Configure as output, initially high (no fault)
    gpio_init(FAULT_PIN);
    gpio_set_dir(FAULT_PIN, GPIO_OUT);
    gpio_put(FAULT_PIN, 1);  // No fault (active low)

    // TODO: Configure as open-drain if needed (requires additional setup)

    // RESET pin: Input with pull-up (active low)
    gpio_init(RESET_PIN);
    gpio_set_dir(RESET_PIN, GPIO_IN);
    gpio_pull_up(RESET_PIN);

    printf("[GPIO] Fault/Reset pins initialized (FAULT=%d, RESET=%d)\n",
           FAULT_PIN, RESET_PIN);
}

/**
 * @brief Initialize status LEDs
 *
 * Heartbeat LED on GPIO 25 (onboard LED)
 * Optional external LEDs if ENABLE_EXTERNAL_LEDS is set
 */
static void gpio_init_leds(void) {
    // Heartbeat LED (onboard)
    gpio_init(LED_HEARTBEAT_PIN);
    gpio_set_dir(LED_HEARTBEAT_PIN, GPIO_OUT);
    gpio_put(LED_HEARTBEAT_PIN, 0);  // Initially off

#if ENABLE_EXTERNAL_LEDS
    // RS-485 activity LED
    gpio_init(LED_RS485_ACTIVE_PIN);
    gpio_set_dir(LED_RS485_ACTIVE_PIN, GPIO_OUT);
    gpio_put(LED_RS485_ACTIVE_PIN, 0);

    // Fault LED
    gpio_init(LED_FAULT_PIN);
    gpio_set_dir(LED_FAULT_PIN, GPIO_OUT);
    gpio_put(LED_FAULT_PIN, 0);

    // Mode LED
    gpio_init(LED_MODE_PIN);
    gpio_set_dir(LED_MODE_PIN, GPIO_OUT);
    gpio_put(LED_MODE_PIN, 0);

    printf("[GPIO] LEDs initialized (heartbeat=%d, rs485=%d, fault=%d, mode=%d)\n",
           LED_HEARTBEAT_PIN, LED_RS485_ACTIVE_PIN, LED_FAULT_PIN, LED_MODE_PIN);
#else
    printf("[GPIO] Heartbeat LED initialized (pin=%d)\n", LED_HEARTBEAT_PIN);
#endif
}

/**
 * @brief Initialize all GPIOs for the emulator
 *
 * Call this once during startup before configuring peripherals.
 */
void gpio_init_all(void) {
    printf("[GPIO] Initializing all GPIO pins...\n");

    gpio_init_rs485_control();
    gpio_init_address_pins();
    gpio_init_fault_reset();
    gpio_init_leds();

    printf("[GPIO] All GPIOs initialized successfully\n");
}

/**
 * @brief Read device address from ADDR[2:0] pins
 *
 * @return Device address (0-7)
 */
uint8_t gpio_read_address(void) {
    uint8_t addr = 0;

    if (gpio_get(ADDR0_PIN)) addr |= 0x01;
    if (gpio_get(ADDR1_PIN)) addr |= 0x02;
    if (gpio_get(ADDR2_PIN)) addr |= 0x04;

    return addr;
}

/**
 * @brief Set RS-485 to transmit mode
 *
 * Call this before sending data on the UART.
 * DE goes high, RE goes high (disables receiver to avoid echo).
 */
void gpio_rs485_tx_enable(void) {
    gpio_put(RS485_DE_PIN, 1);  // Enable driver
    gpio_put(RS485_RE_PIN, 1);  // Disable receiver (optional, prevents echo)
}

/**
 * @brief Set RS-485 to receive mode
 *
 * Call this after sending data to tri-state the transmitter.
 * DE goes low, RE goes low (enables receiver).
 */
void gpio_rs485_rx_enable(void) {
    gpio_put(RS485_DE_PIN, 0);  // Tri-state driver
    gpio_put(RS485_RE_PIN, 0);  // Enable receiver
}

/**
 * @brief Set fault pin state
 *
 * @param fault_active true to assert fault (pin low), false to clear (pin high)
 */
void gpio_set_fault(bool fault_active) {
    gpio_put(FAULT_PIN, !fault_active);  // Active low
}

/**
 * @brief Read reset pin state
 *
 * @return true if reset is asserted (pin low), false otherwise
 */
bool gpio_is_reset_asserted(void) {
    return !gpio_get(RESET_PIN);  // Active low
}

/**
 * @brief Set heartbeat LED state
 *
 * @param on true to turn LED on, false to turn off
 */
void gpio_set_heartbeat_led(bool on) {
    gpio_put(LED_HEARTBEAT_PIN, on);
}

#if ENABLE_EXTERNAL_LEDS
/**
 * @brief Set RS-485 activity LED state
 *
 * @param on true to turn LED on, false to turn off
 */
void gpio_set_rs485_led(bool on) {
    gpio_put(LED_RS485_ACTIVE_PIN, on);
}

/**
 * @brief Set fault LED state
 *
 * @param on true to turn LED on, false to turn off
 */
void gpio_set_fault_led(bool on) {
    gpio_put(LED_FAULT_PIN, on);
}

/**
 * @brief Set mode LED state
 *
 * @param on true to turn LED on, false to turn off
 */
void gpio_set_mode_led(bool on) {
    gpio_put(LED_MODE_PIN, on);
}
#endif // ENABLE_EXTERNAL_LEDS
