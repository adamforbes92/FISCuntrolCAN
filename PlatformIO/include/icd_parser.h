#pragma once

/*
 * icd_parser.h — Parser for dashCAN "ICD01" custom-CAN definition files.
 *
 * Format reverse-engineered in docs/ICD01_format.md. The file is a set of
 * fixed-size parallel-array tables of little-endian uint32 values:
 *   - Identifier table: 7 columns x 16 rows  (the CAN frames to transmit)
 *   - Signal table:     9 columns x 64 rows  (the data packed into the frames)
 *
 * This parser is allocation-free and safe to run on the ESP32: it validates the
 * buffer length before every read and copies everything into POD structs.
 */

#include <Arduino.h>

static const uint8_t ICD_MAX_IDENTIFIERS = 16;
static const uint8_t ICD_MAX_SIGNALS     = 64;

struct IcdIdentifier {
    uint32_t canId;     // 11-bit standard CAN ID
    uint8_t  dlc;       // frame length in bytes
    uint16_t repeatMs;  // transmit period in milliseconds
};

struct IcdSignal {
    uint16_t dataType;  // dashCAN signal-type enum (see docs)
    uint32_t multiply;  // scale, fixed-point x1000 (1000 == 1.000)
    int32_t  add;       // integer offset applied before clamping
    int32_t  minVal;    // raw clamp minimum
    int32_t  maxVal;    // raw clamp maximum
    bool     invert;    // value inversion flag
    uint32_t canId;     // identifier this signal is packed into
    uint8_t  startBit;  // bit offset within the frame
    uint8_t  length;    // signal length in bits
};

struct IcdFile {
    uint8_t       identifierCount;
    uint8_t       signalCount;
    IcdIdentifier identifiers[ICD_MAX_IDENTIFIERS];
    IcdSignal     signals[ICD_MAX_SIGNALS];
    uint32_t      trailer;  // file footer / checksum word
};

/*
 * Parse an in-memory .icd buffer. Returns true on success and fills `out`.
 * Returns false if the buffer is too small, the magic is wrong, or the declared
 * counts exceed the table capacities.
 */
bool icdParse(const uint8_t* data, size_t len, IcdFile& out);

/*
 * Encode a physical value into the raw integer dashCAN expects for `sig`:
 *   raw = clamp( round(physical * multiply/1000) + add, minVal, maxVal )
 * Use this when transmitting; mask/shift the result into the frame at
 * sig.startBit / sig.length (little-endian).
 */
uint32_t icdEncodeSignal(const IcdSignal& sig, float physical);
