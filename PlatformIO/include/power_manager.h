#ifndef POWER_MANAGER_H
#define POWER_MANAGER_H

/*
  power_manager — universal ESP32 reduced-power module
  -----------------------------------------------------
  Drop-in, project-agnostic power saver for ESP32 (DevKit V1 / WROOM-32) builds
  that are powered from ignition and therefore CANNOT deep-sleep permanently,
  but CAN shed a lot of average current once the user has stopped interacting.

  Why this matters for your hardware
  ----------------------------------
  A linear regulator dissipates heat = (Vin - Vout) * I_load. The ESP32's own
  current draw IS most of I_load, so cutting current directly cuts regulator
  heat. The biggest single consumer is the WiFi radio (~80-120 mA average,
  bursting to 300-500 mA). Turning WiFi off after a timeout, dropping the CPU
  clock, killing the unused Bluetooth controller and the onboard LED can take a
  typical "WiFi-on, 240 MHz" board from ~140-160 mA down to ~25-40 mA.

  What this module does (all individually toggle-able):
    1. WiFi auto-off after an inactivity timeout (default 1 min).
    2. CPU frequency scaling           240 MHz active -> 80 MHz reduced.
    3. Bluetooth/BLE controller release (frees ~60 KB RAM too).
    4. WiFi modem power-save + reduced TX power WHILE WiFi is still on.
    5. Onboard LED off.
    6. (Optional, OFF by default) automatic light-sleep via esp_pm — only safe
       if your project is NOT continuously bit-banging PWM/CAN/UART. See notes
       at enableAutoLightSleep below.

  How to use (any project)
  ------------------------
    #include "power_manager.h"

    // 1) In setup(), AFTER you start WiFi / your web server:
    power_config_t pcfg = powerDefaultConfig();
    pcfg.wifiIdleTimeoutMs = 5 * 60 * 1000UL;   // tweak as desired
    powerInit(&pcfg);

    // 2) Call powerKick() whenever the user "does something" you want to keep
    //    the device fully awake for (web request, button press, client
    //    connect). This resets the idle timer.
    powerKick();

    // 3) (Optional) Tell the manager to never drop WiFi while something is
    //    using it, by overriding the weak hook below to return true:
    //       bool powerIsBusy() { return WiFi.softAPgetStationNum() > 0; }

    // 4) (Optional) React to power-state changes to stop/start YOUR own
    //    services (AsyncWebServer, TCP servers, mDNS, etc.) by overriding:
    //       void powerOnEnterReduced() { server.end(); }
    //       void powerOnExitReduced()  { server.begin(); }
    //    The module turns the RADIO off/on for you; these hooks let your
    //    higher-level services shut down cleanly first / come back after.

  The module runs its own low-priority FreeRTOS task, so you don't have to call
  anything in loop(). It is plain C++ with no external library dependencies.

  ESP32-C3 / S2 note
  ------------------
  powerDefaultConfig() auto-detects the target chip at compile time:
    - CPU active clock caps at 160 MHz on C3/S2 (hardware limit).
    - Bluetooth memory is released with BLE mode on C3 (no Classic BT).
    - The onboard LED pin is set to -1 on C3 (LOLIN C3 Mini uses a WS2812B
      RGB LED on GPIO 7 that cannot be driven as a plain digital output).
  All other behaviour is identical across ESP32 variants.
*/

#include <Arduino.h>

// ---- Power states -----------------------------------------------------------
typedef enum
{
  POWER_STATE_ACTIVE = 0, // full power: WiFi on, CPU at active clock
  POWER_STATE_REDUCED = 1 // low power: WiFi radio off, CPU at reduced clock
} power_state_t;

