# Reset and Fault Handling Requirements

**Baseline Document**: NRWA-T6 ICD v10.02 (Effective Date: 18 December 2023)
**Purpose**: Define reset and fault handling behavior for emulator to match actual hardware
**Status**: Requirements extracted from datasheet analysis

---

## 1. Hardware Reset Signal (RESET)

**Reference**: ICD Section 10.2.6, Table 10-9 (pg 36-37)

### 1.1 Electrical Specification

| Parameter | Min | Typ | Max | Unit | Notes |
|-----------|-----|-----|-----|------|-------|
| Logic High (IDLE) | 3.15 | - | 5.0 | V | Normal operation |
| Logic Low (RESET) | 0.0 | - | 1.35 | V | Reset active |
| Termination | - | 10 | - | kΩ | Pullup to +5V |
| Input Pin | GPIO 14 | - | - | - | Active low |

### 1.2 Functional Behavior

**From ICD Section 10.2.6**:
> "The RESET signal is pulled low to reset the internal LCL circuit, cycling and restoring power to the internal WDE [+1.5V, +3.3V, +5V, +12V] supply rails."

**Interpretation for Emulator**:
- Reset cycles the Latching Current Limiter (LCL)
- Restores internal power supply rails to operational state
- Clears LCL trip condition (if previously triggered)
- **Critical**: Reset is a hardware power cycle mechanism, not just a register clear

### 1.3 Reset Duration

**Specification**: TBD - Need to verify minimum pulse width from datasheet
**Assumption**: >1ms for clean reset (avoid contact bounce)

---

## 2. Fault Signal (FAULT)

**Reference**: ICD Section 10.2.5, Table 10-9 (pg 36-37)

### 2.1 Electrical Specification

| Parameter | Value | Unit | Notes |
|-----------|-------|------|-------|
| Output Type | Open-drain | - | Active low |
| Logic Low (Fault Active) | 0V - 0.4V | V | Pulled low by WDE |
| Logic High (No Fault) | 3.15V - 5V | V | Pulled high externally |
| Output Pin | GPIO 13 | - | Implemented |

### 2.2 Functional Behavior

**Purpose**: Indicates when LCL has triggered and isolated power to internal circuits

**States**:
- **Low (0)**: LCL has triggered, fault condition present
- **High (1)**: Normal operation, no LCL trip

**Clearing**:
- FAULT signal de-asserts when RESET is applied (LCL cycle)
- CLEAR-FAULT command [0x09] clears fault flags in registers but may not affect FAULT pin if LCL is still tripped
- **Hardware RESET** is the definitive way to clear LCL trip

---

## 3. Latching Current Limiter (LCL)

**Reference**: ICD Sections 9.5.1, 9.5.2, 10.2.5, 10.2.6

### 3.1 Purpose
Protection circuit that isolates internal power rails (+1.5V, +3.3V, +5V, +12V) when fault conditions occur.

### 3.2 Trigger Conditions

| Fault Type | Threshold | Latching | Clear Method |
|------------|-----------|----------|--------------|
| Overvoltage | 36V | Yes | RESET pin |
| Overcurrent (hard) | 6A | Yes | RESET pin |
| Overspeed (hard) | 6000 RPM | Yes | RESET pin + CLEAR-FAULT |
| Overpower | 100W | Yes | RESET pin + CLEAR-FAULT |
| Max Duty Cycle | 97.85% | Yes | RESET pin + CLEAR-FAULT |

### 3.3 LCL vs Fault Flags

**Critical Distinction**:
- **LCL Trip**: Physical power isolation, requires RESET to restore
- **Fault Flags**: Software register bits, can be cleared by CLEAR-FAULT command [0x09]

**Emulator Behavior**:
- When LCL trip condition occurs:
  1. Assert FAULT pin (drive GPIO 13 low)
  2. Set fault_latch bits in wheel state
  3. Disable motor output (current_out_a = 0.0f)
  4. **Require hardware RESET to re-enable** (not just CLEAR-FAULT)

---

## 4. CLEAR-FAULT Command [0x09]

**Reference**: ICD Section 12.7 (pg 55-56)

### 4.1 Command Structure
```
Opcode: 0x09
Payload: 4 bytes (fault mask, little-endian)
Response: ACK (0x06) or NACK (0x15)
```

