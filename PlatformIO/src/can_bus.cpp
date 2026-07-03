/*
 * can_bus.cpp — ESP32 TWAI (native CAN) implementation
 *
 * Uses the ESP-IDF TWAI driver (driver/twai.h) included with the Arduino ESP32
 * core.  No third-party CAN library is required.
 *
 * The receive path runs in its own FreeRTOS task (canTask) so that frames are
 * never dropped waiting for the main loop.
 */

#include "can_bus.h"
#include "config.h"
#include "cards.h"

#include <driver/twai.h>

static bool canDriverInitialised = false;

// After a local OpenHaldex mode change, ignore the controller's echoed mode for
// this long so a stale echo can't overwrite (and appear to "revert") the change.
static const uint32_t kHaldexModeEchoSuppressMs = 2000;

// ---------------------------------------------------------------------------
// Unified CAN frame store + signal tables
//
// Every CAN card — the standard MOTOR cards and the Ignitron ICD01 sensor
// pages — is described by a table of CanSignal entries and rendered by a single
// decode/format block (decodeSignalValue / formatSignalLine / renderSignalCard).
// Raw frames are captured once in updateCanCardFrame() and decoded on demand at
// render time.
// ---------------------------------------------------------------------------

// Line formatting for a decoded signal.
enum CanSigFmt : uint8_t {
    FMT_DEC = 0,   // "LABEL: 1234"
    FMT_PCT,       // "LABEL: 12%"
    FMT_BOOL,      // "LABEL: ON" / "LABEL: OFF"
    FMT_COMPACT,   // "LABEL:1234"  (narrow Ignitron lines)
};

struct CanSignal {
    const char* label;
    uint16_t    canId;       // source frame (used when getter == nullptr)
    uint8_t     startBit;    // bit offset within the frame
    uint8_t     length;      // signal length in bits
    int32_t     add;         // offset subtracted on decode
    uint16_t    mul;         // scale, fixed-point x1000 (1000 = x1)
    uint8_t     fmt;         // CanSigFmt
    int32_t   (*getter)();   // when set, value is taken from here (a global)
};

struct CanCard {
    const char*      name;       // card title / screen-change key
    const CanSignal* signals;
    uint8_t          signalCount;
    uint16_t         frameId;    // raw-byte source frame (0 = none)
};

// Captured raw frames (covers every CAN id any card decodes from).
struct CanFrame {
    uint16_t id;
    bool     hasFrame;
    uint8_t  dlc;
    uint8_t  data[8];
};

static CanFrame canFrames[] = {
    { MOTOR1_ID, false, 0, {0} },
    { MOTOR2_ID, false, 0, {0} },
    { MOTOR5_ID, false, 0, {0} },
    { 0x600,     false, 0, {0} },
    { 0x601,     false, 0, {0} },
    { 0x602,     false, 0, {0} },
    { 0x603,     false, 0, {0} },
    { 0x604,     false, 0, {0} },
};

static CanFrame* findFrame(uint16_t id) {
    for (CanFrame& f : canFrames) {
        if (f.id == id) {
            return &f;
        }
    }
    return nullptr;
}

// --- Standard MOTOR cards -------------------------------------------------
// These signals are decoded into shared globals in canTask() (because other
// subsystems consume them), so the table picks them up locally via getters
// rather than re-decoding the raw frame.
static const CanSignal motor1Signals[] = {
    { "RPM",   0, 0, 0, 0, 0, FMT_DEC, []() -> int32_t { return (int32_t)vehicleRPM; } },
    { "PEDAL", 0, 0, 0, 0, 0, FMT_PCT, []() -> int32_t { return (int32_t)pedValue;   } },
};
static const CanSignal motor2Signals[] = {
    { "SPEED", 0, 0, 0, 0, 0, FMT_DEC, []() -> int32_t { return (int32_t)vehicleSpeed; } },
};
static const CanSignal motor5Signals[] = {
    { "EML", 0, 0, 0, 0, 0, FMT_BOOL, []() -> int32_t { return (int32_t)vehicleEML; } },
    { "EPC", 0, 0, 0, 0, 0, FMT_BOOL, []() -> int32_t { return (int32_t)vehicleEPC; } },
};

static const CanCard motorCards[] = {
    { "MOTOR_1", motor1Signals, (uint8_t)arraySize(motor1Signals), MOTOR1_ID },
    { "MOTOR_2", motor2Signals, (uint8_t)arraySize(motor2Signals), MOTOR2_ID },
    { "MOTOR_5", motor5Signals, (uint8_t)arraySize(motor5Signals), MOTOR5_ID },
};

