/*
 * onboot.cpp — Pin initialisation, boot sequence, connection setup, shutdown
 */

#include "onboot.h"
#include "config.h"

#include "fis_display.h"
#include "can_bus.h"
#include "kline.h"
#include "buttons.h"
#include "bootmessage.h"
#include "bootLogos.h"
#include "tasks.h"
#include "cards.h"

#include <Preferences.h>

static const char* kNavNs = "navsave";

void saveNavState() {
    Preferences p;
    p.begin(kNavNs, false);
    p.putUChar("canCard", getCurrentCanCardIndex());
    p.putUChar("canView", (uint8_t)canViewMode);
    p.putBool ("haldex",  showHaldex);
    p.putUChar("block",   (uint8_t)readBlock);
    p.end();
}

void loadNavState() {
    Preferences p;
    p.begin(kNavNs, true);
    const uint8_t canCard = p.getUChar("canCard", 0);
    const uint8_t canView = p.getUChar("canView", 0);
    const bool    haldex  = p.getBool ("haldex",  false);
    const uint8_t block   = p.getUChar("block",   1);
    p.end();

    setCurrentCanCardIndex(canCard);
    canViewMode = (canView == (uint8_t)CAN_VIEW_IGNITRON) ? CAN_VIEW_IGNITRON : CAN_VIEW_STANDARD;
    if (hasHaldex) showHaldex = haldex;
    if (hasK)      readBlock  = (block == 0) ? 1 : block;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------
static void blinkLED(int duration, int pinRef) {
    digitalWrite(pinRef, LOW);
    delay(duration);
    digitalWrite(pinRef, HIGH);
    delay(duration);
}

static void simulateOutputTest() {
    DEBUG("Simulate outputs / LED check...");
    blinkLED(150, getButtonOutputPin(BUTTON_UP));
    blinkLED(150, getButtonOutputPin(BUTTON_DOWN));
    blinkLED(150, getButtonOutputPin(BUTTON_RESET));
    DEBUG("Simulate outputs complete.");
}

static void showConnectionStatus(card_source_id source, const char* title, const char* line0, const char* line1 = "") {
    if (hasFIS && fisBootReady) {
        FIS.clear();
    }
    beginCard(source, title);
    setCardLine(0, line0 != nullptr ? String(line0) : String(""));
    setCardLine(1, line1 != nullptr ? String(line1) : String(""));
    commitCard();

    // Keep this transient status card visible while bus connect is in progress.
    fisStatusHoldUntilMs = millis() + fisStatusCardHoldMs;

    if (hasFIS && fisBootReady) {
        displayFIS();
    }
}

static void clearRuntimeState() {
    fisDisable           = false;
    fisBeenToggled       = false;
    runOnce              = false;
    isConnectedK         = false;
    isConnectedCAN       = false;
    hasOpenHaldex        = false;
    mimickSet            = false;
    isStandalone         = false;
    fisBootReady         = false;
    fisFeedback          = false;

    vehicleSpeed         = 0;
    haldexVehicleSpeed   = 0;
    haldexEngagement     = 0;
    haldexState          = 0;
    lockTarget           = 0;
    pedValue             = 0;
    vehicleRPM           = 0;
    vehicleEML           = false;
    vehicleEPC           = false;

    lastTransmission     = 0;
    lastKlineTransmission = 0;
    fisStatusHoldUntilMs = 0;
    calcSpeed            = 0;

    readBlock            = 1;
    lastBlock            = -1;
    lastHaldex           = -1;

    for (int i = 0; i < 8; i++) {
        fisLine[i] = "";
    }

    resetCanCards();
    clearCard();
    resetButtonOutputStates();
}

static void restoreStartupScreen() {
    // OpenHaldex takes precedence only if that was the selected source.
    if (bootSource == BOOT_SOURCE_NONE) {
        showHaldex = hasHaldex;
        return;
    }

    if (bootSource == BOOT_SOURCE_KLINE) {
        showHaldex = false;
        return;
    }

    showHaldex = false;
}

// ---------------------------------------------------------------------------
// Pin setup
// ---------------------------------------------------------------------------
void setupPins() {
    DEBUG("Setting up pin I/O...");

    configureButtonOutputPins();
    resetButtonOutputStates();

    pinMode(ignitionMonitorPin, INPUT);
#if defined(ESP32)
    analogSetPinAttenuation(ignitionMonitorPin, ADC_11db);
#endif
    const int ignitionRaw = analogRead(ignitionMonitorPin);
    ignitionState = ignitionRaw >= ignitionAdcOnThreshold;
    LOGIO("Ignition baseline adc=%d state=%d", ignitionRaw, ignitionState ? 1 : 0);

    // Interrupts trigger on FALLING: idle=HIGH (pullup), press=LOW (active-LOW hardware).
    attachInterrupt(digitalPinToInterrupt(stalkPushUp),    checkTicks, FALLING);
    attachInterrupt(digitalPinToInterrupt(stalkPushDown),  checkTicks, FALLING);
    attachInterrupt(digitalPinToInterrupt(stalkPushReset), checkTicks, FALLING);

    // External Output pin \u2014 initialise LOW regardless of extOutputEnabled (set after settings load)
    pinMode(extOutputPin, OUTPUT);
    digitalWrite(extOutputPin, LOW);
    // Ignition is sampled in loop() via serviceIgnitionFallback().
    // Avoid GPIO interrupt storms on noisy ignition lines that can starve the app.

#if checkLED
    simulateOutputTest();
#endif

    DEBUG("Pin I/O setup complete.");
}

// ---------------------------------------------------------------------------
// Boot sequence
// ---------------------------------------------------------------------------
void launchBoot() {
    DEBUG("Beginning boot sequence...");

    // Keep fisBootReady=false during the boot screen so fisRenderTask cannot
    // interfere with the display while the logo/message is showing.
    fisBootReady = false;
    fisFeedback  = false;

    bool fisHwReady = false;
    if (hasFIS) {
        fisHwReady = bootFISWithTimeout(fisBootTimeoutMs);

        if (!fisHwReady) {
            DEBUG("FIS: boot failed/timeout. Continuing startup without FIS output.");
        }
    }

    if (hasFIS && fisHwReady) {
        // Build the greeting / special-date text (returns true if there is a
        // text screen to show). A matching special date overrides the logo.
        bool showText = returnBootMessage();

        if (showText) {
            FIS.clear();
            FIS.setTextAlignment(TLBFISLib::CENTER);
            FIS.setFont(TLBFISLib::COMPACT);

            char combinedArray[500] = {};
            for (uint8_t i = 0; i < 8; i++) {
                char buf1[32];
                fisLine[i].toUpperCase();
                fisLine[i].toCharArray(buf1, sizeof(buf1));
                if (i == 0) {
                    snprintf(combinedArray, sizeof(combinedArray), "%s", buf1);
                } else {
                    size_t used = strlen(combinedArray);
                    snprintf(combinedArray + used, sizeof(combinedArray) - used, "\n%s", buf1);
                }
            }
            DEBUG("%s", combinedArray);
            FIS.writeMultiLineText(0, 15, combinedArray, false);
            delay(bootScreenDuration);
        } else {
            // No greeting / special-date text: show the selected boot logo.
            // Any selection without a valid bitmap (0 = off, 1 = legacy) simply
            // shows nothing and boot proceeds straight to the card.
            const uint8_t* selectedLogo = getBootLogoBitmap(bootScreenSelection);

            if (selectedLogo != nullptr) {
                FIS.clear();
                FIS.drawBitmap(0, 0, 64, 88, selectedLogo, true);
                FIS.update();
                delay(bootScreenDuration);
            }
        }

        for (int i = 0; i < 8; i++) fisLine[i] = "";

        // Blank the display, then open the gate for fisRenderTask.
        FIS.clear();
        FIS.update();
    }

    // Now allow the render task to run — display is blank and buffer is clean.
    fisBootReady = fisHwReady;
    fisFeedback  = fisHwReady;

    DEBUG("Boot sequence complete.");
}

// ---------------------------------------------------------------------------
// Connection launch
// ---------------------------------------------------------------------------
void launchConnections() {
    const bool wantsKline = (bootSource == BOOT_SOURCE_KLINE);
    const bool wantsCan   = (bootSource == BOOT_SOURCE_CAN);

    // Boot source is authoritative for runtime bus startup.
    hasK = wantsKline;
    hasCAN = wantsCan;

    // Normalize startup display source up front so stale showHaldex state
    // cannot overwrite the explicit K-line "connecting" card.
    restoreStartupScreen();

    DEBUG("Launching connections for boot source: %s",
          wantsKline ? "K-LINE" : (wantsCan ? "CAN" : "NONE"));

    isConnectedK = false;
    isConnectedCAN = false;

    if (wantsKline) {
        if (hasFIS && fisBootReady) {
            FIS.clear();
            delay(40);
        }

        showConnectionStatus(CARD_SOURCE_KLINE, "K-LINE", "CONNECTING TO K", "PLEASE WAIT");

        if (hasFIS && fisBootReady) {
            displayFIS();
            delay(40);
            displayFIS();
        }

        if (diag.attemptConnect(klineDefaultModule, K_Baud) == KLineKWP1281Lib::SUCCESS) {
            isConnectedK = true;
            LOGKLN("K-line connected.");
        } else {
            LOGKLN("K-line connection failed.");
        }

        LOGKLN("K-line part: %s  ID: %s", diag.getPartNumber(), diag.getIdentification());
    } else if (wantsCan) {
        LOGCAN("Initialising CAN (TWAI)...");
        canInit();
        startCANReceiveTask();
    } else {
        DEBUG("Boot source set to NONE; skipping K-line/CAN startup.");
    }

    // Restore last navigation state (card index, showHaldex, readBlock) saved before
    // the previous shutdown/restart. Applied after restoreStartupScreen() so the persisted
    // Haldex preference overrides the bootSource default where hardware allows.
    loadNavState();

    // CAN is async — isConnectedCAN is set by the receive task on first frame arrival,
    // so it is always false here immediately after startCANReceiveTask(). Only fall back
    // to OEM pass-through if no bus source was configured, or K-line was selected but failed.
    if (!wantsCan && !isConnectedK) {
        DEBUG("No synchronous connection available – defaulting to OEM pass-through.");
        fisDisable = true;
        mimickSet  = true;
        fisBeenToggled = true;
    }

}

void refreshConnections() {
    if (hasK) {
        diag.disconnect(false);
    }

    canDeinit();

    isConnectedK = false;
    isConnectedCAN = false;
    hasOpenHaldex = false;
    isStandalone = false;
    lastTransmission = 0;
    lastKlineTransmission = 0;
    lastBlock = -1;
    lastHaldex = -1;

    resetCanCards();
    clearCard();
    for (int i = 0; i < 8; i++) {
        fisLine[i] = "";
    }

    launchConnections();
}

void requestRestartIgnitionSequence() {
    restartIgnitionRequested = true;
    LOGIO("IGNITION: restart queued");
}

void restartIgnitionSequence() {
    if (!ignitionState) {
        LOGIO("IGNITION: restart canceled (OFF) — ignition is OFF");
        return;
    }

    LOGIO("IGNITION: restart begin");

    LOGIO("IGNITION: running boot sequence");
    triggerShutdown = true;
    ignitionRiseEvent = false;
    ignitionFallEvent = false;
    ignitionStateRunOnce = false;

    beginCard(CARD_SOURCE_NONE, "IGNITION");
    setCardLine(0, "SIMULATED TRIGGER");
    setCardLine(1, "BOOTING SYSTEM");
    commitCard();
    if (hasFIS && fisBootReady && !fisDisable) {
        displayFIS();
    }

    launchBoot();
    LOGIO("IGNITION: reconnecting K/CAN");
    refreshConnections();
    ignitionStateRunOnce = true;
    LOGIO("IGNITION: restart complete");
}

// ---------------------------------------------------------------------------
// FIS disable / re-enable prep
// ---------------------------------------------------------------------------
void fisDisablePrep() {
    if (fisDisable) {
        // Only tear down K-line if it is currently connected.
        if (hasK && isConnectedK) {
            diag.disconnect(false);
            isConnectedK = false;
        }

        // Avoid touching FIS hardware if boot/init never completed.
        if (hasFIS && fisBootReady && ignitionStateRunOnce) {
            // Clear the workspace before handing control back to the trip
            // computer so no stale content (menu/cards) lingers on screen.
            FIS.clear();
            FIS.turnOff();
        }
    } else {
        // Stop the render task immediately; do NOT call bootFISWithTimeout() here.
        // Setting ignitionStateRunOnce=false lets ignitionTask run the full
        // launchBoot()+launchConnections() sequence — a single, clean FIS.begin()
        // with no window where fisRenderTask can start rendering on a blank card
        // between two successive FIS.begin() calls.
        fisBootReady         = false;
        fisFeedback          = false;
        ignitionStateRunOnce = false;
        clearCard();
    }

    if (mimickSet) {
        mimickSet = false;
        setButtonOutputLevel(BUTTON_UP, LOW);
        setButtonOutputLevel(BUTTON_DOWN, LOW);
        setButtonOutputLevel(BUTTON_RESET, LOW);
    }
}

// ---------------------------------------------------------------------------
// Ignition-off shutdown
// ---------------------------------------------------------------------------
void beginShutdown() {
    if (hasK) {
        diag.disconnect(false);
    }
    canDeinit();

    if (hasFIS && fisBootReady) {
        FIS.clear();
        FIS.turnOff();
        // Do NOT call FIS.end() here. SPI.end()+SPI.begin() on ESP32 is
        // unreliable (GPIO pins may not restore), causing FIS.begin() on
        // the next boot to fail silently. Leaving SPI intact is consistent
        // with fisDisablePrep() (manual EXIT), which also omits FIS.end()
        // and is known to re-initialise cleanly via launchBoot() → FIS.begin().
    }

    clearRuntimeState();

    ignitionStateRunOnce = false;
    triggerShutdown      = false;
}
