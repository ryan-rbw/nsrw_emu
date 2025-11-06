/**
 * @file fixedpoint.h
 * @brief Fixed-Point Math Library (Header-Only)
 *
 * Unsigned fixed-point formats for physics calculations on RP2040.
 * All types are uint32_t with different integer/fractional bit splits.
 *
 * Formats:
 * - UQ14.18: Speed (RPM) - Range: 0 to 16383.999996, Resolution: 0.0000038 RPM
 * - UQ16.16: Voltage (V) - Range: 0 to 65535.999985, Resolution: 0.000015 V
 * - UQ18.14: Torque (mN·m), Current (mA), Power (mW) - Range: 0 to 262143.999939, Resolution: 0.000061
 *
 * Example:
 *   float speed_rpm = 3000.0f;
 *   uq14_18_t speed_fixed = float_to_uq14_18(speed_rpm);
 *   float speed_back = uq14_18_to_float(speed_fixed);
 */

#ifndef FIXEDPOINT_H
#define FIXEDPOINT_H

#include <stdint.h>
#include <stdbool.h>
#include <math.h>

// ============================================================================
// Fixed-Point Type Definitions
// ============================================================================

/** @brief UQ14.18: 14 integer bits, 18 fractional bits (for speed in RPM) */
typedef uint32_t uq14_18_t;

/** @brief UQ16.16: 16 integer bits, 16 fractional bits (for voltage in V) */
typedef uint32_t uq16_16_t;

/** @brief UQ18.14: 18 integer bits, 14 fractional bits (for torque, current, power) */
typedef uint32_t uq18_14_t;

// ============================================================================
// Format Constants
// ============================================================================

// UQ14.18 (Speed)
#define UQ14_18_FRAC_BITS   18
#define UQ14_18_INT_BITS    14
#define UQ14_18_ONE         (1U << UQ14_18_FRAC_BITS)  // 262144
#define UQ14_18_MAX         UINT32_MAX                  // Full 32-bit range
#define UQ14_18_MAX_INT     ((1U << UQ14_18_INT_BITS) - 1)  // 16383

// UQ16.16 (Voltage)
#define UQ16_16_FRAC_BITS   16
#define UQ16_16_INT_BITS    16
#define UQ16_16_ONE         (1U << UQ16_16_FRAC_BITS)  // 65536
#define UQ16_16_MAX         UINT32_MAX                  // Full 32-bit range
#define UQ16_16_MAX_INT     ((1U << UQ16_16_INT_BITS) - 1)  // 65535

// UQ18.14 (Torque, Current, Power)
#define UQ18_14_FRAC_BITS   14
#define UQ18_14_INT_BITS    18
#define UQ18_14_ONE         (1U << UQ18_14_FRAC_BITS)  // 16384
#define UQ18_14_MAX         UINT32_MAX                  // Full 32-bit range
#define UQ18_14_MAX_INT     ((1U << UQ18_14_INT_BITS) - 1)  // 262143

// ============================================================================
// Conversion Functions: Float ↔ Fixed-Point
// ============================================================================

/**
 * @brief Convert float to UQ14.18 (Speed in RPM)
 *
 * @param f Float value (0.0 to 16383.999996)
 * @return Fixed-point value, saturated to UQ14_18_MAX
 */
static inline uq14_18_t float_to_uq14_18(float f) {
    if (f <= 0.0f) {
        return 0;
    }
    if (f >= (float)UQ14_18_MAX_INT) {
        return UQ14_18_MAX;  // Saturate
    }
    return (uq14_18_t)(f * (float)UQ14_18_ONE + 0.5f);  // Round to nearest
}

/**
 * @brief Convert UQ14.18 to float
 *
 * @param x Fixed-point value
 * @return Float value
 */
static inline float uq14_18_to_float(uq14_18_t x) {
    return (float)x / (float)UQ14_18_ONE;
}

/**
 * @brief Convert float to UQ16.16 (Voltage in V)
 *
 * @param f Float value (0.0 to 65535.999985)
 * @return Fixed-point value, saturated to UQ16_16_MAX
 */
static inline uq16_16_t float_to_uq16_16(float f) {
    if (f <= 0.0f) {
        return 0;
    }
    if (f >= (float)UQ16_16_MAX_INT) {
        return UQ16_16_MAX;  // Saturate
    }
    return (uq16_16_t)(f * (float)UQ16_16_ONE + 0.5f);  // Round to nearest
}

/**
 * @brief Convert UQ16.16 to float
 *
 * @param x Fixed-point value
 * @return Float value
 */
static inline float uq16_16_to_float(uq16_16_t x) {
    return (float)x / (float)UQ16_16_ONE;
}

/**
 * @brief Convert float to UQ18.14 (Torque, Current, Power)
 *
 * @param f Float value (0.0 to 262143.999939)
 * @return Fixed-point value, saturated to UQ18_14_MAX
 */
static inline uq18_14_t float_to_uq18_14(float f) {
    if (f <= 0.0f) {
        return 0;
    }
    if (f >= (float)UQ18_14_MAX_INT) {
        return UQ18_14_MAX;  // Saturate
    }
    return (uq18_14_t)(f * (float)UQ18_14_ONE + 0.5f);  // Round to nearest
}

/**
 * @brief Convert UQ18.14 to float
 *
 * @param x Fixed-point value
 * @return Float value
 */
