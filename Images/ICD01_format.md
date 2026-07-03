# dashCAN `.icd` File Format (magic `ICD01`)

Reverse-engineered from `dashCAN_send_custom_CAN_Messages.icd` (2769 bytes) and
cross-checked field-by-field against the dashCAN "Import" UI. Every value below
was validated against the running tool.

## Conventions

- All multi-byte integers are **little-endian `uint32`**.
- The format is a set of **fixed-size parallel-array tables** ("columns").
  Each column is a flat array of `uint32`; row *i* of every column describes the
  same entry. A separate `uint32` count says how many rows are actually used;
  unused slots are left at their default value (often non-zero).
- Two tables exist: the **Identifier table** (the CAN frames to transmit) and
  the **Signal table** (the data packed into those frames).

## Top-level layout

| Offset  | Size | Meaning                                            |
|---------|------|----------------------------------------------------|
| `0x000` | 5    | Magic ASCII `"ICD01"`                              |
| `0x005` | 4    | `idCount` — number of identifiers used (here 5)    |
| `0x009` | 448  | Identifier table: **7 columns × 16 rows × 4 bytes**|
| `0x1C9` | 4    | `sigCount` — number of signals used (here 20)      |
| `0x1CD` | 2304 | Signal table: **9 columns × 64 rows × 4 bytes**    |
| `0xACD` | 4    | Trailer / checksum (`0x000370E7` in this file)     |
| `0xAD1` | —    | EOF (2769 bytes total)                              |

`0x009 + 7*16*4 = 0x1C9` and `0x1CD + 9*64*4 = 0xACD`, so the sections abut exactly.

### Identifier table — base `0x009`, 16 rows/column, stride `16*4 = 0x40`

| Col | Offset  | Field            | Notes                                   |
|-----|---------|------------------|-----------------------------------------|
| 0   | `0x009` | CAN ID           | 11-bit standard ID (0x600…0x604)        |
| 1   | `0x049` | DLC / Length     | Frame length in bytes (8)               |
| 2   | `0x089` | Repeat           | Repeat ÷10 → ms. `5` ⇒ **50 ms**        |
| 3–6 | `0x0C9` | Reserved (zero)  | ID-type / data-init / flags — all 0 here|

### Signal table — base `0x1CD`, 64 rows/column, stride `64*4 = 0x100`

| Col | Offset  | Field        | Notes                                              |
|-----|---------|--------------|----------------------------------------------------|
| 0   | `0x1CD` | Data type    | dashCAN signal enum (see table below)              |
| 1   | `0x2CD` | Multiply     | Fixed-point ×1000. `1000` ⇒ **1.000**              |
| 2   | `0x3CD` | Add (offset) | Raw integer offset added before scaling            |
| 3   | `0x4CD` | Min          | Raw clamp minimum                                  |
| 4   | `0x5CD` | Max          | Raw clamp maximum                                  |
| 5   | `0x6CD` | Invert       | 0 = normal, 1 = inverted                           |
| 6   | `0x7CD` | CAN ID       | Which identifier this signal is packed into        |
| 7   | `0x8CD` | Start bit    | Bit offset within the frame (0,16,32,48…)          |
| 8   | `0x9CD` | Length       | Signal length in bits (16)                         |

Byte order is **little-endian** for all packed signals (matches the UI's
"Little endian" selection); it is not stored as a per-signal column in `ICD01`.

## Decoded contents of this file

**Identifiers** (all DLC 8, repeat 50 ms): `0x600 0x601 0x602 0x603 0x604`

**Signals** (type = dashCAN enum, scale = mul/1000, off = add):