static uint8_t motorCardCount() {
    return (uint8_t)arraySize(motorCards);
}

// --- Ignitron ICD01 sensor signals ---------------------------------------
// Validated decode of the dashCAN ICD01 map (docs/ICD01_format.md). Each is a
// little-endian, byte-aligned 16-bit field; "add" is subtracted on receive and
// "mul" (fixed-point x1000, always 1.000 here) is divided back out.
static const CanSignal ignitronSignals[] = {
    { "RPM",   0x600, 0,  16, 0,    1000, FMT_COMPACT, nullptr },  // Sensors RPM
    { "CLT",   0x600, 16, 16, 400,  1000, FMT_COMPACT, nullptr },  // Coolant temperature
    { "MAP",   0x600, 32, 16, 0,    1000, FMT_COMPACT, nullptr },  // Manifold absolute pressure
    { "IAT",   0x600, 48, 16, 400,  1000, FMT_COMPACT, nullptr },  // Intake air temperature
    { "EGT",   0x601, 0,  16, 0,    1000, FMT_COMPACT, nullptr },  // Exhaust gas temperature
    { "OILP",  0x601, 16, 16, 0,    1000, FMT_COMPACT, nullptr },  // Oil pressure
    { "OILT",  0x601, 32, 16, 400,  1000, FMT_COMPACT, nullptr },  // Oil temperature
    { "FUELP", 0x601, 48, 16, 0,    1000, FMT_COMPACT, nullptr },  // Fuel rail pressure
    { "LAMB",  0x602, 0,  16, 0,    1000, FMT_COMPACT, nullptr },  // Lambda
    { "PEDAL", 0x602, 16, 16, 0,    1000, FMT_COMPACT, nullptr },  // Throttle pedal position
    { "SPEED", 0x602, 32, 16, 0,    1000, FMT_COMPACT, nullptr },  // Vehicle speed
    { "TPS",   0x603, 0,  16, 0,    1000, FMT_COMPACT, nullptr },  // Throttle position sensor
    { "VBAT",  0x603, 16, 16, 0,    1000, FMT_COMPACT, nullptr },  // Battery voltage
    { "ADV",   0x603, 32, 16, 400,  1000, FMT_COMPACT, nullptr },  // Ignition advance
    { "STFT",  0x603, 48, 16, 1000, 1000, FMT_COMPACT, nullptr },  // Fuel trim (short term)
    { "LTFT",  0x604, 0,  16, 1000, 1000, FMT_COMPACT, nullptr },  // Fuel trim (long term)
    { "AUX1",  0x604, 48, 16, 0,    1000, FMT_COMPACT, nullptr },  // enum 0x70
    { "AUX2",  0x604, 16, 16, 0,    1000, FMT_COMPACT, nullptr },  // enum 0xF7
    { "TORQ",  0x602, 48, 16, 0,    1000, FMT_COMPACT, nullptr },  // ECU output torque
    { "AUX3",  0x604, 32, 16, 0,    1000, FMT_COMPACT, nullptr },  // enum 0x11B
};
static const uint8_t kIgnSignalCount  = (uint8_t)arraySize(ignitronSignals);
static const uint8_t kIgnLinesPerCard = 6;

static uint8_t ignPageCount() {
    return (uint8_t)((kIgnSignalCount + kIgnLinesPerCard - 1) / kIgnLinesPerCard);
}

// --- Shared view state ----------------------------------------------------
static uint8_t currentCanCard = 0;   // index into motorCards (standard view)
static uint8_t ignPage        = 0;   // current Ignitron page (Ignitron view)

static const char* ignViewName() {
    static char name[16];
    snprintf(name, sizeof(name), "IGN %u/%u",
             (unsigned)(ignPage + 1), (unsigned)ignPageCount());
    return name;
}

// ---------------------------------------------------------------------------
// Frame capture
// ---------------------------------------------------------------------------
void updateCanCardFrame(uint32_t id, uint8_t dlc, const uint8_t* data) {
    CanFrame* f = findFrame((uint16_t)id);
    if (f == nullptr) {
        return;
    }
    f->dlc = dlc > 8 ? 8 : dlc;
    memcpy(f->data, data, f->dlc);
    f->hasFrame = true;
}

// ---------------------------------------------------------------------------
// One decode/format block for every CAN card
// ---------------------------------------------------------------------------

