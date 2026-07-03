/*
  power_manager — universal ESP32 reduced-power module (implementation)
  See power_manager.h for the full rationale and usage notes.

  ESP32-C3 / S2 compatibility
  ---------------------------
  Compile-time checks select the correct CPU frequency cap and Bluetooth
  memory-release mode for each chip family:
    - Original ESP32 (WROOM-32 / DevKit V1): 240 MHz, BTDM release.
    - ESP32-C3 (LOLIN C3 Mini etc.):         160 MHz, BLE-only release.
    - ESP32-S2:                              160 MHz, no Bluetooth at all.
  All run-time behaviour (WiFi radio management, CPU scaling, FreeRTOS task)
  is identical across variants.
*/

#include "power_manager.h"
#include <WiFi.h>

#if __has_include("esp_bt.h")
#include "esp_bt.h"
#define POWER_HAS_BT 1
#endif

#if __has_include("esp_pm.h")
#include "esp_pm.h"
#define POWER_HAS_PM 1
#endif

// ---- Module state -----------------------------------------------------------
static power_config_t g_cfg;
static volatile uint32_t g_lastActivityMs = 0;
static volatile power_state_t g_state = POWER_STATE_ACTIVE;
static volatile bool g_forceActive = false;
static volatile bool g_forceReduced = false;
static bool g_initialised = false;
static TaskHandle_t g_taskHandle = NULL;

// ---- Weak default hooks (override in your project) --------------------------
__attribute__((weak)) bool powerIsBusy(void) { return false; }
__attribute__((weak)) void powerOnEnterReduced(void) {}
__attribute__((weak)) void powerOnExitReduced(void) {}

// ---- Helpers ----------------------------------------------------------------
static void powerLog(const char *fmt, ...)
{
  if (!g_cfg.verbose)
    return;
  char buf[128];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  Serial.printf("[PWR] %s\n", buf);
}

static void powerApplyLightSleep(bool enable)
{
#if POWER_HAS_PM
  if (!g_cfg.enableAutoLightSleep)
    return;
  esp_pm_config_t pm = {};
  pm.max_freq_mhz = g_cfg.cpuFreqActiveMhz;
  pm.min_freq_mhz = g_cfg.cpuFreqReducedMhz;
  pm.light_sleep_enable = enable;
  esp_err_t err = esp_pm_configure(&pm);
  if (err != ESP_OK)
    powerLog("auto light-sleep unavailable (esp_pm err 0x%X) — needs CONFIG_PM_ENABLE", err);
  else
    powerLog("auto light-sleep %s", enable ? "ON" : "OFF");
#else
  (void)enable;
#endif
}

static void powerSetCpu(uint16_t mhz)
{
  if (!g_cfg.scaleCpuFrequency)
    return;
  setCpuFrequencyMhz(mhz);
  powerLog("CPU @ %u MHz", (unsigned)getCpuFrequencyMhz());
}

// ---- State transitions (only ever called from the manager task) -------------
static void powerEnterReduced(void)
{
  if (g_state == POWER_STATE_REDUCED)
    return;
  powerLog("--> REDUCED (idle %lus)", (unsigned long)(powerIdleMs() / 1000));

  // 1) Let the project shut down its own services (web/TCP servers, mDNS) while
  //    the radio is still up so sockets close cleanly.
  powerOnEnterReduced();

  // 2) Kill the radio — the single biggest current saving.
  if (g_cfg.manageWifiRadio)
  {
    WiFi.disconnect(true, false);
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_OFF);
    powerLog("WiFi radio OFF");
  }

  // 3) Drop the clock (safe to go <80 MHz now WiFi is off).
  powerSetCpu(g_cfg.cpuFreqReducedMhz);

  // 4) Optional tickless auto light-sleep.
  powerApplyLightSleep(true);

  g_state = POWER_STATE_REDUCED;
}

static void powerExitToActive(void)
{
  if (g_state == POWER_STATE_ACTIVE)
    return;
  powerLog("--> ACTIVE");

  // 1) Stop auto light-sleep first so timing-sensitive bring-up is clean.
  powerApplyLightSleep(false);

  // 2) Raise the clock BEFORE WiFi (the radio requires >= 80 MHz).
  powerSetCpu(g_cfg.cpuFreqActiveMhz);

  // 3) Re-apply the "WiFi on but frugal" settings for when the project brings
  //    the radio back up inside powerOnExitReduced().
  if (g_cfg.wifiModemSleepWhileOn)
    WiFi.setSleep(WIFI_PS_MIN_MODEM);

  // 4) Hand back to the project to re-establish WiFi (AP/STA) + servers.
  //    NOTE: because re-joining/AP-config is project specific, the module does
  //    not blindly restore the radio — override powerOnExitReduced() to call
  //    your own connectWifi()/server.begin() (see header for an example).
  powerOnExitReduced();

  if (g_cfg.reduceWifiTxPower)
    WiFi.setTxPower((wifi_power_t)g_cfg.wifiTxPowerActive);

  g_state = POWER_STATE_ACTIVE;
}

