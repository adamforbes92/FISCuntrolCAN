#pragma once

/*
 * FISCuntrolCAN - Global Configuration
 * All compile-time defines, pin assignments, CAN IDs, and shared types.
 */

// ---------------------------------------------------------------------------
// Debug
// ---------------------------------------------------------------------------
#define enableDebug        1
#define serialBaud         115200
#define checkLED           0    // 1 = run LED output test on boot (debug only)

#include "logging.h"

// Categorised logging. Each macro routes to a runtime-toggleable category that
// gates output to BOTH serial and the UI log ring (see logging.h / logging.cpp).
//   DEBUG/LOGSYS -> [SYS]   LOGCAN -> [CAN]   LOGKLN -> [KLINE]
//   LOGIO -> [IO]           LOGWIFI -> [WIFI]
#if enableDebug
  #define DEBUG(x, ...)   logPrintf(LOG_SYS,   x, ##__VA_ARGS__)
  #define LOGSYS(x, ...)  logPrintf(LOG_SYS,   x, ##__VA_ARGS__)
  #define LOGCAN(x, ...)  logPrintf(LOG_CAN,   x, ##__VA_ARGS__)
  #define LOGKLN(x, ...)  logPrintf(LOG_KLINE, x, ##__VA_ARGS__)
  #define LOGIO(x, ...)   logPrintf(LOG_IO,    x, ##__VA_ARGS__)
  #define LOGWIFI(x, ...) logPrintf(LOG_WIFI,  x, ##__VA_ARGS__)
#else
  #define DEBUG(x, ...)
  #define LOGSYS(x, ...)
  #define LOGCAN(x, ...)
  #define LOGKLN(x, ...)
  #define LOGIO(x, ...)
  #define LOGWIFI(x, ...)
#endif

// ---------------------------------------------------------------------------
// Feature toggles
// ---------------------------------------------------------------------------
#define hasRTC     0    // Hardware RTC (DS1307)
#define isMPH      1    // Convert km/h readings to mph

// RTC (DS1307) I2C pins — used when "Use RTC" is enabled in the UI.
#define RTC_SDA_PIN  21
#define RTC_SCL_PIN  22

// ---------------------------------------------------------------------------
// Timing
// ---------------------------------------------------------------------------
#define fisWakeDelay           500    // ms delay after FIS power-on before sending data
#define fisBootTimeoutMs       1500   // max ms to wait for FIS init before continuing startup
#define fisStatusCardHoldMs    1800   // ms to keep redrawing transient status cards (e.g. K-line connecting)
#define bootScreenDuration     4000   // ms to show boot logo/message
#define connectionDelayDuration 0     // ms to show "connected" messages (0 = skip)
#define logFrequency           100    // K-line reads per second (ms between reads)
#define canRefresh             50     // ms between OpenHaldex CAN broadcasts

// ---------------------------------------------------------------------------
// FIS display
// ---------------------------------------------------------------------------
#include <TLBFISLib.h>

#define fisCLK   18   // SPI CLK  (default ESP32 VSPI CLK)
#define fisDATA  23   // SPI MOSI (default ESP32 VSPI MOSI)
#define fisENA    19  // SPI CS — ENA line (bidirectional, driven by TLBLib as INPUT/OUTPUT)
#define fisENADIR 32  // 2DIR on CJ74LVC4T245: HIGH = ESP32→FIS (A→B), LOW = FIS→ESP32 (B→A)
                      // 1DIR is hardwired to 3.3V — CLK and DATA are always A→B

#define screenSize           TLBFISLib::FULLSCREEN
#define globalTextAlignment  TLBFISLib::LEFT
#define SPI_INSTANCE         SPI

#define defaultBootScreen 2    // 0=off, 1=text welcome, 2=MK4Golf, 3=logo, 4=finger, 5=custom BMP
#define displayECUonBoot  0    // show ECU part number on successful K-line connect

