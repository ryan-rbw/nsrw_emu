/**
 * @file scenario.h
 * @brief Fault Injection Scenario Engine
 *
 * Implements timeline-based fault injection for HIL testing.
 * Scenarios loaded from JSON define timed events that inject errors
 * into transport (CRC, SLIP), device (faults), or physics (limits).
 */

#ifndef SCENARIO_H
#define SCENARIO_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// ============================================================================
// Constants
// ============================================================================

#define MAX_SCENARIO_NAME_LEN   32
#define MAX_SCENARIO_DESC_LEN   128
#define MAX_EVENTS_PER_SCENARIO 32

// ============================================================================
// Condition Structure
// ============================================================================

/**
 * @brief Event trigger condition
 *
 * All specified conditions must be true (AND logic).
 * Unspecified fields (default values) are wildcards.
 */
typedef struct {
    // Control mode condition
    bool check_mode;
    uint8_t mode_value;      // CONTROL_MODE_CURRENT, SPEED, TORQUE, PWM

    // RPM range condition
    bool check_rpm_gt;
    float rpm_gt;            // Trigger if speed > this value

    bool check_rpm_lt;
    float rpm_lt;            // Trigger if speed < this value

    // NSP command condition
    bool check_nsp_cmd;
    uint8_t nsp_cmd_value;   // Trigger on specific NSP command
} scenario_condition_t;

// ============================================================================
// Action Structure
// ============================================================================

/**
 * @brief Event injection action
 *
 * Defines what fault/error to inject when event triggers.
 * Multiple actions can be combined in a single event.
 */
typedef struct {
    // Transport-layer injections
    bool inject_crc_error;      // Corrupt CRC before sending
    uint8_t drop_frames_pct;    // Drop N% of frames (0-100)
    uint16_t delay_reply_ms;    // Delay reply by N milliseconds
    bool force_nack;            // Force NACK response

    // Device-layer injections
    bool flip_status_bits_en;
    uint32_t flip_status_bits;  // XOR status register with mask

    bool set_fault_bits_en;
    uint32_t set_fault_bits;    // Set fault bits (bitwise OR)

    bool clear_fault_bits_en;
    uint32_t clear_fault_bits;  // Clear fault bits (bitwise AND NOT)

    // Physics-layer injections
    bool limit_power_en;
    float limit_power_w;        // Override power limit (W)

    bool limit_current_en;
    float limit_current_a;      // Override current limit (A)

    bool limit_speed_en;
    float limit_speed_rpm;      // Override speed limit (RPM)

    bool override_torque_en;
    float override_torque_mNm;  // Force torque output (mN·m)

    // Direct fault triggers
    bool overspeed_fault;       // Trigger overspeed fault immediately
    bool trip_lcl;              // Trip LCL (requires RESET to clear)
} scenario_action_t;

// ============================================================================
// Event Structure
// ============================================================================

/**
 * @brief Timed injection event
 *
 * Represents a single event in the scenario timeline.
 * Events are sorted by t_ms for efficient timeline processing.
 */
typedef struct {
    uint32_t t_ms;              // Time offset from scenario activation (ms)
    uint32_t duration_ms;       // How long action persists (0 = instant)
    scenario_condition_t condition;
    scenario_action_t action;

    // Runtime state
    bool triggered;             // Has this event been triggered?
    uint32_t trigger_time_ms;   // Absolute time when triggered
} scenario_event_t;

// ============================================================================
// Scenario Structure
// ============================================================================

/**
 * @brief Complete fault injection scenario
 *
 * Loaded from JSON file, contains metadata and event timeline.
 */
typedef struct {
    char name[MAX_SCENARIO_NAME_LEN];
    char description[MAX_SCENARIO_DESC_LEN];

    uint8_t event_count;
    scenario_event_t events[MAX_EVENTS_PER_SCENARIO];

    // Runtime state
    bool active;
    uint32_t activation_time_ms;  // Absolute time when scenario started
} scenario_t;

// ============================================================================
// Scenario Engine API
// ============================================================================

/**
 * @brief Initialize scenario engine
 */
void scenario_engine_init(void);

/**
 * @brief Load scenario from JSON string
 *
 * @param json_str JSON scenario definition
 * @param json_len Length of JSON string
 * @return true if loaded successfully, false on parse error
 */
bool scenario_load(const char* json_str, size_t json_len);

/**
 * @brief Activate currently loaded scenario
 *
 * Starts the timeline from t=0.
 *
 * @return true if activated, false if no scenario loaded
 */
bool scenario_activate(void);

/**
 * @brief Deactivate current scenario
 *
 * Stops timeline and clears all active injections.
 */
void scenario_deactivate(void);

/**
 * @brief Update scenario timeline
 *
 * Call from main loop (Core0) to check for triggered events.
 * Applies actions when event conditions are met.
 */
void scenario_update(void);

/**
 * @brief Check if scenario is active
 *
 * @return true if scenario is running
 */
bool scenario_is_active(void);

/**
 * @brief Get current scenario name
 *
 * @return Scenario name or NULL if no scenario loaded
 */
const char* scenario_get_name(void);

/**
 * @brief Get current scenario description
 *
 * @return Scenario description or NULL if no scenario loaded
 */
const char* scenario_get_description(void);

/**
 * @brief Get scenario elapsed time
 *
 * @return Milliseconds since scenario activation (0 if inactive)
 */
uint32_t scenario_get_elapsed_ms(void);

/**
 * @brief Get count of triggered events
 *
 * @return Number of events that have triggered
 */
uint8_t scenario_get_triggered_count(void);

/**
 * @brief Get total event count
 *
 * @return Total number of events in scenario
 */
uint8_t scenario_get_total_events(void);

// ============================================================================
// Injection Action Applicators
// ============================================================================

/**
 * @brief Apply transport-layer injection
 *
 * Called from RS-485 TX/RX path to inject errors.
 *
 * @param action Action to apply
 * @param packet Packet buffer (may be modified)
 * @param len Packet length
 * @return true if packet should be sent, false if dropped
 */
bool scenario_apply_transport(const scenario_action_t* action, uint8_t* packet, size_t* len);

/**
 * @brief Apply device-layer injection
 *
 * Called from device model to inject faults/status changes.
 *
 * @param action Action to apply
 */
void scenario_apply_device(const scenario_action_t* action);

/**
 * @brief Apply physics-layer injection
 *
 * Called from physics tick to override limits/outputs.
 *
 * @param action Action to apply
 */
void scenario_apply_physics(const scenario_action_t* action);

#endif // SCENARIO_H