// ---- Manager task -----------------------------------------------------------
static void powerTask(void *arg)
{
  (void)arg;
  for (;;)
  {
    if (g_forceReduced)
    {
      g_forceReduced = false;
      powerEnterReduced();
    }
    else if (g_forceActive)
    {
      g_forceActive = false;
      g_lastActivityMs = millis();
      powerExitToActive();
    }
    else
    {
      // Anything the project reports as "busy" keeps us awake.
      if (g_cfg.keepWifiWhileBusy && powerIsBusy())
        g_lastActivityMs = millis();

      if (g_state == POWER_STATE_ACTIVE &&
          g_cfg.wifiIdleTimeoutMs > 0 &&
          (millis() - g_lastActivityMs) >= g_cfg.wifiIdleTimeoutMs)
      {
        powerEnterReduced();
      }
    }

    vTaskDelay(pdMS_TO_TICKS(g_cfg.taskTickMs));
  }
}

// ---- Public API -------------------------------------------------------------
power_config_t powerDefaultConfig(void)
{
  power_config_t c = {};
  c.wifiIdleTimeoutMs = 1UL * 60UL * 1000UL; // 1 minute
  c.manageWifiRadio = true;
  c.keepWifiWhileBusy = true;

  c.scaleCpuFrequency = true;
  // ESP32-C3 and S2 are capped at 160 MHz; original ESP32 supports 240 MHz.
#if defined(CONFIG_IDF_TARGET_ESP32C3) || defined(CONFIG_IDF_TARGET_ESP32S2)
  c.cpuFreqActiveMhz = 160;
#else
  c.cpuFreqActiveMhz = 240;
#endif
  c.cpuFreqReducedMhz = 80;

  c.wifiModemSleepWhileOn = true;
  c.reduceWifiTxPower = true;
  c.wifiTxPowerActive = (int8_t)WIFI_POWER_8_5dBm;

  c.disableBluetooth = true;

  // LOLIN C3 Mini uses a WS2812B RGB LED on GPIO 7 — not a simple GPIO output.
  // Skip the LED-off step on C3. DevKit V1 onboard LED is on GPIO 2 (active-HIGH).
#if defined(CONFIG_IDF_TARGET_ESP32C3)
  c.turnOffOnboardLed = false;
  c.onboardLedPin = -1;
  c.ledActiveHigh = false;
#else
  c.turnOffOnboardLed = true;
  c.onboardLedPin = 2; // DevKit V1
  c.ledActiveHigh = true;
#endif

  c.enableAutoLightSleep = false; // advanced — see header

  c.verbose = true;
  c.taskTickMs = 1000;
  return c;
}

void powerInit(const power_config_t *cfg)
{
  if (g_initialised)
    return;

  g_cfg = cfg ? *cfg : powerDefaultConfig();
  g_state = POWER_STATE_ACTIVE;
  g_lastActivityMs = millis();

  // --- One-shot savings applied immediately ---

  // Bluetooth: never used here — disable controller and reclaim its RAM.
  if (g_cfg.disableBluetooth)
  {
#if POWER_HAS_BT
    btStop();
    esp_bt_controller_disable();
    // Memory release mode differs by chip:
    //   - Original ESP32: Classic BT + BLE (BTDM).
    //   - ESP32-C3/S3/H2: BLE only — BTDM mode does not exist.
#if defined(CONFIG_IDF_TARGET_ESP32)
    esp_bt_controller_mem_release(ESP_BT_MODE_BTDM);
#else
    esp_bt_controller_mem_release(ESP_BT_MODE_BLE);
#endif
    powerLog("Bluetooth controller released");
#endif
  }

  // Onboard LED off.
  if (g_cfg.turnOffOnboardLed && g_cfg.onboardLedPin >= 0)
  {
    pinMode(g_cfg.onboardLedPin, OUTPUT);
    digitalWrite(g_cfg.onboardLedPin, g_cfg.ledActiveHigh ? LOW : HIGH);
    powerLog("Onboard LED off (pin %d)", (int)g_cfg.onboardLedPin);
  }

  // Frugal-but-on WiFi tuning (only takes effect while the radio is up).
  if (g_cfg.wifiModemSleepWhileOn)
    WiFi.setSleep(WIFI_PS_MIN_MODEM);
  if (g_cfg.reduceWifiTxPower)
    WiFi.setTxPower((wifi_power_t)g_cfg.wifiTxPowerActive);

  // Active CPU clock.
  powerSetCpu(g_cfg.cpuFreqActiveMhz);

  // Background watcher — pin to core 0 (valid for both single-core C3 and dual-core ESP32).
  xTaskCreatePinnedToCore(powerTask, "powerMgr", 3072, NULL, 1, &g_taskHandle, 0);
  g_initialised = true;

  powerLog("init: timeout %lus, %u->%u MHz",
           (unsigned long)(g_cfg.wifiIdleTimeoutMs / 1000),
           (unsigned)g_cfg.cpuFreqActiveMhz,
           (unsigned)g_cfg.cpuFreqReducedMhz);
}

void powerKick(void)
{
  g_lastActivityMs = millis();
  if (g_state == POWER_STATE_REDUCED)
    g_forceActive = true;
}

void powerForceActive(void)
{
  g_lastActivityMs = millis();
  g_forceReduced = false;
  g_forceActive = true;
}

void powerForceReduced(void)
{
  g_forceActive = false;
  g_forceReduced = true;
}

power_state_t powerGetState(void) { return g_state; }

uint32_t powerIdleMs(void) { return millis() - g_lastActivityMs; }

const char *powerStateName(power_state_t s)
{
  return (s == POWER_STATE_REDUCED) ? "REDUCED" : "ACTIVE";
}