// Decode a signal's value. Returns false if its source frame is missing.
static bool decodeSignalValue(const CanSignal& s, int32_t& out) {
    if (s.getter != nullptr) {
        out = s.getter();
        return true;
    }

    const CanFrame* f = findFrame(s.canId);
    if (f == nullptr || !f->hasFrame) {
        return false;
    }

    const uint8_t byteOff = (uint8_t)(s.startBit / 8);
    const uint8_t nBytes  = (uint8_t)(s.length / 8);
    if ((uint16_t)(byteOff + nBytes) > f->dlc) {
        return false;
    }

    uint32_t raw = 0;
    for (uint8_t b = 0; b < nBytes; b++) {
        raw |= (uint32_t)f->data[byteOff + b] << (8 * b);
    }

    int32_t val = (int32_t)raw - s.add;
    if (s.mul != 0 && s.mul != 1000) {
        val = (int32_t)((int64_t)val * 1000 / s.mul);
    }
    out = val;
    return true;
}

static void formatSignalLine(char* buf, size_t n, const CanSignal& s) {
    int32_t v = 0;
    if (!decodeSignalValue(s, v)) {
        snprintf(buf, n, "%s: --", s.label);
        return;
    }
    switch (s.fmt) {
        case FMT_PCT:     snprintf(buf, n, "%s: %ld%%", s.label, (long)v);          break;
        case FMT_BOOL:    snprintf(buf, n, "%s: %s",    s.label, v ? "ON" : "OFF"); break;
        case FMT_COMPACT: snprintf(buf, n, "%s:%ld",    s.label, (long)v);          break;
        case FMT_DEC:
        default:          snprintf(buf, n, "%s: %ld",   s.label, (long)v);          break;
    }
}

static uint8_t appendRawByteLines(String outLines[8], uint8_t startLine, const CanFrame& f) {
    uint8_t line = startLine;
    for (uint8_t i = 0; i < f.dlc && line < 8; i++) {
        char rawBuf[20];
        snprintf(rawBuf, sizeof(rawBuf), "B%u: 0X%02X", i, f.data[i]);
        outLines[line++] = String(rawBuf);
    }
    return line;
}

// Render up to 8 lines: `count` signals starting at `firstSignal`, optionally
// followed by a raw-byte dump of `rawFrame`. Drives MOTOR cards and Ignitron
// pages alike.
static void renderSignalCard(const char* name,
                             const CanSignal* sigs, uint8_t count, uint8_t firstSignal,
                             const CanFrame* rawFrame) {
    String lines[8];
    for (uint8_t i = 0; i < 8; i++) {
        lines[i] = "";
    }

    if (rawFrame != nullptr && !rawFrame->hasFrame) {
        lines[0] = "WAITING FOR DATA";
        lines[1] = "NO FRAME RECEIVED";
    } else {
        uint8_t line = 0;
        for (uint8_t i = 0; i < count && line < 8; i++) {
            char buf[24];
            formatSignalLine(buf, sizeof(buf), sigs[firstSignal + i]);
            lines[line++] = String(buf);
        }
        if (rawFrame != nullptr) {
            appendRawByteLines(lines, line, *rawFrame);
        }
    }

    beginCard(CARD_SOURCE_CAN, name);
    for (uint8_t i = 0; i < 8; i++) {
        setCardLine(i, lines[i]);
    }
    commitCard();
}

static const char* getOpenHaldexModeLabel(uint8_t mode) {
    switch (mode) {
        case MODE_STOCK:  return "Stock";
        case MODE_FWD:    return "FWD";
        case MODE_5050:   return "5050";
        case MODE_6040:   return "6040";
        case MODE_7525:   return "7525";
        case MODE_EXPERT: return "Expert";
        default:          return "Unknown";
    }
}

// ---------------------------------------------------------------------------
// Initialisation
// ---------------------------------------------------------------------------
void canInit() {
    if (canDriverInitialised) {
        return;
    }

    const twai_general_config_t g_config =
        TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t)pinCAN_TX,
                                    (gpio_num_t)pinCAN_RX,
                                    TWAI_MODE_NORMAL);

    const twai_timing_config_t  t_config = TWAI_TIMING_CONFIG_500KBITS();

    // Accept all frames (no hardware filter)
    const twai_filter_config_t  f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    if (twai_driver_install(&g_config, &t_config, &f_config) != ESP_OK) {
        LOGCAN("TWAI driver install failed!");
        return;
    }
    if (twai_start() != ESP_OK) {
        LOGCAN("TWAI start failed!");
        twai_driver_uninstall();
        return;
    }

    canDriverInitialised = true;
    LOGCAN("TWAI (CAN) initialised OK");
}

