# NRWA-T6 Emulator Protocol Reference

**Complete byte-level structure documentation for all NSP messages**

This document provides the authoritative reference for all message structures transmitted and received by the NRWA-T6 emulator. All structures are verified against the ICD specification.

---

## Table of Contents

1. [Endianness and Encoding](#endianness-and-encoding)
2. [NSP Packet Structure](#nsp-packet-structure)
3. [NSP Commands](#nsp-commands)
4. [Telemetry Blocks](#telemetry-blocks)
5. [Register Map](#register-map)
6. [Fixed-Point Formats](#fixed-point-formats)

---

## Endianness and Encoding

### Critical Rules

| Data Type | Endianness | Notes |
|-----------|------------|-------|
| **NSP CRC** | Little-endian (LSB-first) | `[CRC_L, CRC_H]` per ICD Table 11-1 |
| **Telemetry Data** | Big-endian (MSB-first) | Network byte order for all multi-byte fields |
| **Register Values** | Little-endian (LSB-first) | PEEK/POKE uses little-endian |

**Example**:
- Value `0x12345678` in telemetry → bytes: `12 34 56 78` (big-endian)
- Value `0x12345678` in register → bytes: `78 56 34 12` (little-endian)
- CRC value `0xA53F` → bytes: `3F A5` (little-endian)

---

## NSP Packet Structure

### Packet Format (after SLIP decoding)

```
[Dest | Src | Ctrl | Data... | CRC_L | CRC_H]
```

**There is NO Length field** - packet boundaries determined by SLIP framing.

### Header Fields

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 1 | Dest | Destination address (0x00-0x07 or 0xFF for broadcast) |
| 1 | 1 | Src | Source address (0x00-0x07) |
| 2 | 1 | Ctrl | Control byte (see below) |
| 3 | N | Data | Command payload (0-255 bytes) |
| 3+N | 1 | CRC_L | CRC low byte (LSB-first) |
| 4+N | 1 | CRC_H | CRC high byte |

### Control Byte Format

```
Bit 7     Bit 6  Bit 5  Bits 4-0
[Poll] | [B] | [A] | [Command]
```

- **Poll** (bit 7): 1 = reply expected, 0 = no reply
- **B** (bit 6): ACK bit (preserved in replies)
- **A** (bit 5): ACK bit (1 = ACK, 0 = NACK in replies)
- **Command** (bits 4-0): 5-bit command code (0x00-0x1F)

**Examples**:
- `0x80` = Poll=1, B=0, A=0, Cmd=0x00 (PING with poll)
- `0x20` = Poll=0, B=0, A=1, Cmd=0x00 (ACK reply)

### CRC Calculation

- **Algorithm**: CRC-CCITT
- **Polynomial**: 0x1021 (reversed: 0x8408 for LSB-first)
- **Initial value**: 0xFFFF
- **Coverage**: Dest + Src + Ctrl + Data (NOT including CRC bytes)
- **Transmission**: LSB-first `[CRC_L, CRC_H]`

---

## NSP Commands

### Command Codes

| Code | Name | Description |
|------|------|-------------|
| 0x00 | PING | Connectivity test (ACK with no data) |
| 0x02 | PEEK | Read register(s) |
| 0x03 | POKE | Write register(s) |
| 0x07 | APP-TELEM | Request telemetry block |
| 0x08 | APP-CMD | Application command (mode/setpoint changes) |
| 0x09 | CLEAR-FAULT | Clear latched faults |
| 0x0A | CONFIG-PROT | Configure protection thresholds |
| 0x0B | TRIP-LCL | Trigger local current limit (test) |

### 0x00: PING

**Request**:
```
Dest | Src | Ctrl | CRC_L | CRC_H
```
- No data payload
- Ctrl typically `0x80` (Poll=1, Cmd=0x00)

**Reply**:
```
Dest | Src | Ctrl | CRC_L | CRC_H
```
- Ctrl = `0x20` (A=1, Cmd=0x00) for ACK
- No data payload

### 0x02: PEEK (Read Registers)

**Request**:
```
Dest | Src | Ctrl | Addr_H | Addr_L | Count | CRC_L | CRC_H
```

| Field | Size | Type | Description |
|-------|------|------|-------------|
| Addr_H | 1 | uint8 | Register address high byte |
| Addr_L | 1 | uint8 | Register address low byte |
| Count | 1 | uint8 | Number of 32-bit registers to read (1-63) |

**Reply (ACK)**:
```
Dest | Src | Ctrl | Data... | CRC_L | CRC_H
```
- Data = Count × 4 bytes (little-endian 32-bit values)
- Example: Reading 2 registers returns 8 bytes

**Example**:
```
Request:  01 11 82 03 00 02 [CRC]  // Read 2 regs starting at 0x0300
Reply:    11 01 22 [8 bytes] [CRC] // 2×4=8 bytes, little-endian
```

### 0x03: POKE (Write Registers)

**Request**:
```
Dest | Src | Ctrl | Addr_H | Addr_L | Count | Data... | CRC_L | CRC_H
```

| Field | Size | Type | Description |
|-------|------|------|-------------|
| Addr_H | 1 | uint8 | Register address high byte |
| Addr_L | 1 | uint8 | Register address low byte |
| Count | 1 | uint8 | Number of 32-bit registers to write (1-63) |
| Data | Count×4 | bytes | Register values (little-endian) |

**Reply**:
```
Dest | Src | Ctrl | CRC_L | CRC_H
```
- ACK (A=1) if successful
- NACK (A=0) if read-only or invalid address

### 0x07: APP-TELEM (Application Telemetry)

**Request**:
```
Dest | Src | Ctrl | Block_ID | CRC_L | CRC_H
```

| Field | Size | Type | Description |
|-------|------|------|-------------|
| Block_ID | 1 | uint8 | Telemetry block ID (0x00-0x04) |

**Reply (ACK)**:
```
Dest | Src | Ctrl | Telemetry_Data... | CRC_L | CRC_H
```
- Telemetry data size depends on Block_ID (see Telemetry Blocks section)
- **All telemetry data is BIG-ENDIAN**

### 0x08: APP-CMD (Application Command)

**Subcommands**:

| Subcmd | Name | Payload |
|--------|------|---------|
| 0x00 | SET-MODE | `[subcmd, mode]` - mode: 0-3 |
| 0x01 | SET-SPEED | `[subcmd, speed_b3, speed_b2, speed_b1, speed_b0]` - UQ14.18 RPM |
| 0x02 | SET-CURRENT | `[subcmd, current_b3, current_b2, current_b1, current_b0]` - UQ18.14 mA |
| 0x03 | SET-TORQUE | `[subcmd, torque_b3, torque_b2, torque_b1, torque_b0]` - UQ18.14 mN·m |
| 0x04 | SET-PWM | `[subcmd, duty_b1, duty_b0]` - UQ8.8 % |
| 0x05 | SET-DIRECTION | `[subcmd, direction]` - 0=POS, 1=NEG |

**All multi-byte values in APP-CMD are BIG-ENDIAN**

**Example (SET-SPEED to 1000.0 RPM)**:
```
UQ14.18 encoding of 1000.0 = 0x000F4240 (1000 << 18)
Request: 01 11 88 01 00 0F 42 40 [CRC]
         [Dest Src Ctrl Sub ---Speed--- CRC]
```

### 0x09: CLEAR-FAULT

**Request**:
```
Dest | Src | Ctrl | Mask_3 | Mask_2 | Mask_1 | Mask_0 | CRC_L | CRC_H
```
- Mask = 32-bit bitmask (big-endian) of faults to clear
- Set bit to 1 to clear corresponding fault

**Reply**: ACK (no data)

### 0x0A: CONFIG-PROT (Configure Protection)

**Request**:
```
Dest | Src | Ctrl | Param_ID | Value_3 | Value_2 | Value_1 | Value_0 | CRC_L | CRC_H
```

| Param_ID | Parameter | Type | Units |
|----------|-----------|------|-------|
| 0x00 | Overvoltage threshold | UQ16.16 | V |
| 0x01 | Overspeed fault | UQ14.18 | RPM |
| 0x02 | Overspeed soft | UQ14.18 | RPM |
| 0x03 | Max duty cycle | UQ8.8 | % |
| 0x04 | Motor overpower | UQ16.16 | W |
| 0x05 | Soft overcurrent | UQ16.16 | A |

**Value is BIG-ENDIAN**

**Reply**: ACK (no data)

### 0x0B: TRIP-LCL

**Request**:
```
Dest | Src | Ctrl | CRC_L | CRC_H
```
- No payload

**Reply**: ACK (no data)

---

## Telemetry Blocks

**ALL TELEMETRY DATA IS BIG-ENDIAN (MSB-first)**

### Block 0x00: STANDARD (38 bytes)

| Offset | Size | Field | Type | Units | Description |
|--------|------|-------|------|-------|-------------|
| 0 | 4 | Status Register | uint32 | - | 0x00000001=Operational, bit31=LCL trip |
| 4 | 4 | Fault Status | uint32 | - | Active faults bitmask |
| 8 | 4 | Fault Latch | uint32 | - | Latched faults bitmask |
| 12 | 4 | Warning Status | uint32 | - | Active warnings bitmask |
| 16 | 1 | Control Mode | uint8 | - | 0=CURRENT, 1=SPEED, 2=TORQUE, 3=PWM |
| 17 | 1 | Direction | uint8 | - | 0=POSITIVE, 1=NEGATIVE |
| 18 | 4 | Speed | UQ14.18 | RPM | Angular velocity |
| 22 | 4 | Current | UQ18.14 | mA | Motor current |
| 26 | 4 | Torque | UQ18.14 | mN·m | Output torque |
| 30 | 4 | Power | UQ18.14 | mW | Electrical power |
| 34 | 4 | Momentum | UQ18.14 | µN·m·s | Angular momentum |

**Example bytes for 1000 RPM**:
```
Speed (UQ14.18): 1000.0 RPM = 0x000F4240
Bytes: 00 0F 42 40  (big-endian)
```

### Block 0x01: TEMPERATURES (6 bytes)

| Offset | Size | Field | Type | Units | Description |
|--------|------|-------|------|-------|-------------|
| 0 | 2 | Motor Temperature | UQ8.8 | °C | Simulated at 25.0°C |
| 2 | 2 | Driver Temperature | UQ8.8 | °C | Simulated at 25.0°C |
| 4 | 2 | Board Temperature | UQ8.8 | °C | Simulated at 25.0°C |

**Example bytes for 25.0°C**:
```
Temp (UQ8.8): 25.0 = 0x1900 (25 << 8)
Bytes: 19 00  (big-endian)
```

### Block 0x02: VOLTAGES (12 bytes)

| Offset | Size | Field | Type | Units | Description |
|--------|------|-------|------|-------|-------------|
| 0 | 4 | Bus Voltage | UQ16.16 | V | Main bus voltage (default 28V) |
| 4 | 4 | Phase A Voltage | UQ16.16 | V | Motor phase A voltage |
| 8 | 4 | Phase B Voltage | UQ16.16 | V | Motor phase B voltage |

### Block 0x03: CURRENTS (12 bytes)

| Offset | Size | Field | Type | Units | Description |
|--------|------|-------|------|-------|-------------|
| 0 | 4 | Phase A Current | UQ18.14 | mA | Motor phase A current |
| 4 | 4 | Phase B Current | UQ18.14 | mA | Motor phase B current |
| 8 | 4 | Bus Current | UQ18.14 | mA | Main bus current |

### Block 0x04: DIAGNOSTICS (18 bytes)

| Offset | Size | Field | Type | Units | Description |
|--------|------|-------|------|-------|-------------|
| 0 | 4 | Tick Count | uint32 | - | Physics tick counter (100 Hz) |
| 4 | 4 | Uptime | uint32 | s | Uptime in seconds |
| 8 | 4 | Fault Count | uint32 | - | Total fault events (placeholder: 0) |
| 12 | 4 | Command Count | uint32 | - | Total commands received (placeholder: 0) |
| 16 | 2 | Max Jitter | uint16 | µs | Maximum tick jitter (placeholder: 0) |

---

## Register Map

### Address Ranges

| Range | Access | Description |
|-------|--------|-------------|
| 0x0000-0x00FF | RO | Device Information |
| 0x0100-0x01FF | RW | Protection Configuration |
| 0x0200-0x02FF | RW | Control Registers |
| 0x0300-0x03FF | RO | Status Registers |
| 0x0400-0x04FF | RW/RO | Fault & Diagnostics |

**ALL REGISTER VALUES ARE LITTLE-ENDIAN** (LSB-first when accessed via PEEK/POKE)

### Device Information (0x0000-0x00FF) - READ ONLY

| Address | Size | Name | Type | Description |
|---------|------|------|------|-------------|
| 0x0000 | 4 | DEVICE_ID | uint32 | 0x4E525754 ("NRWT") |
| 0x0004 | 4 | FIRMWARE_VERSION | uint32 | BCD format (0x00010203 = v1.2.3) |
| 0x0008 | 2 | HARDWARE_REVISION | uint16 | Hardware revision number |
| 0x000A | 4 | SERIAL_NUMBER | uint32 | Serial number |
| 0x000E | 4 | BUILD_TIMESTAMP | uint32 | Unix timestamp |
| 0x0012 | 4 | INERTIA_KGM2 | UQ16.16 | Wheel inertia (kg·m²) |
| 0x0016 | 4 | MOTOR_KT_NMA | UQ16.16 | Torque constant k_t (N·m/A) |

### Protection Configuration (0x0100-0x01FF) - READ/WRITE

| Address | Size | Name | Type | Default | Description |
|---------|------|------|------|---------|-------------|
| 0x0100 | 4 | OVERVOLTAGE_THRESHOLD | UQ16.16 | 36.0 V | Hard overvoltage limit |
| 0x0104 | 4 | OVERSPEED_FAULT_RPM | UQ14.18 | 6000.0 | Hard overspeed limit |
| 0x0108 | 4 | MAX_DUTY_CYCLE | UQ16.16 | 97.85% | Maximum PWM duty cycle |
| 0x010C | 4 | MOTOR_OVERPOWER_LIMIT | UQ18.14 | 100000 mW | Overpower limit (100W) |
| 0x0110 | 4 | BRAKING_LOAD_SETPOINT | UQ16.16 | 31.0 V | Braking load voltage |
| 0x0114 | 4 | SOFT_OVERCURRENT_MA | UQ18.14 | 6000 mA | Soft overcurrent (6A) |
| 0x0118 | 4 | SOFT_OVERSPEED_RPM | UQ14.18 | 5000.0 | Soft overspeed warning |
| 0x011C | 4 | PROTECTION_ENABLE | uint32 | 0x3F | Protection enable bitmask |

### Control Registers (0x0200-0x02FF) - READ/WRITE

| Address | Size | Name | Type | Default | Description |
|---------|------|------|------|---------|-------------|
| 0x0200 | 1 | CONTROL_MODE | uint8 | 0 | 0=CURRENT, 1=SPEED, 2=TORQUE, 3=PWM |
| 0x0204 | 4 | SPEED_SETPOINT_RPM | UQ14.18 | 0.0 | Speed setpoint |
| 0x0208 | 4 | CURRENT_SETPOINT_MA | UQ18.14 | 0.0 | Current setpoint |
| 0x020C | 4 | TORQUE_SETPOINT_MNM | UQ18.14 | 0.0 | Torque setpoint |
| 0x0210 | 4 | PWM_DUTY_CYCLE | UQ16.16 | 0.0 | PWM duty cycle |
| 0x0214 | 1 | DIRECTION | uint8 | 0 | 0=POSITIVE, 1=NEGATIVE |
| 0x0218 | 4 | PI_KP | UQ16.16 | varies | Proportional gain |
| 0x021C | 4 | PI_KI | UQ16.16 | varies | Integral gain |
| 0x0220 | 4 | PI_I_MAX_MA | UQ18.14 | varies | Integral limit |

### Status Registers (0x0300-0x03FF) - READ ONLY

| Address | Size | Name | Type | Description |
|---------|------|------|------|-------------|
| 0x0300 | 4 | CURRENT_SPEED_RPM | UQ14.18 | Current wheel speed (RPM) |
| 0x0304 | 4 | CURRENT_SPEED_RADS | UQ16.16 | Current wheel speed (rad/s) |
| 0x0308 | 4 | CURRENT_MOMENTUM_NMS | UQ18.14 | Angular momentum (N·m·s) |
| 0x030C | 4 | CURRENT_TORQUE_MNM | UQ18.14 | Output torque (mN·m) |
| 0x0310 | 4 | CURRENT_CURRENT_MA | UQ18.14 | Motor current (mA) |
| 0x0314 | 4 | CURRENT_POWER_MW | UQ18.14 | Electrical power (mW) |
| 0x0318 | 4 | CURRENT_VOLTAGE_V | UQ16.16 | Bus voltage (V) |
| 0x031C | 4 | TOTAL_ENERGY_WH | uint32 | Total energy (mW·h) |
| 0x0320 | 4 | TOTAL_REVOLUTIONS | uint32 | Total revolutions |
| 0x0324 | 4 | UPTIME_SECONDS | uint32 | Uptime (s) |
| 0x0328 | 4 | TEMP_MOTOR_C | UQ16.16 | Motor temperature (°C) |
| 0x032C | 4 | TEMP_ELECTRONICS_C | UQ16.16 | Electronics temp (°C) |
| 0x0330 | 4 | TEMP_BEARING_C | UQ16.16 | Bearing temp (°C) |

### Fault & Diagnostics (0x0400-0x04FF) - Mixed RO/RW

| Address | Size | Name | Type | Access | Description |
|---------|------|------|------|--------|-------------|
| 0x0400 | 4 | FAULT_STATUS | uint32 | RO | Active fault bitmask |
| 0x0404 | 4 | FAULT_LATCH | uint32 | RW | Latched fault bitmask |
| 0x0408 | 4 | WARNING_STATUS | uint32 | RO | Warning bitmask |
| 0x040C | 4 | COMM_ERRORS_CRC | uint32 | RO | CRC error count |
| 0x0410 | 4 | COMM_ERRORS_FRAMING | uint32 | RO | Framing error count |
| 0x0414 | 4 | COMM_ERRORS_OVERRUN | uint32 | RO | Overrun count |
| 0x0418 | 4 | COMM_PACKETS_RX | uint32 | RO | Packets received |
| 0x041C | 4 | COMM_PACKETS_TX | uint32 | RO | Packets transmitted |
| 0x0420 | 4 | TICK_JITTER_MAX_US | uint32 | RO | Max jitter (µs) |
| 0x0424 | 4 | TICK_JITTER_AVG_US | uint32 | RO | Avg jitter (µs) |
| 0x0428 | 1 | LAST_COMMAND_CODE | uint8 | RO | Last NSP command |
| 0x042C | 4 | LAST_COMMAND_TIMESTAMP | uint32 | RO | Timestamp (ms) |

---

## Fixed-Point Formats

### UQ14.18 (Speed/RPM)
- **Total bits**: 32
- **Integer bits**: 14 (range: 0 to 16383)
- **Fractional bits**: 18 (resolution: 1/262144 ≈ 0.0000038)
- **Encoding**: `value_int = (float_value * 262144)`
- **Decoding**: `float_value = (value_int / 262144.0)`

**Example**: 1000.0 RPM
```
Encoded: 1000.0 * 262144 = 262144000 = 0x0F9E8680
Big-endian bytes: 0F 9E 86 80
```

### UQ16.16 (Voltage, Percentage)
- **Total bits**: 32
- **Integer bits**: 16 (range: 0 to 65535)
- **Fractional bits**: 16 (resolution: 1/65536 ≈ 0.000015)
- **Encoding**: `value_int = (float_value * 65536)`
- **Decoding**: `float_value = (value_int / 65536.0)`

**Example**: 28.0 V
```
Encoded: 28.0 * 65536 = 1835008 = 0x001C0000
Big-endian bytes: 00 1C 00 00
Little-endian bytes (registers): 00 00 1C 00
```

### UQ18.14 (Torque, Current, Power)
- **Total bits**: 32
- **Integer bits**: 18 (range: 0 to 262143)
- **Fractional bits**: 14 (resolution: 1/16384 ≈ 0.000061)
- **Encoding**: `value_int = (float_value * 16384)`
- **Decoding**: `float_value = (value_int / 16384.0)`

**Example**: 50.0 mN·m
```
Encoded: 50.0 * 16384 = 819200 = 0x000C8000
Big-endian bytes: 00 0C 80 00
```

### UQ8.8 (Temperature, Small Values)
- **Total bits**: 16
- **Integer bits**: 8 (range: 0 to 255)
- **Fractional bits**: 8 (resolution: 1/256 ≈ 0.0039)
- **Encoding**: `value_int = (float_value * 256)`
- **Decoding**: `float_value = (value_int / 256.0)`

**Example**: 25.0°C
```
Encoded: 25.0 * 256 = 6400 = 0x1900
Big-endian bytes: 19 00
```

---

## Quick Reference

### Endianness Summary

| Context | Endianness | Example |
|---------|------------|---------|
| NSP CRC | Little-endian | `0xA53F` → `3F A5` |
| Telemetry fields | Big-endian | `0x12345678` → `12 34 56 78` |
| APP-CMD payloads | Big-endian | `0x000F4240` → `00 0F 42 40` |
| Register values (PEEK/POKE) | Little-endian | `0x12345678` → `78 56 34 12` |

### Common Mistakes to Avoid

1. **Mixing endianness**: Telemetry is big-endian, registers are little-endian
2. **Missing Length field**: NSP packets have NO length field (use SLIP framing)
3. **Wrong CRC byte order**: Always LSB-first `[CRC_L, CRC_H]`
4. **Wrong fixed-point format**: Check if UQ14.18, UQ16.16, or UQ18.14
5. **Multi-byte alignment**: Registers are 4-byte aligned (addresses like 0x0100, 0x0104, etc.)

---

**Document Version**: 1.0
**Last Updated**: 2025-11-22
**Emulator Version**: See `REG_FIRMWARE_VERSION` (0x0004)
