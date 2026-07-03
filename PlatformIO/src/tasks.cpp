#include "tasks.h"

#include "config.h"
#include "can_bus.h"
#include "fis_display.h"
#include "fis_menu.h"
#include "kline.h"
#include "buttons.h"
#include "oil_sensor.h"
#include "onboot.h"

#include <driver/twai.h>

static bool openHaldexTaskRunning = false;
static bool parseTaskRunning      = false;
static bool klineTaskRunning      = false;
static bool ignitionTaskRunning   = false;
static bool buttonOutputTaskRunning = false;
static bool oilSensorTaskRunning  = false;

// Liveness heartbeat written at the top of every fisRenderTask iteration. The
// ignitionTask (an independent task) watches it: if a card should be on-screen
// but the render loop has not advanced for a sustained window, the render task
// is wedged (a frozen screen that produces no transfer errors, so the transfer
// watchdog cannot see it) and a FIS re-boot is forced.
static volatile uint32_t s_fisRenderHeartbeatMs = 0;

static void openHaldexBroadcastTask(void* pvParameters) {
    (void)pvParameters;
    for (;;) {
        if (hasHaldex && ignitionState && isConnectedCAN) {
            broadcastOpenHaldex();
        }
        vTaskDelay(pdMS_TO_TICKS(canRefresh));
    }
}

static void klineTask(void* pvParameters) {
    (void)pvParameters;

    for (;;) {
        if (!ignitionState || fisDisable || !hasK || !isConnectedK || showHaldex) {
            // Service a pending module reconnect even when not actively polling
            menuServiceKlineReconnect();
            vTaskDelay(pdMS_TO_TICKS(logFrequency));
            continue;
        }

        // Service a pending K-line module change before the next poll
        menuServiceKlineReconnect();

        if (readBlock > 255) {
            readBlock = 1;
        }
        showMeasurements(readBlock);
        lastKlineTransmission = millis();

        vTaskDelay(pdMS_TO_TICKS(logFrequency));
    }
}