void canDeinit() {
    if (!canDriverInitialised) {
        isConnectedCAN = false;
        return;
    }

    isConnectedCAN = false;
    lastTransmission = 0;

    twai_stop();
    // Give canTask time to exit twai_receive() (10 ms timeout) and see !hasCAN
    // before the driver queues are freed by twai_driver_uninstall().
    vTaskDelay(pdMS_TO_TICKS(20));
    if (twai_driver_uninstall() != ESP_OK) {
        LOGCAN("TWAI driver uninstall failed!");
        return;
    }

    canDriverInitialised = false;
    LOGCAN("TWAI (CAN) deinitialised");
}

void nextCanCard() {
    if (canViewMode == CAN_VIEW_IGNITRON) {
        const uint8_t pages = ignPageCount();
        if (pages != 0) {
            ignPage = (uint8_t)((ignPage + 1U) % pages);
        }
        return;
    }
    const uint8_t count = motorCardCount();
    if (count == 0) {
        return;
    }
    currentCanCard = (uint8_t)((currentCanCard + 1U) % count);
}

void prevCanCard() {
    if (canViewMode == CAN_VIEW_IGNITRON) {
        const uint8_t pages = ignPageCount();
        if (pages != 0) {
            ignPage = (ignPage == 0) ? (uint8_t)(pages - 1U) : (uint8_t)(ignPage - 1U);
        }
        return;
    }
    const uint8_t count = motorCardCount();
    if (count == 0) {
        return;
    }
    if (currentCanCard == 0) {
        currentCanCard = (uint8_t)(count - 1U);
    } else {
        currentCanCard--;
    }
}

void resetCanCards() {
    currentCanCard = 0;
    ignPage = 0;
    for (CanFrame& f : canFrames) {
        f.hasFrame = false;
        f.dlc = 0;
        memset(f.data, 0, sizeof(f.data));
    }
}

const char* getCurrentCanCardName() {
    if (canViewMode == CAN_VIEW_IGNITRON) {
        return ignViewName();
    }
    if (motorCardCount() == 0) {
        return "CAN";
    }
    return motorCards[currentCanCard].name;
}

uint8_t getCurrentCanCardIndex() {
    return currentCanCard;
}

void setCurrentCanCardIndex(uint8_t index) {
    const uint8_t count = motorCardCount();
    if (count > 0) {
        currentCanCard = index % count;
    }
}

uint8_t getCanCardCount() {
    return motorCardCount();
}

const char* getCanCardName(uint8_t index) {
    if (index < motorCardCount()) {
        return motorCards[index].name;
    }
    return nullptr;
}

// ---------------------------------------------------------------------------
// Parse prepared data into fisLine[] strings
// ---------------------------------------------------------------------------
void renderHaldexCard() {
    char buf[20], buf1[20], buf2[40], buf3[40], buf4[40], buf5[40], buf6[24];

    snprintf(buf, sizeof(buf), "Mode: %s", hasOpenHaldex ? getOpenHaldexModeLabel(lastMode) : "...");
    snprintf(buf1, sizeof(buf1), "Conn.: %s",  hasOpenHaldex ? "Yes" : "No");
    snprintf(buf2, sizeof(buf2), "Act: %-3d%%", haldexEngagement);
    snprintf(buf3, sizeof(buf3), "Req: %-3d%%", lockTarget);
    snprintf(buf4, sizeof(buf4), "Speed: %-4d",       haldexVehicleSpeed);
    snprintf(buf5, sizeof(buf5), "Pedal: %-3d%%",     pedValue);

    snprintf(buf6, sizeof(buf6), "Standalone: %s", isStandalone ? "Yes" : "No");

    beginCard(CARD_SOURCE_HALDEX, "OPENHALDEX");
    setCardLine(0, String(buf1));
    setCardLine(1, String(buf));
    setCardLine(2, String(buf2));
    setCardLine(3, String(buf3));
    setCardLine(4, String(buf4));
    setCardLine(5, String(buf5));
    setCardLine(6, String(buf6));
    commitCard();
}

void renderCanCard() {
    if (canViewMode == CAN_VIEW_IGNITRON) {
        const uint8_t pages = ignPageCount();
        if (pages != 0 && ignPage >= pages) {
            ignPage = 0;
        }
        const uint8_t first = (uint8_t)(ignPage * kIgnLinesPerCard);
        uint8_t cnt = (uint8_t)(kIgnSignalCount - first);
        if (cnt > kIgnLinesPerCard) {
            cnt = kIgnLinesPerCard;
        }
        renderSignalCard(ignViewName(), ignitronSignals, cnt, first, nullptr);
        return;
    }

    if (motorCardCount() == 0) {
        beginCard(CARD_SOURCE_CAN, "CAN");
        setCardLine(0, "NO CAN CARDS");
        commitCard();
        return;
    }

    const CanCard& card = motorCards[currentCanCard];
    renderSignalCard(card.name, card.signals, card.signalCount, 0, findFrame(card.frameId));
}

