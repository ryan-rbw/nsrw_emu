/**
 * @file unaligned.h
 * @brief Safe Unaligned Memory Access Utilities
 *
 * Provides safe functions for reading/writing multi-byte values from/to
 * potentially unaligned memory addresses. Critical for ARM Cortex-M0+ which
 * does not support unaligned memory access in hardware.
 *
 * All functions handle little-endian byte order explicitly for protocol
 * compatibility and cross-platform consistency.
 *
 * Usage:
 *   uint8_t buffer[10];
 *   write_u32_le(&buffer[1], 0x12345678);  // Safe even if buffer[1] is unaligned
 *   uint32_t val = read_u32_le(&buffer[1]);
 */

#ifndef UNALIGNED_H
#define UNALIGNED_H

#include <stdint.h>

// ============================================================================
// Little-Endian Read Functions (Safe for Unaligned Access)
// ============================================================================

/**
 * @brief Read uint8_t (always aligned)
 *
 * @param ptr Pointer to data
 * @return 8-bit value
 */
static inline uint8_t read_u8(const void* ptr) {
    return *(const uint8_t*)ptr;
}

/**
 * @brief Read uint16_t from potentially unaligned address (little-endian)
 *
 * @param ptr Pointer to 2-byte data (little-endian)
 * @return 16-bit value in host byte order
 */