### 4.2 Behavior

**From ICD Section 12.7**:
- Clears fault flags in the fault register based on mask
- Does **not** clear LCL trip condition
- Does **not** de-assert FAULT pin if LCL is still tripped
- Used for clearing soft fault flags after condition resolved

**Example**:
```
CLEAR-FAULT 0x00000001  → Clear overspeed fault flag
```

### 4.3 Interaction with Hardware RESET

| Scenario | CLEAR-FAULT [0x09] | Hardware RESET |
|----------|-------------------|----------------|
| Soft fault flag set | ✅ Clears flag | ✅ Clears flag |
| LCL tripped | ❌ No effect on LCL | ✅ Cycles LCL |
| FAULT pin asserted | ❌ No effect on pin | ✅ De-asserts pin |
| Motor output disabled | ❌ Stays disabled | ✅ Re-enables after init |

**Critical**: Hardware RESET is the **only** way to recover from LCL trip condition.

---

## 5. TRIP-LCL Command [0x0B]

**Reference**: ICD Section 12.9 (pg 57)

### 5.1 Command Structure
```
Opcode: 0x0B
Payload: 0 bytes
Response: ACK (0x06) or NACK (0x15)
```

### 5.2 Purpose
Test command to **force trigger** the LCL protection circuit.

### 5.3 Behavior
1. Immediately trigger LCL (simulate physical fault)
2. Assert FAULT pin (drive GPIO 13 low)
3. Set fault_latch flags
4. Disable motor output
5. **Requires hardware RESET to recover**

### 5.4 Test Use Case
Validate that host software can detect and recover from LCL trip via hardware RESET.

---

## 6. Reset Scenarios and Expected Behavior

### 6.1 Scenario 1: Reset During Normal Operation

**Initial State**:
- Mode: SPEED, setpoint 3000 RPM
- Actual speed: 3000 RPM (steady-state)
- Current: 0.5A (maintaining speed)
- Faults: None

**Reset Action**: Assert RESET pin (pull GPIO 14 low)

**Expected Behavior**:
1. **Immediate**: Motor output → 0A (power rails cycle)
2. **Immediate**: Wheel begins to coast down (no torque)
3. **Immediate**: FAULT pin de-asserts (if it was asserted)
4. **On RESET release**: System re-initializes to default state
   - Mode: CURRENT (default)
   - Current command: 0A
   - Speed: Coasting down from 3000 RPM
   - Fault flags: Cleared
   - Protection thresholds: Reset to defaults

**Test Validation**:
- ✅ Motor output drops to 0A within 1 tick (10ms)
- ✅ Wheel speed decreases monotonically (no instant stop)
- ✅ fault_latch register reads 0x00000000
- ✅ Mode register reads CURRENT (0x00)
- ✅ FAULT pin reads HIGH

### 6.2 Scenario 2: Reset During Spin-Up

**Initial State**:
- Mode: SPEED, setpoint 3000 RPM
- Actual speed: 1200 RPM (accelerating)
- Current: 2.5A (max acceleration)
- Faults: None

**Reset Action**: Assert RESET pin during acceleration

**Expected Behavior**:
1. Motor output → 0A immediately
2. Wheel coasts at 1200 RPM (slowly decelerating due to losses)
3. On RESET release: System re-initializes
   - Mode: CURRENT (default)
   - Current command: 0A
   - Speed: ~1200 RPM (coasting down)
   - PI integrator: Cleared

