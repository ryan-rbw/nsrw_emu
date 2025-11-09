/**
 * @file json_loader.c
 * @brief Minimal JSON Parser for Scenario Files
 *
 * Lightweight JSON parser specifically designed for fault injection scenarios.
 * Does not support full JSON spec - only features needed for scenarios.
 */

#include "json_loader.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

// ============================================================================
// Parser State
// ============================================================================

static const char* g_last_error = NULL;
static const char* g_parse_ptr = NULL;

// ============================================================================
// Utility Functions
// ============================================================================

static void set_error(const char* msg) {
    g_last_error = msg;
}

static void skip_whitespace(void) {
    while (*g_parse_ptr && isspace(*g_parse_ptr)) {
        g_parse_ptr++;
    }
}

static bool expect_char(char c) {
    skip_whitespace();
    if (*g_parse_ptr == c) {
        g_parse_ptr++;
        return true;
    }
    return false;
}

static bool parse_string(char* buf, size_t buflen) {
    skip_whitespace();
    if (*g_parse_ptr != '"') {
        set_error("Expected string");
        return false;
    }
    g_parse_ptr++; // Skip opening quote

    size_t i = 0;
    while (*g_parse_ptr && *g_parse_ptr != '"' && i < buflen - 1) {
        if (*g_parse_ptr == '\\') {
            g_parse_ptr++; // Skip escape char
            if (!*g_parse_ptr) {
                set_error("Unterminated string escape");
                return false;
            }
        }
        buf[i++] = *g_parse_ptr++;
    }

    if (*g_parse_ptr != '"') {
        set_error("Unterminated string");
        return false;
    }
    g_parse_ptr++; // Skip closing quote

    buf[i] = '\0';
    return true;
}

static bool parse_number(float* value) {
    skip_whitespace();
    char* endptr;
    *value = strtof(g_parse_ptr, &endptr);
    if (endptr == g_parse_ptr) {
        set_error("Expected number");
        return false;
    }
    g_parse_ptr = endptr;
    return true;
}

static bool parse_int(uint32_t* value) {
    skip_whitespace();
    char* endptr;
    *value = strtoul(g_parse_ptr, &endptr, 10);
    if (endptr == g_parse_ptr) {
        set_error("Expected integer");
        return false;
    }
    g_parse_ptr = endptr;
    return true;
}

static bool parse_bool(bool* value) {
    skip_whitespace();
    if (strncmp(g_parse_ptr, "true", 4) == 0) {
        *value = true;
        g_parse_ptr += 4;
        return true;
    }
    if (strncmp(g_parse_ptr, "false", 5) == 0) {
        *value = false;
        g_parse_ptr += 5;
        return true;
    }
    set_error("Expected boolean");
    return false;
}

// ============================================================================
// JSON Object Parsers
// ============================================================================

static bool parse_condition(scenario_condition_t* cond) {
    memset(cond, 0, sizeof(*cond));

    if (!expect_char('{')) {
        set_error("Expected '{' for condition");
        return false;
    }

    bool first = true;
    while (!expect_char('}')) {
        if (!first && !expect_char(',')) {
            set_error("Expected ',' in condition");
            return false;
        }
        first = false;

        char key[32];
        if (!parse_string(key, sizeof(key))) {
            return false;
        }

        if (!expect_char(':')) {
            set_error("Expected ':' after key");
            return false;
        }

        if (strcmp(key, "mode_in") == 0) {
            char mode_str[16];
            if (!parse_string(mode_str, sizeof(mode_str))) {
                return false;
            }
            cond->check_mode = true;
            if (strcmp(mode_str, "CURRENT") == 0) cond->mode_value = 0;
            else if (strcmp(mode_str, "SPEED") == 0) cond->mode_value = 1;
            else if (strcmp(mode_str, "TORQUE") == 0) cond->mode_value = 2;
            else if (strcmp(mode_str, "PWM") == 0) cond->mode_value = 3;
            else {
                set_error("Invalid mode value");
                return false;
            }
        }
        else if (strcmp(key, "rpm_gt") == 0) {
            if (!parse_number(&cond->rpm_gt)) return false;
            cond->check_rpm_gt = true;
        }
        else if (strcmp(key, "rpm_lt") == 0) {
            if (!parse_number(&cond->rpm_lt)) return false;
            cond->check_rpm_lt = true;
        }
        else if (strcmp(key, "nsp_cmd_eq") == 0) {
            char cmd_str[8];
            if (!parse_string(cmd_str, sizeof(cmd_str))) return false;
            if (strncmp(cmd_str, "0x", 2) == 0) {
                cond->nsp_cmd_value = (uint8_t)strtoul(cmd_str + 2, NULL, 16);
                cond->check_nsp_cmd = true;
            } else {
                set_error("Invalid NSP command format");
                return false;
            }
        }
        else {
            // Unknown key - skip value
            skip_whitespace();
            if (*g_parse_ptr == '"') {
                char dummy[64];
                parse_string(dummy, sizeof(dummy));
            } else if (isdigit(*g_parse_ptr) || *g_parse_ptr == '-') {
                float dummy_f;
                parse_number(&dummy_f);
            } else if (*g_parse_ptr == 't' || *g_parse_ptr == 'f') {
                bool dummy_b;
                parse_bool(&dummy_b);
            }
        }
    }

    return true;
}