static inline uint16_t read_u16_le(const void* ptr) {
    const uint8_t* p = (const uint8_t*)ptr;
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

/**
 * @brief Read uint32_t from potentially unaligned address (little-endian)
 *
 * @param ptr Pointer to 4-byte data (little-endian)
 * @return 32-bit value in host byte order
 */
static inline uint32_t read_u32_le(const void* ptr) {
    const uint8_t* p = (const uint8_t*)ptr;
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/**
 * @brief Read uint64_t from potentially unaligned address (little-endian)
 *
 * @param ptr Pointer to 8-byte data (little-endian)
 * @return 64-bit value in host byte order
 */
static inline uint64_t read_u64_le(const void* ptr) {
    const uint8_t* p = (const uint8_t*)ptr;
    return (uint64_t)p[0] | ((uint64_t)p[1] << 8) |
           ((uint64_t)p[2] << 16) | ((uint64_t)p[3] << 24) |
           ((uint64_t)p[4] << 32) | ((uint64_t)p[5] << 40) |
           ((uint64_t)p[6] << 48) | ((uint64_t)p[7] << 56);
}

// ============================================================================
// Big-Endian Read Functions (Safe for Unaligned Access)
// ============================================================================

/**
 * @brief Read uint16_t from potentially unaligned address (big-endian)
 *
 * @param ptr Pointer to 2-byte data (big-endian)
 * @return 16-bit value in host byte order
 */
static inline uint16_t read_u16_be(const void* ptr) {
    const uint8_t* p = (const uint8_t*)ptr;
    return ((uint16_t)p[0] << 8) | (uint16_t)p[1];
}

/**
 * @brief Read uint32_t from potentially unaligned address (big-endian)
 *
 * @param ptr Pointer to 4-byte data (big-endian)
 * @return 32-bit value in host byte order
 */
static inline uint32_t read_u32_be(const void* ptr) {
    const uint8_t* p = (const uint8_t*)ptr;
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
           ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

// ============================================================================
// Little-Endian Write Functions (Safe for Unaligned Access)
// ============================================================================

/**
 * @brief Write uint8_t (always aligned)
 *
 * @param ptr Pointer to destination
 * @param value 8-bit value to write
 */
static inline void write_u8(void* ptr, uint8_t value) {
    *(uint8_t*)ptr = value;
}

/**
 * @brief Write uint16_t to potentially unaligned address (little-endian)
 *
 * @param ptr Pointer to 2-byte destination
 * @param value 16-bit value to write
 */
static inline void write_u16_le(void* ptr, uint16_t value) {
    uint8_t* p = (uint8_t*)ptr;
    p[0] = (uint8_t)(value & 0xFF);
    p[1] = (uint8_t)((value >> 8) & 0xFF);
}

/**
 * @brief Write uint32_t to potentially unaligned address (little-endian)
 *
 * @param ptr Pointer to 4-byte destination
 * @param value 32-bit value to write
 */
static inline void write_u32_le(void* ptr, uint32_t value) {
    uint8_t* p = (uint8_t*)ptr;
    p[0] = (uint8_t)(value & 0xFF);
    p[1] = (uint8_t)((value >> 8) & 0xFF);
    p[2] = (uint8_t)((value >> 16) & 0xFF);
    p[3] = (uint8_t)((value >> 24) & 0xFF);
}

/**
 * @brief Write uint64_t to potentially unaligned address (little-endian)
 *
 * @param ptr Pointer to 8-byte destination
 * @param value 64-bit value to write
 */
static inline void write_u64_le(void* ptr, uint64_t value) {
    uint8_t* p = (uint8_t*)ptr;
    p[0] = (uint8_t)(value & 0xFF);
    p[1] = (uint8_t)((value >> 8) & 0xFF);
    p[2] = (uint8_t)((value >> 16) & 0xFF);
    p[3] = (uint8_t)((value >> 24) & 0xFF);
    p[4] = (uint8_t)((value >> 32) & 0xFF);
    p[5] = (uint8_t)((value >> 40) & 0xFF);
    p[6] = (uint8_t)((value >> 48) & 0xFF);
    p[7] = (uint8_t)((value >> 56) & 0xFF);
}

// ============================================================================
// Big-Endian Write Functions (Safe for Unaligned Access)
// ============================================================================

/**
 * @brief Write uint16_t to potentially unaligned address (big-endian)
 *
 * @param ptr Pointer to 2-byte destination
 * @param value 16-bit value to write
 */
static inline void write_u16_be(void* ptr, uint16_t value) {
    uint8_t* p = (uint8_t*)ptr;
    p[0] = (uint8_t)((value >> 8) & 0xFF);
    p[1] = (uint8_t)(value & 0xFF);
}

/**
 * @brief Write uint32_t to potentially unaligned address (big-endian)
 *
 * @param ptr Pointer to 4-byte destination
 * @param value 32-bit value to write
 */
static inline void write_u32_be(void* ptr, uint32_t value) {
    uint8_t* p = (uint8_t*)ptr;
    p[0] = (uint8_t)((value >> 24) & 0xFF);
    p[1] = (uint8_t)((value >> 16) & 0xFF);
    p[2] = (uint8_t)((value >> 8) & 0xFF);
    p[3] = (uint8_t)(value & 0xFF);
}

// ============================================================================
// Alignment Check Utilities (Debug/Development)
// ============================================================================

/**
 * @brief Check if pointer is aligned to specified boundary
 *
 * @param ptr Pointer to check
 * @param alignment Required alignment (must be power of 2)
 * @return true if pointer is properly aligned
 */
static inline int is_aligned(const void* ptr, size_t alignment) {
    return (((uintptr_t)ptr) & (alignment - 1)) == 0;
}

/**
 * @brief Get alignment offset needed to reach next aligned address
 *
 * @param ptr Pointer to check
 * @param alignment Required alignment (must be power of 2)
 * @return Number of bytes to add to reach alignment (0 if already aligned)
 */
static inline size_t alignment_offset(const void* ptr, size_t alignment) {
    uintptr_t addr = (uintptr_t)ptr;
    uintptr_t misalignment = addr & (alignment - 1);
    return misalignment ? (alignment - misalignment) : 0;
}

// ============================================================================
// Debug Assertions (Enabled only in DEBUG builds)
// ============================================================================

#ifdef DEBUG
#include <assert.h>

#define ASSERT_ALIGNED(ptr, align) \
    assert(is_aligned((ptr), (align)) && "Unaligned pointer access detected")

#define ASSERT_ALIGNED_U16(ptr) ASSERT_ALIGNED((ptr), 2)
#define ASSERT_ALIGNED_U32(ptr) ASSERT_ALIGNED((ptr), 4)
#define ASSERT_ALIGNED_U64(ptr) ASSERT_ALIGNED((ptr), 8)

#else

#define ASSERT_ALIGNED(ptr, align) ((void)0)
#define ASSERT_ALIGNED_U16(ptr) ((void)0)
#define ASSERT_ALIGNED_U32(ptr) ((void)0)
#define ASSERT_ALIGNED_U64(ptr) ((void)0)

#endif

#endif // UNALIGNED_H
