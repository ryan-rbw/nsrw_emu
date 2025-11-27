# AI-Assisted Embedded Development: A Case Study

## NRWA-T6 Reaction Wheel Emulator Project

**Date**: November 2025
**Calendar Span**: 22 days (Nov 5-27, 2025)
**Estimated Effort**: 40-60 hours (part-time, variable daily investment)
**Methodology**: Spec-Driven Development with AI Coding Assistance

---

## Executive Summary

This report documents an experiment using AI-assisted coding (Claude Code) combined with spec-driven development to create a production-quality embedded systems project: a high-fidelity hardware-in-the-loop emulator for the NewSpace Systems NRWA-T6 reaction wheel.

The project achieved full completion with an estimated **40-60 hours of actual effort**—work that would typically require 400-600 hours (3-6 engineer-months) using traditional approaches.

---

## Project Deliverables

### What We Built

A dual-core bare-metal firmware for the RP2040 microcontroller that:

- **Emulates** an aerospace-grade reaction wheel with protocol-perfect RS-485/NSP communication
- **Simulates** real-time physics at 100 Hz with <200µs jitter
- **Provides** an interactive USB console for monitoring and control
- **Includes** 14 built-in test modes for hardware validation
- **Supports** fault injection scenarios for HIL testing

### Why It Matters

Real reaction wheels cost $10,000-50,000+. This emulator enables spacecraft software teams to develop and test their systems without expensive hardware, reducing both cost and schedule risk.

---

## Project Analytics

| Metric | Value |
|--------|-------|
| **C Source Files** | 37 |
| **Header Files** | 41 |
| **Lines of C Code** | 16,581 |
| **Lines of Documentation** | 6,916 |
| **Total Commits** | 100 |
| **Built-in Tests** | 46 (all passing) |
| **Test Modes** | 14 |
| **Binary Size** | 118 KB (46% of flash) |
| **RAM Usage** | 17.4 KB (66% of available) |

### Code Distribution

```
firmware/
├── platform/      ~700 LOC   - Hardware abstraction (GPIO, timers)
├── drivers/     ~2,500 LOC   - RS-485, SLIP, NSP, CRC
├── device/      ~4,000 LOC   - Physics model, commands, telemetry
├── console/     ~6,500 LOC   - TUI, tables, command interface
└── util/        ~1,000 LOC   - Ring buffers, fixed-point math
```

### Documentation Suite

| Document | Lines | Purpose |
|----------|-------|---------|
| SPEC.md | 494 | Complete system specification |
| IMP.md | 665 | 10-phase implementation plan |
| PROGRESS.md | 1,872 | Development status tracking |
| README.md | 355 | User guide and quick start |
| BUILD.md | 940 | Build system documentation |
| CLAUDE.md | 508 | AI assistant context guide |

---

## Development Approach

### Spec-Driven Development

1. **Specification First**: Complete SPEC.md defined all requirements before coding
2. **Phased Implementation**: IMP.md broke work into 10 testable phases
3. **Continuous Validation**: Each phase had acceptance criteria verified on hardware
4. **Living Documentation**: PROGRESS.md tracked completion in real-time

### AI Assistance Model

The AI assistant (Claude Code) contributed to:

- Understanding RP2040 platform constraints and Pico SDK APIs
- Implementing protocol stacks (SLIP, NSP, CRC-CCITT)
- Writing physics simulation with fixed-point math
- Debugging embedded-specific issues (alignment faults, timing)
- Maintaining documentation consistency
- Managing git workflow and commits

---

## Comparative Analysis

### Traditional vs AI-Assisted Development

| Factor | Traditional Approach | AI-Assisted (This Project) |
|--------|---------------------|---------------------------|
| **Effort** | 400-600 hours (3-6 months FTE) | 40-60 hours |
| **Team Size** | 2-3 engineers | 1 engineer + AI |
| **Documentation** | Often deferred | Concurrent with code |
| **Protocol Implementation** | Reference manual lookup | Instant recall + validation |
| **Debugging Time** | Hours per issue | Minutes with context |
| **Code Consistency** | Varies with fatigue | Maintained throughout |

### Estimated Effort Comparison

| Task Category | Traditional (est.) | AI-Assisted (est.) | Speedup |
|---------------|-------------------|-------------------|---------|
| Protocol stack (SLIP, NSP, CRC) | 80-120 hrs | 8-12 hrs | ~10x |
| Physics model + control loops | 80-120 hrs | 10-15 hrs | ~8x |
| Console/TUI system | 120-160 hrs | 12-18 hrs | ~10x |
| Documentation | 40-60 hrs | 5-8 hrs | ~8x |
| Debugging & integration | 80-120 hrs | 5-10 hrs | ~10x |
| **Total** | **400-600 hrs** | **40-60 hrs** | **~10x** |

*Note: Calendar span was 22 days with variable daily investment (minutes to hours per session).*

### Quality Indicators

- **46 automated tests** run at every boot
- **Zero known critical bugs** at completion
- **Protocol verified** against ICD specification
- **Real-time constraints met** (<200µs jitter achieved)

---

## Key Learnings

### What Worked Well

1. **Spec-first approach**: SPEC.md prevented scope creep and provided clear acceptance criteria
2. **Phased implementation**: Small, testable increments reduced integration risk
3. **Hardware-in-loop validation**: Each phase verified on actual Pico hardware
4. **AI context document**: CLAUDE.md gave the assistant project-specific knowledge

### Challenges Overcome

1. **ARM alignment faults**: AI helped identify and fix struct access issues
2. **Fixed-point precision**: Correct UQ format selection for each data type
3. **Dual-core synchronization**: Lock-free ring buffer design
4. **RS-485 timing**: DE/RE control with microsecond precision

### Recommendations for Future Projects

1. **Invest in specification**: Time spent on SPEC.md pays dividends
2. **Create AI context files**: Project-specific CLAUDE.md accelerates assistance
3. **Validate incrementally**: Don't batch integration—test each component
4. **Maintain documentation**: AI can help keep docs synchronized with code

---

## Conclusion

This experiment demonstrates that AI-assisted coding, combined with disciplined spec-driven development, can dramatically accelerate embedded systems projects without sacrificing quality.

The NRWA-T6 emulator—a non-trivial embedded project with real-time constraints, protocol compliance requirements, and hardware abstraction—was completed in approximately **one-fifth the traditional timeline** while maintaining high code quality and comprehensive documentation.

The approach is particularly effective for projects with:
- Well-defined protocols or specifications
- Complex but documented algorithms
- Need for extensive documentation
- Single developer with domain knowledge

---

**Project Repository**: github.com/ryan-rbw/nsrw_emu
**Final Status**: Phase 10 Complete - Production Ready
**Total Effort**: ~40-60 hours over 22 calendar days
