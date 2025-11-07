/**
 * @file nss_nrwa_t6_model.h
 * @brief NSS NRWA-T6 Reaction Wheel Physics Model
 *
 * Implements dynamics simulation for the NRWA-T6 reaction wheel with:
 * - 4 control modes: CURRENT, SPEED, TORQUE, PWM
 * - Loss model: viscous + coulomb + copper losses
 * - Protection limits: power, current, duty cycle
 * - PI speed controller with anti-windup
 */

#ifndef NSS_NRWA_T6_MODEL_H
#define NSS_NRWA_T6_MODEL_H

#include <stdint.h>
#include <stdbool.h>
#include "nss_nrwa_t6_regs.h"
#include "fixedpoint.h"

// ============================================================================
// Physical Constants (from SPEC.md and ICD)
// ============================================================================

// Wheel inertia: I = 5.35e-5 kg·m² (from ICD)
#define WHEEL_INERTIA_KGM2      0.0000535f   // kg·m²

// Motor torque constant: k_t = 0.0534 N·m/A (from SPEC.md)
#define MOTOR_KT_NM_PER_A       0.0534f      // N·m/A

// Loss model coefficients (tuned for realistic behavior)
#define LOSS_VISCOUS_A          0.00001f     // a·ω (N·m·s/rad)
#define LOSS_COULOMB_B          0.0005f      // b·sign(ω) (N·m)
#define LOSS_COPPER_C           0.0001f      // c·i² (N·m/A²)

// Simulation timestep
#define MODEL_DT_MS             10           // 10 ms (100 Hz)
#define MODEL_DT_S              0.010f       // 10 ms in seconds

// Default protection limits (from SPEC.md)
#define DEFAULT_OVERVOLTAGE_V           36.0f      // V
#define DEFAULT_OVERSPEED_FAULT_RPM     6000.0f    // RPM (latched fault)
#define DEFAULT_OVERSPEED_SOFT_RPM      5000.0f    // RPM (warning)
#define DEFAULT_MAX_DUTY_CYCLE          97.85f     // %
#define DEFAULT_MOTOR_OVERPOWER_W       100.0f     // W
#define DEFAULT_SOFT_OVERCURRENT_A      6.0f       // A
#define DEFAULT_BRAKING_LOAD_V          31.0f      // V

// Default PI controller parameters (tuned for reasonable response)
#define DEFAULT_PI_KP                   0.05f      // Proportional gain
#define DEFAULT_PI_KI                   0.01f      // Integral gain
#define DEFAULT_PI_I_MAX_A              3.0f       // Integral windup limit (A)

// Conversion constants
#define RPM_TO_RAD_S                    0.10471975512f  // π/30
#define RAD_S_TO_RPM                    9.54929658551f  // 30/π

// ============================================================================
// Wheel State Structure
// ============================================================================

/**
 * @brief Reaction wheel state variables
 *
 * All internal calculations use float for simplicity and accuracy.
 * Fixed-point conversions happen only at register boundaries.
 */
typedef struct {
    // Dynamic state
    float omega_rad_s;          // Angular velocity (rad/s)
    float momentum_nms;         // Angular momentum H = I·ω (N·m·s)

    // Control command inputs
    float current_cmd_a;        // Commanded current (A)
    float torque_cmd_mnm;       // Commanded torque (mN·m)
    float speed_cmd_rpm;        // Commanded speed (RPM)
    float pwm_duty_pct;         // Commanded PWM duty cycle (%)

    // Output state
    float current_out_a;        // Actual current output (A)
    float torque_out_mnm;       // Output torque (mN·m)
    float power_w;              // Electrical power (W)
    float voltage_v;            // Bus voltage (V, simulated as ~28V)

    // Loss model components
    float torque_loss_mnm;      // Total loss torque (mN·m)
    float alpha_rad_s2;         // Angular acceleration (rad/s²)

    // PI controller state (for SPEED mode)
    float pi_error_integral;    // Integral error accumulator (RPM·s)
    float pi_output_a;          // PI controller output (A)

    // Control mode
    control_mode_t mode;        // Active control mode
    direction_t direction;      // Rotation direction

    // Protection thresholds (configurable via PEEK/POKE)
    float overvoltage_threshold_v;
    float overspeed_fault_rpm;
    float overspeed_soft_rpm;
    float max_duty_cycle_pct;
    float motor_overpower_limit_w;
    float soft_overcurrent_a;
    float braking_load_setpoint_v;

    // PI tuning parameters
    float pi_kp;
    float pi_ki;
    float pi_i_max_a;

    // Protection enable flags
    uint32_t protection_enable;

    // Fault/warning status
    uint32_t fault_status;
    uint32_t fault_latch;
    uint32_t warning_status;

    // LCL (Latching Current Limiter) state
    bool lcl_tripped;           // LCL trip flag (cleared only by hardware reset)

    // Statistics
    uint32_t tick_count;        // Total physics ticks
    uint32_t uptime_seconds;    // Total uptime

} wheel_state_t;

