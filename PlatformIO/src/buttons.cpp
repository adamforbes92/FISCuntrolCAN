/*
 * buttons.cpp — Stalk button initialisation and click handlers
 */

#include "buttons.h"
#include "config.h"
#include "can_bus.h"
#include "cards.h"
#include "kline.h"
#include "onboot.h"
#include "fis_menu.h"
#include "fis_display.h"

// Fixed compile-time pin tables — no runtime remapping.
static const uint8_t kInputPin[BUTTON_COUNT]  = { stalkPushUp,       stalkPushDown,       stalkPushReset       };
static const uint8_t kOutputPin[BUTTON_COUNT] = { stalkPushUpReturn,  stalkPushDownReturn,  stalkPushResetReturn  };

static const uint32_t kButtonPulseMs = 80;
static const uint32_t kButtonDisplayHoldMs = 2000;
static const uint32_t kFisDisableToggleDebounceMs = 800;

// Hold Reset this long to force a full FIS boot + reconnect recovery. This is a
// hardware fallback for when the display hangs and the menu is unreachable.
static const uint32_t kResetRecoveryHoldMs = 3000;

static card_source_id getButtonNavigationSource() {
    if (showHaldex && hasHaldex) {
        return CARD_SOURCE_HALDEX;
    }

    return getSelectedCardSource();
}

static void refreshDisplayedSourceAfterButton() {
    if (fisDisable || !ignitionState) {
        return;
    }

    switch (getButtonNavigationSource()) {
        case CARD_SOURCE_KLINE:
            if (hasK && isConnectedK) {
                showMeasurements(readBlock);
            }
            break;

        case CARD_SOURCE_CAN:
        case CARD_SOURCE_HALDEX:
            if (hasCAN || hasHaldex) {
                parseFIS();
            }
            break;

        default:
            break;
    }
}

static void executeSinglePress(button_id button);
static void executeDoublePress(button_id button);
static void executeLongPress(button_id button);
static void latchButton(button_id button);
static void physicalSingleClickUp();
static void physicalSingleClickDown();
static void physicalSingleClickReset();
static void physicalDoubleClickUp();
static void physicalDoubleClickDown();
static void physicalLongPressUp();
static void physicalLongPressDown();
static void physicalLongPressReset();

static const char* buttonNameForLog(button_id button) {
    switch (button) {
        case BUTTON_UP:
            return "UP";
        case BUTTON_DOWN:
            return "DOWN";
        case BUTTON_RESET:
            return "RESET";
        default:
            return "UNKNOWN";
    }
}

// ---------------------------------------------------------------------------
// Button instances
// ---------------------------------------------------------------------------
// Inputs are active-LOW: external pullups hold idle=HIGH; pressing pulls GPIO to GND.
// pullupActive=false: GPIO34/36/39 are input-only and cannot drive internal pullups —
// the external pullup on the PCB is the only one needed.
OneButton stalkUpButton(stalkPushUp, true, false);
OneButton stalkDownButton(stalkPushDown, true, false);
OneButton stalkResetButton(stalkPushReset, true, false);

static bool     buttonOutputLevels[BUTTON_COUNT]      = { LOW, LOW, LOW };
static uint32_t buttonOutputPulseUntil[BUTTON_COUNT]  = { 0, 0, 0 };
static uint32_t buttonOutputDisplayUntil[BUTTON_COUNT]= { 0, 0, 0 };
static uint32_t buttonInputDisplayUntil[BUTTON_COUNT] = { 0, 0, 0 };

// ---------------------------------------------------------------------------
// ISR – called from GPIO interrupt; just ticks all buttons
// ---------------------------------------------------------------------------
void IRAM_ATTR checkTicks() {
    stalkUpButton.tick();
    stalkDownButton.tick();
    stalkResetButton.tick();
}

static void latchButton(button_id button) {
    if (button < BUTTON_COUNT) {
        buttonInputDisplayUntil[button] = millis() + kButtonDisplayHoldMs;
    }
}

