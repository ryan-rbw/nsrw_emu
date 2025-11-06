/**
 * @file ringbuf.h
 * @brief Lock-Free SPSC Ring Buffer for Inter-Core Communication
 *
 * Single Producer, Single Consumer (SPSC) ring buffer designed for
 * communication between RP2040 cores without locks or mutexes.
 *
 * Design:
 * - Power-of-2 size for fast modulo operations (bitwise AND)
 * - Separate read/write indices with memory barriers
 * - Producer (Core1) only writes head index
 * - Consumer (Core0) only writes tail index
 * - No locks required - naturally thread-safe for SPSC
 *
 * Usage:
 *   ringbuf_t rb;
 *   ringbuf_init(&rb, 256);  // Must be power of 2
 *
 *   // Producer (Core1):
 *   uint32_t data = 42;
 *   ringbuf_push(&rb, data);
 *
 *   // Consumer (Core0):
 *   uint32_t value;
 *   if (ringbuf_pop(&rb, &value)) {
 *       // Process value
 *   }
 */

#ifndef RINGBUF_H
#define RINGBUF_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// ============================================================================
// Ring Buffer Configuration
// ============================================================================

/** @brief Maximum ring buffer size (must be power of 2) */
#define RINGBUF_MAX_SIZE 256

// ============================================================================
// Ring Buffer Structure
// ============================================================================

/**
 * @brief Lock-free SPSC ring buffer
 *
 * IMPORTANT: Size must be a power of 2 for fast modulo via bitwise AND.
 *
 * Memory ordering:
 * - Producer writes to head with release semantics
 * - Consumer reads head with acquire semantics
 * - Consumer writes to tail with release semantics
 * - Producer reads tail with acquire semantics
 */
typedef struct {
    uint32_t buffer[RINGBUF_MAX_SIZE];  /**< Data storage (fixed size for simplicity) */
    volatile uint32_t head;              /**< Write index (modified by producer only) */
    volatile uint32_t tail;              /**< Read index (modified by consumer only) */
    uint32_t size;                       /**< Buffer size (must be power of 2) */
    uint32_t mask;                       /**< Size - 1, for fast modulo (index & mask) */
} ringbuf_t;

// ============================================================================
// Ring Buffer API
// ============================================================================

/**
 * @brief Initialize ring buffer
 *
 * @param rb Pointer to ring buffer structure
 * @param size Buffer size (MUST be power of 2, max RINGBUF_MAX_SIZE)
 * @return true on success, false if size is invalid
 */
bool ringbuf_init(ringbuf_t *rb, uint32_t size);

/**
 * @brief Push item into ring buffer (producer side)
 *
 * Thread-safe for single producer. Non-blocking.
 *
 * @param rb Pointer to ring buffer
 * @param item Data to push
 * @return true if pushed, false if buffer is full
 */
bool ringbuf_push(ringbuf_t *rb, uint32_t item);

/**
 * @brief Pop item from ring buffer (consumer side)
 *
 * Thread-safe for single consumer. Non-blocking.
 *
 * @param rb Pointer to ring buffer
 * @param item Pointer to receive popped data
 * @return true if popped, false if buffer is empty
 */
bool ringbuf_pop(ringbuf_t *rb, uint32_t *item);

/**
 * @brief Check if ring buffer is empty
 *
 * @param rb Pointer to ring buffer
 * @return true if empty, false otherwise
 */
bool ringbuf_is_empty(const ringbuf_t *rb);

/**
 * @brief Check if ring buffer is full
 *
 * @param rb Pointer to ring buffer
 * @return true if full, false otherwise
 */
bool ringbuf_is_full(const ringbuf_t *rb);

/**
 * @brief Get number of items currently in buffer
 *
 * @param rb Pointer to ring buffer
 * @return Number of items in buffer
 */
uint32_t ringbuf_count(const ringbuf_t *rb);

/**
 * @brief Get available space in buffer
 *
 * @param rb Pointer to ring buffer
 * @return Number of free slots
 */
uint32_t ringbuf_available(const ringbuf_t *rb);

/**
 * @brief Reset ring buffer to empty state
 *
 * WARNING: Not thread-safe! Should only be called when buffer is not in use.
 *
 * @param rb Pointer to ring buffer
 */
void ringbuf_reset(ringbuf_t *rb);

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Check if a value is a power of 2
 *
 * @param x Value to check
 * @return true if power of 2, false otherwise
 */
static inline bool is_power_of_2(uint32_t x) {
    return (x != 0) && ((x & (x - 1)) == 0);
}

#endif // RINGBUF_H
