/**
 * @file nss_nrwa_t6_protection.h
 * @brief NSS NRWA-T6 Protection System - Threshold Management
 *
 * Manages protection thresholds and enable/disable flags.
 * Works with nss_nrwa_t6_model.c which implements the actual protection checks.
 *
 * Protection Categories:
 * - Hard protections: Immediate shutdown (overvoltage, hard overcurrent, overspeed fault)
 * - Soft protections: Warnings that can escalate (soft overcurrent, overspeed limit)
 * - Latching faults: Require CLEAR-FAULT to reset (overspeed at 6000 RPM)
 */

#ifndef NSS_NRWA_T6_PROTECTION_H
#define NSS_NRWA_T6_PROTECTION_H

#include <stdint.h>
#include <stdbool.h>
#include "nss_nrwa_t6_regs.h"
#include "nss_nrwa_t6_model.h"

// ============================================================================
// Protection Parameter IDs (for CONFIGURE-PROTECTION command)
// ============================================================================

/**
 * @brief Protection parameter identifiers
 *
 * Used by CONFIGURE-PROTECTION [0x0A] command to update protection thresholds.
 */
typedef enum {
    PROT_PARAM_OVERVOLTAGE_THRESHOLD    = 0x00,  // UQ16.16 V (default: 36V)
    PROT_PARAM_OVERSPEED_FAULT_RPM      = 0x01,  // UQ14.18 RPM (default: 6000 RPM, latching)
    PROT_PARAM_OVERSPEED_SOFT_RPM       = 0x02,  // UQ14.18 RPM (default: 5000 RPM, warning)
    PROT_PARAM_OVERPOWER_LIMIT_W        = 0x03,  // UQ18.14 mW (default: 100 W)
    PROT_PARAM_SOFT_OVERCURRENT_A       = 0x04,  // UQ18.14 mA (default: 6 A)
    PROT_PARAM_HARD_OVERCURRENT_A       = 0x05,  // UQ18.14 mA (default: 6 A, trips LCL)
    PROT_PARAM_BRAKING_LOAD_V           = 0x06,  // UQ16.16 V (default: 31V)
    PROT_PARAM_MAX_DUTY_CYCLE_PCT       = 0x07,  // UQ16.16 % (default: 97.85%)

    PROT_PARAM_COUNT  // Total number of parameters
} protection_param_t;

// ============================================================================
// Protection Defaults (per SPEC.md and RESET_FAULT_REQUIREMENTS.md)
// ============================================================================

// Hard Protection Defaults (immediate shutdown, trip LCL)
#define DEFAULT_OVERVOLTAGE_THRESHOLD_V     36.0f   // Bus overvoltage
#define DEFAULT_HARD_OVERCURRENT_A          6.0f    // Phase overcurrent (trips LCL)
#define DEFAULT_MAX_DUTY_CYCLE_PCT          97.85f  // PWM duty cycle limit
#define DEFAULT_OVERPOWER_LIMIT_W           100.0f  // Motor power limit

// Soft Protection Defaults (warnings → eventual fault)
#define DEFAULT_BRAKING_LOAD_V              31.0f   // Regenerative braking threshold
#define DEFAULT_SOFT_OVERCURRENT_A          6.0f    // Current warning threshold
#define DEFAULT_OVERSPEED_SOFT_RPM          5000.0f // Speed warning threshold

// Latching Fault Defaults (require CLEAR-FAULT to reset)
#define DEFAULT_OVERSPEED_FAULT_RPM         6000.0f // Hard fault, trips LCL

// ============================================================================
// Protection Configuration API
// ============================================================================

/**
 * @brief Initialize protection system with default thresholds
 *
 * Sets all protection thresholds to default values and enables all protections.
 * Called during wheel_model_init().
 *
 * @param state Wheel state structure
 */
void protection_init(wheel_state_t* state);

/**
 * @brief Update a protection parameter
 *
 * Used by CONFIGURE-PROTECTION [0x0A] command to dynamically adjust thresholds.
 *
 * @param state Wheel state structure
 * @param param_id Protection parameter ID (see protection_param_t)
 * @param value_fixed Fixed-point value (format depends on parameter)
 * @return true if successful, false if invalid parameter ID
 */
bool protection_set_parameter(wheel_state_t* state, uint8_t param_id, uint32_t value_fixed);

/**
 * @brief Get a protection parameter
 *
 * @param state Wheel state structure
 * @param param_id Protection parameter ID
 * @param value_fixed Output: fixed-point value
 * @return true if successful, false if invalid parameter ID
 */
bool protection_get_parameter(const wheel_state_t* state, uint8_t param_id, uint32_t* value_fixed);

/**
 * @brief Enable/disable a specific protection
 *
 * @param state Wheel state structure
 * @param prot_mask Protection enable mask (see PROT_ENABLE_* in nss_nrwa_t6_regs.h)
 * @param enable true to enable, false to disable
 */
void protection_set_enable(wheel_state_t* state, uint32_t prot_mask, bool enable);

/**
 * @brief Check if a protection is enabled
 *
 * @param state Wheel state structure
 * @param prot_mask Protection enable mask
 * @return true if enabled
 */
bool protection_is_enabled(const wheel_state_t* state, uint32_t prot_mask);

/**
 * @brief Restore all protection thresholds to defaults
 *
 * @param state Wheel state structure
 */
void protection_restore_defaults(wheel_state_t* state);

/**
 * @brief Get protection parameter name (for console/debug)
 *
 * @param param_id Protection parameter ID
 * @return Parameter name string, or "UNKNOWN" if invalid
 */
const char* protection_get_param_name(uint8_t param_id);

/**
 * @brief Get protection parameter units (for console/debug)
 *
 * @param param_id Protection parameter ID
 * @return Units string (e.g., "V", "RPM", "W")
 */
const char* protection_get_param_units(uint8_t param_id);

// ============================================================================
// Protection Status Query
// ============================================================================

/**
 * @brief Get human-readable fault name
 *
 * @param fault_bit Single fault bit (e.g., FAULT_OVERSPEED)
 * @return Fault name string
 */
const char* protection_get_fault_name(uint32_t fault_bit);

/**
 * @brief Format multiple fault names from bitmask
 *
 * Iterates through all fault bits and builds a comma-separated string
 * of active fault names (e.g., "OVERSPEED,OVERPOWER").
 *
 * @param fault_mask Fault bitmask (may have multiple bits set)
 * @param buf Output buffer for fault string
 * @param buf_size Size of output buffer
 * @return Number of faults found (0 if none)
 */
int protection_format_fault_string(uint32_t fault_mask, char* buf, size_t buf_size);

/**
 * @brief Check if fault is latching type
 *
 * Latching faults require CLEAR-FAULT to reset.
 *
 * @param fault_bit Single fault bit
 * @return true if latching
 */
bool protection_is_latching_fault(uint32_t fault_bit);

/**
 * @brief Check if fault trips LCL
 *
 * LCL-tripping faults disable motor and require hardware RESET to clear.
 *
 * @param fault_bit Single fault bit
 * @return true if fault trips LCL
 */
bool protection_trips_lcl(uint32_t fault_bit);

#endif // NSS_NRWA_T6_PROTECTION_H