// ============================================================================
// Model API
// ============================================================================

/**
 * @brief Initialize wheel model with default state
 *
 * @param state Pointer to wheel state structure
 */
void wheel_model_init(wheel_state_t* state);

/**
 * @brief Execute one physics tick (10 ms)
 *
 * Updates dynamics, applies control law, enforces limits.
 *
 * @param state Pointer to wheel state structure
 */
void wheel_model_tick(wheel_state_t* state);

/**
 * @brief Set control mode
 *
 * @param state Pointer to wheel state structure
 * @param mode New control mode
 */
void wheel_model_set_mode(wheel_state_t* state, control_mode_t mode);

/**
 * @brief Set speed setpoint (for SPEED mode)
 *
 * @param state Pointer to wheel state structure
 * @param speed_rpm Speed setpoint in RPM
 */
void wheel_model_set_speed(wheel_state_t* state, float speed_rpm);

/**
 * @brief Set current setpoint (for CURRENT mode)
 *
 * @param state Pointer to wheel state structure
 * @param current_a Current setpoint in A
 */
void wheel_model_set_current(wheel_state_t* state, float current_a);

/**
 * @brief Set torque setpoint (for TORQUE mode)
 *
 * @param state Pointer to wheel state structure
 * @param torque_mnm Torque setpoint in mN·m
 */
void wheel_model_set_torque(wheel_state_t* state, float torque_mnm);

/**
 * @brief Set PWM duty cycle (for PWM mode)
 *
 * @param state Pointer to wheel state structure
 * @param duty_pct PWM duty cycle in % (0-97.85)
 */
void wheel_model_set_pwm(wheel_state_t* state, float duty_pct);

/**
 * @brief Set rotation direction
 *
 * @param state Pointer to wheel state structure
 * @param direction Rotation direction
 */
void wheel_model_set_direction(wheel_state_t* state, direction_t direction);

/**
 * @brief Clear latched faults
 *
 * @param state Pointer to wheel state structure
 * @param fault_mask Bitmask of faults to clear (0xFFFFFFFF for all)
 */
void wheel_model_clear_faults(wheel_state_t* state, uint32_t fault_mask);

/**
 * @brief Update protection thresholds from registers
 *
 * Called when POKE writes to protection configuration registers.
 *
 * @param state Pointer to wheel state structure
 */
void wheel_model_update_protections(wheel_state_t* state);

/**
 * @brief Update PI controller parameters from registers
 *
 * Called when POKE writes to PI tuning registers.
 *
 * @param state Pointer to wheel state structure
 */
void wheel_model_update_pi_params(wheel_state_t* state);

/**
 * @brief Handle hardware reset
 *
 * Per ICD Section 10.2.6: "The RESET signal is pulled low to reset the
 * internal LCL circuit, cycling and restoring power to the internal WDE
 * [+1.5V, +3.3V, +5V, +12V] supply rails."
 *
 * Reset behavior:
 * - Clears LCL trip condition
 * - Clears all fault flags
 * - Resets to default configuration
 * - Preserves momentum (wheel continues coasting by physics)
 *
 * @param state Pointer to wheel state structure
 */
void wheel_model_reset(wheel_state_t* state);

/**
 * @brief Check if LCL is tripped
 *
 * LCL trip requires hardware reset to clear. CLEAR-FAULT command [0x09]
 * does not affect LCL state.
 *
 * @param state Pointer to wheel state structure
 * @return true if LCL is tripped (motor disabled, requires reset)
 */
bool wheel_model_is_lcl_tripped(const wheel_state_t* state);

/**
 * @brief Force trigger LCL (for TRIP-LCL command [0x0B])
 *
 * Per ICD Section 12.9: Test command to simulate LCL trip condition.
 * Requires hardware reset to recover.
 *
 * @param state Pointer to wheel state structure
 */
void wheel_model_trip_lcl(wheel_state_t* state);

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * @brief Get current speed in RPM
 *
 * @param state Pointer to wheel state structure
 * @return Current speed in RPM
 */
static inline float wheel_model_get_speed_rpm(const wheel_state_t* state) {
    return state->omega_rad_s * RAD_S_TO_RPM;
}

/**
 * @brief Get current momentum in N·m·s
 *
 * @param state Pointer to wheel state structure
 * @return Current momentum in N·m·s
 */
static inline float wheel_model_get_momentum_nms(const wheel_state_t* state) {
    return state->momentum_nms;
}

/**
 * @brief Check if any faults are active
 *
 * @param state Pointer to wheel state structure
 * @return true if any faults are latched
 */
static inline bool wheel_model_has_faults(const wheel_state_t* state) {
    return (state->fault_latch != 0);
}

#endif // NSS_NRWA_T6_MODEL_H
