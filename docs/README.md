# Documentation Directory

This directory contains design baseline documents and requirements specifications for the NRWA-T6 emulator project.

## Contents

### Design Baseline

- **NRWA-T6_ICD_v10.02.pdf** - Official Interface Control Document from NewSpace Systems
  - Version: 10.02 (Effective Date: 18 December 2023)
  - **Status**: TO BE ADDED - This is the authoritative hardware specification
  - **Purpose**: Defines electrical, mechanical, and protocol specifications for NRWA-T6
  - **Critical Sections**:
    - Section 10.2: Digital I/O (RESET, FAULT pins)
    - Section 12: NSP Protocol Commands
    - Section 9.5: Protection and Fault Handling
    - Table 10-9: Signal Electrical Characteristics

### Requirements Documents

- **[RESET_FAULT_REQUIREMENTS.md](RESET_FAULT_REQUIREMENTS.md)** - Reset and fault handling requirements
  - Extracted from NRWA-T6 ICD v10.02
  - Defines hardware reset behavior, LCL trip conditions, and fault handling
  - Specifies test scenarios for reset during various operational states
  - Implementation guide for `wheel_model_reset()` and related functions

## Adding the Datasheet

**User Action Required**: Please add the NRWA-T6 ICD datasheet to this directory.

**Recommended Filename**: `NRWA-T6_ICD_v10.02.pdf`

**Why This Matters**:
1. **Traceability**: Links implementation to specific baseline version
2. **Regression Prevention**: Future changes can be validated against spec
3. **Collaboration**: Team members can reference the same document
4. **Compliance**: Demonstrates emulator fidelity to real hardware

**Git Considerations**:
- PDF files can be committed to git (this one is ~57 pages, reasonable size)
- Alternative: Store in git LFS if repo size becomes an issue
- Add to `.gitignore` if licensing prohibits redistribution (and document how to obtain it)

## Document Hierarchy

```
Project Root
├── SPEC.md              # High-level emulator specification (our design)
├── IMP.md               # Implementation plan (10 phases)
├── PROGRESS.md          # Development progress tracker
└── docs/
    ├── README.md                        # This file
    ├── NRWA-T6_ICD_v10.02.pdf          # Hardware specification (TO BE ADDED)
    └── RESET_FAULT_REQUIREMENTS.md     # Derived requirements (from ICD analysis)
```

**Relationship**:
- NRWA-T6 ICD (datasheet) → Hardware ground truth
- SPEC.md → Our interpretation for emulator
- RESET_FAULT_REQUIREMENTS.md → Detailed analysis of specific ICD sections
- IMP.md → How we implement SPEC.md
- PROGRESS.md → What we've completed

## Version Control

When the datasheet is added, update this README with:
- [ ] File SHA256 checksum (for integrity verification)
- [ ] Date added to repository
- [ ] Commit hash

Example:
```
NRWA-T6_ICD_v10.02.pdf
- SHA256: [to be calculated]
- Added: 2025-11-07
- Commit: [hash]
```

## Usage

**During Development**:
- Reference ICD for protocol details (NSP commands, SLIP framing, CRC)
- Reference ICD for electrical specs (voltage thresholds, timing)
- Reference ICD for protection thresholds (overspeed, overpower, etc.)

**During Testing**:
- Validate emulator behavior matches ICD specifications
- Use ICD command tables to verify NSP responses
- Use ICD fault tables to verify protection behavior

**For Reviews**:
- Cite ICD sections in code comments (e.g., "// Per ICD Section 10.2.6")
- Link ICD sections in commit messages
- Reference ICD in pull request descriptions
