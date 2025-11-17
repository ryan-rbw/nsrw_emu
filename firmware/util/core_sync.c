/**
 * @file core_sync.c
 * @brief Inter-Core Synchronization Implementation
 */

#include "core_sync.h"
#include "pico/stdlib.h"
#include "pico/sync.h"
#include "hardware/sync.h"
#include <string.h>

// ============================================================================
// Internal State
// ============================================================================

// Command mailbox (spinlock-protected)
static spin_lock_t* command_spinlock = NULL;
static command_mailbox_t command_mailbox;

// Telemetry snapshot (spinlock-protected)
static spin_lock_t* telemetry_spinlock = NULL;
static telemetry_snapshot_t telemetry_snapshot;
static volatile bool telemetry_valid = false;

// ============================================================================
// Initialization
// ============================================================================

void core_sync_init(void) {
    // Allocate spinlock for command mailbox
    uint cmd_lock_num = spin_lock_claim_unused(true);
    command_spinlock = spin_lock_init(cmd_lock_num);

    // Initialize command mailbox to empty
    memset(&command_mailbox, 0, sizeof(command_mailbox));
    command_mailbox.type = CMD_NONE;

    // Allocate spinlock for telemetry snapshot
    uint telem_lock_num = spin_lock_claim_unused(true);
    telemetry_spinlock = spin_lock_init(telem_lock_num);

    // Initialize telemetry snapshot
    memset(&telemetry_snapshot, 0, sizeof(telemetry_snapshot));
    telemetry_valid = false;
}

// ============================================================================
// Command Mailbox API (Core0 → Core1)
// ============================================================================

bool core_sync_send_command(command_type_t type, float param1, float param2) {
    if (command_spinlock == NULL) {
        return false;  // Not initialized
    }

    uint32_t save = spin_lock_blocking(command_spinlock);

    // Check if previous command is still pending
    if (command_mailbox.type != CMD_NONE) {
        spin_unlock(command_spinlock, save);
        return false;  // Mailbox full
    }

    // Write command to mailbox
    command_mailbox.type = type;
    command_mailbox.param1 = param1;
    command_mailbox.param2 = param2;
    command_mailbox.timestamp_us = (uint32_t)time_us_64();

    // Memory barrier to ensure visibility across cores
    __dmb();

    spin_unlock(command_spinlock, save);
    return true;
}

bool core_sync_read_command(command_mailbox_t* cmd) {
    if (command_spinlock == NULL || cmd == NULL) {
        return false;
    }

    uint32_t save = spin_lock_blocking(command_spinlock);

    // Check if command is pending
    if (command_mailbox.type == CMD_NONE) {
        spin_unlock(command_spinlock, save);
        return false;  // No command pending
    }

    // Copy command to output
    memcpy(cmd, &command_mailbox, sizeof(command_mailbox_t));

    // Clear mailbox
    command_mailbox.type = CMD_NONE;

    // Memory barrier
    __dmb();

    spin_unlock(command_spinlock, save);
    return true;
}

// ============================================================================
// Telemetry API (Core1 → Core0)
// ============================================================================

void core_sync_publish_telemetry(const telemetry_snapshot_t* snapshot) {
    if (telemetry_spinlock == NULL || snapshot == NULL) {
        return;
    }

    uint32_t save = spin_lock_blocking(telemetry_spinlock);

    // Copy snapshot (Core1 writes, Core0 reads)
    memcpy(&telemetry_snapshot, snapshot, sizeof(telemetry_snapshot_t));
    telemetry_valid = true;

    // Memory barrier
    __dmb();

    spin_unlock(telemetry_spinlock, save);
}

bool core_sync_read_telemetry(telemetry_snapshot_t* snapshot) {
    if (telemetry_spinlock == NULL || snapshot == NULL) {
        return false;
    }

    uint32_t save = spin_lock_blocking(telemetry_spinlock);

    // Check if telemetry is valid
    if (!telemetry_valid) {
        spin_unlock(telemetry_spinlock, save);
        return false;
    }

    // Copy snapshot (Core0 reads, Core1 writes)
    memcpy(snapshot, &telemetry_snapshot, sizeof(telemetry_snapshot_t));

    // Memory barrier
    __dmb();

    spin_unlock(telemetry_spinlock, save);
    return true;
}

uint32_t core_sync_telemetry_available(void) {
    return telemetry_valid ? 1 : 0;
}