static void fisRenderTask(void* pvParameters) {
    (void)pvParameters;
    // lastCanCardName uses '\xFF' sentinel (unmatchable) so first CAN render always clears.
    // lastShowHaldex=false so Haldex entry is always detected cleanly on re-enable/boot.
    static char lastCanCardName[24] = {'\xFF', 0};
    static bool lastShowHaldex      = false;
    static bool rpmLogoWasActive    = false;

    // FIS watchdog: when a card should be on-screen but transfers to the
    // cluster keep failing (fisErrorEventCount climbing) over a sustained
    // window, force a full FIS re-boot. A cooldown prevents restart loops.
    // Failures need not be back-to-back: a short error-free gap does not reset
    // the streak, so an intermittently-NACKing/frozen cluster is still caught.
    static const uint32_t kFisWatchdogFailMs     = 3000;
    static const uint32_t kFisWatchdogCooldownMs = 20000;
    static const uint32_t kFisWatchdogClearMs    = 750;  // error-free gap that ends a streak
    static uint32_t lastFisErrCount   = 0;
    static uint32_t fisBadStreakSince = 0;   // 0 = no active failure streak
    static uint32_t lastFisErrorMs    = 0;   // millis() of the most recent errored cycle
    static uint32_t lastFisRecoveryMs = 0;

    for (;;) {
        // Liveness heartbeat — updated every iteration before any branch so the
        // ignitionTask can detect a wedged render loop (frozen screen).
        s_fisRenderHeartbeatMs = millis();

        // Apply deferred actions first — must run before the fisDisable guard
        // so that EXIT can clear the screen and set fisDisable in one shot.
        menuApplyPendingAction();

        // -----------------------------------------------------------------
        // FIS watchdog — evaluated from the previous cycle's transfer result.
        // -----------------------------------------------------------------
        {
            const uint32_t now    = millis();
            const uint32_t errNow = fisErrorEventCount;
            const bool cycleErrored = (errNow != lastFisErrCount);
            lastFisErrCount = errNow;

            const bool expectingCard =
                hasFIS && fisBootReady && ignitionState && !fisDisable && !isMenuOpen() &&
                ((hasCAN && isConnectedCAN) || (hasK && isConnectedK) || (hasHaldex && showHaldex));

            if (!expectingCard) {
                fisBadStreakSince = 0;
            } else if (cycleErrored) {
                lastFisErrorMs = now;
                if (fisBadStreakSince == 0) {
                    fisBadStreakSince = now;
                } else if ((now - fisBadStreakSince) >= kFisWatchdogFailMs &&
                           (now - lastFisRecoveryMs) >= kFisWatchdogCooldownMs) {
                    LOGSYS("FIS watchdog: unresponsive for %lums — forcing FIS re-boot",
                           (unsigned long)(now - fisBadStreakSince));
                    lastFisRecoveryMs = now;
                    fisBadStreakSince = 0;
                    requestRestartIgnitionSequence();
                }
            } else if (fisBadStreakSince != 0 &&
                       (now - lastFisErrorMs) >= kFisWatchdogClearMs) {
                // Only a sustained error-free gap ends the streak, so brief
                // successful cycles between failures do not mask a bad cluster.
                fisBadStreakSince = 0;
            }
        }

        if (!hasFIS || !fisBootReady || !ignitionState || fisDisable) {

            // Reset to unmatchable sentinel — re-enable always triggers a fresh clear.
            // lastShowHaldex=false so the Haldex entry path fires correctly on next enable.
            lastCanCardName[0] = '\xFF';
            lastShowHaldex     = false;
            rpmLogoWasActive   = false;
            menuClose(); // close menu if FIS is disabled or ignition off
            vTaskDelay(pdMS_TO_TICKS(50));
            continue;
        }

        // -----------------------------------------------------------------
        // Menu: takes priority over all other rendering when open
        // -----------------------------------------------------------------
        if (isMenuOpen()) {
            menuRender();
            // Reset trackers so normal rendering resumes cleanly after close
            lastCanCardName[0] = '\xFF';
            lastShowHaldex     = false;
            rpmLogoWasActive   = false;
            vTaskDelay(pdMS_TO_TICKS(40));
            continue;
        }

        if (fisStatusHoldUntilMs != 0 && (int32_t)(fisStatusHoldUntilMs - millis()) > 0) {
            if (fisStatusClearPending) {
                fisStatusClearPending = false;
                FIS.clear();
            }
            displayFIS();
            vTaskDelay(pdMS_TO_TICKS(40));
            continue;
        }

        // -----------------------------------------------------------------
        // RPM logo overlay: takes priority over all card rendering.
        // Drawn once on entry; cleared + forced re-render when RPM drops.
        // -----------------------------------------------------------------
        const bool rpmLogoTestActive = (rpmLogoTestUntilMs != 0 && (int32_t)(rpmLogoTestUntilMs - millis()) > 0);
        const bool rpmLogoActive = rpmLogoTestActive || (rpmLogoThreshold > 0 && vehicleRPM >= rpmLogoThreshold);
        if (rpmLogoActive) {
            if (!rpmLogoWasActive) {
                drawScreen(); // renders atomically: clear→bitmap→update
            }
            rpmLogoWasActive = true;
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        if (rpmLogoWasActive) {
            // Dropped below threshold (or test window ended): clear and force a fresh card render.
            rpmLogoWasActive   = false;
            rpmLogoTestUntilMs = 0;
            lastCanCardName[0] = '\xFF';
            FIS.clear();
            FIS.update();
            vTaskDelay(pdMS_TO_TICKS(fisWakeDelay));
            continue;
        }

        // -----------------------------------------------------------------
        // K-line: clear whenever the selected block changes.
        // Only if showHaldex is false — Haldex takes priority over K-line display.
        // -----------------------------------------------------------------
        if (hasK && isConnectedK && !showHaldex) {
            if (lastBlock != readBlock) {
                lastBlock = readBlock;
                FIS.clear();
                FIS.update();
                vTaskDelay(pdMS_TO_TICKS(fisWakeDelay));
                continue;
            }
            displayFIS();
            vTaskDelay(pdMS_TO_TICKS(40));
            continue;
        }

        // -----------------------------------------------------------------
        // Haldex overlay: clear when first entering (!lastShowHaldex) OR mode changes.
        // -----------------------------------------------------------------
        if (hasHaldex && showHaldex) {
            if (!lastShowHaldex || lastHaldex != lastMode) {
                lastShowHaldex = true;
                lastHaldex     = lastMode;
                FIS.clear();
                FIS.update();
                vTaskDelay(pdMS_TO_TICKS(fisWakeDelay));
                continue;
            }
            parseFIS();
            displayFIS();
            vTaskDelay(pdMS_TO_TICKS(40));
            continue;
        }

        // -----------------------------------------------------------------
        // Haldex just turned OFF: clear before showing the CAN card.
        // -----------------------------------------------------------------
        if (lastShowHaldex) {
            lastShowHaldex     = false;
            lastCanCardName[0] = '\xFF'; // force CAN card change detection too
            FIS.clear();
            FIS.update();
            vTaskDelay(pdMS_TO_TICKS(fisWakeDelay));
            continue;
        }

        // -----------------------------------------------------------------
        // CAN: clear whenever the displayed card changes
        // -----------------------------------------------------------------
        if (hasCAN && isConnectedCAN) {
            const char* cardName = getCurrentCanCardName();
            if (strcmp(cardName, lastCanCardName) != 0) {
                strncpy(lastCanCardName, cardName, sizeof(lastCanCardName) - 1);
                lastCanCardName[sizeof(lastCanCardName) - 1] = '\0';
                FIS.clear();
                FIS.update();
                vTaskDelay(pdMS_TO_TICKS(fisWakeDelay));
                continue;
            }
            parseFIS();
            displayFIS();
            vTaskDelay(pdMS_TO_TICKS(40));
            continue;
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// ---------------------------------------------------------------------------
// Ignition task — ADC fallback sampling + ignition lifecycle (boot/shutdown).
// Moved out of loop() so the launch/shutdown sequences run in their own task.
// ---------------------------------------------------------------------------
static bool readIgnitionStateFromAdc(bool previousState) {
    const int raw = analogRead(ignitionMonitorPin);
    if (previousState) {
        return raw >= ignitionAdcOffThreshold;
    }
    return raw >= ignitionAdcOnThreshold;
}

static void serviceIgnitionFallback() {
    static uint32_t lastSampleMs = 0;
    const uint32_t now = millis();
    if (now - lastSampleMs < 20) {
        return;
    }
    lastSampleMs = now;

    const bool stateNow = readIgnitionStateFromAdc(ignitionState);
    if (stateNow == ignitionState) {
        return;
    }

    noInterrupts();
    ignitionState = stateNow;
    if (stateNow) {
        ignitionRiseEvent = true;
        triggerShutdown   = true;
    } else {
        ignitionFallEvent = true;
    }
    interrupts();
}

static void ignitionTask(void* pvParameters) {
    (void)pvParameters;
    static uint32_t lastIgnitionPrintMs = 0;
    static uint32_t lastRenderStallRecoveryMs = 0;

    for (;;) {
        serviceIgnitionFallback();

        // -----------------------------------------------------------------
        // Render-loop stall watchdog — catches a wedged fisRenderTask (frozen
        // screen with no transfer errors). Independent of the transfer-error
        // watchdog inside fisRenderTask, which cannot observe its own stall.
        // -----------------------------------------------------------------
        {
            static const uint32_t kRenderStallMs    = 5000;
            static const uint32_t kRenderCooldownMs = 20000;
            const uint32_t now = millis();
            const bool expectingRender =
                hasFIS && fisBootReady && ignitionState && !fisDisable && ignitionStateRunOnce;
            if (expectingRender && s_fisRenderHeartbeatMs != 0 &&
                (now - s_fisRenderHeartbeatMs) >= kRenderStallMs &&
                (now - lastRenderStallRecoveryMs) >= kRenderCooldownMs) {
                LOGSYS("FIS watchdog: render loop stalled for %lums — forcing FIS re-boot",
                       (unsigned long)(now - s_fisRenderHeartbeatMs));
                lastRenderStallRecoveryMs = now;
                requestRestartIgnitionSequence();
            }
        }

        const uint32_t now = millis();
        if (now - lastIgnitionPrintMs >= 200) {
            lastIgnitionPrintMs = now;
            LOGIO("IGN adc=%d dig=%d state=%d",
                  analogRead(ignitionMonitorPin),
                  digitalRead(ignitionMonitorPin),
                  ignitionState ? 1 : 0);
        }

        if (restartIgnitionRequested) {
            noInterrupts();
            restartIgnitionRequested = false;
            interrupts();
            restartIgnitionSequence();
        }

        if (ignitionRiseEvent) {
            noInterrupts();
            ignitionRiseEvent = false;
            interrupts();

            LOGIO("IGNITION: ON");

            if (!ignitionStateRunOnce) {
                DEBUG("Launch sequence initiated...");
                launchBoot();
                launchConnections();
                ignitionStateRunOnce = true;
            }
        }

        if (ignitionFallEvent) {
            noInterrupts();
            ignitionFallEvent = false;
            interrupts();
            LOGIO("IGNITION: OFF");
        }

        // If ignition is already high at boot there may be no rise edge event.
        if (ignitionState && !ignitionStateRunOnce) {
            LOGIO("IGNITION: ON");
            DEBUG("Launch sequence initiated (bootstrap)...");
            launchBoot();
            launchConnections();
            ignitionStateRunOnce = true;
        }

        if (!ignitionState && triggerShutdown) {
            beginShutdown();
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

// ---------------------------------------------------------------------------
// Button-output task — drives the simulated stalk output pulses.
// ---------------------------------------------------------------------------
static void buttonOutputTask(void* pvParameters) {
    (void)pvParameters;
    for (;;) {
        serviceButtonOutputs();
        serviceExternalOutput();
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}

// ---------------------------------------------------------------------------
// Oil-sensor task — runtime-toggleable from the UI (oilSensorEnabled).
// ---------------------------------------------------------------------------
static void oilSensorTask(void* pvParameters) {
    (void)pvParameters;
    for (;;) {
        if (oilSensorEnabled) {
            serviceOilSensor();
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void startTasks() {
    if (!fisTaskRunning) {
        xTaskCreatePinnedToCore(
            fisRenderTask,
            "FIS_RENDER",
            8192,
            nullptr,
            2,          // Priority 2: higher than klineTask so rendering isn't starved
            nullptr,
            1);         // Core 1
        fisTaskRunning = true;
    }

    if (!parseTaskRunning) {
        // parseFIS() is called inline in fisRenderTask to eliminate the
        // fisLine[] String race between two tasks writing/reading concurrently.
        parseTaskRunning = true;
    }

    if (!klineTaskRunning) {
        xTaskCreatePinnedToCore(
            klineTask,
            "KLINE_POLL",
            8192,
            nullptr,
            1,          // Priority 1
            nullptr,
            0);         // Core 0: keep slow blocking K-line I/O off the render core
        klineTaskRunning = true;
    }

    if (!openHaldexTaskRunning) {
        xTaskCreatePinnedToCore(
            openHaldexBroadcastTask,
            "OH_BCAST",
            3072,
            nullptr,
            1,
            nullptr,
            0);         // Core 0
        openHaldexTaskRunning = true;
    }

    if (!ignitionTaskRunning) {
        xTaskCreatePinnedToCore(
            ignitionTask,
            "IGNITION",
            8192,       // launchBoot()/launchConnections() run here — needs headroom
            nullptr,
            2,
            nullptr,
            1);         // Core 1
        ignitionTaskRunning = true;
    }

    if (!buttonOutputTaskRunning) {
        xTaskCreatePinnedToCore(
            buttonOutputTask,
            "BTN_OUT",
            2048,
            nullptr,
            1,
            nullptr,
            1);         // Core 1
        buttonOutputTaskRunning = true;
    }

    if (!oilSensorTaskRunning) {
        xTaskCreatePinnedToCore(
            oilSensorTask,
            "OIL_SENSOR",
            2560,
            nullptr,
            1,
            nullptr,
            0);         // Core 0
        oilSensorTaskRunning = true;
    }
}

void IRAM_ATTR ignitionMonitorISR() {
    const bool stateNow = digitalRead(ignitionMonitorPin);
    ignitionState = stateNow;
    if (stateNow) {
        ignitionRiseEvent = true;
        triggerShutdown = true;
    } else {
        ignitionFallEvent = true;
    }
}
