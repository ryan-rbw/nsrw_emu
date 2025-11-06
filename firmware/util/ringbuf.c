/**
 * @file ringbuf.c
 * @brief Lock-Free SPSC Ring Buffer Implementation
 *
 * Uses memory barriers to ensure proper ordering across RP2040 cores.
 * No locks or mutexes required for single producer, single consumer.
 */

#include "ringbuf.h"
#include "pico/stdlib.h"    // For __compiler_memory_barrier()
#include <string.h>

// ============================================================================
// Memory Barriers
// ============================================================================

/**
 * @brief Compiler memory barrier (prevents reordering)
 *
 * On RP2040 (Cortex-M0+), the hardware ensures sequential consistency
 * for normal memory accesses, but we still need compiler barriers to
 * prevent the compiler from reordering our volatile accesses.
 */
#define memory_barrier() __compiler_memory_barrier()

// ============================================================================
// Initialization
// ============================================================================

bool ringbuf_init(ringbuf_t *rb, uint32_t size) {
    if (rb == NULL) {
        return false;
    }

    // Validate size is power of 2
    if (!is_power_of_2(size)) {
        return false;
    }

    // Validate size is within limits
    if (size == 0 || size > RINGBUF_MAX_SIZE) {
        return false;
    }

    // Initialize structure
    rb->head = 0;
    rb->tail = 0;
    rb->size = size;
    rb->mask = size - 1;  // For fast modulo: index & mask

    // Zero out buffer (optional, but good practice)
    memset((void*)rb->buffer, 0, sizeof(rb->buffer));

    return true;
}

// ============================================================================
// Producer Operations (Core1)
// ============================================================================

bool ringbuf_push(ringbuf_t *rb, uint32_t item) {
    if (rb == NULL) {
        return false;
    }

    // Read current indices
    uint32_t head = rb->head;
    uint32_t tail = rb->tail;  // Read tail with acquire semantics (volatile)
    memory_barrier();

    // Calculate next head position
    uint32_t next_head = (head + 1) & rb->mask;

    // Check if buffer is full
    // Buffer is full when next_head would equal tail
    if (next_head == tail) {
        return false;  // Buffer full
    }

    // Write data to current head position
    rb->buffer[head] = item;

    // Release barrier: ensure data write completes before head update
    memory_barrier();

    // Update head (visible to consumer)
    rb->head = next_head;

    return true;
}

// ============================================================================
// Consumer Operations (Core0)
// ============================================================================

bool ringbuf_pop(ringbuf_t *rb, uint32_t *item) {
    if (rb == NULL || item == NULL) {
        return false;
    }

    // Read current indices
    uint32_t head = rb->head;  // Read head with acquire semantics (volatile)
    uint32_t tail = rb->tail;
    memory_barrier();

    // Check if buffer is empty
    // Buffer is empty when head equals tail
    if (head == tail) {
        return false;  // Buffer empty
    }

    // Read data from current tail position
    *item = rb->buffer[tail];

    // Release barrier: ensure data read completes before tail update
    memory_barrier();

    // Update tail (visible to producer)
    rb->tail = (tail + 1) & rb->mask;

    return true;
}

// ============================================================================
// Query Operations
// ============================================================================

bool ringbuf_is_empty(const ringbuf_t *rb) {
    if (rb == NULL) {
        return true;
    }

    uint32_t head = rb->head;
    uint32_t tail = rb->tail;
    memory_barrier();

    return (head == tail);
}

bool ringbuf_is_full(const ringbuf_t *rb) {
    if (rb == NULL) {
        return false;
    }

    uint32_t head = rb->head;
    uint32_t tail = rb->tail;
    memory_barrier();

    uint32_t next_head = (head + 1) & rb->mask;
    return (next_head == tail);
}

uint32_t ringbuf_count(const ringbuf_t *rb) {
    if (rb == NULL) {
        return 0;
    }

    uint32_t head = rb->head;
    uint32_t tail = rb->tail;
    memory_barrier();

    // Handle wraparound: (head - tail) & mask
    return (head - tail) & rb->mask;
}

uint32_t ringbuf_available(const ringbuf_t *rb) {
    if (rb == NULL) {
        return 0;
    }

    // Available = size - count - 1 (we sacrifice one slot to detect full vs empty)
    uint32_t count = ringbuf_count(rb);
    return rb->size - count - 1;
}

void ringbuf_reset(ringbuf_t *rb) {
    if (rb == NULL) {
        return;
    }

    // WARNING: Not thread-safe! Only call when buffer is not in use.
    rb->head = 0;
    rb->tail = 0;
    memory_barrier();
}
