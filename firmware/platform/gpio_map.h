/**
 * @file gpio_map.h
 * @brief GPIO Management API for NRWA-T6 Emulator
 */

#ifndef GPIO_MAP_H
#define GPIO_MAP_H

#include <stdint.h>
#include <stdbool.h>

/**
 * @brief Initialize all GPIOs for the emulator
 *
 * Must be called once during startup before using any GPIO functions.
 */
void gpio_init_all(void);

/**
 * @brief Read device address from ADDR[2:0] pins
 *
 * @return Device address (0-7)
 */
uint8_t gpio_read_address(void);

/**
 * @brief Set RS-485 to transmit mode
 *
 * Enables the driver (DE high) and disables receiver (RE high).
 * Call before writing to UART TX buffer.
 */
void gpio_rs485_tx_enable(void);

/**
 * @brief Set RS-485 to receive mode
 *
 * Tri-states the driver (DE low) and enables receiver (RE low).
 * Call after UART transmission completes.
 */
void gpio_rs485_rx_enable(void);

/**
 * @brief Set fault pin state
 *
 * @param fault_active true to assert fault (pin low), false to clear (pin high)
 */
void gpio_set_fault(bool fault_active);

/**
 * @brief Read reset pin state
 *
 * @return true if reset is asserted (pin low), false otherwise
 */
bool gpio_is_reset_asserted(void);

/**
 * @brief Set heartbeat LED state
 *
 * @param on true to turn LED on, false to turn off
 */
void gpio_set_heartbeat_led(bool on);

#if ENABLE_EXTERNAL_LEDS
/**
 * @brief Set RS-485 activity LED state
 *
 * @param on true to turn LED on, false to turn off
 */
void gpio_set_rs485_led(bool on);

/**
 * @brief Set fault LED state
 *
 * @param on true to turn LED on, false to turn off
 */
void gpio_set_fault_led(bool on);

/**
 * @brief Set mode LED state
 *
 * @param on true to turn LED on, false to turn off
 */
void gpio_set_mode_led(bool on);
#endif // ENABLE_EXTERNAL_LEDS

#endif // GPIO_MAP_H
