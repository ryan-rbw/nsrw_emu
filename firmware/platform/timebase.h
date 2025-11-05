/**
 * @file timebase.h
 * @brief Timebase Management API for NRWA-T6 Emulator
 */

#ifndef TIMEBASE_H
#define TIMEBASE_H

#include <stdint.h>

/**
 * @brief Callback function type for physics tick
 *
 * This function is called from the hardware alarm ISR at 100 Hz.
 * Keep it short and avoid blocking operations.
 */
typedef void (*timebase_tick_callback_t)(void);

/**
 * @brief Initialize timebase with hardware alarm
 *
 * Sets up a repeating alarm at 100 Hz for physics simulation.
 * Call this during startup before starting the timer.
 *
 * @param callback Function to call on each tick (can be NULL)
 */
void timebase_init(timebase_tick_callback_t callback);

/**
 * @brief Start the physics tick timer
 *
 * Begins generating ticks at 100 Hz. The callback registered in
 * timebase_init() will be called on each tick.
 */
void timebase_start(void);

/**
 * @brief Stop the physics tick timer
 *
 * Stops generating ticks. Can be restarted with timebase_start().
 */
void timebase_stop(void);

/**
 * @brief Get current tick count
 *
 * @return Number of ticks since timebase_start() was called
 */
uint32_t timebase_get_tick_count(void);

/**
 * @brief Get current time in microseconds
 *
 * Uses the RP2040 hardware timer (64-bit, 1 MHz).
 *
 * @return Microseconds since boot
 */
uint64_t timebase_get_us(void);

/**
 * @brief Get current time in milliseconds
 *
 * @return Milliseconds since boot
 */
uint32_t timebase_get_ms(void);

/**
 * @brief Get maximum observed jitter in microseconds
 *
 * Tracks the worst-case deviation from the expected 10ms tick period.
 *
 * @return Maximum jitter since timebase_start() was called
 */
uint32_t timebase_get_max_jitter_us(void);

/**
 * @brief Reset jitter statistics
 *
 * Clears the maximum jitter counter. Useful for re-measuring after
 * a known disturbance (e.g., USB enumeration).
 */
void timebase_reset_jitter_stats(void);

/**
 * @brief Busy-wait delay in microseconds
 *
 * Uses hardware timer for accurate delays. Blocks the CPU.
 *
 * @param us Microseconds to delay
 */
void timebase_delay_us(uint32_t us);

/**
 * @brief Busy-wait delay in milliseconds
 *
 * Blocks the CPU for the specified time.
 *
 * @param ms Milliseconds to delay
 */
void timebase_delay_ms(uint32_t ms);

#endif // TIMEBASE_H