// ---------------------------------------------------------------------------
// Broadcast FISCuntrol mode frame to OpenHaldex
// ---------------------------------------------------------------------------
void broadcastOpenHaldex() {
    // Wait until the current mode has been received from OpenHaldex before
    // broadcasting. Otherwise FISCuntrol would push its default (Stock) onto
    // OpenHaldex before learning the actual current mode.
    if (!hasOpenHaldex) {
        return;
    }

    if (!isValidOpenHaldexMode(lastMode)) {
        lastMode = MODE_STOCK;
    }

    state.mode = (openhaldex_mode_id)lastMode;

    twai_message_t tx;
    memset(&tx, 0, sizeof(tx));
    tx.identifier         = fisCuntrol_ID;
    tx.data_length_code   = 8;
    tx.data[0]            = lastMode;

    if (twai_transmit(&tx, pdMS_TO_TICKS(10)) != ESP_OK) {
        LOGCAN("TWAI TX failed! (broadcastOpenHaldex)");
    }
}

// ---------------------------------------------------------------------------
// Receive task — decodes incoming frames into globals + the unified card store.
// Runs in its own FreeRTOS task (core 0) so frames are never dropped.
// ---------------------------------------------------------------------------
void canTask(void* pvParameters) {
    (void)pvParameters;
    twai_message_t frame;

    for (;;) {
        if (!hasCAN) {
            // Driver may be uninstalled while switching to K-line — do not call twai_receive()
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }
        if (twai_receive(&frame, pdMS_TO_TICKS(10)) != ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        if (logCatEnabled[LOG_CAN]) {
            char dump[96];
            int n = snprintf(dump, sizeof(dump), "RX ID:0x%03X Len:%d Data:",
                             frame.identifier, frame.data_length_code);
            for (uint8_t i = 0; i < frame.data_length_code && n < (int)sizeof(dump) - 4; i++) {
                n += snprintf(dump + n, sizeof(dump) - n, " %02X", frame.data[i]);
            }
            LOGCAN("%s", dump);
        }

        isConnectedCAN   = true;
        lastTransmission = millis();
        updateCanCardFrame(frame.identifier, frame.data_length_code, frame.data);

        switch (frame.identifier) {
            case MOTOR1_ID:
                vehicleRPM = ((frame.data[3] << 8) | frame.data[2]) * 0.25f;
                pedValue   = frame.data[5] * 0.4f;
                break;

            case MOTOR2_ID: {
                calcSpeed    = (uint32_t)(frame.data[3] * 100 * 128) / 10000;
                vehicleSpeed = (uint8_t)(calcSpeed >= 255 ? 255 : calcSpeed);
#if isMPH
                vehicleSpeed = (uint8_t)((vehicleSpeed * (uint32_t)mphFactor) / 1000000UL);
#endif
                break;
            }

            case MOTOR5_ID:
                vehicleEML = bitRead(frame.data[1], 5);
                vehicleEPC = bitRead(frame.data[1], 6);
                break;

            case openHaldex_ID:
                haldexState        = frame.data[0];
                isStandalone       = frame.data[1] != 0;
                haldexEngagement   = (uint8_t)map(frame.data[2], 128, 198, 0, 100);
                if (haldexEngagement > 100) haldexEngagement = 100;
                lockTarget         = frame.data[3];
                haldexVehicleSpeed = frame.data[4];
#if isMPH
                haldexVehicleSpeed = (uint8_t)((haldexVehicleSpeed * (uint32_t)mphFactor) / 1000000UL);
#endif
                state.mode_override = frame.data[5];
                // Adopt the controller's reported mode, but suppress it briefly
                // after a local change so our new selection isn't overwritten by
                // a stale echo (which made the mode appear stuck / unable to wrap).
                if (isValidOpenHaldexMode(frame.data[6]) &&
                    (millis() - haldexModeChangeMs) > kHaldexModeEchoSuppressMs) {
                    lastMode = frame.data[6];
                }
                hasOpenHaldex = true;
                break;

            case ignitron1_ID:
            case ignitron2_ID:
            case ignitron3_ID:
            default:
                break;
        }
    }
}

void startCANReceiveTask() {
    if (!(hasCAN || hasHaldex)) {
        return;
    }
    if (canTaskRunning) {
        return;
    }

    xTaskCreatePinnedToCore(
        canTask,
        "CAN_RX",
        4096,
        nullptr,
        2,
        nullptr,
        0);
    canTaskRunning = true;
    LOGCAN("CAN (TWAI) task started.");
}
