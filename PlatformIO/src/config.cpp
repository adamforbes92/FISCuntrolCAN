#include <Arduino.h>
#include "config.h"

#include <stdarg.h>
#include <stdio.h>

bool     ignitionState         = false;
bool     ignitionStateRunOnce  = false;
bool     hasFIS               = true;
bool     hasCAN               = true;
bool     hasK                 = false;
bool     hasHaldex            = true;
bool     fisDisable            = false;
bool     fisBeenToggled        = false;
bool     runOnce               = false;
bool     showHaldex            = false;
bool     isConnectedK          = false;
bool     isConnectedCAN        = false;
bool     hasOpenHaldex         = false;
bool     mimickSet             = false;
bool     triggerShutdown       = true;
bool     isStandalone          = false;
bool     fisBootReady          = false;
bool     fisFeedback           = false;
volatile uint32_t fisErrorEventCount = 0;

uint8_t  vehicleSpeed          = 0;
uint8_t  haldexVehicleSpeed    = 0;
uint8_t  haldexEngagement      = 0;
uint8_t  haldexState           = 0;
uint8_t  lockTarget            = 0;
uint8_t  pedValue              = 0;
uint8_t  bootScreenSelection   = defaultBootScreen;
uint16_t vehicleRPM            = 0;
bool     vehicleEML            = false;
bool     vehicleEPC            = false;

int      boardSoftwareVersion  = 0;
uint32_t lastTransmission      = 0;
uint8_t  lastMode              = 0;
uint32_t haldexModeChangeMs    = 0;   // millis() of last local mode change; suppresses CAN echo
int8_t   lastBlock             = -1;
int8_t   lastHaldex            = -1;
uint8_t  readBlock             = 1;
uint32_t calcSpeed             = 0;
uint32_t lastKlineTransmission = 0;
uint32_t fisStatusHoldUntilMs  = 0;
uint16_t rpmLogoThreshold      = 3000;
uint8_t  rpmLogoSelection      = 1;   // 1 = Danger preset, 2 = Custom uploaded
volatile uint32_t rpmLogoTestUntilMs = 0;   // non-zero millis() deadline: force shift indicator on display
bool     extOutputEnabled      = false;
uint8_t  extOutputMode         = 0;      // 0 = Manual, 1 = RPM threshold, 2 = Speed threshold
uint16_t extOutputRpmThreshold = 3000;
uint8_t  extOutputSpeedThreshold = 60;
uint8_t  klineDefaultModule    = K_Module;

bool     canTaskRunning        = false;
bool     fisTaskRunning        = false;
volatile bool ignitionRiseEvent = false;
volatile bool ignitionFallEvent = false;
volatile bool restartIgnitionRequested = false;
bool     serialMirrorToCard    = false;
bool     oilSensorEnabled      = hasOilSensor;
volatile bool fisStatusClearPending = false;

String   fisLine[8];

openhaldexState state = { MODE_STOCK, 0, false };
boot_source_id  bootSource = BOOT_SOURCE_CAN;
can_view_id     canViewMode = CAN_VIEW_STANDARD;

// Oil sensor — ISR-shared (volatile) and foreground state
volatile uint32_t oilSensorLastPeriodUs  = 0;
volatile uint32_t oilSensorLastPulseUs   = 0;
volatile uint32_t oilSensorLastGapUs     = 0;
volatile bool     oilSensorPulseReady    = false;
volatile uint32_t oilSensorLastEdgeMs    = 0;

float    oilSensorTemperatureC           = NAN;
float    oilSensorLevelMm                = NAN;
uint32_t oilSensorFrameCount             = 0;
uint32_t oilSensorErrorCount             = 0;
uint32_t oilSensorLastFrameMs            = 0;

const char* webUiTitle          = "FISCuntrolCAN";
const char* webUiWelcomeMessage = "Welcome to FISCuntrolCAN";

bool     welcomeGreetingEnabled = false;
bool     specialDatesEnabled    = false;
bool     useRTC                 = false;
String   welcomeGreetName       = "Adam";
String   specialDateValue[5]    = { "1970-01-01", "1970-01-01", "1970-01-01", "1970-01-01", "1970-01-01" };
String   specialDateText[5]     = { "", "", "", "", "" };