static bool parse_action(scenario_action_t* action) {
    memset(action, 0, sizeof(*action));

    if (!expect_char('{')) {
        set_error("Expected '{' for action");
        return false;
    }

    bool first = true;
    while (!expect_char('}')) {
        if (!first && !expect_char(',')) {
            set_error("Expected ',' in action");
            return false;
        }
        first = false;

        char key[32];
        if (!parse_string(key, sizeof(key))) {
            return false;
        }

        if (!expect_char(':')) {
            set_error("Expected ':' after key");
            return false;
        }

        // Parse action fields
        if (strcmp(key, "inject_crc_error") == 0) {
            if (!parse_bool(&action->inject_crc_error)) return false;
        }
        else if (strcmp(key, "drop_frames_pct") == 0) {
            float pct;
            if (!parse_number(&pct)) return false;
            action->drop_frames_pct = (uint8_t)pct;
        }
        else if (strcmp(key, "delay_reply_ms") == 0) {
            uint32_t delay;
            if (!parse_int(&delay)) return false;
            action->delay_reply_ms = (uint16_t)delay;
        }
        else if (strcmp(key, "force_nack") == 0) {
            if (!parse_bool(&action->force_nack)) return false;
        }
        else if (strcmp(key, "flip_status_bits") == 0) {
            if (!parse_int(&action->flip_status_bits)) return false;
            action->flip_status_bits_en = true;
        }
        else if (strcmp(key, "set_fault_bits") == 0) {
            if (!parse_int(&action->set_fault_bits)) return false;
            action->set_fault_bits_en = true;
        }
        else if (strcmp(key, "clear_fault_bits") == 0) {
            if (!parse_int(&action->clear_fault_bits)) return false;
            action->clear_fault_bits_en = true;
        }
        else if (strcmp(key, "limit_power_w") == 0) {
            if (!parse_number(&action->limit_power_w)) return false;
            action->limit_power_en = true;
        }
        else if (strcmp(key, "limit_current_a") == 0) {
            if (!parse_number(&action->limit_current_a)) return false;
            action->limit_current_en = true;
        }
        else if (strcmp(key, "limit_speed_rpm") == 0) {
            if (!parse_number(&action->limit_speed_rpm)) return false;
            action->limit_speed_en = true;
        }
        else if (strcmp(key, "override_torque_mNm") == 0) {
            if (!parse_number(&action->override_torque_mNm)) return false;
            action->override_torque_en = true;
        }
        else if (strcmp(key, "overspeed_fault") == 0) {
            if (!parse_bool(&action->overspeed_fault)) return false;
        }
        else if (strcmp(key, "trip_lcl") == 0) {
            if (!parse_bool(&action->trip_lcl)) return false;
        }
        else {
            // Unknown key - skip value
            skip_whitespace();
            if (*g_parse_ptr == '"') {
                char dummy[64];
                parse_string(dummy, sizeof(dummy));
            } else if (isdigit(*g_parse_ptr) || *g_parse_ptr == '-') {
                float dummy_f;
                parse_number(&dummy_f);
            } else if (*g_parse_ptr == 't' || *g_parse_ptr == 'f') {
                bool dummy_b;
                parse_bool(&dummy_b);
            }
        }
    }

    return true;
}