**Test Validation**:
- ✅ Acceleration stops immediately (alpha → 0)
- ✅ Speed preserves momentum (doesn't jump)
- ✅ PI error integral cleared on re-init
- ✅ System accepts new commands after reset

### 6.3 Scenario 3: Reset During Fault Condition

**Initial State**:
- Mode: SPEED, setpoint 3000 RPM
- Actual speed: 6100 RPM (overspeed fault)
- Faults: FAULT_OVERSPEED (0x00000001)
- fault_latch: 0x00000001
- FAULT pin: LOW (asserted)
- Motor output: 0A (disabled by protection)

**Reset Action**: Assert RESET pin

**Expected Behavior**:
1. LCL trip condition clears
2. FAULT pin → HIGH (de-asserts)
3. On RESET release:
   - fault_latch → 0x00000000
   - Motor output can be re-enabled
   - Speed: Still ~6100 RPM (coasting)
   - **Warning**: If speed still >6000 RPM, fault will re-trigger immediately

**Test Validation**:
- ✅ FAULT pin de-asserts on RESET
- ✅ fault_latch cleared after RESET
- ✅ Motor can be commanded again (not permanently disabled)
- ✅ If fault condition persists, fault re-triggers
- ✅ Verify CLEAR-FAULT alone does NOT de-assert FAULT pin

### 6.4 Scenario 4: Reset During Deceleration

**Initial State**:
- Mode: SPEED, setpoint 0 RPM (commanded stop)
- Actual speed: 2800 RPM (decelerating)
- Current: -1.5A (braking)
- Faults: None

**Reset Action**: Assert RESET pin

**Expected Behavior**:
1. Motor output → 0A (braking stops)
2. Wheel coasts down slower (only loss torque, no active braking)
3. On RESET release: System re-initializes
   - Mode: CURRENT
   - Current: 0A
   - Speed: ~2800 RPM (coasting)

**Test Validation**:
- ✅ Deceleration rate decreases (no more braking torque)
- ✅ Speed > 0 after reset (doesn't instant-stop)
- ✅ System accepts new commands

### 6.5 Scenario 5: TRIP-LCL → Reset Recovery

**Test Sequence**:
1. Send TRIP-LCL command [0x0B]
2. Verify FAULT pin → LOW
3. Verify fault_latch has fault bits set
4. Send CLEAR-FAULT [0x09] with mask 0xFFFFFFFF
5. Verify fault_latch cleared BUT FAULT pin still LOW
6. Assert hardware RESET
7. Verify FAULT pin → HIGH
8. Verify motor can be commanded

**Expected Behavior**:
- ✅ TRIP-LCL triggers fault condition
- ✅ CLEAR-FAULT clears software flags but NOT FAULT pin
- ✅ Hardware RESET clears FAULT pin
- ✅ System fully operational after reset

### 6.6 Scenario 6: Reset Duration and Bounce Handling

**Test**: Apply reset pulses of varying duration

| Pulse Width | Expected Behavior |
|-------------|-------------------|
| <100µs | May be ignored (glitch filter) |
| 100µs - 1ms | Should trigger reset |
| >1ms | Definite reset |
| 100ms | Full reset, no issues |

**Bounce Handling**:
- Reset detection should use hysteresis or debounce
- Minimum pulse width: 100µs (TBD from datasheet)
- State machine should handle clean vs bouncy reset signals

---

## 7. Implementation Requirements

### 7.1 Current Implementation Gaps

**File**: `firmware/device/nss_nrwa_t6_model.c`

**Missing Functions**:
```c
// Not implemented:
void wheel_model_reset(wheel_state_t* state);
bool wheel_model_is_lcl_tripped(const wheel_state_t* state);
void wheel_model_trip_lcl(wheel_state_t* state);
```

**Unused API**:
```c
// Defined in gpio_map.h but never called:
bool gpio_is_reset_asserted(void);
```

### 7.2 Required Changes

#### A. Add Reset Handler to Wheel Model

**File**: `firmware/device/nss_nrwa_t6_model.h`

```c
/**
 * @brief Handle hardware reset
 *
 * Cycles LCL, clears all faults, resets to default state.
 * Preserves momentum (wheel keeps spinning by physics).
 *
 * @param state Pointer to wheel state
 */
void wheel_model_reset(wheel_state_t* state);

/**
 * @brief Check if LCL is tripped
 *
 * @param state Pointer to wheel state
 * @return true if LCL is tripped (requires reset to clear)
 */
bool wheel_model_is_lcl_tripped(const wheel_state_t* state);

/**
 * @brief Force trigger LCL (for TRIP-LCL command [0x0B])
 *
 * @param state Pointer to wheel state
 */
void wheel_model_trip_lcl(wheel_state_t* state);
```

**File**: `firmware/device/nss_nrwa_t6_model.c`

```c
void wheel_model_reset(wheel_state_t* state) {
    // Save momentum (wheel doesn't instantly stop)
    float omega_saved = state->omega_rad_s;
    float momentum_saved = state->momentum_nms;

    // Reset to default state (like wheel_model_init)
    wheel_model_init(state);

    // Restore momentum (physics preserved)
    state->omega_rad_s = omega_saved;
    state->momentum_nms = momentum_saved;

    // Clear LCL trip flag
    state->lcl_tripped = false;

    printf("[WHEEL] Hardware RESET: LCL cycled, faults cleared, ω=%.1f rad/s\n",
           omega_saved);
}

bool wheel_model_is_lcl_tripped(const wheel_state_t* state) {
    return state->lcl_tripped;
}

void wheel_model_trip_lcl(wheel_state_t* state) {
    state->lcl_tripped = true;
    state->fault_latch = 0xFFFFFFFF;  // Set all fault bits
    state->current_out_a = 0.0f;      // Disable motor
    printf("[WHEEL] LCL TRIPPED (test command)\n");
}
```

#### B. Add LCL Trip Flag to State Structure

**File**: `firmware/device/nss_nrwa_t6_model.h`

```c
typedef struct {
    // ... existing fields ...

    /**
     * @brief LCL trip flag
     *
     * Set by TRIP-LCL command or certain hard faults.
     * Can ONLY be cleared by hardware reset (not CLEAR-FAULT).
     */
    bool lcl_tripped;

} wheel_state_t;
```

#### C. Poll Reset Pin in Main Loop

**File**: `firmware/app_main.c` (or wherever main loop runs)

```c
void core0_main_loop(void) {
    while (1) {
        // Check for hardware reset
        if (gpio_is_reset_asserted()) {
            printf("[MAIN] Hardware RESET detected\n");
            wheel_model_reset(&g_wheel_state);
            gpio_set_fault(false);  // De-assert FAULT pin
        }

        // ... existing protocol handling ...
    }
}
```

#### D. Modify Protection Logic

**File**: `firmware/device/nss_nrwa_t6_model.c`

```c
static void check_protections(wheel_state_t* state) {
    // If LCL is tripped, motor stays disabled
    if (state->lcl_tripped) {
        state->current_out_a = 0.0f;
        return;  // No need to check other protections
    }

    // ... existing protection checks ...

    // Hard faults trip the LCL
    if (new_faults & (FAULT_OVERVOLTAGE | FAULT_OVERSPEED)) {
        state->lcl_tripped = true;
        gpio_set_fault(true);  // Assert FAULT pin
    }
}
```

#### E. Update CLEAR-FAULT Handler

**File**: `firmware/device/nss_nrwa_t6_commands.c`

```c
void nsp_cmd_clear_fault(const uint8_t* payload, uint8_t len) {
    if (len != 4) {
        nsp_send_nack();
        return;
    }

    uint32_t fault_mask = *(uint32_t*)payload;
    wheel_model_clear_faults(&g_wheel_state, fault_mask);

    // IMPORTANT: Does NOT clear LCL trip or FAULT pin
    if (wheel_model_is_lcl_tripped(&g_wheel_state)) {
        printf("[NSP] Fault flags cleared, but LCL still tripped (need RESET)\n");
        // FAULT pin stays asserted
    }

    nsp_send_ack();
}
```

#### F. Implement TRIP-LCL Handler

**File**: `firmware/device/nss_nrwa_t6_commands.c`

```c
void nsp_cmd_trip_lcl(const uint8_t* payload, uint8_t len) {
    if (len != 0) {
        nsp_send_nack();
        return;
    }

    wheel_model_trip_lcl(&g_wheel_state);
    gpio_set_fault(true);  // Assert FAULT pin

    printf("[NSP] TRIP-LCL executed: LCL tripped, FAULT asserted\n");
    nsp_send_ack();
}
```

---

## 8. Test Requirements

### 8.1 Unit Tests

**File**: `firmware/test_mode.c`

Add test function: `test_reset_scenarios()`

**Test Cases**:
1. ✅ Reset clears fault_latch
2. ✅ Reset preserves momentum (omega_rad_s)
3. ✅ Reset restores default mode (CURRENT)
4. ✅ Reset de-asserts FAULT pin
5. ✅ LCL trip requires reset (CLEAR-FAULT insufficient)
6. ✅ TRIP-LCL command sets lcl_tripped flag
7. ✅ Reset during spin-up: omega preserved, mode reset
8. ✅ Reset during fault: fault cleared, motor re-enabled

### 8.2 Integration Tests

**File**: `tools/host_tester.py`

**Test Sequence A: TRIP-LCL → CLEAR-FAULT → RESET**
```python
def test_lcl_reset_sequence():
    # 1. Normal operation
    set_mode(SPEED)
    set_speed(3000)
    wait_for_speed(3000, tolerance=100)

    # 2. Trip LCL
    send_command(TRIP_LCL)
    assert_fault_pin_low()
    assert_fault_latch_nonzero()
    assert_motor_output_zero()

    # 3. Try CLEAR-FAULT (should not clear FAULT pin)
    send_command(CLEAR_FAULT, mask=0xFFFFFFFF)
    assert_fault_latch_zero()  # Flags cleared
    assert_fault_pin_low()     # Pin still asserted (LCL tripped)

    # 4. Apply hardware reset
    assert_reset_pin()
    wait_ms(10)
    deassert_reset_pin()

    # 5. Verify recovery
    assert_fault_pin_high()    # FAULT de-asserted
    assert_fault_latch_zero()
    set_mode(SPEED)
    set_speed(1000)
    wait_for_speed(1000, tolerance=100)  # Motor works again
```

**Test Sequence B: Reset During Spin-Up**
```python
def test_reset_during_spinup():
    # 1. Start acceleration
    set_mode(SPEED)
    set_speed(4000)
    wait_for_speed(1500, tolerance=200)  # Partial spin-up

    speed_before_reset = get_speed()

    # 2. Apply reset mid-acceleration
    assert_reset_pin()
    wait_ms(10)
    deassert_reset_pin()

    # 3. Verify state
    speed_after_reset = get_speed()
    mode_after_reset = get_mode()

    assert abs(speed_after_reset - speed_before_reset) < 300  # Coasting
    assert mode_after_reset == CURRENT  # Reset to default
    assert get_current_command() == 0.0
```

**Test Sequence C: Reset During Fault**
```python
def test_reset_during_overspeed():
    # 1. Force overspeed fault (inject speed >6000 RPM)
    inject_state(omega_rad_s=650.0)  # ~6200 RPM
    wait_ms(100)

    assert_fault_latch_has_flag(FAULT_OVERSPEED)
    assert_fault_pin_low()

    # 2. Apply reset
    assert_reset_pin()
    wait_ms(10)
    deassert_reset_pin()

    # 3. Verify recovery
    assert_fault_latch_zero()
    assert_fault_pin_high()

    # 4. Verify fault can re-trigger if condition persists
    if get_speed() > 6000:
        wait_ms(100)
        assert_fault_latch_has_flag(FAULT_OVERSPEED)  # Fault re-triggers
```

### 8.3 Hardware Validation

**Equipment**:
- Raspberry Pi Pico with emulator firmware
- GPIO test rig for RESET pin (GPIO 14)
- FAULT pin monitor (GPIO 13)
- USB-CDC console connection

**Manual Test Procedure**:
1. Flash firmware with reset handling implemented
2. Connect to USB console
3. Monitor FAULT pin state (LED or oscilloscope)
4. Use jumper wire to pull GPIO 14 low (simulate reset)
5. Verify console output shows "Hardware RESET detected"
6. Verify FAULT pin behavior matches expected state transitions

---

## 9. Documentation Updates

### 9.1 SPEC.md Updates

Add new section: **§3.5 Hardware Reset Behavior**

```markdown
### 3.5 Hardware Reset Behavior

The RESET signal (GPIO 14, active low) cycles the internal LCL circuit and restores the WDE to operational state.

**Electrical Specification**:
- Logic Low (RESET): 0V - 1.35V
- Logic High (IDLE): 3.15V - 5V
- Termination: 10kΩ pullup to +5V

**Functional Behavior**:
1. Motor output immediately disabled (0A)
2. LCL trip condition cleared
3. FAULT pin de-asserted
4. System re-initializes to default state
5. Wheel momentum preserved (coasting)

**Difference from CLEAR-FAULT Command**:
- Hardware RESET: Clears LCL trip + fault flags
- CLEAR-FAULT [0x09]: Clears fault flags only (LCL trip persists)
```

### 9.2 IMP.md Updates

Update Phase 5 acceptance criteria:

```markdown
#### Phase 5 Acceptance Criteria

| Criteria | Status | Evidence |
|----------|--------|----------|
| ... existing criteria ... | ✅ | ... |
| Hardware RESET clears LCL trip | ⏸️ | `wheel_model_reset()` implemented |
| CLEAR-FAULT does not affect LCL | ⏸️ | Tested in `test_reset_scenarios()` |
| Reset preserves momentum | ⏸️ | omega_rad_s unchanged after reset |
| TRIP-LCL command triggers LCL | ⏸️ | `nsp_cmd_trip_lcl()` implemented |
```

### 9.3 PROGRESS.md Updates

Add new checkpoint: **Checkpoint 5.3: Reset and Fault Handling**

```markdown
### Checkpoint 5.3: Reset and Fault Handling ⏸️ PENDING

**Objective**: Implement hardware reset handling and LCL trip logic

**Tasks**:
- [ ] Add `wheel_model_reset()` function
- [ ] Add `lcl_tripped` flag to wheel state
- [ ] Poll `gpio_is_reset_asserted()` in main loop
- [ ] Implement TRIP-LCL command [0x0B]
- [ ] Update CLEAR-FAULT to distinguish from reset
- [ ] Add reset test scenarios
- [ ] Hardware validation: Reset pin → FAULT pin behavior

**Acceptance**:
- ✅ Reset clears LCL trip and de-asserts FAULT pin
- ✅ CLEAR-FAULT does not de-assert FAULT pin if LCL tripped
- ✅ Reset preserves wheel momentum
- ✅ TRIP-LCL → CLEAR-FAULT → RESET sequence validated
```

---

## 10. Open Questions

1. **Reset Pulse Width**: What is the minimum reset pulse width? (Need to verify in datasheet Section 10.2.6 details)

2. **Configuration Preservation**: Does hardware reset preserve:
   - Protection thresholds? (Assumed NO - reset to defaults)
   - PI tuning parameters? (Assumed NO - reset to defaults)
   - Device address? (Assumed YES - hardware pins)

3. **Fault Re-Triggering**: If reset is applied while fault condition persists (e.g., speed still >6000 RPM), does:
   - LCL immediately re-trip? (Assumed YES)
   - System get one control cycle to respond? (Assumed NO)

4. **Multiple Simultaneous Faults**: If multiple faults occur (e.g., overspeed + overpower), does:
   - CLEAR-FAULT require clearing all flags? (Assumed YES, selective clear)
   - Reset clear all faults at once? (Assumed YES)

5. **FAULT Pin Timing**: What is the response time from:
   - Fault condition detected → FAULT pin asserted? (Assumed <10ms)
   - RESET asserted → FAULT pin de-asserted? (Assumed <10ms)

---

## 11. Summary

### Critical Findings

1. **Hardware RESET is the only way to clear LCL trip** - CLEAR-FAULT command is insufficient
2. **Reset preserves momentum** - Wheel doesn't instantly stop, it coasts
3. **TRIP-LCL command exists for testing** - Forces LCL trip to validate recovery
4. **FAULT pin reflects LCL state, not just fault flags** - Must distinguish hardware state from software flags

### Implementation Priority

**High Priority**:
1. Add `wheel_model_reset()` function
2. Add `lcl_tripped` flag to state structure
3. Poll reset pin in main loop
4. Implement TRIP-LCL command [0x0B]

**Medium Priority**:
5. Add reset test scenarios to test suite
6. Update CLEAR-FAULT handler documentation
7. Hardware validation with GPIO test rig

**Low Priority**:
8. Document reset timing specifications
9. Add reset pulse width measurement
10. Characterize fault re-trigger behavior

### Success Criteria

Emulator passes all reset scenarios:
- ✅ Reset during normal operation
- ✅ Reset during spin-up
- ✅ Reset during fault condition
- ✅ TRIP-LCL → CLEAR-FAULT → RESET sequence
- ✅ Reset preserves momentum
- ✅ FAULT pin behavior matches datasheet
- ✅ Host integration tests pass

---

**Document Status**: Requirements complete, ready for implementation
**Next Step**: Implement `wheel_model_reset()` and related functions
**Baseline**: NRWA-T6 ICD v10.02 (to be added to `docs/` directory)
