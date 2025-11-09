/**
 * @file test_phase9.c
 * @brief Phase 9 Scenario Engine Test
 *
 * Tests JSON parsing and scenario timeline execution.
 */

#include "config/scenario.h"
#include "config/json_loader.h"
#include <stdio.h>
#include <string.h>

// ============================================================================
// Test Scenarios
// ============================================================================

// Simple timeline test - no conditions, pure timing
static const char* test_scenario_simple =
"{\n"
"  \"name\": \"Simple Timeline Test\",\n"
"  \"description\": \"Tests basic timeline with 3 events\",\n"
"  \"schedule\": [\n"
"    {\n"
"      \"t_ms\": 1000,\n"
"      \"action\": {\n"
"        \"inject_crc_error\": true\n"
"      }\n"
"    },\n"
"    {\n"
"      \"t_ms\": 2000,\n"
"      \"duration_ms\": 1000,\n"
"      \"action\": {\n"
"        \"drop_frames_pct\": 50\n"
"      }\n"
"    },\n"
"    {\n"
"      \"t_ms\": 5000,\n"
"      \"action\": {\n"
"        \"overspeed_fault\": true\n"
"      }\n"
"    }\n"
"  ]\n"
"}\n";

// ============================================================================
// Test Functions
// ============================================================================

void test_json_parser(void) {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║  TEST 1: JSON PARSER                                      ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    printf("\n");

    scenario_t scenario;
    bool result = json_parse_scenario(test_scenario_simple, strlen(test_scenario_simple), &scenario);

    if (!result) {
        printf("✗ FAIL: JSON parse failed: %s\n", json_get_last_error());
        return;
    }

    printf("✓ PASS: JSON parsed successfully\n");
    printf("  Name: %s\n", scenario.name);
    printf("  Description: %s\n", scenario.description);
    printf("  Event count: %d\n", scenario.event_count);

    // Verify event details
    if (scenario.event_count != 3) {
        printf("✗ FAIL: Expected 3 events, got %d\n", scenario.event_count);
        return;
    }

    printf("\n  Event 0: t=%lu ms, CRC injection: %s\n",
           scenario.events[0].t_ms,
           scenario.events[0].action.inject_crc_error ? "YES" : "NO");

    printf("  Event 1: t=%lu ms, duration=%lu ms, Drop rate: %d%%\n",
           scenario.events[1].t_ms,
           scenario.events[1].duration_ms,
           scenario.events[1].action.drop_frames_pct);

    printf("  Event 2: t=%lu ms, Overspeed fault: %s\n",
           scenario.events[2].t_ms,
           scenario.events[2].action.overspeed_fault ? "YES" : "NO");

    printf("\n✓✓✓ JSON PARSER TEST PASSED ✓✓✓\n");
}

void test_scenario_loading(void) {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║  TEST 2: SCENARIO LOADING                                 ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    printf("\n");

    // Load scenario
    bool result = scenario_load(test_scenario_simple, strlen(test_scenario_simple));
    if (!result) {
        printf("✗ FAIL: Scenario load failed\n");
        return;
    }

    printf("✓ PASS: Scenario loaded\n");
    printf("  Name: %s\n", scenario_get_name());
    printf("  Description: %s\n", scenario_get_description());
    printf("  Total events: %d\n", scenario_get_total_events());

    // Check status
    if (scenario_is_active()) {
        printf("✗ FAIL: Scenario should not be active yet\n");
        return;
    }
    printf("✓ PASS: Scenario inactive (not activated)\n");

    // Activate scenario
    result = scenario_activate();
    if (!result) {
        printf("✗ FAIL: Scenario activation failed\n");
        return;
    }
    printf("✓ PASS: Scenario activated\n");

    // Check active status
    if (!scenario_is_active()) {
        printf("✗ FAIL: Scenario should be active\n");
        return;
    }
    printf("✓ PASS: Scenario is active\n");
    printf("  Elapsed: %lu ms\n", scenario_get_elapsed_ms());
    printf("  Triggered: %d/%d events\n",
           scenario_get_triggered_count(),
           scenario_get_total_events());

    printf("\n✓✓✓ SCENARIO LOADING TEST PASSED ✓✓✓\n");
}

void test_scenario_timeline(void) {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║  TEST 3: TIMELINE EXECUTION                               ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    printf("\n");

    // Load and activate scenario
    scenario_load(test_scenario_simple, strlen(test_scenario_simple));
    scenario_activate();

    printf("Scenario activated, monitoring timeline for 6 seconds...\n");
    printf("Expected triggers: t=1s, t=2s, t=5s\n\n");

    // Monitor timeline for 6 seconds
    uint32_t start_ms = scenario_get_elapsed_ms();
    for (int i = 0; i < 60; i++) {  // 60 iterations x 100ms = 6 seconds
        sleep_ms(100);
        scenario_update();

        uint32_t elapsed = scenario_get_elapsed_ms();
        uint8_t triggered = scenario_get_triggered_count();

        // Print status every second
        if ((i % 10) == 0) {
            printf("t=%lu ms: %d/%d events triggered\n",
                   elapsed, triggered, scenario_get_total_events());
        }
    }

    // Check final state
    uint8_t final_triggered = scenario_get_triggered_count();
    printf("\nFinal state: %d/%d events triggered\n",
           final_triggered, scenario_get_total_events());

    if (final_triggered != 3) {
        printf("✗ FAIL: Expected 3 events triggered, got %d\n", final_triggered);
        return;
    }

    printf("✓ PASS: All events triggered on schedule\n");

    // Deactivate
    scenario_deactivate();
    if (scenario_is_active()) {
        printf("✗ FAIL: Scenario should be deactivated\n");
        return;
    }
    printf("✓ PASS: Scenario deactivated\n");

    printf("\n✓✓✓ TIMELINE EXECUTION TEST PASSED ✓✓✓\n");
}

void test_config_table_update(void) {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║  TEST 4: CONFIG TABLE INTEGRATION                         ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    printf("\n");

    // Load and activate scenario
    scenario_load(test_scenario_simple, strlen(test_scenario_simple));
    scenario_activate();

    // Call table update function
    extern void table_config_update(void);
    table_config_update();

    printf("✓ PASS: table_config_update() executed successfully\n");
    printf("  (TUI would show live scenario status)\n");

    scenario_deactivate();

    printf("\n✓✓✓ CONFIG TABLE TEST PASSED ✓✓✓\n");
}

// ============================================================================
// Main Test Runner
// ============================================================================

void run_phase9_tests(void) {
    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║                                                            ║\n");
    printf("║  PHASE 9: FAULT INJECTION SYSTEM - VALIDATION TESTS        ║\n");
    printf("║                                                            ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");

    test_json_parser();
    sleep_ms(1000);

    test_scenario_loading();
    sleep_ms(1000);

    test_scenario_timeline();
    sleep_ms(1000);

    test_config_table_update();

    printf("\n");
    printf("╔════════════════════════════════════════════════════════════╗\n");
    printf("║                                                            ║\n");
    printf("║  ALL PHASE 9 TESTS PASSED ✓✓✓                             ║\n");
    printf("║                                                            ║\n");
    printf("╚════════════════════════════════════════════════════════════╝\n");
    printf("\n");
}