// ---------------------------------------------------------------------------
// Click handlers
// ---------------------------------------------------------------------------
static void singleClickUp() {
    if (isMenuOpen()) {
        menuNavigateUp();
        return;
    }
    if (!fisDisable) {
        switch (getButtonNavigationSource()) {
            case CARD_SOURCE_HALDEX:
                lastMode++;
                if (!isValidOpenHaldexMode(lastMode)) lastMode = MODE_STOCK;
                haldexModeChangeMs = millis();
                break;

            case CARD_SOURCE_CAN:
                nextCanCard();
                break;

            case CARD_SOURCE_KLINE:
            case CARD_SOURCE_NONE:
            default:
                readBlock++;
                break;
        }
        saveNavState();
    }
}

static void singleClickDown() {
    if (isMenuOpen()) {
        menuNavigateDown();
        return;
    }
    if (!fisDisable) {
        switch (getButtonNavigationSource()) {
            case CARD_SOURCE_HALDEX:
                if (lastMode == MODE_STOCK || !isValidOpenHaldexMode(lastMode)) lastMode = (uint8_t)(MODE_COUNT - 1U);
                else               lastMode--;
                haldexModeChangeMs = millis();
                break;

            case CARD_SOURCE_CAN:
                prevCanCard();
                break;

            case CARD_SOURCE_KLINE:
            case CARD_SOURCE_NONE:
            default:
                readBlock--;
                break;
        }
        saveNavState();
    }
}

static void doubleClickUp() {
    if (!fisDisable) {
        readBlock += 10;
        if (readBlock > 255) readBlock = 1;
        saveNavState();
    }
}

static void doubleClickDown() {
    if (!fisDisable) {
        if (readBlock < 10) readBlock = 255;
        else                readBlock -= 10;
        saveNavState();
    }
}

static void singleClickReset() {
    if (isMenuOpen()) {
        menuSelect();
        return;
    }
    // Single press while on a card: go up to the menu
    if (!fisDisable && hasFIS && fisBootReady) {
        menuOpen();
    }
}

static void pressStartUp() {
    if (hasHaldex) {
        lastHaldex = -1;
        lastBlock  = 0;
        showHaldex = !showHaldex;
        // Do NOT clear fisLine[] here. This runs in button-callback context and
        // would race fisRenderTask's mutex-protected access to the shared String
        // array, corrupting the heap (display crash + sluggish recovery boot).
        // fisRenderTask already clears the screen on the showHaldex transition and
        // repopulates fisLine[] via commitCard() under the mutex.
        saveNavState();
    }
}

static void pressStartDown() {
    readBlock = 1;
    saveNavState();
}

static void pressStartReset() {
    if (isMenuOpen()) {
        // Long-press Reset while menu is open: close without selecting
        menuClose();
        return;
    }

    static uint32_t lastToggleMs = 0;
    const uint32_t now = millis();
    if (now - lastToggleMs < kFisDisableToggleDebounceMs) {
        return;
    }
    lastToggleMs = now;

    if (hasFIS && fisBootReady) {
        if (fisDisable) {
            // FIS was off — re-enable it first
            fisDisable = false;
            mimickSet = true;
            fisBeenToggled = true;
        }
        menuOpen();
    } else {
        fisDisable = !fisDisable;
        mimickSet  = true;
        fisBeenToggled = true;
    }
}

static void physicalSingleClickUp()    { latchButton(BUTTON_UP);    executeSinglePress(BUTTON_UP); }
static void physicalSingleClickDown()  { latchButton(BUTTON_DOWN);  executeSinglePress(BUTTON_DOWN); }
static void physicalSingleClickReset() { latchButton(BUTTON_RESET); executeSinglePress(BUTTON_RESET); }
static void physicalDoubleClickUp()    { latchButton(BUTTON_UP);    executeDoublePress(BUTTON_UP); }
static void physicalDoubleClickDown()  { latchButton(BUTTON_DOWN);  executeDoublePress(BUTTON_DOWN); }
static void physicalLongPressUp()      { latchButton(BUTTON_UP);    executeLongPress(BUTTON_UP); }
static void physicalLongPressDown()    { latchButton(BUTTON_DOWN);  executeLongPress(BUTTON_DOWN); }
static void physicalLongPressReset()   { latchButton(BUTTON_RESET); executeLongPress(BUTTON_RESET); }