static inline float uq18_14_to_float(uq18_14_t x) {
    return (float)x / (float)UQ18_14_ONE;
}

// ============================================================================
// Arithmetic Operations: UQ14.18 (Speed)
// ============================================================================

/**
 * @brief Add two UQ14.18 values with saturation
 *
 * @param a First operand
 * @param b Second operand
 * @return Sum, saturated to UQ14_18_MAX
 */
static inline uq14_18_t uq14_18_add(uq14_18_t a, uq14_18_t b) {
    uint32_t result = a + b;
    if (result < a) {  // Overflow detection
        return UQ14_18_MAX;
    }
    return result;
}

/**
 * @brief Subtract two UQ14.18 values with saturation to zero
 *
 * @param a Minuend
 * @param b Subtrahend
 * @return Difference, saturated to 0 if b > a
 */
static inline uq14_18_t uq14_18_sub(uq14_18_t a, uq14_18_t b) {
    if (b > a) {
        return 0;  // Saturate to zero (unsigned)
    }
    return a - b;
}

/**
 * @brief Multiply two UQ14.18 values
 *
 * Result is shifted right by FRAC_BITS to maintain format.
 * Saturates to UQ14_18_MAX on overflow.
 *
 * @param a First operand
 * @param b Second operand
 * @return Product, saturated to UQ14_18_MAX
 */
static inline uq14_18_t uq14_18_mul(uq14_18_t a, uq14_18_t b) {
    uint64_t product = (uint64_t)a * (uint64_t)b;
    uint64_t result = product >> UQ14_18_FRAC_BITS;

    if (result > UQ14_18_MAX) {
        return UQ14_18_MAX;  // Saturate
    }
    return (uq14_18_t)result;
}

// ============================================================================
// Arithmetic Operations: UQ16.16 (Voltage)
// ============================================================================

/**
 * @brief Add two UQ16.16 values with saturation
 *
 * @param a First operand
 * @param b Second operand
 * @return Sum, saturated to UQ16_16_MAX
 */
static inline uq16_16_t uq16_16_add(uq16_16_t a, uq16_16_t b) {
    uint32_t result = a + b;
    if (result < a) {  // Overflow detection
        return UQ16_16_MAX;
    }
    return result;
}

/**
 * @brief Subtract two UQ16.16 values with saturation to zero
 *
 * @param a Minuend
 * @param b Subtrahend
 * @return Difference, saturated to 0 if b > a
 */
static inline uq16_16_t uq16_16_sub(uq16_16_t a, uq16_16_t b) {
    if (b > a) {
        return 0;  // Saturate to zero (unsigned)
    }
    return a - b;
}

/**
 * @brief Multiply two UQ16.16 values
 *
 * @param a First operand
 * @param b Second operand
 * @return Product, saturated to UQ16_16_MAX
 */
static inline uq16_16_t uq16_16_mul(uq16_16_t a, uq16_16_t b) {
    uint64_t product = (uint64_t)a * (uint64_t)b;
    uint64_t result = product >> UQ16_16_FRAC_BITS;

    if (result > UQ16_16_MAX) {
        return UQ16_16_MAX;  // Saturate
    }
    return (uq16_16_t)result;
}

// ============================================================================
// Arithmetic Operations: UQ18.14 (Torque, Current, Power)
// ============================================================================

/**
 * @brief Add two UQ18.14 values with saturation
 *
 * @param a First operand
 * @param b Second operand
 * @return Sum, saturated to UQ18_14_MAX
 */
static inline uq18_14_t uq18_14_add(uq18_14_t a, uq18_14_t b) {
    uint32_t result = a + b;
    if (result < a) {  // Overflow detection
        return UQ18_14_MAX;
    }
    return result;
}

/**
 * @brief Subtract two UQ18.14 values with saturation to zero
 *
 * @param a Minuend
 * @param b Subtrahend
 * @return Difference, saturated to 0 if b > a
 */
static inline uq18_14_t uq18_14_sub(uq18_14_t a, uq18_14_t b) {
    if (b > a) {
        return 0;  // Saturate to zero (unsigned)
    }
    return a - b;
}

/**
 * @brief Multiply two UQ18.14 values
 *
 * @param a First operand
 * @param b Second operand
 * @return Product, saturated to UQ18_14_MAX
 */
static inline uq18_14_t uq18_14_mul(uq18_14_t a, uq18_14_t b) {
    uint64_t product = (uint64_t)a * (uint64_t)b;
    uint64_t result = product >> UQ18_14_FRAC_BITS;

    if (result > UQ18_14_MAX) {
        return UQ18_14_MAX;  // Saturate
    }
    return (uq18_14_t)result;
}

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * @brief Get resolution (LSB value) of UQ14.18 format
 *
 * @return Resolution in float (approximately 0.0000038)
 */
static inline float uq14_18_resolution(void) {
    return 1.0f / (float)UQ14_18_ONE;
}

/**
 * @brief Get resolution (LSB value) of UQ16.16 format
 *
 * @return Resolution in float (approximately 0.000015)
 */
static inline float uq16_16_resolution(void) {
    return 1.0f / (float)UQ16_16_ONE;
}

/**
 * @brief Get resolution (LSB value) of UQ18.14 format
 *
 * @return Resolution in float (approximately 0.000061)
 */
static inline float uq18_14_resolution(void) {
    return 1.0f / (float)UQ18_14_ONE;
}

#endif // FIXEDPOINT_H
