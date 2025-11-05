/**
 * @file timebase.c
 * @brief Timebase Management for NRWA-T6 Emulator
 *
 * Provides a 100 Hz hardware alarm for Core1 physics simulation and
 * microsecond-resolution timing for performance measurement.
 */

#include "board_pico.h"
#include "timebase.h"
#include "hardware/timer.h"
#include "hardware/irq.h"
#include <stdio.h>

// ============================================================================
// Static Variables
// ============================================================================

/** Callback function for physics tick (set by user) */
static timebase_tick_callback_t tick_callback = NULL;

/** Tick counter (increments at 100 Hz) */
static volatile uint32_t tick_count = 0;

/** Jitter measurement: last tick timestamp (microseconds) */
static volatile uint64_t last_tick_us = 0;

/** Jitter measurement: maximum observed jitter (microseconds) */
static volatile uint32_t max_jitter_us = 0;

/** Alarm number used for physics tick (use alarm 0) */
#define PHYSICS_ALARM_NUM 0

// ============================================================================
// Hardware Alarm ISR
// ============================================================================

/**
 * @brief Hardware alarm interrupt handler
 *
 * Fires at 100 Hz to trigger physics simulation tick.
 * Measures jitter and calls user callback.
 */
static void timebase_alarm_isr(void) {
    // Clear the alarm interrupt
    hw_clear_bits(&timer_hw->intr, 1u << PHYSICS_ALARM_NUM);

    // Measure jitter
    uint64_t now_us = time_us_64();
    if (last_tick_us != 0) {
        uint64_t expected_us = last_tick_us + PHYSICS_TICK_PERIOD_US;
        int64_t jitter = (int64_t)now_us - (int64_t)expected_us;
        uint32_t abs_jitter = (jitter < 0) ? -jitter : jitter;

        if (abs_jitter > max_jitter_us) {
            max_jitter_us = abs_jitter;
        }

        // Warn if jitter exceeds spec
        if (abs_jitter > MAX_TICK_JITTER_US) {
            // Note: Don't printf in ISR for production code
            // This is for debugging only
            // printf("[WARN] Tick jitter: %u us (max allowed: %u us)\n",
            //        abs_jitter, MAX_TICK_JITTER_US);
        }
    }
    last_tick_us = now_us;

    // Increment tick counter
    tick_count++;

    // Call user callback if registered
    if (tick_callback != NULL) {
        tick_callback();
    }

    // Re-arm the alarm for next tick (absolute time to avoid drift)
    uint64_t next_tick_us = last_tick_us + PHYSICS_TICK_PERIOD_US;
    timer_hw->alarm[PHYSICS_ALARM_NUM] = (uint32_t)next_tick_us;
}

// ============================================================================
// Public API
// ============================================================================

/**
 * @brief Initialize timebase with hardware alarm
 *
 * Sets up a repeating alarm at 100 Hz for physics simulation.
 *
 * @param callback Function to call on each tick (can be NULL)
 */
void timebase_init(timebase_tick_callback_t callback) {
    printf("[Timebase] Initializing 100 Hz hardware alarm...\n");

    tick_callback = callback;
    tick_count = 0;
    last_tick_us = 0;
    max_jitter_us = 0;

    // Enable timer interrupt
    hw_set_bits(&timer_hw->inte, 1u << PHYSICS_ALARM_NUM);
    irq_set_exclusive_handler(TIMER_IRQ_0 + PHYSICS_ALARM_NUM, timebase_alarm_isr);
    irq_set_enabled(TIMER_IRQ_0 + PHYSICS_ALARM_NUM, true);

    printf("[Timebase] Alarm configured (period=%u us, rate=%u Hz)\n",
           PHYSICS_TICK_PERIOD_US, PHYSICS_TICK_RATE_HZ);
}

/**
 * @brief Start the physics tick timer
 *
 * Call this after initialization to begin generating ticks.
 */
void timebase_start(void) {
    printf("[Timebase] Starting timer...\n");

    // Schedule first tick
    last_tick_us = time_us_64();
    uint64_t first_tick_us = last_tick_us + PHYSICS_TICK_PERIOD_US;
    timer_hw->alarm[PHYSICS_ALARM_NUM] = (uint32_t)first_tick_us;

    printf("[Timebase] Timer started (first tick in %u us)\n", PHYSICS_TICK_PERIOD_US);
}

/**
 * @brief Stop the physics tick timer
 */
void timebase_stop(void) {
    // Disable alarm interrupt
    hw_clear_bits(&timer_hw->inte, 1u << PHYSICS_ALARM_NUM);
    irq_set_enabled(TIMER_IRQ_0 + PHYSICS_ALARM_NUM, false);

    printf("[Timebase] Timer stopped (total ticks: %u)\n", tick_count);
}

/**
 * @brief Get current tick count
 *
 * @return Number of ticks since timebase_start() was called
 */
uint32_t timebase_get_tick_count(void) {
    return tick_count;
}

/**
 * @brief Get current time in microseconds
 *
 * Uses the RP2040 hardware timer (64-bit, 1 MHz).
 *
 * @return Microseconds since boot
 */
uint64_t timebase_get_us(void) {
    return time_us_64();
}

/**
 * @brief Get current time in milliseconds
 *
 * @return Milliseconds since boot
 */
uint32_t timebase_get_ms(void) {
    return (uint32_t)(time_us_64() / 1000);
}

/**
 * @brief Get maximum observed jitter in microseconds
 *
 * @return Maximum jitter since timebase_start() was called
 */
uint32_t timebase_get_max_jitter_us(void) {
    return max_jitter_us;
}

/**
 * @brief Reset jitter statistics
 */
void timebase_reset_jitter_stats(void) {
    max_jitter_us = 0;
}

/**
 * @brief Busy-wait delay in microseconds
 *
 * Uses hardware timer for accurate delays.
 *
 * @param us Microseconds to delay
 */
void timebase_delay_us(uint32_t us) {
    busy_wait_us(us);
}

/**
 * @brief Busy-wait delay in milliseconds
 *
 * @param ms Milliseconds to delay
 */
void timebase_delay_ms(uint32_t ms) {
    busy_wait_us_32(ms * 1000);
}
