/*
 * main.cpp — FISCuntrolCAN
 *
 * PlatformIO / Arduino-framework entry point for ESP32 DevKit V1.
 *
 * CAN receive runs in a dedicated FreeRTOS task (canTask, core 0).
 * The Arduino loop() runs on core 1 and handles:
 *   - Ignition monitoring
 *   - Button ticks
 *   - Ignition lifecycle handling
 *   - CAN data parsing → fisLine[]
 *   - FIS rendering
 */

#include <Arduino.h>
#include "config.h"

#include "web_server.h"
#include "fis_display.h"
#include "can_bus.h"
#include "buttons.h"
#include "onboot.h"
#include "tasks.h"
#include "cards.h"
#include "oil_sensor.h"
#include "power_manager.h"
#include "timekeeper.h"

// ---------------------------------------------------------------------------
// setup()
// ---------------------------------------------------------------------------
void setup()
{
#if enableDebug
    Serial.begin(serialBaud);
    logInit();   // start the categorised serial/UI logging task
    DEBUG("ESP32 FISCuntrolCAN initialising...");
#endif

    setupWiFi();
    setupWebServer();
    timeInit();   // device clock (browser-set; optional DS1307 RTC) — after settings load

    // Universal reduced-power module: turns WiFi off 1 min after the last client
    // disconnects, scales CPU 240->80 MHz, releases Bluetooth and kills the
    // onboard LED to cut current draw (and therefore linear-regulator heat).
    power_config_t pcfg = powerDefaultConfig();
    powerInit(&pcfg);

    initCardMutex();   // Must be before any task that calls commitCard() or displayFIS()
    setupPins();
    setupButtons();
    setupOilSensor();
    startTasks();
}

// ---------------------------------------------------------------------------
// loop()
//
// Ignition lifecycle, button-output pulsing and the oil sensor now run in
// dedicated FreeRTOS tasks (see tasks.cpp). loop() only services the OneButton
// stalk inputs (which must be polled frequently) and the FIS bypass path.
// ---------------------------------------------------------------------------
void loop()
{
    // --- Button polling ---
    stalkUpButton.tick();
    stalkDownButton.tick();
    stalkResetButton.tick();

    // --- FIS disable/re-enable ---
    if (fisBeenToggled)
    {
        noInterrupts();
        fisBeenToggled = false;
        interrupts();

        DEBUG("FIS disable toggled – prepping...");
        fisDisablePrep();
        DEBUG("FIS disable prep complete.");
    }

    if (fisDisable)
    {
        mimickStalkButtons();
        delay(1);
        return; // nothing else to do when bypassed
    }
    // Ignition lifecycle + FIS parsing/rendering run in dedicated tasks.
    delay(1);
}
