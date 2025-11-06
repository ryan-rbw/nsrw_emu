# Checkpoint Structure Standardization - TODO

**Created**: 2025-11-05
**Status**: Phase 5 complete, Phases 6-10 need restructuring

## Summary

Phase 3, Phase 4, and Phase 5 now have proper checkpoint structure with:
- Explicit "#### Checkpoint X.Y: Title (N%)" headers
- **Implement** section with file names
- **Details** section with implementation requirements
- **Test Mode** section with test specifications
- **Hardware Validation** section with step-by-step testing
- **Acceptance** section with clear success criteria

Phases 6-10 currently have checkpoint counts declared but use inconsistent formatting. They need to be restructured to match the Phase 3/4/5 format.

## Completed

✅ **Phase 1**: Project Foundation (no checkpoints - single deliverable)
✅ **Phase 2**: Platform Layer (no checkpoints - single deliverable)
✅ **Phase 3**: Core Communication Drivers (4 checkpoints) - **PROPERLY FORMATTED**
✅ **Phase 4**: Utilities Foundation (2 checkpoints) - **PROPERLY FORMATTED**
✅ **Phase 5**: Device Model & Physics (3 checkpoints) - **PROPERLY FORMATTED** ✨ JUST COMPLETED

## Remaining Work

### Phase 6: Device Commands & Telemetry (2 checkpoints)

**Current state**: Has "Checkpoints: 2" declared, uses subsections 6.1 and 6.2

**Needs**:
- #### Checkpoint 6.1: NSP Command Handlers (50%)
  - Implement all 8 NSP commands (PEEK, POKE, APPLICATION-TELEMETRY, etc.)
  - Test mode for each command
  - Hardware validation with host_tester.py
  - Acceptance criteria

- #### Checkpoint 6.2: Telemetry Blocks (100%)
  - Implement 5 telemetry blocks (STANDARD, TEMP, VOLT, CURR, DIAG)
  - Test mode for block encoding/sizes
  - Hardware validation
  - Acceptance criteria

### Phase 7: Protection System (2 checkpoints)

**Current state**: Has "Checkpoints: 2" declared

**Needs**:
- #### Checkpoint 7.1: Protection Thresholds & Checks (50%)
  - Hard protections (overvoltage, overcurrent, max duty, overpower)
  - Soft protections (braking load, soft overcurrent, overspeed)
  - Test mode
  - Hardware validation
  - Acceptance criteria

- #### Checkpoint 7.2: Fault Latching & CLEAR-FAULT (100%)
  - Latched fault semantics
  - CLEAR-FAULT command integration
  - Fault register implementation
  - Test mode
  - Hardware validation
  - Acceptance criteria

### Phase 8: Console & TUI (3 checkpoints)

**Current state**: Has "Checkpoints: 3" declared

**Needs**:
- #### Checkpoint 8.1: Menu System & Navigation (33%)
  - Interactive menu framework
  - Number-based navigation
  - q/r/e commands
  - Test mode (if applicable)
  - Hardware validation via USB-CDC
  - Acceptance criteria

- #### Checkpoint 8.2: Tables & Fields Catalog (66%)
  - 7 tables implementation (Serial, NSP, Control, Dynamics, etc.)
  - Field drill-down and editing
  - Live refresh
  - Test mode
  - Hardware validation
  - Acceptance criteria

- #### Checkpoint 8.3: Command Palette (100%)
  - All console commands (get, set, defaults, scenario, etc.)
  - Command parser
  - Help system
  - Test mode
  - Hardware validation
  - Acceptance criteria

### Phase 9: Fault Injection System (2 checkpoints)

**Current state**: Has "Checkpoints: 2" declared

**Needs**:
- #### Checkpoint 9.1: JSON Scenario Parser (50%)
  - cJSON integration
  - Schema validation
  - Parse fault injection scenarios
  - Test mode with sample scenarios
  - Hardware validation
  - Acceptance criteria

- #### Checkpoint 9.2: Scenario Engine (100%)
  - Timeline execution
  - Condition evaluation
  - Action injection
  - Test mode with live scenarios
  - Hardware validation
  - Acceptance criteria

### Phase 10: Main Application & Dual-Core (3 checkpoints)

**Current state**: Has "Checkpoints: 3" declared

**Needs**:
- #### Checkpoint 10.1: Core Integration & Orchestration (33%)
  - Final dual-core wiring
  - Command mailbox
  - Telemetry ring buffer
  - Test mode
  - Hardware validation
  - Acceptance criteria

- #### Checkpoint 10.2: Host Tools (66%)
  - tools/host_tester.py implementation
  - NSP packet sender
  - Response validation
  - Test mode (if applicable)
  - Hardware validation
  - Acceptance criteria

- #### Checkpoint 10.3: Final Integration & System Test (100%)
  - End-to-end testing
  - Full command/telemetry cycle
  - Performance benchmarks
  - Test mode (comprehensive)
  - Hardware validation
  - Acceptance criteria

## Implementation Plan

For each phase above:

1. **Read the current phase section in IMP.md**
2. **Identify the checkpoint boundaries** (what's currently in subsections)
3. **Restructure into "#### Checkpoint X.Y" format**
4. **Add Test Mode section** with specific test cases
5. **Add Hardware Validation** with step-by-step instructions
6. **Add Acceptance** criteria with clear pass/fail
7. **Commit the changes** for that phase

## Format Template

Use this template for each checkpoint:

```markdown
#### Checkpoint X.Y: Title (N%)

**Implement**: `path/to/file.c` + `file.h`

**Details**:
- Feature 1 with implementation notes
- Feature 2 with implementation notes
- Code examples if helpful

**Test Mode**: `test_function_name()` in `test_mode.c`
- Test 1: Description
- Test 2: Description
- Test N: Description

**Hardware Validation**:
1. Flash to Pico
2. Connect console
3. Run test and observe output
4. Verify specific behaviors

**Acceptance**: ✅ Clear, measurable success criterion

---
```

## References

- See Phase 3 checkpoints (lines 244-356 in IMP.md) for excellent examples
- See Phase 4 checkpoints (lines 382-431 in IMP.md) for consistency
- See Phase 5 checkpoints (lines 452-567 in IMP.md) for latest format