// ---------------------------------------------------------------------------
// K-line
// ---------------------------------------------------------------------------
#define K_TX     17
#define K_RX     16
#define K_line   Serial2
#define K_Baud   10400
#define K_Module 0x01
#define is_full_duplex false

// ---------------------------------------------------------------------------
// CAN / TWAI pins
// ---------------------------------------------------------------------------
#define pinCAN_RX  13
#define pinCAN_TX  14
#define CAN_BAUD   500000   // 500 kbit/s

// ---------------------------------------------------------------------------
// Stalk buttons
// ---------------------------------------------------------------------------
#define stalkPushUp          34
#define stalkPushDown        39
#define stalkPushReset       36
#define stalkPushUpReturn    27
#define stalkPushDownReturn  26
#define stalkPushResetReturn 25

// ---------------------------------------------------------------------------
// Misc GPIO
// ---------------------------------------------------------------------------
#define ignitionMonitorPin 35
#define extOutputPin       12   // External Output — GPIO 12

// Ignition monitor uses ADC sampling on GPIO35 with hysteresis.
// Tune these values based on measured OFF/ON raw ADC values (0..4095).
#define ignitionAdcOnThreshold  2400
#define ignitionAdcOffThreshold 1600

// ---------------------------------------------------------------------------
// Oil level / temperature sensor (Hella / VAG G266, single-wire PWM)
// ---------------------------------------------------------------------------
#define hasOilSensor        0          // 1 = compile-in oil sensor module
#define oilSensorPin        4          // GPIO connected to level-shifted signal
#define oilSensorRawLog     0          // 1 = print every decoded pulse to Serial
                                       //     (use to validate timings against scope)
#define oilSensorStaleMs    3000       // mark reading invalid after this many ms
                                       //     without a fresh frame

// Signal model: CONTINUOUS single-wire PWM (NOT the BMW/Hella PULS 3-pulse
// frame). The MK4 G266 emits a free-running ~18-30 Hz square wave whose PERIOD
// and HIGH-TIME both rise with oil temperature. See the captures analysed in
// MK4GolfOilLevelSensor/readme.md. All timings in microseconds.

// Per-cycle sanity window — cycles outside this are counted as errors and
// dropped. Captures ranged ~36 ms (cold) to ~55 ms (boiling) period.
#define oilSensorPeriodMinUs      25000UL   // ~40 Hz — faster than ever observed
#define oilSensorPeriodMaxUs      70000UL   // ~14 Hz — slower than ever observed
#define oilSensorPulseMinUs        4000UL   // 4 ms high (cold was ~7-9 ms)
#define oilSensorPulseMaxUs       65000UL   // 65 ms high (must stay < period)

// Exponential smoothing applied to the per-cycle temperature (0..1).
// Lower = smoother/slower. The waveform is ~20 Hz so even 0.08 settles in
// well under a second.
#define oilSensorFilterAlpha        0.08f

// --- Temperature calibration ------------------------------------------------
// Two-point LINEAR fit of PERIOD -> temperature, derived from the water-bath
// captures (room / cold tap water ~= 20 C, boiling ~= 100 C). Values are
// extrapolated linearly outside this range. RE-CHECK against a dash / CAN
// oil-temperature reading — the real curve is likely mildly non-linear.
#define oilSensorTempPeriod0Us    36500.0f   // ~27 Hz  -> 20 C
#define oilSensorTempPeriod0C        20.0f
#define oilSensorTempPeriod1Us    54000.0f   // ~18.5 Hz -> 100 C
#define oilSensorTempPeriod1C       100.0f

