/**
 * @file nss_nrwa_t6_model.c
 * @brief NSS NRWA-T6 Reaction Wheel Physics Model Implementation
 */

#include "nss_nrwa_t6_model.h"
#include <math.h>
#include <string.h>
#include <stdio.h>

// ============================================================================
// Internal Helper Functions
// ============================================================================

/**
 * @brief Sign function (returns -1, 0, or +1)
 */
static inline float sign(float x) {
    if (x > 0.0f) return 1.0f;
    if (x < 0.0f) return -1.0f;
    return 0.0f;
}

/**
 * @brief Clamp value to range [min, max]
 */
static inline float clamp(float value, float min_val, float max_val) {
    if (value < min_val) return min_val;
    if (value > max_val) return max_val;
    return value;
}

/**
 * @brief Calculate motor torque from current
 *
 * τ_motor = k_t · i
 *
 * @param current_a Motor current in A
 * @return Motor torque in mN·m
 */
static float calculate_motor_torque(float current_a) {
    // k_t = 0.0534 N·m/A
    float torque_nm = MOTOR_KT_NM_PER_A * current_a;
    return torque_nm * 1000.0f;  // Convert to mN·m
}

/**
 * @brief Calculate loss torque
 *
 * τ_loss = a·ω + b·sign(ω) + c·i²
 *
 * @param omega_rad_s Angular velocity in rad/s
 * @param current_a Motor current in A
 * @return Loss torque in mN·m
 */
static float calculate_loss_torque(float omega_rad_s, float current_a) {
    // Viscous loss: a·ω
    float loss_viscous = LOSS_VISCOUS_A * omega_rad_s;

    // Coulomb friction: b·sign(ω)
    float loss_coulomb = LOSS_COULOMB_B * sign(omega_rad_s);

    // Copper loss: c·i²
    float loss_copper = LOSS_COPPER_C * current_a * current_a;

    // Total loss (N·m)
    float loss_nm = loss_viscous + loss_coulomb + loss_copper;

    return loss_nm * 1000.0f;  // Convert to mN·m
}

/**
 * @brief Update dynamics for one timestep
 *
 * α = (τ_motor - τ_loss) / I
 * ω_new = ω_old + α·Δt
 * H = I·ω
 *
 * @param state Pointer to wheel state structure
 */
static void update_dynamics(wheel_state_t* state) {
    // Calculate motor torque from output current
    float torque_motor_mnm = calculate_motor_torque(state->current_out_a);

    // Calculate loss torque
    float torque_loss_mnm = calculate_loss_torque(state->omega_rad_s, state->current_out_a);

    // Apply direction (negative direction inverts torque)
    float direction_sign = (state->direction == DIRECTION_POSITIVE) ? 1.0f : -1.0f;
    torque_motor_mnm *= direction_sign;

    // Net torque (mN·m)
    float torque_net_mnm = torque_motor_mnm - torque_loss_mnm;
    float torque_net_nm = torque_net_mnm / 1000.0f;  // Convert to N·m

    // Angular acceleration: α = τ / I
    float alpha_rad_s2 = torque_net_nm / WHEEL_INERTIA_KGM2;

    // Integrate velocity: ω_new = ω_old + α·Δt
    state->omega_rad_s += alpha_rad_s2 * MODEL_DT_S;

    // Update momentum: H = I·ω
    state->momentum_nms = WHEEL_INERTIA_KGM2 * state->omega_rad_s;

    // Store for telemetry
    state->torque_out_mnm = torque_motor_mnm;
    state->torque_loss_mnm = torque_loss_mnm;
    state->alpha_rad_s2 = alpha_rad_s2;

    // Calculate electrical power: P = τ·ω
    state->power_w = (torque_motor_mnm / 1000.0f) * state->omega_rad_s;
}

/**
 * @brief Check and enforce protection limits
 *
 * Updates fault_status, fault_latch, and warning_status.
 *
 * @param state Pointer to wheel state structure
 */
