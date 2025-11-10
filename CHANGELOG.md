# Changelog - NRWA-T6 Emulator

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

---

## [Unreleased] - 2025-11-09

### Added
- **TUI Enum System** - Comprehensive enum field support with UPPERCASE display
  - ENUM fields display as text instead of numbers (e.g., "SPEED" not "1")
  - Case-insensitive text input ("speed", "SPEED", "Speed" all work)
  - Interactive "?" help to show available enum values
  - Backward compatible numeric input (0, 1, 2 still work)
  - Applied to: Table 4 (mode, direction), Table 10 (scenario_index)

- **BOOL Field Enhancements** - Boolean fields now support text input
  - Display as "TRUE" / "FALSE" in UPPERCASE (was lowercase)
  - Accept text input: "true", "false", "yes", "no" (case-insensitive)
  - "?" help shows available values
  - Backward compatible: "1" and "0" still work

- **Live Banner Updates** - Status banner now shows real-time values
  - Mode displays as UPPERCASE enum string (not number)
  - Speed (RPM) and current (mA) update live from Table 4
  - Status changes from IDLE to ACTIVE based on speed
  - Color-coded values (IDLE=green, ACTIVE=cyan)

- **Getter Functions** - Added API to access Table 4 control values
  - `table_control_get_mode()`, `table_control_get_speed_rpm()`, etc.
  - `table_control_get_mode_string()` - Get mode as UPPERCASE string
  - Used by banner to display live values

- **Fault Injection Improvements**
  - Clear instructions: "Press any key to abort scenario execution"
  - Auto-clear notification: "Trigger field auto-cleared (one-shot activation)"
  - Better user guidance for scenario execution flow

- **UI Polish**
  - Table numbers now right-aligned (1-9 pad with space to match 10+)
  - "?" help hint shown for ENUM and BOOL fields during editing
  - Conditional prompt based on field type

### Fixed
- **Critical Crash Fix** - STRING type fields caused crash when displaying
  - Fixed incorrect pointer dereferencing in TUI for STRING fields
  - All tables now safe to navigate and expand

- **Struct Initialization** - Missing enum field initialization in all tables
  - Added `.enum_values = NULL, .enum_count = 0` to all non-ENUM fields
  - Prevents undefined behavior from uninitialized struct members
  - Fixed in 10 table files (table_*.c)

- **BOOL Field Input** - BOOL fields now accept letter input for text values
  - Updated character acceptance filter to allow letters for BOOL fields
  - Enables typing "true" or "false" instead of just "1" or "0"

- **Stale Status Messages** - Status messages persisted after navigation
  - Clear status message when refreshing (r key)
  - Clear status message when expanding/collapsing tables
  - Prevents confusion from outdated "Saved:" messages

### Changed
- **Test Organization** - Moved fault injection scenarios to proper location
  - Scenarios moved from `firmware/config/scenarios/` to `tests/scenarios/`
  - Updated all documentation references
  - More intuitive organization for test-related files

- **Documentation Structure** - Comprehensive documentation updates
  - Created `ENUM_SYSTEM.md` - Complete enum system guide (320 lines)
  - Created `TODO_AUDIT.md` - Categorized audit of all 27 TODOs/stubs (180 lines)
  - Created `RECENT_CHANGES.md` - Summary of all TUI improvements (350 lines)
  - Updated `FAULT_INJECTION.md` - New scenario paths (4 changes)
  - Updated `PROGRESS.md` - Phase 9 section with new paths

### Technical Details
- **Files Modified**: 15 files (tables, TUI, documentation)
- **Lines Changed**: ~353 lines of code, ~850 lines of documentation
- **Code Size Impact**: +248 bytes flash (111,048 bytes total, 0.2% increase)
- **Build Status**: Clean build, no warnings or errors

---

## [0.1.0] - 2025-11-09

### Completed Phases
- ✅ Phase 1: Project Foundation
- ✅ Phase 2: Platform Layer (GPIO, Timebase, Board Config)
- ✅ Phase 5: Register Map Structure
- ✅ Phase 8: Console TUI (Tables, Navigation, Edit, Enums)
- ✅ Phase 9: Fault Injection System (Scenarios, JSON Parser, Execution)

### Summary
- **Total Lines of Code**: ~3,370 lines (console), ~10,000+ lines (total)
- **Phases Complete**: 5/10 (50%)
- **Flash Usage**: 111,024 bytes (43% of 256 KB)
- **Tables Implemented**: 10 tables with 50+ fields
- **Scenarios**: 5 fault injection scenarios
- **Documentation**: 2,000+ lines across multiple files

### Next Steps
- Phase 3: Communication Drivers (RS-485, SLIP, NSP)
- Phase 4: Fixed-Point Math Library
- Phase 10: Physics Model Integration

---

## Version History

### Phase Completion Dates
| Phase | Date | Status |
|-------|------|--------|
| Phase 1 | 2025-11-05 | ✅ Complete |
| Phase 2 | 2025-11-05 | ✅ Complete |
| Phase 3 | - | ⏸️ Pending |
| Phase 4 | - | ⏸️ Pending |
| Phase 5 | 2025-11-08 | ✅ Complete |
| Phase 6 | - | ⏸️ Pending |
| Phase 7 | - | ⏸️ Pending |
| Phase 8 | 2025-11-09 | ✅ Complete |
| Phase 9 | 2025-11-09 | ✅ Complete |
| Phase 10 | - | ⏸️ Pending |

---

## Contributors
- Initial implementation: Claude Code (Anthropic)
- Project specification: User requirements
- Hardware validation: User testing

---

**For detailed technical information, see:**
- [ENUM_SYSTEM.md](ENUM_SYSTEM.md) - Enum field system documentation
- [FAULT_INJECTION.md](FAULT_INJECTION.md) - Fault injection user guide
- [PROGRESS.md](PROGRESS.md) - Detailed phase completion tracking
- [TODO_AUDIT.md](TODO_AUDIT.md) - Complete TODO/stub audit
- [RECENT_CHANGES.md](RECENT_CHANGES.md) - Latest update summary