static bool parse_event(scenario_event_t* event) {
    memset(event, 0, sizeof(*event));

    if (!expect_char('{')) {
        set_error("Expected '{' for event");
        return false;
    }

    bool has_t_ms = false;
    bool has_action = false;

    bool first = true;
    while (!expect_char('}')) {
        if (!first && !expect_char(',')) {
            set_error("Expected ',' in event");
            return false;
        }
        first = false;

        char key[32];
        if (!parse_string(key, sizeof(key))) {
            return false;
        }

        if (!expect_char(':')) {
            set_error("Expected ':' after key");
            return false;
        }

        if (strcmp(key, "t_ms") == 0) {
            if (!parse_int(&event->t_ms)) return false;
            has_t_ms = true;
        }
        else if (strcmp(key, "duration_ms") == 0) {
            if (!parse_int(&event->duration_ms)) return false;
        }
        else if (strcmp(key, "condition") == 0) {
            if (!parse_condition(&event->condition)) return false;
        }
        else if (strcmp(key, "action") == 0) {
            if (!parse_action(&event->action)) return false;
            has_action = true;
        }
        else {
            // Unknown key - skip
            skip_whitespace();
            if (*g_parse_ptr == '{') {
                int depth = 0;
                while (*g_parse_ptr) {
                    if (*g_parse_ptr == '{') depth++;
                    if (*g_parse_ptr == '}') {
                        depth--;
                        g_parse_ptr++;
                        if (depth == 0) break;
                    } else {
                        g_parse_ptr++;
                    }
                }
            }
        }
    }

    if (!has_t_ms || !has_action) {
        set_error("Event missing required fields (t_ms, action)");
        return false;
    }

    return true;
}

// ============================================================================
// Main Parser
// ============================================================================

bool json_parse_scenario(const char* json_str, size_t json_len, scenario_t* scenario) {
    if (!json_str || !scenario) {
        set_error("NULL parameter");
        return false;
    }

    g_last_error = NULL;
    g_parse_ptr = json_str;

    memset(scenario, 0, sizeof(*scenario));

    if (!expect_char('{')) {
        set_error("Expected '{' at root");
        return false;
    }

    bool has_name = false;
    bool has_schedule = false;

    bool first = true;
    while (!expect_char('}')) {
        if (!first && !expect_char(',')) {
            set_error("Expected ',' in root object");
            return false;
        }
        first = false;

        char key[32];
        if (!parse_string(key, sizeof(key))) {
            return false;
        }

        if (!expect_char(':')) {
            set_error("Expected ':' after key");
            return false;
        }

        if (strcmp(key, "name") == 0) {
            if (!parse_string(scenario->name, sizeof(scenario->name))) {
                return false;
            }
            has_name = true;
        }
        else if (strcmp(key, "description") == 0) {
            if (!parse_string(scenario->description, sizeof(scenario->description))) {
                return false;
            }
        }
        else if (strcmp(key, "version") == 0) {
            char version[16];
            if (!parse_string(version, sizeof(version))) {
                return false;
            }
        }
        else if (strcmp(key, "schedule") == 0) {
            if (!expect_char('[')) {
                set_error("Expected '[' for schedule");
                return false;
            }

            scenario->event_count = 0;
            bool first_event = true;
            while (!expect_char(']')) {
                if (!first_event && !expect_char(',')) {
                    set_error("Expected ',' in schedule array");
                    return false;
                }
                first_event = false;

                if (scenario->event_count >= MAX_EVENTS_PER_SCENARIO) {
                    set_error("Too many events in scenario");
                    return false;
                }

                if (!parse_event(&scenario->events[scenario->event_count])) {
                    return false;
                }
                scenario->event_count++;
            }

            has_schedule = true;
        }
        else {
            // Unknown key - skip value
            skip_whitespace();
            if (*g_parse_ptr == '"') {
                char dummy[128];
                parse_string(dummy, sizeof(dummy));
            }
        }
    }

    if (!has_name || !has_schedule) {
        set_error("Scenario missing required fields (name, schedule)");
        return false;
    }

    // Sort events by t_ms for efficient timeline processing
    for (uint8_t i = 0; i < scenario->event_count - 1; i++) {
        for (uint8_t j = i + 1; j < scenario->event_count; j++) {
            if (scenario->events[j].t_ms < scenario->events[i].t_ms) {
                scenario_event_t temp = scenario->events[i];
                scenario->events[i] = scenario->events[j];
                scenario->events[j] = temp;
            }
        }
    }

    return true;
}

const char* json_get_last_error(void) {
    return g_last_error ? g_last_error : "No error";
}
