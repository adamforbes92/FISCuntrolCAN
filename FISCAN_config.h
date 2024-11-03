#include <SPI.h>

#include <LiquidCrystal_I2C.h>
#include <TLBFISLib.h>

#include "KLineKWP1281Lib.h"
#include "FISCAN_configuration.h"
#include "FISCAN_communication.h"
#include <ESP32_CAN.h>  // for CAN

#include <ClickButton.h>
#include <BluetoothSerial.h>
#include <TickTwo.h>  // for repeated tasks
#include <map>

#define serialDebug 1         // if 1, will Serial print
#define ChassisCANDebug 0     // if 1, will print CAN 2 (Chassis) messages

#define hasLCD 1              // toggle for LCD display
#define hasFIS 0              // toggle for FIS display
#define lcdRow 20             // lcd size
#define lcdColumn 4           // lcd size

#define showBootScreen 1      // 0 = off, 1 = Welcome message, 2 = Custom Logo
#define checkLED 1            // 0 = off, 1 = do LED check

#define hasK 0                // use K-line for diag
#define hasCAN 0              // use CAN for diag
#define hasHaldex 1           // has OpenHaldex

#define logFrequency 1        // logs Per Second
#define connectionAttempts 3  // attempt to reconnect to ECU x times

// define pins
#define ignitionMonitorPin 35                                    // pins 2, 3, 18, 19, 20, 21 are interrupt friendly.  Consider changing...

#define K_TX 17                                                  // K-line TX (MC33290)
#define K_RX 16                                                  // K-line RX (MC33290)
#define pinCAN_RX 16                                             // pin output for SN65HVD230 (CAN_RX)
#define pinCAN_TX 17                                             // pin output for SN65HVD230 (CAN_TX)

#define fisCLK 18                                                // FIS Clock Output
#define fisDATA 23                                               // FIS Data Output
#define fisENA 19                                                // FIS Enable Output

#define stalkPushUp 36                                           // input stalk UP CANNOT USE
#define stalkPushDown 39                                         // input stalk DOWN
#define stalkPushReset 34                                        // input stalk RESET

#define stalkPushUpMonitor 25                                    // input stalk UP (to monitor when FIS Disabled)
#define stalkPushDownMonitor 26                                  // input stalk DOWN (to monitor when FIS Disabled)
#define stalkPushResetMonitor 27                                 // input stalk RESET (to monitor when FIS Disabled)
#define stalkPushUpReturn stalkPushUpMonitor                     // if FIS disable - use this to match stalk UP
#define stalkPushDownReturn stalkPushDownMonitor                 // if FIS disable - use this to match stalk DOWN
#define stalkPushResetReturn stalkPushResetMonitor               // if FIS disable - use this to match stalk RESET

#define SPI_INSTANCE SPI                                         // SPI interface for FIS
#define ENA_PIN fisENA                                           // FIS enable pin (needed for library, use the same as above)

#define deviceName "FISCuntrol"                                  // for ESP32 Bluetooth name - visible on other devices
#define btRefresh 250                                            // BT Send Data Refresh in ms
#define btDiscoverTime 5000                                      // delay time for finding other devices

#define arraySize(array) (sizeof((array)) / sizeof((array)[0]))  // generic array size calculator, handy to have
#define serialPacketEnd 0xFF                                     // define Bluetooth Serial Packet end

#define MOTOR1_ID 0x280
#define MOTOR2_ID 0x288
#define MOTOR3_ID 0x380
#define MOTOR5_ID 0x480
#define MOTOR6_ID 0x488
#define MOTOR7_ID 0x588

#define MOTOR_FLEX_ID 0x580
#define GRA_ID 0x38A

#define BRAKES1_ID 0x1A0
#define BRAKES2_ID 0x2A0
#define BRAKES3_ID 0x4A0
#define BRAKES5_ID 0x5A0

#define HALDEX_ID 0x2C0

// Haldex mode types
typedef enum openhaldex_mode_id {
  MODE_STOCK,
  MODE_FWD,
  MODE_5050,
  MODE_CUSTOM
} openhaldex_mode_id;

typedef struct openhaldexState {
  openhaldex_mode_id mode;
  byte ped_threshold;
  bool mode_override;
} openhaldexState;

esp_spp_sec_t sec_mask = ESP_SPP_SEC_NONE;  // or ESP_SPP_SEC_ENCRYPT|ESP_SPP_SEC_AUTHENTICATE to request pincode confirmation
esp_spp_role_t role = ESP_SPP_ROLE_SLAVE;   // or ESP_SPP_ROLE_MASTER
char* p;

extern bool ignitionState = false;         // variable for reading the ignition pin status
extern bool ignitionStateRunOnce = false;  // variable for reading the first run loop
extern bool fisDisable = false;            // is the FIS turned off?
extern bool fisBeenToggled = false;        // been toggled on/off?  Don't display welcome if!
extern bool btConnected = false;
extern bool runOnce = false;
extern bool showHaldex = false;

extern byte vehicleSpeed = 0;
extern byte haldexEngagement = 0;
extern byte haldexState = 0;
extern float lockTarget = 0;
extern float pedValue = 0;
extern int boardSoftwareVersion = 0;
extern uint32_t lastTransmission = 0;

extern int vehicleRPM;         // current RPM
extern bool vehicleEML;        // current EML light status
extern bool vehicleEPC;        // current EPC light status
extern int calcSpeed;          // temp var for calculating speed

byte btIncoming[10];
byte btOutgoing[4];
int incomingLen;

extern String fisLine1 = "";
extern String fisLine2 = "";
extern String fisLine3 = "";
extern String fisLine4 = "";
extern String fisLine5 = "";
extern String fisLine6 = "";
extern String fisLine7 = "";
extern String fisLine8 = "";

extern uint8_t readBlock = 1;