// --- Level calibration ------------------------------------------------------
// NOT YET CALIBRATED. The capture set was made in WATER with the level pad
// dry, so there is no valid pulse-width -> level mapping. While
// oilSensorLevelCalibrated=0 the level output is forced to NAN so it can't be
// mistaken for a real reading. To enable it, run an oil-bath depth test, set
// oilSensorLevelCalibrated=1 and fill in the two points below.
#define oilSensorLevelCalibrated      0
#define oilSensorLevelPulse0Us    23000.0f   // PLACEHOLDER — high-time at 0 mm
#define oilSensorLevelPulse0Mm       0.0f
#define oilSensorLevelPulse1Us    47000.0f   // PLACEHOLDER — high-time at full
#define oilSensorLevelPulse1Mm     150.0f
#define oilSensorLevelSpanMm       150.0f   // mm corresponding to 100 %

// ---------------------------------------------------------------------------
// WiFi / OTA
// ---------------------------------------------------------------------------
#define wifiHostName "FISCuntrolCAN"

// ---------------------------------------------------------------------------
// CAN frame IDs
// ---------------------------------------------------------------------------
#define MOTOR1_ID       0x280
#define MOTOR2_ID       0x288
#define MOTOR3_ID       0x380
#define MOTOR5_ID       0x480
#define MOTOR6_ID       0x488
#define MOTOR7_ID       0x588
#define MOTOR_FLEX_ID   0x580
#define GRA_ID          0x38A
#define BRAKES1_ID      0x1A0
#define BRAKES2_ID      0x2A0
#define BRAKES3_ID      0x4A0
#define BRAKES5_ID      0x5A0
#define HALDEX_ID       0x2C0
#define fisCuntrol_ID   0x6A0
#define openHaldex_ID   0x6B0
#define ignitron1_ID    0x7C4
#define ignitron2_ID    0x7C6
#define ignitron3_ID    0x7C8

// Ignitron custom-CAN broadcast (dashCAN "ICD01" map) — sequential frames
// 0x600..0x604. See docs/ICD01_format.md and the ignitronSignals[] table in
// src/can_bus.cpp for the signal layout packed into these frames.
#define IGNITRON_ICD_ID_BASE   0x600
#define IGNITRON_ICD_ID_COUNT  5

// ---------------------------------------------------------------------------
// Conversion constants
// ---------------------------------------------------------------------------
#define mphFactor   621371   // multiply km/h by this then divide by 1 000 000

// ---------------------------------------------------------------------------
// Utility macros
// ---------------------------------------------------------------------------
#define arraySize(a)    (sizeof((a)) / sizeof((a)[0]))
#define serialPacketEnd 0xFF

// ---------------------------------------------------------------------------
// OpenHaldex types (shared across modules)
// ---------------------------------------------------------------------------
typedef enum {
  MODE_STOCK,
  MODE_FWD,
  MODE_5050,
  MODE_6040,
  MODE_7525,
  MODE_EXPERT,
  MODE_COUNT,
} openhaldex_mode_id;

static inline bool isValidOpenHaldexMode(uint8_t mode) {
  return mode < (uint8_t)MODE_COUNT;
}

typedef struct {
  openhaldex_mode_id mode;
  uint8_t            ped_threshold;
  bool               mode_override;
} openhaldexState;

typedef enum {
  BOOT_SOURCE_KLINE = 0,
  BOOT_SOURCE_CAN   = 1,
  BOOT_SOURCE_NONE  = 2,
} boot_source_id;

// Which CAN view is shown when the CAN source is active. Persisted so the
// long-press CAN/OpenHaldex toggle returns to whichever CAN view was last
// selected (standard MOTOR cards or the Ignitron ICD01 sensor cards).
typedef enum {
  CAN_VIEW_STANDARD = 0,   // decoded MOTOR_x cards
  CAN_VIEW_IGNITRON = 1,   // Ignitron ICD01 sensor cards
} can_view_id;