static void check_protections(wheel_state_t* state) {
    uint32_t new_faults = 0;
    uint32_t new_warnings = 0;

    // If LCL is tripped, motor MUST stay disabled
    // Only hardware reset can clear this condition
    if (state->lcl_tripped) {
        state->current_out_a = 0.0f;
        return;  // No other protections matter until reset
    }

    // Clear active faults (not latched)
    state->fault_status = 0;
    state->warning_status = 0;

    float speed_rpm = fabsf(wheel_model_get_speed_rpm(state));

    // Check overspeed fault (hard, latched)
    if (state->protection_enable & PROT_ENABLE_OVERSPEED) {
        if (speed_rpm > state->overspeed_fault_rpm) {
            new_faults |= FAULT_OVERSPEED;
        }
    }

    // Check overspeed warning (soft, non-latching)
    if (state->protection_enable & PROT_ENABLE_SOFT_OVERSPEED) {
        if (speed_rpm > state->overspeed_soft_rpm) {
            new_warnings |= WARN_SOFT_OVERSPEED;
        }
    }

    // Check overpower (hard, latched)
    if (state->protection_enable & PROT_ENABLE_OVERPOWER) {
        if (fabsf(state->power_w) > state->motor_overpower_limit_w) {
            new_faults |= FAULT_OVERPOWER;
        }
    }

    // Check soft overcurrent (warning)
    if (state->protection_enable & PROT_ENABLE_SOFT_OVERCURR) {
        if (fabsf(state->current_out_a) > state->soft_overcurrent_a) {
            new_warnings |= WARN_SOFT_OVERCURRENT;
        }
    }

    // Check overvoltage (hard, latched)
    if (state->protection_enable & PROT_ENABLE_OVERVOLTAGE) {
        if (state->voltage_v > state->overvoltage_threshold_v) {
            new_faults |= FAULT_OVERVOLTAGE;
        }
    }

    // Update fault status
    state->fault_status = new_faults;
    state->fault_latch |= new_faults;  // Latch new faults

    // Update warning status
    state->warning_status = new_warnings;

    // Hard faults trip the LCL (requires hardware reset to recover)
    // Per ICD: Overvoltage and hard overspeed trip LCL
    if (new_faults & (FAULT_OVERVOLTAGE | FAULT_OVERSPEED)) {
        state->lcl_tripped = true;
        printf("[WHEEL] LCL TRIPPED: Hard fault detected (0x%08X)\n", new_faults);
        // Note: FAULT pin assertion handled by gpio_map layer
    }

    // If any faults are latched, disable motor output
    if (state->fault_latch != 0) {
        state->current_out_a = 0.0f;
    }
}

/**
 * @brief Apply power and duty cycle limits
 *
 * @param state Pointer to wheel state structure
 */
static void apply_limits(wheel_state_t* state) {
    // Power limit: |τ·ω| ≤ P_lim
    if (state->protection_enable & PROT_ENABLE_OVERPOWER) {
        float omega_abs = fabsf(state->omega_rad_s);
        if (omega_abs > 0.001f) {  // Avoid division by near-zero
            float max_torque_nm = state->motor_overpower_limit_w / omega_abs;
            float max_current_a = max_torque_nm / MOTOR_KT_NM_PER_A;
            state->current_out_a = clamp(state->current_out_a, -max_current_a, max_current_a);
        }
    }

    // Current limit
    state->current_out_a = clamp(state->current_out_a, -state->soft_overcurrent_a, state->soft_overcurrent_a);

    // Duty cycle limit (simplified: duty ∝ current)
    // In a real motor driver, duty cycle is modulated by BEMF
    // For this model, we assume max duty corresponds to max current
    float max_current_from_duty = state->soft_overcurrent_a * (state->max_duty_cycle_pct / 100.0f);
    state->current_out_a = clamp(state->current_out_a, -max_current_from_duty, max_current_from_duty);
}

// ============================================================================
// Control Mode Implementations
// ============================================================================

/**
 * @brief CURRENT mode: Direct current control
 *
 * i_out = i_cmd
 */
