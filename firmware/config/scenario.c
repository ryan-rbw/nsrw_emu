/**
 * @file scenario.c
 * @brief Fault Injection Scenario Engine Implementation
 *
 * Timeline-based fault injection for HIL testing.
 */

#include "scenario.h"
#include "json_loader.h"
#include "../device/nss_nrwa_t6_model.h"
#include "../drivers/crc_ccitt.h"
#include "pico/stdlib.h"
#include <stdio.h>
#include <string.h>

// ============================================================================
// Global State
// ============================================================================

static scenario_t g_scenario;
static bool g_initialized = false;

// Active injections (for duration-based events)
static scenario_action_t g_active_transport_action;
static scenario_action_t g_active_device_action;
static scenario_action_t g_active_physics_action;

static uint32_t g_transport_action_end_ms = 0;
static uint32_t g_device_action_end_ms = 0;
static uint32_t g_physics_action_end_ms = 0;

// ============================================================================
// Initialization
// ============================================================================

void scenario_engine_init(void) {
    memset(&g_scenario, 0, sizeof(g_scenario));
    memset(&g_active_transport_action, 0, sizeof(g_active_transport_action));
    memset(&g_active_device_action, 0, sizeof(g_active_device_action));
    memset(&g_active_physics_action, 0, sizeof(g_active_physics_action));
    g_initialized = true;
    printf("[SCENARIO] Engine initialized\n");
}

// ============================================================================
// Scenario Management
// ============================================================================

bool scenario_load(const char* json_str, size_t json_len) {
    if (!g_initialized) {
        printf("[SCENARIO] ERROR: Engine not initialized\n");
        return false;
    }

    // Deactivate current scenario if active
    if (g_scenario.active) {
        scenario_deactivate();
    }

    // Parse JSON
    if (!json_parse_scenario(json_str, json_len, &g_scenario)) {
        printf("[SCENARIO] ERROR: Parse failed: %s\n", json_get_last_error());
        return false;
    }

    printf("[SCENARIO] Loaded: %s (%d events)\n", g_scenario.name, g_scenario.event_count);
    if (g_scenario.description[0]) {
        printf("[SCENARIO]   %s\n", g_scenario.description);
    }

    return true;
}

bool scenario_activate(void) {
    if (!g_initialized) {
        printf("[SCENARIO] ERROR: Engine not initialized\n");
        return false;
    }

    if (g_scenario.event_count == 0) {
        printf("[SCENARIO] ERROR: No scenario loaded\n");
        return false;
    }

    if (g_scenario.active) {
        printf("[SCENARIO] WARNING: Scenario already active\n");
        return false;
    }

    // Reset all event states
    for (uint8_t i = 0; i < g_scenario.event_count; i++) {
        g_scenario.events[i].triggered = false;
        g_scenario.events[i].trigger_time_ms = 0;
    }

    // Clear active actions
    memset(&g_active_transport_action, 0, sizeof(g_active_transport_action));
    memset(&g_active_device_action, 0, sizeof(g_active_device_action));
    memset(&g_active_physics_action, 0, sizeof(g_active_physics_action));
    g_transport_action_end_ms = 0;
    g_device_action_end_ms = 0;
    g_physics_action_end_ms = 0;

    // Activate scenario
    g_scenario.active = true;
    g_scenario.activation_time_ms = to_ms_since_boot(get_absolute_time());

    printf("[SCENARIO] Activated: %s\n", g_scenario.name);
    return true;
}

void scenario_deactivate(void) {
    if (!g_scenario.active) {
        return;
    }

    g_scenario.active = false;

    // Clear all active actions
    memset(&g_active_transport_action, 0, sizeof(g_active_transport_action));
    memset(&g_active_device_action, 0, sizeof(g_active_device_action));
    memset(&g_active_physics_action, 0, sizeof(g_active_physics_action));
    g_transport_action_end_ms = 0;
    g_device_action_end_ms = 0;
    g_physics_action_end_ms = 0;

    printf("[SCENARIO] Deactivated\n");
}

