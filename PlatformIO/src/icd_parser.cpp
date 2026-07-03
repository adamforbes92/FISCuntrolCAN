/*
 * icd_parser.cpp — Implementation of the dashCAN "ICD01" parser.
 * See icd_parser.h and docs/ICD01_format.md.
 */

#include "icd_parser.h"
#include <math.h>
#include <string.h>

namespace {

// --- On-disk layout constants (all offsets in bytes) ---
const char    ICD_MAGIC[5]      = {'I', 'C', 'D', '0', '1'};
const size_t  ICD_ID_COUNT_OFF  = 0x05;
const size_t  ICD_ID_BASE       = 0x09;   // first identifier column
const size_t  ICD_ID_ROWS       = 16;     // rows per identifier column
const size_t  ICD_ID_STRIDE     = ICD_ID_ROWS * 4;

const size_t  ICD_SIG_COUNT_OFF = 0x1C9;
const size_t  ICD_SIG_BASE      = 0x1CD;  // first signal column
const size_t  ICD_SIG_ROWS      = 64;     // rows per signal column
const size_t  ICD_SIG_STRIDE    = ICD_SIG_ROWS * 4;

// Identifier columns
const size_t  ID_COL_CANID  = 0;
const size_t  ID_COL_DLC    = 1;
const size_t  ID_COL_REPEAT = 2;

// Signal columns
const size_t  SIG_COL_TYPE  = 0;
const size_t  SIG_COL_MUL   = 1;
const size_t  SIG_COL_ADD   = 2;
const size_t  SIG_COL_MIN   = 3;
const size_t  SIG_COL_MAX   = 4;
const size_t  SIG_COL_INV   = 5;
const size_t  SIG_COL_CANID = 6;
const size_t  SIG_COL_START = 7;
const size_t  SIG_COL_LEN   = 8;
const size_t  SIG_NUM_COLS  = 9;

inline uint32_t rdU32(const uint8_t* d, size_t off) {
    return  (uint32_t)d[off]
          | ((uint32_t)d[off + 1] << 8)
          | ((uint32_t)d[off + 2] << 16)
          | ((uint32_t)d[off + 3] << 24);
}

inline size_t idCell(size_t col, size_t row) {
    return ICD_ID_BASE + col * ICD_ID_STRIDE + row * 4;
}

inline size_t sigCell(size_t col, size_t row) {
    return ICD_SIG_BASE + col * ICD_SIG_STRIDE + row * 4;
}

} // namespace

bool icdParse(const uint8_t* data, size_t len, IcdFile& out) {
    if (data == nullptr) return false;

    // The trailer word sits immediately after the signal table; the file must be
    // at least this long for every fixed read below to be in bounds.
    const size_t trailerOff = ICD_SIG_BASE + SIG_NUM_COLS * ICD_SIG_STRIDE;
    if (len < trailerOff + 4) return false;

    if (memcmp(data, ICD_MAGIC, sizeof(ICD_MAGIC)) != 0) return false;

    memset(&out, 0, sizeof(out));

    uint32_t idCount  = rdU32(data, ICD_ID_COUNT_OFF);
    uint32_t sigCount = rdU32(data, ICD_SIG_COUNT_OFF);
    if (idCount  > ICD_MAX_IDENTIFIERS) return false;
    if (sigCount > ICD_MAX_SIGNALS)     return false;

    out.identifierCount = (uint8_t)idCount;
    out.signalCount     = (uint8_t)sigCount;

    for (uint32_t i = 0; i < idCount; ++i) {
        IcdIdentifier& id = out.identifiers[i];
        id.canId    = rdU32(data, idCell(ID_COL_CANID,  i));
        id.dlc      = (uint8_t)rdU32(data, idCell(ID_COL_DLC,    i));
        id.repeatMs = (uint16_t)(rdU32(data, idCell(ID_COL_REPEAT, i)) * 10);
    }

    for (uint32_t i = 0; i < sigCount; ++i) {
        IcdSignal& s = out.signals[i];
        s.dataType = (uint16_t)rdU32(data, sigCell(SIG_COL_TYPE,  i));
        s.multiply = rdU32(data, sigCell(SIG_COL_MUL,   i));
        s.add      = (int32_t)rdU32(data, sigCell(SIG_COL_ADD,   i));
        s.minVal   = (int32_t)rdU32(data, sigCell(SIG_COL_MIN,   i));
        s.maxVal   = (int32_t)rdU32(data, sigCell(SIG_COL_MAX,   i));
        s.invert   = rdU32(data, sigCell(SIG_COL_INV,   i)) != 0;
        s.canId    = rdU32(data, sigCell(SIG_COL_CANID, i));
        s.startBit = (uint8_t)rdU32(data, sigCell(SIG_COL_START, i));
        s.length   = (uint8_t)rdU32(data, sigCell(SIG_COL_LEN,   i));
    }

    out.trailer = rdU32(data, trailerOff);
    return true;
}

uint32_t icdEncodeSignal(const IcdSignal& sig, float physical) {
    float scale = (sig.multiply != 0) ? (float)sig.multiply / 1000.0f : 1.0f;
    long raw = lroundf(physical * scale) + sig.add;
    if (raw < sig.minVal) raw = sig.minVal;
    if (raw > sig.maxVal) raw = sig.maxVal;
    if (raw < 0) raw = 0;
    return (uint32_t)raw;
}
