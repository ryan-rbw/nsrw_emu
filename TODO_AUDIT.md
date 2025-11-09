# TODO/Stub Audit - NRWA-T6 Emulator

**Date**: 2025-11-09
**Total TODOs Found**: 27

---

## Category 1: Future Phase Work (Not Implemented Yet)

These TODOs are for features planned in later phases and are **intentional placeholders**:

### Phase 3: Communication Drivers (Not Started)
| File | Line | Description | Status |
|------|------|-------------|--------|
| [app_main.c](firmware/app_main.c#L108) | 108 | Initialize drivers (RS-485, SLIP, NSP) | ⏸️ Phase 3 |
| [app_main.c](firmware/app_main.c#L180) | 180 | Poll RS-485 for incoming NSP packets | ⏸️ Phase 3 |

### Phase 10: Physics Model Integration (Not Started)
| File | Line | Description | Status |
|------|------|-------------|--------|
| [app_main.c](firmware/app_main.c#L76) | 76 | Implement physics loop on Core1 | ⏸️ Phase 10 |
| [scenario.c](firmware/config/scenario.c#L140) | 140 | Integrate condition checking with global wheel state | ⏸️ Phase 10 |
| [scenario.c](firmware/config/scenario.c#L303) | 303 | Integrate device actions with wheel state for fault injection | ⏸️ Phase 10 |
| [scenario.c](firmware/config/scenario.c#L333) | 333 | Integrate physics actions with wheel state for overrides | ⏸️ Phase 10 |

**Total**: 6 TODOs (intentional, waiting for Phase 3 and Phase 10)

---

## Category 2: Fixed-Point Format Conversions (Phase 4 Work)

These TODOs reference converting to fixed-point formats (UQ14.18, UQ16.16, UQ18.14). This is **Phase 4** work (Fixed-Point Math Library) which hasn't been started yet:

### Table Control Fields (table_control.c)
| Field | Line | Current Type | Target Type | Status |
|-------|------|--------------|-------------|--------|
| speed_rpm | 62 | U32 | UQ14.18 | ⏸️ Phase 4 |
| current_ma | 72 | U32 | UQ18.14 | ⏸️ Phase 4 |
| torque_mnm | 82 | U32 | UQ18.14 | ⏸️ Phase 4 |
| pwm_pct | 92 | U32 | UQ16.16 | ⏸️ Phase 4 |

### Table Dynamics Fields (table_dynamics.c)
| Field | Line | Current Type | Target Type | Status |
|-------|------|--------------|-------------|--------|
| speed_rpm | 35 | FLOAT | UQ14.18 | ⏸️ Phase 4 |
| momentum_nms | 45 | FLOAT | UQ18.14 | ⏸️ Phase 4 |
| motor_torque_mnm | 55 | FLOAT | UQ18.14 | ⏸️ Phase 4 |
| load_torque_mnm | 65 | FLOAT | UQ18.14 | ⏸️ Phase 4 |
| current_ma | 75 | FLOAT | UQ18.14 | ⏸️ Phase 4 |
| voltage_v | 85 | FLOAT | UQ18.14 | ⏸️ Phase 4 |
| power_w | 95 | FLOAT | UQ18.14 | ⏸️ Phase 4 |

**Total**: 11 TODOs (intentional, waiting for Phase 4: Fixed-Point Math Library)

---

## Category 3: Stub Variables (Active - Need Review)

These are stub variables that are currently **disconnected from the physics model** but are displayed in the TUI. They should be reviewed:

### Table Control Stubs (table_control.c)
| Variable | Line | Description | Status |
|----------|------|-------------|--------|
| control_speed_rpm | 18 | Speed setpoint (stub) | ✅ **ACTIVE** - Connected to TUI banner |
| control_current_ma | 19 | Current setpoint (stub) | ✅ **ACTIVE** - Connected to TUI banner |
| control_torque_mnm | 20 | Torque setpoint (stub) | ⚠️ Stub - not connected to physics |
| control_pwm_pct | 21 | PWM duty cycle % (stub) | ⚠️ Stub - not connected to physics |

**Note**: These stubs are **functional for TUI testing** but will be connected to the actual physics model in Phase 10. The banner now properly reads these values, so they are **partially complete**.

**Total**: 4 stub comments (2 active in banner, 2 waiting for physics integration)

---

## Category 4: Low-Priority Implementation TODOs

These are low-priority features that can be implemented later or are optional:

### Catalog System (tables.c)
| Function | Line | Description | Priority |
|----------|------|-------------|----------|
| catalog_get_field_u32 | 138 | Implement type-specific decoding | Low - not currently needed |
| catalog_set_field_u32 | 152 | Implement type-specific encoding and write | Low - TUI uses direct pointers |
| catalog_mark_dirty | 170 | Implement dirty tracking | Medium - useful for sync |
| catalog_restore_defaults | 176 | Implement defaults restoration | Low - nice-to-have |

### RS-485 UART Driver (rs485_uart.c)
| Function | Line | Description | Priority |
|----------|------|-------------|----------|
| rs485_uart_init | 135 | Access hardware registers for detailed error reporting | Low - basic error handling works |

### GPIO Map (gpio_map.c)
| Function | Line | Description | Priority |
|----------|------|-------------|----------|
| gpio_set_fault_pin | 69 | Configure as open-drain if needed | Low - push-pull works for now |

**Total**: 6 TODOs (low priority, optional enhancements)

---

## Summary by Status

| Category | Count | Status |
|----------|-------|--------|
| **Future Phase Work** | 6 | ⏸️ Waiting for Phase 3, 10 |
| **Fixed-Point Conversions** | 11 | ⏸️ Waiting for Phase 4 |
| **Stub Variables** | 4 | ✅ 2 active (connected to banner), ⚠️ 2 waiting for physics |
| **Low-Priority Features** | 6 | 📋 Optional enhancements |
| **TOTAL** | 27 | |

---

## Completed Work (No More TODOs)

The following components are **fully implemented** with no remaining TODOs:

✅ **Phase 1**: Project Foundation - Complete
✅ **Phase 2**: Platform Layer (GPIO, Timebase, Board Config) - Complete
✅ **Phase 5**: Register Map Structure - Complete
✅ **Phase 8**: Console TUI (Tables, Navigation, Edit, Enums) - Complete
✅ **Phase 9**: Fault Injection System (Scenarios, JSON Parser, Execution) - Complete

---

## Recommendations

### Immediate Action: None Required
All TODOs are either:
1. **Intentional placeholders** for future phases (Phase 3, 4, 10)
2. **Low-priority enhancements** that don't block current functionality
3. **Stub variables** that are functional for current testing

### Next Steps
1. ✅ **Banner now connected** - Table 4 control values display live in TUI banner
2. Continue with **Phase 3** (Communication Drivers) when ready
3. Implement **Phase 4** (Fixed-Point Math) to replace FLOAT fields
4. Defer low-priority catalog features until actually needed

### Clean Code Status
**Code Quality**: All TODOs are documented, categorized, and intentional. No orphaned or forgotten work items.

---

**Generated**: 2025-11-09
**Tool**: Claude Code Audit
**Next Review**: After Phase 3 or 4 completion