// ============================================================================
// Condition Checking
// ============================================================================

static bool check_condition(const scenario_condition_t* cond) {
    // If no conditions specified, always true
    if (!cond->check_mode && !cond->check_rpm_gt && !cond->check_rpm_lt && !cond->check_nsp_cmd) {
        return true;
    }

    // TODO Phase 10: Integrate with global wheel state
    // For now, unconditional triggers only (simple timeline-based execution)
    // Condition checking will be added when global wheel state access is available

    printf("[SCENARIO] Conditional triggers not yet supported (Phase 10)\n");
    return true; // Simplified for Phase 9 - all conditions pass
}

// ============================================================================
// Timeline Processing
// ============================================================================

void scenario_update(void) {
    if (!g_initialized || !g_scenario.active) {
        return;
    }

    uint32_t now_ms = to_ms_since_boot(get_absolute_time());
    uint32_t elapsed_ms = now_ms - g_scenario.activation_time_ms;

    // Check for expired duration-based actions
    if (g_transport_action_end_ms != 0 && now_ms >= g_transport_action_end_ms) {
        memset(&g_active_transport_action, 0, sizeof(g_active_transport_action));
        g_transport_action_end_ms = 0;
    }
    if (g_device_action_end_ms != 0 && now_ms >= g_device_action_end_ms) {
        memset(&g_active_device_action, 0, sizeof(g_active_device_action));
        g_device_action_end_ms = 0;
    }
    if (g_physics_action_end_ms != 0 && now_ms >= g_physics_action_end_ms) {
        memset(&g_active_physics_action, 0, sizeof(g_active_physics_action));
        g_physics_action_end_ms = 0;
    }

    // Check for new events to trigger
    for (uint8_t i = 0; i < g_scenario.event_count; i++) {
        scenario_event_t* event = &g_scenario.events[i];

        // Skip if already triggered or not yet time
        if (event->triggered || elapsed_ms < event->t_ms) {
            continue;
        }

        // Check conditions
        if (!check_condition(&event->condition)) {
            continue;
        }

        // Trigger event
        event->triggered = true;
        event->trigger_time_ms = now_ms;

        printf("[SCENARIO] Event %d triggered at t=%lu ms\n", i, elapsed_ms);

        // Apply action
        const scenario_action_t* action = &event->action;

        // Determine action layer and set active duration
        bool has_transport = action->inject_crc_error || action->drop_frames_pct > 0 ||
                           action->delay_reply_ms > 0 || action->force_nack;
        bool has_device = action->flip_status_bits_en || action->set_fault_bits_en ||
                        action->clear_fault_bits_en || action->overspeed_fault || action->trip_lcl;
        bool has_physics = action->limit_power_en || action->limit_current_en ||
                         action->limit_speed_en || action->override_torque_en;

        if (has_transport) {
            g_active_transport_action = *action;
            if (event->duration_ms > 0) {
                g_transport_action_end_ms = now_ms + event->duration_ms;
            } else {
                g_transport_action_end_ms = 0; // Instant/persistent
            }
        }

        if (has_device) {
            scenario_apply_device(action);
            if (event->duration_ms > 0) {
                g_active_device_action = *action;
                g_device_action_end_ms = now_ms + event->duration_ms;
            }
        }

        if (has_physics) {
            g_active_physics_action = *action;
            if (event->duration_ms > 0) {
                g_physics_action_end_ms = now_ms + event->duration_ms;
            } else {
                g_physics_action_end_ms = 0; // Instant/persistent
            }
        }
    }
}

// ============================================================================
// Query Functions
// ============================================================================

bool scenario_is_active(void) {
    return g_scenario.active;
}

const char* scenario_get_name(void) {
    return g_scenario.name[0] ? g_scenario.name : NULL;
}