| # | Signal name (UI)                     | enum  | CAN ID | start | len | mul   | add  | min | max   |
|---|--------------------------------------|-------|--------|-------|-----|-------|------|-----|-------|
| 0 | Sensors RPM                          | 0x48  | 0x600  | 0     | 16  | 1.000 | 0    | 0   | 10000 |
| 1 | Sensors engine coolant temperature   | 0x4B  | 0x600  | 16    | 16  | 1.000 | 400  | 0   | 1800  |
| 2 | Sensors manifold absolute pressure   | 0x4A  | 0x600  | 32    | 16  | 1.000 | 0    | 0   | 10000 |
| 3 | Sensors intake air temperature       | 0x4C  | 0x600  | 48    | 16  | 1.000 | 400  | 0   | 1800  |
| 4 | Sensors exhaust gas temperature      | 0x4D  | 0x601  | 0     | 16  | 1.000 | 0    | 0   | 1200  |
| 5 | Sensors oil pressure                 | 0x62  | 0x601  | 16    | 16  | 1.000 | 0    | 0   | 10000 |
| 6 | Sensors oil temperature              | 0x61  | 0x601  | 32    | 16  | 1.000 | 400  | 0   | 2000  |
| 7 | Sensors fuel rail pressure           | 0x56  | 0x601  | 48    | 16  | 1.000 | 0    | 0   | 10000 |
| 8 | Sensors lambda                       | 0x4F  | 0x602  | 0     | 16  | 1.000 | 0    | 0   | 9999  |
| 9 | Sensors throttle pedal position      | 0x58  | 0x602  | 16    | 16  | 1.000 | 0    | 0   | 10000 |
| 10| Sensors vehicle speed signal         | 0x4E  | 0x602  | 32    | 16  | 1.000 | 0    | 0   | 3200  |
| 11| Sensors throttle position sensor     | 0x59  | 0x603  | 0     | 16  | 1.000 | 0    | 0   | 10000 |
| 12| Sensors battery voltage              | 0x53  | 0x603  | 16    | 16  | 1.000 | 0    | 0   | 10000 |
| 13| Ignition advance                     | 0x81  | 0x603  | 32    | 16  | 1.000 | 400  | 0   | 10000 |
| 14| Injection fuel trim (short term)     | 0xED  | 0x603  | 48    | 16  | 1.000 | 1000 | 0   | 10000 |
| 15| Injection fuel trim (long term add.) | 0xEE  | 0x604  | 0     | 16  | 1.000 | 1000 | 0   | 10000 |
| 16| ECU Engine Output Power              | 0x70  | 0x604  | 48    | 16  | 1.000 | 0    | 0   | 10000 |
| 17| Knock Amplitude                      | 0xF7  | 0x604  | 16    | 16  | 1.000 | 0    | 0   | 10000 |
| 18| ECU Engine Output Torque             | 0x6F  | 0x602  | 48    | 16  | 1.000 | 0    | 0   | 10000 |
| 19| ECU Gear Number                      | 0x11B | 0x604  | 32    | 16  | 1.000 | 0    | 0   | 10000 |

### Decode/scale formula

Physical value the UI shows is derived from the raw 16-bit signal as:

```
raw   = signal bits (little-endian, unsigned)
value = (raw / multiply_fixed_x1000) ... then UI subtracts the "Add" offset
```

When *encoding* a physical value to transmit (the direction the dashboard uses):

```
raw = clamp( round(physical * (multiply/1000)) + add, min, max )
```

The `add` column is an integer pre-offset (e.g. coolant/IAT use +400 so a 0 °C
reading transmits as 400; trims use +1000 so 0 % transmits as 1000).

## dashCAN signal-type enum (the codes seen here)

| enum  | UI name                              |
|-------|--------------------------------------|
| 0x48  | Sensors RPM                          |
| 0x4A  | Sensors manifold absolute pressure   |
| 0x4B  | Sensors engine coolant temperature   |
| 0x4C  | Sensors intake air temperature       |
| 0x4D  | Sensors exhaust gas temperature      |
| 0x4E  | Sensors vehicle speed signal         |
| 0x4F  | Sensors lambda                       |
| 0x53  | Sensors battery voltage              |
| 0x56  | Sensors fuel rail pressure           |
| 0x58  | Sensors throttle pedal position      |
| 0x59  | Sensors throttle position sensor     |
| 0x61  | Sensors oil temperature              |
| 0x62  | Sensors oil pressure                 |
| 0x6F  | ECU engine output torque             |
| 0x81  | Ignition advance                     |
| 0xED  | Injection fuel trim (short term)     |
| 0xEE  | Injection fuel trim (long term add.) |
| 0x70  | (unknown — not labelled in UI)       |
| 0xF7  | (unknown — not labelled in UI)       |
| 0x11B | (unknown — not labelled in UI)       |

> Only the codes present in this file are mapped. The full dashCAN PID enum is
> larger; extend the table as new `.icd` files reveal more codes.