// Fires repeatedly while Reset is held.  Holding past kResetRecoveryHoldMs
// forces a full FIS boot + reconnect — a recovery path when the display hangs.
static void physicalDuringLongPressReset() {
    static bool recoveryFired = false;
    if (stalkResetButton.getPressedMs() >= kResetRecoveryHoldMs) {
        if (!recoveryFired) {
            recoveryFired = true;
            menuClose();
            requestRestartIgnitionSequence();
        }
    } else {
        recoveryFired = false;
    }
}

static void executeSinglePress(button_id button) {
    LOGIO("BUTTON: %s", buttonNameForLog(button));

    switch (button) {
        case BUTTON_UP:
            singleClickUp();
            break;
        case BUTTON_DOWN:
            singleClickDown();
            break;
        case BUTTON_RESET:
            singleClickReset();
            break;
        default:
            break;
    }
}

static void executeDoublePress(button_id button) {
    switch (button) {
        case BUTTON_UP:
            doubleClickUp();
            break;
        case BUTTON_DOWN:
            doubleClickDown();
            break;
        default:
            break;
    }
}

static void executeLongPress(button_id button) {
    switch (button) {
        case BUTTON_UP:
            pressStartUp();
            break;
        case BUTTON_DOWN:
            pressStartDown();
            break;
        case BUTTON_RESET:
            pressStartReset();
            break;
        default:
            break;
    }
}

// ---------------------------------------------------------------------------
// Setup
// ---------------------------------------------------------------------------
void setupButtons() {
    stalkUpButton.attachClick(physicalSingleClickUp);
    stalkUpButton.attachMultiClick(physicalDoubleClickUp);
    stalkUpButton.setPressMs(1000);
    stalkUpButton.attachLongPressStart(physicalLongPressUp);

    stalkDownButton.attachClick(physicalSingleClickDown);
    stalkDownButton.attachMultiClick(physicalDoubleClickDown);
    stalkDownButton.setPressMs(1000);
    stalkDownButton.attachLongPressStart(physicalLongPressDown);

    stalkResetButton.attachClick(physicalSingleClickReset);
    stalkResetButton.setPressMs(1000);
    stalkResetButton.attachLongPressStart(physicalLongPressReset);
    stalkResetButton.attachDuringLongPress(physicalDuringLongPressReset);
}

void configureButtonOutputPins() {
    for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
        pinMode(kOutputPin[i], OUTPUT);
        digitalWrite(kOutputPin[i], buttonOutputLevels[i]);
    }
}

void setButtonOutputLevel(button_id button, bool level) {
    if (button >= BUTTON_COUNT) {
        return;
    }

    buttonOutputLevels[button] = level;
    digitalWrite(kOutputPin[button], level);
    if (level == HIGH) {
        buttonOutputDisplayUntil[button] = millis() + kButtonDisplayHoldMs;
    }
}

void resetButtonOutputStates() {
    for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
        buttonOutputPulseUntil[i] = 0;
        buttonOutputDisplayUntil[i] = 0;
        buttonInputDisplayUntil[i] = 0;
        setButtonOutputLevel((button_id)i, LOW);
    }
}