const char* scenario_get_description(void) {
    return g_scenario.description[0] ? g_scenario.description : NULL;
}

uint32_t scenario_get_elapsed_ms(void) {
    if (!g_scenario.active) {
        return 0;
    }
    uint32_t now_ms = to_ms_since_boot(get_absolute_time());
    return now_ms - g_scenario.activation_time_ms;
}

uint8_t scenario_get_triggered_count(void) {
    uint8_t count = 0;
    for (uint8_t i = 0; i < g_scenario.event_count; i++) {
        if (g_scenario.events[i].triggered) {
            count++;
        }
    }
    return count;
}

uint8_t scenario_get_total_events(void) {
    return g_scenario.event_count;
}

// ============================================================================
// Injection Action Applicators
// ============================================================================

bool scenario_apply_transport(const scenario_action_t* action, uint8_t* packet, size_t* len) {
    // Use active transport action if available
    const scenario_action_t* active_action = &g_active_transport_action;

    // Drop frames
    if (active_action->drop_frames_pct > 0) {
        // Simple RNG: use current time
        uint32_t now = to_ms_since_boot(get_absolute_time());
        if ((now % 100) < active_action->drop_frames_pct) {
            printf("[SCENARIO] Frame dropped (%.0f%%)\n", (float)active_action->drop_frames_pct);
            return false; // Drop this frame
        }
    }

    // Inject CRC error
    if (active_action->inject_crc_error && *len >= 2) {
        // Corrupt last 2 bytes (CRC)
        packet[*len - 2] ^= 0xFF;
        packet[*len - 1] ^= 0xFF;
        printf("[SCENARIO] CRC corrupted\n");
    }

    // Note: delay_reply_ms and force_nack handled by NSP layer

    return true; // Send frame
}

void scenario_apply_device(const scenario_action_t* action) {
    // TODO Phase 10: Integrate with wheel state for direct fault injection
    // For now, just log the actions

    // Flip status bits
    if (action->flip_status_bits_en) {
        printf("[SCENARIO] Status bits flip requested: 0x%08lX (not yet implemented)\n", action->flip_status_bits);
    }

    // Set fault bits
    if (action->set_fault_bits_en) {
        printf("[SCENARIO] Fault bits set requested: 0x%08lX (not yet implemented)\n", action->set_fault_bits);
    }

    // Clear fault bits
    if (action->clear_fault_bits_en) {
        printf("[SCENARIO] Fault bits clear requested: 0x%08lX (not yet implemented)\n", action->clear_fault_bits);
    }

    // Trigger overspeed fault
    if (action->overspeed_fault) {
        printf("[SCENARIO] Overspeed fault requested (not yet implemented)\n");
    }

    // Trip LCL
    if (action->trip_lcl) {
        printf("[SCENARIO] LCL trip requested (not yet implemented)\n");
    }
}

void scenario_apply_physics(const scenario_action_t* action) {
    // TODO Phase 10: Integrate with wheel state for physics overrides
    // For now, just log the actions

    // Use active physics action if available
    const scenario_action_t* active_action = &g_active_physics_action;

    // Override power limit
    if (active_action->limit_power_en) {
        printf("[SCENARIO] Power limit override: %.1f W (not yet implemented)\n", active_action->limit_power_w);
    }

    // Override current limit
    if (active_action->limit_current_en) {
        printf("[SCENARIO] Current limit override: %.1f A (not yet implemented)\n", active_action->limit_current_a);
    }

    // Override speed limit
    if (active_action->limit_speed_en) {
        printf("[SCENARIO] Speed limit override: %.1f RPM (not yet implemented)\n", active_action->limit_speed_rpm);
    }

    // Override torque output
    if (active_action->override_torque_en) {
        printf("[SCENARIO] Torque override: %.1f mN·m (not yet implemented)\n", active_action->override_torque_mNm);
    }
}