// ---- Configuration ----------------------------------------------------------
typedef struct
{
  // --- WiFi auto-off ---
  uint32_t wifiIdleTimeoutMs;   // idle time before entering reduced power (0 = never auto-reduce)
  bool manageWifiRadio;         // if true, module calls WiFi.mode(WIFI_OFF)/restores on its own
  bool keepWifiWhileBusy;       // if true, powerIsBusy()==true keeps the device active

  // --- CPU frequency scaling ---
  bool scaleCpuFrequency;       // enable dynamic CPU clock switching
  uint16_t cpuFreqActiveMhz;    // clock while ACTIVE   (240 / 160 / 80)
  uint16_t cpuFreqReducedMhz;   // clock while REDUCED  (80 recommended; 40/20 only with WiFi off)

  // --- WiFi while still on (ACTIVE state) ---
  bool wifiModemSleepWhileOn;   // enable WIFI_PS_MIN_MODEM (saves power, tiny latency cost)
  bool reduceWifiTxPower;       // lower TX power (plenty for in-car close range)
  int8_t wifiTxPowerActive;     // wifi_power_t value (e.g. WIFI_POWER_8_5dBm). Only used if reduceWifiTxPower.

  // --- Bluetooth ---
  bool disableBluetooth;        // release BT/BLE controller + RAM at init (we never use it)

  // --- Onboard LED ---
  bool turnOffOnboardLed;       // drive the onboard LED off at init
  int8_t onboardLedPin;         // LED pin (DevKit V1 = 2). -1 to skip.
  bool ledActiveHigh;           // true = HIGH lights the LED (DevKit V1)

  // --- Automatic light sleep (ADVANCED, default OFF) ---
  // Uses esp_pm_configure() for tickless automatic light-sleep between idle
  // periods. This can shave another few mA but WILL disrupt continuous
  // peripheral activity (LEDC/PWM square-wave outputs, TWAI/CAN timing, polled
  // UART). Only enable on projects that sit idle between events. Also requires
  // CONFIG_PM_ENABLE + CONFIG_FREERTOS_USE_TICKLESS_IDLE in sdkconfig; if those
  // are off in your build, the call is a harmless no-op.
  bool enableAutoLightSleep;

  // --- Housekeeping ---
  bool verbose;                 // Serial.printf status lines
  uint32_t taskTickMs;          // how often the manager task evaluates (default 1000)
} power_config_t;

// Returns a sensible default config (1 min timeout, 240->80 MHz, BT off,
// LED off, modem-sleep on, light-sleep off).
// On ESP32-C3/S2, active clock defaults to 160 MHz and LED pin to -1.
power_config_t powerDefaultConfig(void);

// Start the manager. Pass NULL to use powerDefaultConfig(). Spawns the
// background task; safe to call once. Applies the one-shot savings (BT release,
// LED off, modem sleep, TX power, active CPU clock) immediately.
void powerInit(const power_config_t *cfg);

// Reset the inactivity timer. If currently REDUCED, transitions back to ACTIVE.
// Call on any user activity you want to wake/keep the device fully awake for.
void powerKick(void);

// Force transitions immediately (also (re)arms the idle timer for ForceActive).
void powerForceActive(void);
void powerForceReduced(void);

// Introspection.
power_state_t powerGetState(void);
uint32_t powerIdleMs(void);    // ms since last powerKick()
const char *powerStateName(power_state_t s);

// ---- Weak hooks: override any of these in YOUR project to customise ----------

// Return true to keep the device ACTIVE (timer is continuously re-armed).
// Default returns false. Typical override:
//   bool powerIsBusy() { return WiFi.softAPgetStationNum() > 0; }
bool powerIsBusy(void);

// Called when transitioning ACTIVE -> REDUCED, BEFORE the radio is turned off.
// Stop your web/TCP servers, mDNS, etc. here. Default: no-op.
void powerOnEnterReduced(void);

// Called when transitioning REDUCED -> ACTIVE, AFTER the radio is restored.
// Restart your servers here. Default: no-op.
void powerOnExitReduced(void);

#endif // POWER_MANAGER_H