// ---------------------------------------------------------------------------
// Global shared state (declared in main.cpp, extern-ed everywhere else)
// ---------------------------------------------------------------------------
extern bool     ignitionState;
extern bool     ignitionStateRunOnce;
extern bool     hasFIS;
extern bool     hasCAN;
extern bool     hasK;
extern bool     hasHaldex;
extern bool     fisDisable;
extern bool     fisBeenToggled;
extern bool     runOnce;
extern bool     showHaldex;
extern bool     isConnectedK;
extern bool     isConnectedCAN;
extern bool     hasOpenHaldex;
extern bool     mimickSet;
extern bool     triggerShutdown;
extern bool     isStandalone;
extern bool     fisBootReady;
extern bool     fisFeedback;
extern volatile uint32_t fisErrorEventCount;   // incremented by the FIS error callback (comm failure)

extern uint8_t  vehicleSpeed;
extern uint8_t  haldexVehicleSpeed;
extern uint8_t  haldexEngagement;
extern uint8_t  haldexState;
extern uint8_t  lockTarget;
extern uint8_t  pedValue;
extern uint8_t  bootScreenSelection;
extern uint16_t vehicleRPM;
extern bool     vehicleEML;
extern bool     vehicleEPC;

extern int      boardSoftwareVersion;
extern uint32_t lastTransmission;
extern uint8_t  lastMode;
extern uint32_t haldexModeChangeMs;   // millis() of last local OpenHaldex mode change
extern int8_t   lastBlock;
extern int8_t   lastHaldex;
extern uint8_t  readBlock;
extern uint32_t calcSpeed;
extern uint32_t lastKlineTransmission;
extern uint32_t fisStatusHoldUntilMs;
extern uint16_t rpmLogoThreshold;
extern uint8_t  rpmLogoSelection;
extern volatile uint32_t rpmLogoTestUntilMs;
extern bool     extOutputEnabled;
extern uint8_t  extOutputMode;            // 0 = Manual, 1 = RPM threshold, 2 = Speed threshold
extern uint16_t extOutputRpmThreshold;   // RPM at/above which the output turns on (mode 1)
extern uint8_t  extOutputSpeedThreshold; // Speed at/above which the output turns on (mode 2)
extern uint8_t  klineDefaultModule;

extern bool     canTaskRunning;
extern bool     fisTaskRunning;
extern volatile bool ignitionRiseEvent;
extern volatile bool ignitionFallEvent;
extern volatile bool restartIgnitionRequested;
extern bool     serialMirrorToCard;
extern bool     oilSensorEnabled;
extern volatile bool fisStatusClearPending;

extern String   fisLine[8];

extern openhaldexState state;
extern boot_source_id  bootSource;
extern can_view_id     canViewMode;

// Oil sensor — ISR-shared (volatile) and foreground state
extern volatile uint32_t oilSensorLastPeriodUs;  // most recent rise-to-rise period, us
extern volatile uint32_t oilSensorLastPulseUs;   // most recent measured high-time, us
extern volatile uint32_t oilSensorLastGapUs;     // most recent low-time (period minus high-time)
extern volatile bool     oilSensorPulseReady;    // ISR sets, foreground clears
extern volatile uint32_t oilSensorLastEdgeMs;    // millis() of last edge seen

extern float    oilSensorTemperatureC;           // latest decoded temperature, NAN if stale
extern float    oilSensorLevelMm;                // latest decoded level, NAN if stale/uncal
extern uint32_t oilSensorFrameCount;             // good cycles decoded
extern uint32_t oilSensorErrorCount;             // cycles discarded (timing/sanity)
extern uint32_t oilSensorLastFrameMs;            // millis() of last good cycle

extern const char* webUiTitle;
extern const char* webUiWelcomeMessage;

// Boot greeting / special dates / clock (all persisted in NVS "settings").
extern bool     welcomeGreetingEnabled;   // mutually exclusive with the boot logo
extern bool     specialDatesEnabled;      // check the 5 special dates on boot
extern bool     useRTC;                    // use the DS1307 RTC if present
extern String   welcomeGreetName;          // name appended after "Good Morning/..." 
extern String   specialDateValue[5];       // "YYYY-MM-DD"; year <= 1970 = unset
extern String   specialDateText[5];        // custom multi-line message ('\n' separated)