void serviceButtonOutputs() {
    const uint32_t now = millis();
    for (uint8_t i = 0; i < BUTTON_COUNT; i++) {
        const bool inputLow = digitalRead(kInputPin[i]) == LOW;
        if (inputLow) {
            buttonInputDisplayUntil[i] = now + kButtonDisplayHoldMs;
        }
        if (buttonOutputPulseUntil[i] != 0 && now >= buttonOutputPulseUntil[i]) {
            buttonOutputPulseUntil[i] = 0;
            setButtonOutputLevel((button_id)i, LOW);
        }
        // Pass-through: mirror physical input to output whenever no timed pulse
        // (simulateButtonPress / web-UI) is active.  This ensures the trip
        // computer always sees stalk presses even when the FIS is active.
        if (buttonOutputPulseUntil[i] == 0) {
            setButtonOutputLevel((button_id)i, inputLow ? HIGH : LOW);
        }
    }
}

// Drives the dedicated external output (extOutputPin) according to the selected
// mode. Manual mode simply follows extOutputEnabled; the RPM/Speed modes gate
// the pin on a threshold so it engages automatically while driving. The pin is
// only written when its computed level changes, to avoid needless bus traffic.
void serviceExternalOutput() {
    bool desired;
    switch (extOutputMode) {
        case 1:  // RPM threshold
            desired = (extOutputRpmThreshold > 0) && (vehicleRPM >= extOutputRpmThreshold);
            break;
        case 2:  // Speed threshold
            desired = (extOutputSpeedThreshold > 0) && (vehicleSpeed >= extOutputSpeedThreshold);
            break;
        default: // Manual
            desired = extOutputEnabled;
            break;
    }

    static int8_t lastWritten = -1;  // -1 forces the first write
    const int8_t desiredLevel = desired ? 1 : 0;
    if (desiredLevel != lastWritten) {
        lastWritten = desiredLevel;
        digitalWrite(extOutputPin, desired ? HIGH : LOW);
    }
}

// Pin remapping is not supported at runtime — pins are fixed at compile time.
bool setButtonInputPin(button_id /*button*/, uint8_t /*gpio*/) { return false; }
bool setButtonOutputPin(button_id /*button*/, uint8_t /*gpio*/) { return false; }

uint8_t getButtonInputPin(button_id button) {
    return button < BUTTON_COUNT ? kInputPin[button] : 0;
}

uint8_t getButtonOutputPin(button_id button) {
    return button < BUTTON_COUNT ? kOutputPin[button] : 0;
}

bool getButtonInputPressed(button_id button) {
    if (button >= BUTTON_COUNT) {
        return false;
    }

    return digitalRead(kInputPin[button]) == LOW || millis() < buttonInputDisplayUntil[button];
}

bool getButtonOutputActive(button_id button) {
    if (button >= BUTTON_COUNT) {
        return false;
    }

    return buttonOutputLevels[button] == HIGH || millis() < buttonOutputDisplayUntil[button];
}

bool getButtonOutputPhysicalLevel(button_id button) {
    if (button >= BUTTON_COUNT) {
        return LOW;
    }

    return buttonOutputLevels[button];
}

void simulateButtonPress(button_id button) {
    if (button >= BUTTON_COUNT) {
        return;
    }

    buttonInputDisplayUntil[button] = millis() + kButtonDisplayHoldMs;
    setButtonOutputLevel(button, HIGH);
    buttonOutputPulseUntil[button] = millis() + kButtonPulseMs;
    executeSinglePress(button);
    refreshDisplayedSourceAfterButton();
}

// ---------------------------------------------------------------------------
// Mirror stalk inputs to return outputs (pass-through when FIS is off)
// ---------------------------------------------------------------------------
void mimickStalkButtons() {
    // Inverted: input LOW (pressed) → output HIGH; input HIGH (idle) → output LOW.
    setButtonOutputLevel(BUTTON_UP,    !digitalRead(kInputPin[BUTTON_UP]));
    setButtonOutputLevel(BUTTON_DOWN,  !digitalRead(kInputPin[BUTTON_DOWN]));
    setButtonOutputLevel(BUTTON_RESET, !digitalRead(kInputPin[BUTTON_RESET]));
}
