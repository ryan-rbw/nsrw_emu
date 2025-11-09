/**
 * @file scenario_registry.c
 * @brief Scenario Registry Implementation
 *
 * All scenarios are embedded as C string constants (no filesystem needed).
 */

#include "scenario_registry.h"
#include <string.h>

// ============================================================================
// Embedded Scenarios (String Constants)
// ============================================================================

// Scenario 1: Single CRC Error
static const char scenario_crc_single[] =
"{\n"
"  \"name\": \"Single CRC Error\",\n"
"  \"description\": \"Inject one CRC error at t=5s\",\n"
"  \"version\": \"1.0\",\n"
"  \"schedule\": [\n"
"    {\n"
"      \"t_ms\": 5000,\n"
"      \"action\": {\n"
"        \"inject_crc_error\": true\n"
"      }\n"
"    }\n"
"  ]\n"
"}\n";

// Scenario 2: CRC Burst Test
static const char scenario_crc_burst[] =
"{\n"
"  \"name\": \"CRC Burst Test\",\n"
"  \"description\": \"Multiple CRC errors at t=2s, 3s, 4s\",\n"
"  \"version\": \"1.0\",\n"
"  \"schedule\": [\n"
"    {\n"
"      \"t_ms\": 2000,\n"
"      \"action\": {\n"
"        \"inject_crc_error\": true\n"
"      }\n"
"    },\n"
"    {\n"
"      \"t_ms\": 3000,\n"
"      \"action\": {\n"
"        \"inject_crc_error\": true\n"
"      }\n"
"    },\n"
"    {\n"
"      \"t_ms\": 4000,\n"
"      \"action\": {\n"
"        \"inject_crc_error\": true\n"
"      }\n"
"    }\n"
"  ]\n"
"}\n";

// Scenario 3: Frame Drop Test
static const char scenario_frame_drop[] =
"{\n"
"  \"name\": \"Frame Drop 50%\",\n"
"  \"description\": \"Drop 50% of frames for 5 seconds\",\n"
"  \"version\": \"1.0\",\n"
"  \"schedule\": [\n"
"    {\n"
"      \"t_ms\": 2000,\n"
"      \"duration_ms\": 5000,\n"
"      \"action\": {\n"
"        \"drop_frames_pct\": 50\n"
"      }\n"
"    }\n"
"  ]\n"
"}\n";

// Scenario 4: Overspeed Fault
static const char scenario_overspeed[] =
"{\n"
"  \"name\": \"Overspeed Fault\",\n"
"  \"description\": \"Trigger overspeed fault at t=5s\",\n"
"  \"version\": \"1.0\",\n"
"  \"schedule\": [\n"
"    {\n"
"      \"t_ms\": 5000,\n"
"      \"action\": {\n"
"        \"overspeed_fault\": true\n"
"      }\n"
"    }\n"
"  ]\n"
"}\n";

// Scenario 5: Power Limit Test
static const char scenario_power_limit[] =
"{\n"
"  \"name\": \"Power Limit Test\",\n"
"  \"description\": \"Reduce power limit to 50W for 10s\",\n"
"  \"version\": \"1.0\",\n"
"  \"schedule\": [\n"
"    {\n"
"      \"t_ms\": 1000,\n"
"      \"duration_ms\": 10000,\n"
"      \"action\": {\n"
"        \"limit_power_w\": 50.0\n"
"      }\n"
"    }\n"
"  ]\n"
"}\n";

// ============================================================================
// Scenario Registry
// ============================================================================

const scenario_entry_t g_scenario_registry[] = {
    { "Single CRC Error", scenario_crc_single, sizeof(scenario_crc_single) - 1 },
    { "CRC Burst Test", scenario_crc_burst, sizeof(scenario_crc_burst) - 1 },
    { "Frame Drop 50%", scenario_frame_drop, sizeof(scenario_frame_drop) - 1 },
    { "Overspeed Fault", scenario_overspeed, sizeof(scenario_overspeed) - 1 },
    { "Power Limit Test", scenario_power_limit, sizeof(scenario_power_limit) - 1 },
};

const uint8_t g_scenario_count = sizeof(g_scenario_registry) / sizeof(g_scenario_registry[0]);

// ============================================================================
// Registry API
// ============================================================================

const scenario_entry_t* scenario_registry_get(uint8_t index) {
    if (index >= g_scenario_count) {
        return NULL;
    }
    return &g_scenario_registry[index];
}

uint8_t scenario_registry_count(void) {
    return g_scenario_count;
}

uint8_t scenario_registry_find(const char* name) {
    for (uint8_t i = 0; i < g_scenario_count; i++) {
        if (strcmp(g_scenario_registry[i].name, name) == 0) {
            return i;
        }
    }
    return 0xFF;  // Not found
}
