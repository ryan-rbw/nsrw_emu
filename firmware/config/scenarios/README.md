# Fault Injection Scenarios

This directory contains JSON scenario files for Hardware-in-the-Loop (HIL) testing.

## Schema

Scenarios follow the JSON schema defined in [SPEC.md §9](../../../SPEC.md#L213-L266).

### Required Fields

- `name` (string): Short scenario name
- `schedule` (array): Timeline of injection events

### Optional Fields

- `description` (string): Scenario purpose and expected behavior
- `version` (string): Scenario version

### Event Structure

Each event in `schedule` contains:

- **`t_ms`** (required): Time offset from scenario activation in milliseconds
- **`duration_ms`** (optional): How long action persists (0 = instant, default)
- **`condition`** (optional): Trigger conditions (all must be true):
  - `mode_in`: Control mode ("CURRENT", "SPEED", "TORQUE", "PWM")
  - `rpm_gt`: Trigger if speed > value
  - `rpm_lt`: Trigger if speed < value
  - `nsp_cmd_eq`: Trigger on NSP command (e.g., "0x02" for PEEK)
- **`action`** (required): Injection actions (multiple can be combined):
  - **Transport layer**:
    - `inject_crc_error` (bool): Corrupt CRC before sending
    - `drop_frames_pct` (0-100): Drop N% of frames
    - `delay_reply_ms` (int): Delay reply by N milliseconds
    - `force_nack` (bool): Force NACK response
  - **Device layer**:
    - `flip_status_bits` (int): XOR status register with mask
    - `set_fault_bits` (int): Set fault bits (bitwise OR)
    - `clear_fault_bits` (int): Clear fault bits (bitwise AND NOT)
  - **Physics layer**:
    - `limit_power_w` (float): Override power limit
    - `limit_current_a` (float): Override current limit
    - `limit_speed_rpm` (float): Override speed limit
    - `override_torque_mNm` (float): Force torque output
  - **Direct faults**:
    - `overspeed_fault` (bool): Trigger overspeed fault immediately
    - `trip_lcl` (bool): Trip LCL (requires RESET to clear)

## Example Scenarios

### 1. overspeed_fault.json

**Purpose**: Test fault latching and CLEAR-FAULT command handling

Triggers overspeed fault at t=5s. Verify:
- Fault bit latches in status register
- Motor output disabled
- CLEAR-FAULT command clears fault
- Drive can restart after clearing

### 2. crc_injection.json

**Purpose**: Test host retry logic and error recovery

Injects CRC errors on 50% of frames for 10 seconds starting at t=2s. Verify:
- Host detects CRC errors
- Host retries failed commands
- Communication recovers after injection stops

### 3. lcl_trip.json

**Purpose**: Test LCL trip and hardware RESET requirement

Trips LCL at t=3s if wheel is in SPEED mode above 1000 RPM. Verify:
- LCL trip disables motor output immediately
- CLEAR-FAULT command does NOT clear LCL
- Only hardware RESET can clear LCL
- Wheel momentum preserved across RESET

### 4. power_limit_override.json

**Purpose**: Test power limiting logic

Reduces power limit to 50W for 5 seconds, then restores to 100W. Verify:
- Current backs off when power limit exceeded
- Speed control still functional at reduced power
- Limit restoration works correctly

### 5. complex_test.json

**Purpose**: Multi-stage integration test

Three-phase test:
1. t=1s-4s: CRC errors
2. t=5s-10s: Reduced power/current limits
3. t=12s: Conditional fault injection (if SPEED mode > 2000 RPM)
4. t=15s: Clear all faults

## Usage

### Loading Scenarios

**Via TUI** (Future Phase 10):
```
> scenario load overspeed_fault
Loaded: Overspeed Fault Test
> scenario activate
Scenario started at t=0
```

**Via NSP Commands** (Future):
```
APPLICATION-COMMAND subcmd=LOAD_SCENARIO, data="overspeed_fault"
APPLICATION-COMMAND subcmd=ACTIVATE_SCENARIO
```

### Monitoring

Check scenario status in Config Status table (Table 9):
- Active scenario name
- Elapsed time
- Triggered event count
- Next event time

### Deactivating

```
> scenario stop
Scenario deactivated, all injections cleared
```

## Creating Custom Scenarios

1. Copy an existing scenario as a template
2. Modify `name`, `description`, and `schedule` events
3. Validate JSON syntax (use `jq` or online validator)
4. Test with: `scenario load <name>`
5. Verify timing and conditions trigger correctly

## Best Practices

- **Keep it simple**: Start with single-event scenarios
- **Test timing**: Verify events trigger at expected times
- **Condition carefully**: Complex conditions may never trigger
- **Document clearly**: Explain expected behavior in `description`
- **Version scenarios**: Update `version` when modifying

## Technical Notes

- Time is relative to scenario activation (not absolute)
- Conditions are ANDed (all must be true)
- Omitted condition fields are wildcards (always true)
- `duration_ms = 0` means instant/one-shot action
- `duration_ms > 0` means action persists for duration
- Events are processed sequentially by timeline
- Multiple events can trigger simultaneously (same t_ms)

## Troubleshooting

**Event not triggering:**
- Check condition logic (all conditions must be true)
- Verify timing (use Config Status table to see elapsed time)
- Check mode/speed values match condition

**Scenario won't load:**
- Validate JSON syntax (missing comma, bracket, quote)
- Check required fields (`name`, `schedule`, `t_ms`, `action`)
- Verify scenario fits in MAX_EVENTS_PER_SCENARIO (32 events)

**Action not working:**
- Check action is supported in current firmware
- Verify physics/device layer integration complete
- Check console for error messages