static void control_mode_current(wheel_state_t* state) {
    state->current_out_a = state->current_cmd_a;
}

/**
 * @brief SPEED mode: PI controller with anti-windup
 *
 * e = ω_setpoint - ω_actual
 * i_out = Kp·e + Ki·∫e
 */
static void control_mode_speed(wheel_state_t* state) {
    // Convert command from RPM to rad/s
    float omega_setpoint = state->speed_cmd_rpm * RPM_TO_RAD_S;
    float omega_actual = state->omega_rad_s;

    // Error in rad/s
    float error_rad_s = omega_setpoint - omega_actual;

    // Proportional term
    float p_term = state->pi_kp * error_rad_s;

    // Integral term with anti-windup
    state->pi_error_integral += error_rad_s * MODEL_DT_S;

    // Clamp integral to prevent windup
    float i_max = state->pi_i_max_a / state->pi_ki;  // Convert current limit to error integral limit
    state->pi_error_integral = clamp(state->pi_error_integral, -i_max, i_max);

    float i_term = state->pi_ki * state->pi_error_integral;

    // PI output
    state->pi_output_a = p_term + i_term;

    // Output current
    state->current_out_a = state->pi_output_a;
}

/**
 * @brief TORQUE mode: ΔH feed-forward with current limiting
 *
 * τ_cmd → i_out = τ_cmd / k_t
 */
static void control_mode_torque(wheel_state_t* state) {
    // Convert torque command from mN·m to N·m
    float torque_nm = state->torque_cmd_mnm / 1000.0f;

    // Calculate required current: i = τ / k_t
    float current_required = torque_nm / MOTOR_KT_NM_PER_A;

    // Output current
    state->current_out_a = current_required;
}

/**
 * @brief PWM mode: Backup duty-cycle control
 *
 * duty → i_out (simplified model: i ∝ duty)
 */
static void control_mode_pwm(wheel_state_t* state) {
    // Simplified model: assume linear relationship between duty and current
    // In reality, current depends on BEMF, but this is a backup mode
    float duty_normalized = state->pwm_duty_pct / 100.0f;
    float max_current = state->soft_overcurrent_a;

    // Output current proportional to duty cycle
    state->current_out_a = duty_normalized * max_current;
}

// ============================================================================
// Public API Implementation
// ============================================================================

void wheel_model_init(wheel_state_t* state) {
    // Zero all state
    memset(state, 0, sizeof(wheel_state_t));

    // Set default control mode
    state->mode = CONTROL_MODE_CURRENT;
    state->direction = DIRECTION_POSITIVE;

    // Set default protection thresholds
    state->overvoltage_threshold_v = DEFAULT_OVERVOLTAGE_V;
    state->overspeed_fault_rpm = DEFAULT_OVERSPEED_FAULT_RPM;
    state->overspeed_soft_rpm = DEFAULT_OVERSPEED_SOFT_RPM;
    state->max_duty_cycle_pct = DEFAULT_MAX_DUTY_CYCLE;
    state->motor_overpower_limit_w = DEFAULT_MOTOR_OVERPOWER_W;
    state->soft_overcurrent_a = DEFAULT_SOFT_OVERCURRENT_A;
    state->braking_load_setpoint_v = DEFAULT_BRAKING_LOAD_V;

    // Set default PI parameters
    state->pi_kp = DEFAULT_PI_KP;
    state->pi_ki = DEFAULT_PI_KI;
    state->pi_i_max_a = DEFAULT_PI_I_MAX_A;

    // Enable all protections by default
    state->protection_enable = PROT_ENABLE_OVERVOLTAGE |
                                PROT_ENABLE_OVERSPEED |
                                PROT_ENABLE_OVERDUTY |
                                PROT_ENABLE_OVERPOWER |
                                PROT_ENABLE_SOFT_OVERCURR |
                                PROT_ENABLE_SOFT_OVERSPEED;

    // Simulated bus voltage (constant for now)
    state->voltage_v = 28.0f;  // Typical 28V bus
}

void wheel_model_tick(wheel_state_t* state) {
    // Run control law based on mode
    switch (state->mode) {
        case CONTROL_MODE_CURRENT:
            control_mode_current(state);
            break;

        case CONTROL_MODE_SPEED:
            control_mode_speed(state);
            break;

        case CONTROL_MODE_TORQUE:
            control_mode_torque(state);
            break;

        case CONTROL_MODE_PWM:
            control_mode_pwm(state);
            break;

        default:
            // Invalid mode, default to zero output
            state->current_out_a = 0.0f;
            break;
    }

    // Apply limits
    apply_limits(state);

    // Update dynamics
    update_dynamics(state);

    // Check protections
    check_protections(state);

    // Update statistics
    state->tick_count++;
    if ((state->tick_count % 100) == 0) {
        // Every 100 ticks = 1 second
        state->uptime_seconds++;
    }
}

void wheel_model_set_mode(wheel_state_t* state, control_mode_t mode) {
    state->mode = mode;

    // Reset PI controller state when entering/leaving SPEED mode
    if (mode == CONTROL_MODE_SPEED) {
        state->pi_error_integral = 0.0f;
        state->pi_output_a = 0.0f;
    }
}

void wheel_model_set_speed(wheel_state_t* state, float speed_rpm) {
    state->speed_cmd_rpm = speed_rpm;
}

void wheel_model_set_current(wheel_state_t* state, float current_a) {
    state->current_cmd_a = current_a;
}

void wheel_model_set_torque(wheel_state_t* state, float torque_mnm) {
    state->torque_cmd_mnm = torque_mnm;
}

void wheel_model_set_pwm(wheel_state_t* state, float duty_pct) {
    // Clamp to valid range
    state->pwm_duty_pct = clamp(duty_pct, 0.0f, state->max_duty_cycle_pct);
}

void wheel_model_set_direction(wheel_state_t* state, direction_t direction) {
    state->direction = direction;
}

void wheel_model_clear_faults(wheel_state_t* state, uint32_t fault_mask) {
    // Clear latched faults
    state->fault_latch &= ~fault_mask;
}

void wheel_model_update_protections(wheel_state_t* state) {
    // This function would be called when POKE writes to protection registers
    // For now, it's a placeholder for future register integration
    // In Phase 6, this will read from the register map
}

void wheel_model_update_pi_params(wheel_state_t* state) {
    // This function would be called when POKE writes to PI tuning registers
    // For now, it's a placeholder for future register integration
    // In Phase 6, this will read from the register map
}

void wheel_model_reset(wheel_state_t* state) {
    // Save momentum - wheel doesn't instantly stop on reset
    // Physics preserved: wheel continues coasting by inertia
    float omega_saved = state->omega_rad_s;
    float momentum_saved = state->momentum_nms;

    // Reset to default state (equivalent to power cycle)
    wheel_model_init(state);

    // Restore momentum - only dynamics preserved, not commands or mode
    state->omega_rad_s = omega_saved;
    state->momentum_nms = momentum_saved;

    // Critical: Clear LCL trip flag (only reset can do this)
    state->lcl_tripped = false;

    // Note: FAULT pin de-assertion handled by gpio_map layer
    // Console output for debugging
    printf("[WHEEL] Hardware RESET: LCL cycled, faults cleared, ω=%.1f rad/s\n",
           omega_saved);
}

bool wheel_model_is_lcl_tripped(const wheel_state_t* state) {
    return state->lcl_tripped;
}

void wheel_model_trip_lcl(wheel_state_t* state) {
    // Per ICD Section 12.9: TRIP-LCL command [0x0B]
    // Force trigger LCL protection circuit
    state->lcl_tripped = true;

    // Set all fault flags (simulates physical fault condition)
    state->fault_latch = 0xFFFFFFFF;

    // Disable motor output immediately
    state->current_out_a = 0.0f;

    // Note: FAULT pin assertion handled by gpio_map layer
    printf("[WHEEL] LCL TRIPPED (test command [0x0B]): Motor disabled, reset required\n");
}
