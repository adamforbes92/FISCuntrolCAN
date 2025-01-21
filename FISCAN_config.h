/* for defining all used variables, libraries, etc.
*/
#include <SPI.h>        // for FIS display
#include <TLBFISLib.h>  // for FIS display

#include <KLineKWP1281Lib.h>  // for k-line diag (external to this proj.)

#include <ESP32_CAN.h>  // for CAN

#include "OneButton.h"  // for monitoring the stalk buttons - it's easier to use a lib. than parse each loop - and it counts hold presses
#include <TickTwo.h>  // for repeated tasks

#define serialDebug 1                                            // if 1, will Serial print
#define serialBaud 115200                                        // define Serial talkback baud rate
#define ChassisCANDebug 0                                        // if 1, will print CAN 1 (Chassis) messages
#define checkLED 0                                               // 0 = off, 1 = do LED check (for debug ONLY, disable on release)

#define hasFIS 1                                                 // toggle for FIS display
#define fisWakeDelay 50                                          // delay to let FIS cluster boot, if data sent immediately it doesn't boot(!)
#define globalTextAlignment TLBFISLib::CENTER                    // TLBFISLib::LEFT / CENTER / RIGHT - note spelling(!)
#define showBootScreen 2                                         // 0 = off, 1 = Welcome message, 2 = Custom Logo
#define bootScreenDuration 4000                                  // boot logo duration
#define connectionDelayDuration 0                                // 'connecting...' information duration
#define displayECUonBoot 0                                       // display ECU Part Number etc when connected

#define hasK 1                                                   // use K-line for diag
#define hasCAN 0                                                 // use CAN for diag - needs a lot of work!  What variables do we want to see?
#define hasHaldex 1                                              // has OpenHaldex
#define hasRTC 0                                                 // has RTC for time control.  Removed to save space - incorporate ESP RTC / WiFi get time lastminuteengineers.com/esp32-ntp-server-date-time-tutorial/

#define logFrequency 250                                         // logs Per Second

#define ignitionMonitorPin 35                                    // for monitoring ignition signal via. optocoupler
#define K_TX 17                                                  // TX pin for K-line (MC33290)
#define K_RX 16                                                  // RX pin for K-line (MC33290)
#define K_line Serial2                                           // use Serial2 as the K-line port
#define K_Baud 10400                                             // define module baud rate (ME7.x = 10400)
#define K_Module 0x01                                            // define address connection.  Could be adjusted to connect to other modules, but who cares?!

#define pinCAN_RX 14                                             // RX pin for SN65HVD230 (CAN_RX)
#define pinCAN_TX 13                                             // TX pin for SN65HVD230 (CAN_TX)

#define fisCLK 18                                                // FIS Clock Output - these are default CLK for ESP32
#define fisDATA 23                                               // FIS Data Output - these are default MOSI for ESP32
#define fisENA 19                                                // FIS Enable Output - required, but set to 19 (chip select) for ESP32
#define screenSize TLBFISLib::FULLSCREEN                         // use the full screen for the FIS
//#define screenSize TLBFISLib::HALFSCREEN                         // use the half screen (bottom half) for the FIS kept for completeness
#define SPI_INSTANCE SPI                                         // SPI interface for FIS

#define stalkPushUp 36                                           // input stalk UP
#define stalkPushDown 39                                         // input stalk DOWN
#define stalkPushReset 34                                        // input stalk RESET
#define stalkPushUpReturn 25                                     // if FIS disable - use this to match stalk UP
#define stalkPushDownReturn 26                                   // if FIS disable - use this to match stalk DOWN
#define stalkPushResetReturn 27                                  // if FIS disable - use this to match stalk RESET

#define deviceName "FISCuntrol"                                  // for ESP32 Bluetooth name - so that it's visible on other devices
#define fisCuntrol_ID 0x7B0                                      // FIS CAN Address
#define appMessageStatus 1                                       // OpenHaldex Message Status
#define openHaldex_ID 0x7C0                                      // OpenHaldex CAN Address
#define canRefresh 50                                            // send CAN updates every xx ms

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

extern char* connectingToK = "Connecting...";
extern char* connectedToK = "Connected...";

extern char* connectingToCAN = "Connecting...";
extern char* connectedToCAN = "Connected...";

extern char* haldexOptions[] = { "OpenHaldex", "Stock", "FWD", "5050" };
extern char* connectingToOpenHaldex = "Connecting...";
extern char* connectedToOpenHaldex = "Connected...";

extern bool ignitionState = false;         // variable for reading the ignition pin status
extern bool ignitionStateRunOnce = false;  // variable for reading the first run loop
extern bool fisDisable = false;            // is the FIS turned off?
extern bool fisBeenToggled = false;        // been toggled on/off?  Don't display welcome if!
extern bool btConnected = false;           // is the Bluetooth connected?
extern bool runOnce = false;               // for launching boot LCD/FIS
extern bool showHaldex = false;            // for displaying Haldex options instead of data
extern bool isConnectedK = false;          // for monitoring k-line connection
extern bool isConnectedCAN = false;        // for monitoring CAN connection
extern bool hasOpenHaldex = false;         // visbility of OpenHaldex via. CAN
extern bool mimickSet = false;             // can't change FIS screen etc in interrupts, so set a flag to catch in the next loop

extern byte vehicleSpeed = 0;
extern byte haldexEngagement = 0;
extern byte haldexState = 0;
extern float lockTarget = 0;
extern float pedValue = 0;
extern int boardSoftwareVersion = 0;
extern uint32_t lastTransmission = 0;
extern int lastMode = 0;
extern int lastBlock = -1;
extern int lastHaldex = -1;
extern unsigned long lastDataRetrieve = 0;  // for checking if it's time to get more data...

extern int vehicleRPM = 0;       // current RPM
extern bool vehicleEML = false;  // current EML light status
extern bool vehicleEPC = false;  // current EPC light status
extern int calcSpeed = 0;        // temp var for calculating speed

byte btIncoming[10];
byte btOutgoing[4];
int incomingLen;

extern String fisLine[8] = { "" };

// Enable/disable printing library debug information on the Serial Monitor.
#define debug_info false  // You may change the debug level in "KLineKWP1281Lib.h".
// Select whether or not your serial interface can receive at the same time as sending (or whether or not your K-line interface has echo as it should).
// Most software serial libraries for AVR microcontrollers are half-duplex (can't receive while sending), so if using such library this should be false.
#define is_full_duplex true

// Initialize the serial port
void beginFunction(unsigned long baud) {
  K_line.begin(K_Baud, SERIAL_8N1, K_RX, K_TX);
}

// Stop communication on the serial port
void endFunction() {
  K_line.end();
}

// Send a byte
void sendFunction(uint8_t data) {
  K_line.write(data);
}

// Receive a byte
bool receiveFunction(uint8_t* data) {
  if (K_line.available()) {
    *data = K_line.read();
    return true;
  }
  return false;
}

// GRAPHICS 
extern const unsigned char avant[] PROGMEM = {
  0x07, 0xfe, 0x00, 0x3f, 0xff, 0xc0, 0x78, 0x01, 0xe0, 0xc8, 0x01, 0x30, 0xb4, 0x02, 0xd0, 0xc4, 0x02, 0x30, 0x84, 0x02, 0x10, 0x84, 0x02, 0x10, 0x84, 0x02, 0x10, 0x84, 0x02, 0x10, 0x84, 0x02, 0x10, 0x9f, 0xff, 0x90, 0xb0, 0x00, 0xd0, 0xe0, 0x00, 0x70, 0x80, 0x00, 0x10, 0x80, 0x00, 0x10, 0x80, 0x00, 0x10, 0xe0, 0x00, 0x70, 0xb7, 0xfe, 0xd0, 0x98, 0x01, 0x90, 0x90, 0x00, 0x90, 0x90, 0x00, 0x90, 0x90, 0x00, 0x90, 0x90, 0x00, 0x90, 0x90, 0x00, 0x90, 0xf0, 0x00, 0xf0, 0x90, 0x00, 0x90, 0x90, 0x00, 0x90, 0x90, 0x00, 0x90, 0x90, 0x00, 0x90, 0x90, 0x00, 0x90, 0x90, 0x00, 0x90, 0x90, 0x00, 0x90, 0xb0, 0x00, 0xd0, 0xd0, 0x00, 0xb0, 0x90, 0x00, 0x90, 0x90, 0x00, 0x90, 0x90, 0x00, 0x90, 0x90, 0x00, 0x90, 0x98, 0x01, 0x90, 0x97, 0xfe, 0x90, 0xa0, 0x00, 0x50, 0xe0, 0x00, 0x70, 0x90, 0x00, 0x90, 0x7f, 0xff, 0xe0, 0x1f, 0xff, 0x80
};

extern const unsigned char sedan[] PROGMEM = {
  0x07, 0xfe, 0x00, 0x3f, 0xff, 0xc0, 0x78, 0x01, 0xe0, 0xc8, 0x01, 0x30, 0xb4, 0x02, 0xd0, 0xc4, 0x02, 0x30, 0x84, 0x02, 0x10, 0x84, 0x02, 0x10, 0x84, 0x02, 0x10, 0x84, 0x02, 0x10, 0x84, 0x02, 0x10, 0x9f, 0xff, 0x90, 0xb0, 0x00, 0xd0, 0xe0, 0x00, 0x70, 0x80, 0x00, 0x10, 0x80, 0x00, 0x10, 0x80, 0x00, 0x10, 0xe0, 0x00, 0x70, 0xb7, 0xfe, 0xd0, 0x98, 0x01, 0x90, 0x90, 0x00, 0x90, 0x90, 0x00, 0x90, 0x90, 0x00, 0x90, 0x90, 0x00, 0x90, 0x90, 0x00, 0x90, 0xf0, 0x00, 0xf0, 0x90, 0x00, 0x90, 0x90, 0x00, 0x90, 0x90, 0x00, 0x90, 0x90, 0x00, 0x90, 0x90, 0x00, 0x90, 0x90, 0x00, 0x90, 0x90, 0x00, 0x90, 0xb8, 0x01, 0xd0, 0xe7, 0xfe, 0x70, 0x80, 0x00, 0x10, 0xc0, 0x00, 0x30, 0xa0, 0x00, 0x50, 0x90, 0x00, 0x90, 0x8c, 0x03, 0x10, 0x83, 0xfc, 0x10, 0xc0, 0x00, 0x30, 0x60, 0x00, 0x60, 0x50, 0x00, 0xa0, 0x3f, 0xff, 0xc0, 0x07, 0xfe, 0x00
};

extern const unsigned char left_door[] PROGMEM = {
  0x02, 0x07, 0x0c, 0x18, 0x30, 0x63, 0xc5, 0x79
};

extern const unsigned char right_door[] PROGMEM = {
  0xc0, 0xe0, 0x30, 0x18, 0x0c, 0xc6, 0xa3, 0x9e
};

extern const unsigned char avant_trunc[] PROGMEM = {
  0x40, 0x08, 0xff, 0xfc, 0xff, 0xfc, 0x7f, 0xf8
};

extern const unsigned char sedan_trunc[] PROGMEM = {
  0x80, 0x00, 0x40, 0xc0, 0x00, 0xc0, 0xe0, 0x01, 0xc0, 0xf8, 0x07, 0xc0, 0x7f, 0xff, 0x80, 0x3f, 0xff, 0x00, 0x1f, 0xfe, 0x00
};

extern const unsigned char b5f[] PROGMEM = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x80, 0x1f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x80, 0x1f, 0xff, 0xf0, 0x00, 0x1f, 0xff, 0xf1, 0x80, 0x1f, 0xff, 0xc0, 0x00, 0x07, 0xff, 0x01, 0x80, 0x1f, 0xff, 0x80, 0x00, 0x03, 0xf0, 0x01, 0x80, 0x1c, 0x0f, 0x80, 0x00, 0x01, 0xe0, 0x01, 0x80, 0x1c, 0x0f, 0x00, 0x00, 0x00, 0xe0, 0x01, 0x80, 0x1c, 0x0f, 0x00, 0x00, 0x00, 0xe0, 0x07, 0x80, 0x1c, 0x0e, 0x00, 0x00, 0x00, 0xe0, 0xc7, 0x80, 0x1c, 0x0e, 0x00, 0x00, 0x00, 0x60, 0x03, 0x80, 0x1c, 0x0e, 0x00, 0x00, 0x00, 0x60, 0x01, 0x80, 0x1c, 0x0e, 0x00, 0x00, 0x00, 0x60, 0x01, 0x80, 0x1c, 0x0e, 0x00, 0x00, 0x00, 0x78, 0x01, 0x80, 0x1c, 0x0e, 0x00, 0x00, 0x00, 0x7f, 0x81, 0x80, 0x1c, 0x0e, 0x00, 0x00, 0x00, 0x3f, 0xff, 0x80, 0x1c, 0x0e, 0x00, 0x00, 0x00, 0x20, 0x03, 0x80, 0x1c, 0x0e, 0x00, 0x00, 0x00, 0x20, 0x01, 0x80, 0x1c, 0x0e, 0x03, 0xff, 0xe0, 0x20, 0x01, 0x80, 0x1c, 0x0e, 0x07, 0xff, 0xf0, 0x20, 0x01, 0x80, 0x1c, 0x0e, 0x07, 0xff, 0xf0, 0x20, 0x01, 0x80, 0x1c, 0x0f, 0x07, 0xff, 0xf0, 0x3f, 0xff, 0x80, 0x1c, 0x0f, 0x07, 0xff, 0xe0, 0x3f, 0xf3, 0x80, 0x1c, 0x0f, 0x81, 0xf0, 0x00, 0x3f, 0xf1, 0x80, 0x1c, 0x00, 0x18, 0xe0, 0x00, 0x3f, 0xf1, 0x80, 0x1f, 0xe0, 0xd6, 0xe0, 0x00, 0x60, 0x01, 0x80, 0x1f, 0xfc, 0xf8, 0xe0, 0x00, 0x60, 0x01, 0x80, 0x1f, 0xfe, 0x78, 0xe0, 0x00, 0x60, 0x01, 0x80, 0x1f, 0xff, 0x5e, 0xe0, 0x00, 0x60, 0x01, 0x80, 0x1d, 0xff, 0x31, 0xe0, 0x00, 0x60, 0x01, 0x80, 0x1c, 0xff, 0x83, 0xe0, 0x00, 0x7f, 0xff, 0x80, 0x1c, 0x7f, 0xfe, 0xe0, 0x00, 0xff, 0xff, 0x80, 0x1c, 0x1f, 0xff, 0xe0, 0x00, 0xe0, 0x03, 0x80, 0x1c, 0x00, 0xff, 0xe0, 0x01, 0xe0, 0x01, 0x80, 0x1c, 0x00, 0xff, 0xe0, 0x01, 0xe0, 0x01, 0x80, 0x1c, 0x00, 0xff, 0xf0, 0x03, 0xe0, 0x01, 0x80, 0x1c, 0x00, 0x7f, 0xf8, 0x07, 0xe0, 0x01, 0x80, 0x1c, 0x00, 0x77, 0xfe, 0x0f, 0xff, 0xff, 0x80, 0x1f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x80, 0x1f, 0xff, 0xff, 0xff, 0xdf, 0xe0, 0x01, 0x80, 0x1f, 0xff, 0xff, 0xff, 0xc3, 0xe0, 0x01, 0x80, 0x1f, 0xe3, 0x8f, 0x1f, 0xe1, 0xe0, 0x01, 0x80, 0x1f, 0xdf, 0x86, 0x0f, 0xe1, 0xe0, 0x01, 0x80, 0x1f, 0x8a, 0x66, 0x03, 0xe0, 0xe0, 0x03, 0x80, 0x1f, 0x07, 0x06, 0x3f, 0xf1, 0xe0, 0x07, 0x80, 0x1e, 0x00, 0x00, 0x7f, 0xff, 0xf8, 0x01, 0x80, 0x1e, 0x00, 0x00, 0x27, 0xff, 0xff, 0x41, 0x80, 0x1e, 0x00, 0x03, 0x21, 0xfc, 0x75, 0x41, 0x80, 0x1c, 0x00, 0x00, 0xf0, 0x78, 0x65, 0xb3, 0x80, 0x1c, 0x00, 0x01, 0xf0, 0x38, 0x67, 0xff, 0x80, 0x1c, 0x00, 0x0c, 0x58, 0x18, 0x63, 0xc1, 0x80, 0x1c, 0x00, 0x00, 0x48, 0x18, 0x60, 0x31, 0x80, 0x1c, 0x00, 0x00, 0xc0, 0x1c, 0x61, 0x01, 0x80, 0x1c, 0x00, 0x00, 0x00, 0x0c, 0x61, 0x01, 0x80, 0x1c, 0x00, 0x00, 0x00, 0x0c, 0x7f, 0xff, 0x80, 0x1c, 0x07, 0xc0, 0x3f, 0x8c, 0x7f, 0xf1, 0x80, 0x1c, 0x07, 0xe0, 0x3f, 0xcc, 0x7f, 0x01, 0x80, 0x1c, 0x0f, 0xe0, 0x3f, 0xcc, 0x70, 0x01, 0x80, 0x1c, 0x0f, 0xe0, 0x7f, 0xec, 0x60, 0x01, 0x80, 0x1c, 0x0f, 0xe0, 0x7f, 0xcc, 0x60, 0x01, 0x80, 0x1c, 0x00, 0x00, 0x00, 0x0c, 0x60, 0x07, 0x80, 0x1c, 0x00, 0x00, 0x00, 0x08, 0x60, 0xc7, 0x80, 0x1c, 0x00, 0x00, 0x00, 0x08, 0x60, 0x03, 0x80, 0x1c, 0x00, 0x00, 0x00, 0x08, 0x60, 0x01, 0x80, 0x1c, 0x00, 0x00, 0x00, 0x0c, 0x60, 0x01, 0x80, 0x1c, 0x00, 0x00, 0x00, 0x04, 0x78, 0x01, 0x80, 0x1c, 0x00, 0x00, 0x00, 0x06, 0x7f, 0x81, 0x80, 0x1c, 0x00, 0x00, 0x00, 0x03, 0x62, 0x3f, 0x80, 0x1c, 0x00, 0x00, 0x00, 0x01, 0xe2, 0x3f, 0x80, 0x1c, 0x00, 0x00, 0x00, 0x00, 0xe2, 0x3f, 0x80, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x60, 0x03, 0x80, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x70, 0x01, 0x80, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x78, 0x01, 0x80, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x68, 0x01, 0x80, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x6c, 0x01, 0x80, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x6c, 0x03, 0x80, 0x1f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

extern const unsigned char Q[] PROGMEM = {
  0x00, 0x00, 0x00, 0x7f, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0xff, 0xff, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x7f, 0xff, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x07, 0xff, 0xff, 0xff, 0xff, 0xc0, 0x00, 0x00, 0x0f, 0xff, 0xff, 0xff, 0xff, 0xe0, 0x00, 0x00, 0x3f, 0xff, 0xff, 0xff, 0xff, 0xf0, 0x00, 0x00, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xf8, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x03, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x80, 0x07, 0xff, 0xff, 0xfc, 0x3f, 0xff, 0xff, 0xc0, 0x07, 0xff, 0xff, 0xc0, 0x00, 0xff, 0xff, 0xe0, 0x0f, 0xff, 0xfe, 0x00, 0x00, 0x3f, 0xff, 0xf0, 0x1f, 0xff, 0xf8, 0x00, 0x00, 0x1f, 0xff, 0xf0, 0x1f, 0xff, 0xf0, 0x00, 0x00, 0x0f, 0xff, 0xf0, 0x3f, 0xff, 0xe0, 0x00, 0x00, 0x07, 0xff, 0xf8, 0x3f, 0xff, 0xc0, 0x00, 0x00, 0x03, 0xff, 0xf8, 0x3f, 0xff, 0x80, 0x00, 0x00, 0x01, 0xff, 0xfc, 0x7f, 0xff, 0x00, 0x00, 0x00, 0x00, 0xff, 0xfc, 0x7f, 0xff, 0x00, 0x00, 0x00, 0x00, 0xff, 0xfc, 0x7f, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x7f, 0xfe, 0xff, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x7f, 0xfe, 0xff, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xfe, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xfe, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xff, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x1f, 0xff, 0xff, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x1f, 0xff, 0xff, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x1f, 0xff, 0xff, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x1f, 0xff, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x1f, 0xff, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x1f, 0xff, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xfe, 0xff, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xfe, 0x7f, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x7f, 0xfe, 0x7f, 0xff, 0x00, 0x00, 0x00, 0x00, 0x7f, 0xfe, 0x7f, 0xff, 0x00, 0x00, 0x00, 0x00, 0xff, 0xfc, 0x3f, 0xff, 0x00, 0x00, 0x00, 0x00, 0xff, 0xfc, 0x3f, 0xff, 0x80, 0x00, 0x00, 0x01, 0xff, 0xfc, 0x3f, 0xff, 0xc0, 0x00, 0x00, 0x03, 0xff, 0xf8, 0x1f, 0xff, 0xe0, 0x00, 0x00, 0x07, 0xff, 0xf8, 0x1f, 0xff, 0xf0, 0x00, 0x00, 0x0f, 0xff, 0xf8, 0x0f, 0xff, 0xf8, 0x00, 0x00, 0x1f, 0xff, 0xf0, 0x07, 0xff, 0xfe, 0x00, 0x00, 0x7f, 0xff, 0xf0, 0x07, 0xff, 0xff, 0x80, 0x00, 0xff, 0xff, 0xe0, 0x03, 0xff, 0xff, 0xe0, 0x03, 0xff, 0xff, 0xc0, 0x01, 0xff, 0xff, 0xfe, 0x0f, 0xff, 0xff, 0xc0, 0x00, 0xff, 0xff, 0xff, 0xc3, 0xff, 0xff, 0x80, 0x00, 0x7f, 0xff, 0xff, 0xe0, 0x7f, 0xff, 0x00, 0x00, 0x3f, 0xff, 0xff, 0xfe, 0x07, 0xfe, 0x00, 0x00, 0x1f, 0xff, 0xff, 0xff, 0xc0, 0x7c, 0x00, 0x00, 0x07, 0xff, 0xff, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x03, 0xff, 0xff, 0xff, 0xff, 0xc0, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x0f, 0xff, 0xff, 0xff, 0xff, 0xf8, 0x00, 0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x00, 0x00, 0x00, 0x1f, 0xff, 0xff, 0xff, 0xfe, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0xff, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xe0
};

extern const unsigned char QBSW[] PROGMEM = {
  0x07, 0xe0, 0x1f, 0xc0, 0x3e, 0x38, 0x0c, 0x07, 0x1f, 0xf8, 0x1f, 0xf0, 0x7f, 0xbc, 0x1e, 0x0f,
  0x3f, 0xfc, 0x1f, 0xf8, 0xff, 0x9c, 0x1e, 0x0e, 0x7c, 0x1e, 0x1c, 0x3d, 0xe1, 0x1c, 0x3e, 0x1e,
  0x78, 0x0f, 0x1c, 0x1d, 0xc0, 0x1c, 0x3e, 0x1c, 0xf0, 0x07, 0x1c, 0x3d, 0xe0, 0x1c, 0x7e, 0x3c,
  0xe0, 0x07, 0x9c, 0x78, 0xf8, 0x1c, 0x76, 0x38, 0xe0, 0x07, 0x9f, 0xf0, 0xfe, 0x1c, 0xe7, 0x78,
  0xe0, 0x07, 0x9f, 0xf8, 0x3f, 0x9c, 0xe7, 0x70, 0xe0, 0x07, 0x1c, 0x3c, 0x07, 0x9d, 0xc7, 0x70,
  0xf0, 0x07, 0x1c, 0x1c, 0x03, 0xdf, 0x87, 0xe0, 0x78, 0x0f, 0x1c, 0x1c, 0x01, 0xdf, 0x87, 0xe0,
  0x7c, 0x3e, 0x1c, 0x3c, 0x83, 0xcf, 0x07, 0xc0, 0x3f, 0xff, 0xdf, 0xfd, 0xff, 0x8f, 0x07, 0xc0,
  0x0f, 0xff, 0xdf, 0xf9, 0xff, 0x0e, 0x03, 0x80, 0x01, 0xff, 0x9f, 0xc0, 0x7e, 0x0e, 0x03, 0x80
};

extern const unsigned char MK4Golf[] PROGMEM = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x7f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0xc0, 0xc0, 0x00, 0x03, 0x00, 0x00, 0x00, 0x00, 0x9e, 0x40, 0x00, 0x02, 0x80, 0x18, 0x00,
  0x00, 0x91, 0x20, 0x00, 0x1f, 0xff, 0xd4, 0x00, 0x00, 0x91, 0x20, 0x00, 0x19, 0xff, 0xff, 0x00,
  0x00, 0x89, 0x20, 0x00, 0x18, 0x00, 0x03, 0x00, 0x00, 0x9f, 0x20, 0x00, 0x18, 0x00, 0x01, 0x00,
  0x00, 0x10, 0x60, 0x00, 0x18, 0x00, 0x01, 0x00, 0x00, 0x1f, 0xc0, 0x00, 0x18, 0x00, 0x01, 0x00,
  0x00, 0x07, 0x80, 0x00, 0x18, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x7f, 0xf8, 0x00, 0x01, 0x00,
  0x00, 0x10, 0x01, 0xbf, 0xf8, 0x00, 0x01, 0x00, 0x00, 0x39, 0x07, 0xbf, 0xf8, 0x00, 0x03, 0x00,
  0x00, 0x25, 0x84, 0xf0, 0x3f, 0x00, 0x0f, 0x00, 0x00, 0x25, 0x84, 0xc0, 0x19, 0x80, 0x07, 0x00,
  0x00, 0x3f, 0x07, 0x80, 0x0f, 0xe0, 0x27, 0x00, 0x00, 0x3e, 0x03, 0x80, 0x07, 0xf0, 0x1f, 0x00,
  0x00, 0x00, 0x03, 0x80, 0x07, 0xff, 0x3e, 0x00, 0x00, 0x1e, 0x03, 0x00, 0x07, 0xff, 0xfe, 0x00,
  0x00, 0x3f, 0x03, 0x00, 0x03, 0xff, 0xfe, 0x00, 0x00, 0x21, 0x83, 0x80, 0x07, 0xff, 0xfe, 0x00,
  0x00, 0x21, 0x83, 0x80, 0x07, 0xff, 0xfe, 0x00, 0x00, 0x31, 0x07, 0x80, 0x07, 0xff, 0xbc, 0x00,
  0x00, 0x3f, 0xe4, 0xc0, 0x0b, 0xe1, 0xfc, 0x00, 0x00, 0x00, 0x04, 0xe0, 0x19, 0x80, 0x74, 0x00,
  0x00, 0x00, 0x07, 0xf8, 0x7f, 0x00, 0x34, 0x00, 0x00, 0x39, 0x03, 0xff, 0xfe, 0x00, 0x3c, 0x00,
  0x00, 0x2d, 0x83, 0xff, 0xfe, 0x00, 0x18, 0x00, 0x00, 0x24, 0x83, 0xff, 0xfe, 0x00, 0x18, 0x00,
  0x00, 0x2d, 0x83, 0xff, 0xfe, 0x00, 0x18, 0x00, 0x00, 0x3f, 0x03, 0xff, 0xfe, 0x00, 0x18, 0x00,
  0x00, 0x00, 0x03, 0xff, 0xfe, 0x00, 0x18, 0x00, 0x00, 0x00, 0x07, 0xff, 0xfe, 0x00, 0x18, 0x00,
  0x00, 0x3f, 0x05, 0xf8, 0x7f, 0x00, 0x3c, 0x00, 0x00, 0x03, 0x04, 0xe0, 0x19, 0x80, 0x74, 0x00,
  0x00, 0x01, 0x85, 0xc0, 0x09, 0xc0, 0xf4, 0x00, 0x00, 0x01, 0x87, 0x80, 0x07, 0xff, 0xfc, 0x00,
  0x00, 0x3f, 0x03, 0x80, 0x07, 0xff, 0xf8, 0x00, 0x00, 0x03, 0x03, 0x00, 0x07, 0xff, 0xf8, 0x00,
  0x00, 0x01, 0x83, 0x00, 0x03, 0xff, 0xf8, 0x00, 0x00, 0x01, 0x83, 0x00, 0x03, 0xff, 0xf8, 0x00,
  0x00, 0x3f, 0x03, 0x80, 0x07, 0xff, 0xf8, 0x00, 0x00, 0x00, 0x03, 0x80, 0x07, 0xff, 0xfc, 0x00,
  0x00, 0x00, 0x07, 0x80, 0x07, 0xf3, 0xfc, 0x00, 0x01, 0x00, 0x04, 0xc0, 0x09, 0x80, 0x74, 0x00,
  0x01, 0x00, 0x04, 0xe0, 0x19, 0x00, 0x34, 0x00, 0x01, 0x00, 0x07, 0xfd, 0xff, 0x00, 0x3c, 0x00,
  0x01, 0x00, 0x03, 0xff, 0xfe, 0x00, 0x18, 0x00, 0x01, 0x00, 0x03, 0xff, 0xfe, 0x00, 0x18, 0x00,
  0x01, 0x00, 0x03, 0xff, 0xfe, 0x00, 0x18, 0x00, 0x00, 0x00, 0x03, 0xff, 0xfe, 0x00, 0x18, 0x00,
  0x01, 0xff, 0x03, 0xff, 0xfe, 0x00, 0x18, 0x00, 0x01, 0x3d, 0x83, 0xff, 0xfe, 0x00, 0x1c, 0x00,
  0x01, 0x28, 0x83, 0xff, 0xfe, 0x00, 0x3c, 0x00, 0x01, 0x2d, 0x05, 0xf0, 0x39, 0x00, 0x70, 0x00,
  0x01, 0xe7, 0x04, 0xe0, 0x19, 0x80, 0xf4, 0x00, 0x00, 0x40, 0x07, 0xc0, 0x0f, 0xe3, 0xfc, 0x00,
  0x00, 0x08, 0x03, 0x80, 0x07, 0xff, 0xf8, 0x00, 0x00, 0x0e, 0x03, 0x80, 0x07, 0xff, 0xf8, 0x00,
  0x00, 0x09, 0x83, 0x00, 0x03, 0xff, 0x78, 0x00, 0x00, 0x08, 0xc3, 0x00, 0x03, 0xff, 0x78, 0x00,
  0x00, 0x3f, 0xe3, 0x00, 0x03, 0xff, 0xf8, 0x00, 0x00, 0x18, 0x03, 0x80, 0x07, 0xff, 0xf8, 0x00,
  0x00, 0x00, 0x03, 0x80, 0x07, 0xff, 0xfc, 0x00, 0x00, 0x01, 0x07, 0xc0, 0x0d, 0xc0, 0xf0, 0x00,
  0x00, 0x3f, 0xc0, 0xc0, 0x19, 0x80, 0x34, 0x00, 0x00, 0x3f, 0xe4, 0xf0, 0x3f, 0x00, 0x3c, 0x00,
  0x00, 0x01, 0x13, 0xff, 0xfe, 0x00, 0x18, 0x00, 0x00, 0x00, 0x03, 0x9f, 0xfe, 0x00, 0x18, 0x00,
  0x00, 0x3f, 0xe1, 0xff, 0xfe, 0x00, 0x18, 0x00, 0x00, 0x3f, 0xe0, 0xff, 0xce, 0x00, 0x18, 0x00,
  0x00, 0x0e, 0x00, 0x7f, 0xde, 0x00, 0x18, 0x00, 0x00, 0x1b, 0x00, 0x3f, 0xfe, 0x00, 0x18, 0x00,
  0x00, 0x31, 0x00, 0x3f, 0xfe, 0x00, 0x3c, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xf9, 0x00, 0x74, 0x00,
  0x00, 0x00, 0x00, 0x30, 0x19, 0x80, 0xfc, 0x00, 0x00, 0x3f, 0x00, 0x18, 0x1f, 0xf7, 0x98, 0x00,
  0x00, 0x20, 0x00, 0x0c, 0x1f, 0x71, 0xd0, 0x00, 0x00, 0x20, 0x00, 0x07, 0x1f, 0x61, 0xe0, 0x00,
  0x00, 0x20, 0x00, 0x01, 0xdf, 0xfe, 0x00, 0x00, 0x00, 0x3f, 0x00, 0x00, 0xff, 0x80, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

extern const unsigned char logo[] PROGMEM = {
  0xff, 0xff, 0xc0, 0x7e, 0x00, 0x03, 0xff, 0xf0, 0xff, 0xff, 0xc0, 0x7e, 0x00, 0x03, 0xff, 0xf8,
  0xff, 0xff, 0xc0, 0x7e, 0x00, 0x03, 0xff, 0xfc, 0xff, 0xff, 0xc0, 0x7e, 0x00, 0x03, 0xff, 0xfc,
  0xff, 0xff, 0xc0, 0x7e, 0x00, 0x03, 0xff, 0xfc, 0xff, 0xff, 0xc0, 0x7e, 0x00, 0x03, 0xff, 0xfc,
  0x03, 0xf0, 0x00, 0x7e, 0x00, 0x03, 0xf0, 0x3c, 0x03, 0xf0, 0x00, 0x7e, 0x00, 0x03, 0xf0, 0x3c,
  0x03, 0xf0, 0x00, 0x7e, 0x00, 0x03, 0xff, 0xfc, 0x03, 0xf0, 0x00, 0x7e, 0x00, 0x03, 0xff, 0xf8,
  0x03, 0xf0, 0x00, 0x7e, 0x00, 0x03, 0xff, 0xf0, 0x03, 0xf0, 0x00, 0x7e, 0x00, 0x03, 0xff, 0xfc,
  0x03, 0xf0, 0x00, 0x7e, 0x00, 0x03, 0xff, 0xfe, 0x03, 0xf0, 0x00, 0x7e, 0x00, 0x03, 0xff, 0xff,
  0x03, 0xf0, 0x00, 0x7e, 0x00, 0x03, 0xf0, 0x3f, 0x03, 0xf0, 0x00, 0x7e, 0x00, 0x03, 0xf0, 0x3f,
  0x03, 0xf0, 0x00, 0x7f, 0xfe, 0x03, 0xff, 0xff, 0x03, 0xf0, 0x00, 0x7f, 0xfe, 0x03, 0xff, 0xff,
  0x03, 0xf0, 0x00, 0x7f, 0xfe, 0x03, 0xff, 0xff, 0x03, 0xf0, 0x00, 0x7f, 0xfe, 0x03, 0xff, 0xff,
  0x03, 0xf0, 0x00, 0x7f, 0xfe, 0x03, 0xff, 0xfe, 0x03, 0xf0, 0x00, 0x7f, 0xfe, 0x03, 0xff, 0xfc,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xc0, 0x07, 0xe0, 0x00, 0xff, 0xff,
  0xff, 0xff, 0xc0, 0x07, 0xe0, 0x01, 0xff, 0xff, 0xff, 0xff, 0xc0, 0x07, 0xe0, 0x03, 0xff, 0xff,
  0xff, 0xff, 0xc0, 0x07, 0xe0, 0x03, 0xff, 0xff, 0xff, 0xff, 0xc0, 0x07, 0xe0, 0x03, 0xff, 0xff,
  0xff, 0xff, 0xc0, 0x07, 0xe0, 0x03, 0xff, 0xff, 0xfc, 0x00, 0x00, 0x07, 0xe0, 0x03, 0xf0, 0x00,
  0xfc, 0x00, 0x00, 0x07, 0xe0, 0x03, 0xf0, 0x00, 0xfc, 0x00, 0x00, 0x07, 0xe0, 0x03, 0xff, 0xfc,
  0xfc, 0x00, 0x00, 0x07, 0xe0, 0x03, 0xff, 0xfe, 0xff, 0xfc, 0x00, 0x07, 0xe0, 0x03, 0xff, 0xff,
  0xff, 0xfc, 0x00, 0x07, 0xe0, 0x03, 0xff, 0xff, 0xff, 0xfc, 0x00, 0x07, 0xe0, 0x01, 0xff, 0xff,
  0xff, 0xfc, 0x00, 0x07, 0xe0, 0x00, 0xff, 0xff, 0xff, 0xfc, 0x00, 0x07, 0xe0, 0x00, 0x00, 0x3f,
  0xff, 0xfc, 0x00, 0x07, 0xe0, 0x00, 0x00, 0x3f, 0xfc, 0x00, 0x00, 0x07, 0xe0, 0x03, 0xff, 0xff,
  0xfc, 0x00, 0x00, 0x07, 0xe0, 0x03, 0xff, 0xff, 0xfc, 0x00, 0x00, 0x07, 0xe0, 0x03, 0xff, 0xff,
  0xfc, 0x00, 0x00, 0x07, 0xe0, 0x03, 0xff, 0xff, 0xfc, 0x00, 0x00, 0x07, 0xe0, 0x03, 0xff, 0xfe,
  0xfc, 0x00, 0x00, 0x07, 0xe0, 0x03, 0xff, 0xfc
};

//64x88
extern const unsigned char finger[] PROGMEM = {
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf0, 0x7f, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xc0, 0x1f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x8f, 0xaf, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0x9f, 0xd7, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xdf, 0xd7, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xeb, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xeb, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xcb, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xdf, 0x03, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xc0, 0x0d, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xe1, 0xfd, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xbf, 0xfd, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xbf, 0xfd, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xb0, 0x7d, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xb8, 0x3d, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xbf, 0xfd, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xbf, 0xfd, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xbf, 0xfd, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xbf, 0xfd, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xbf, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xbf, 0xfe, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xdf, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xdf, 0xfe, 0xe3, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xdf, 0xfe, 0xc9, 0xff, 0xff, 0xff, 0xff, 0xff, 0xdf, 0xfe, 0xcd, 0x7f, 0xff,
  0xff, 0xff, 0xff, 0xdc, 0x1e, 0x8e, 0x3f, 0xff, 0xff, 0xff, 0xff, 0xdf, 0xfe, 0x01, 0x3f, 0xff,
  0xff, 0xff, 0xff, 0x9f, 0xfe, 0x00, 0x1f, 0xff, 0xff, 0xff, 0xc0, 0x1f, 0x1f, 0x70, 0x0f, 0xff,
  0xff, 0xff, 0x01, 0x9f, 0xff, 0xe3, 0x8f, 0xff, 0xff, 0xfe, 0x43, 0x9f, 0x8f, 0xff, 0x87, 0xff,
  0xff, 0xe0, 0xff, 0xde, 0x3f, 0xff, 0xd3, 0xff, 0xff, 0x99, 0xff, 0xdf, 0xff, 0xff, 0xd9, 0xff,
  0xff, 0x0d, 0xff, 0xdf, 0xff, 0xff, 0xdc, 0xff, 0xfe, 0x1d, 0xff, 0xdf, 0xff, 0xff, 0xee, 0xff,
  0xfc, 0x7d, 0xff, 0xdf, 0xff, 0xff, 0xee, 0x7f, 0xfd, 0xfd, 0xff, 0xdf, 0xff, 0xff, 0xef, 0x3f,
  0xfd, 0xfd, 0xff, 0xdf, 0xff, 0xff, 0xef, 0x3f, 0xfd, 0xfd, 0xff, 0xdf, 0xff, 0xff, 0xef, 0x3f,
  0xfd, 0xfd, 0xff, 0xdf, 0xff, 0xff, 0xee, 0x7f, 0xfd, 0xfd, 0xff, 0xdf, 0xff, 0xff, 0xee, 0x7f,
  0xf9, 0xfd, 0xff, 0xdf, 0xff, 0xff, 0xec, 0xff, 0xf9, 0xfd, 0xff, 0xdf, 0xff, 0xff, 0xf1, 0xff,
  0xf9, 0xfd, 0xff, 0xdf, 0xff, 0xff, 0xf3, 0xff, 0xf9, 0xfd, 0xff, 0xdf, 0xff, 0xff, 0xf3, 0xff,
  0xfd, 0xfd, 0xff, 0xdf, 0xff, 0x7f, 0xf7, 0xff, 0xfd, 0xfd, 0xff, 0xdf, 0xff, 0x7f, 0xf7, 0xff,
  0xf9, 0xfc, 0xff, 0xdf, 0xff, 0x7f, 0xf7, 0xff, 0xf9, 0xfc, 0xff, 0xff, 0xff, 0xff, 0xf7, 0xff,
  0xfb, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf7, 0xff, 0xfb, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf7, 0xff,
  0xfd, 0xfe, 0xff, 0xff, 0xff, 0xff, 0xf7, 0xff, 0xfd, 0xfd, 0xff, 0xff, 0xff, 0x7f, 0xef, 0xff,
  0xfe, 0xfd, 0xff, 0xff, 0xff, 0x3f, 0xef, 0xff, 0xff, 0x8f, 0xff, 0xfb, 0xff, 0x3f, 0xdf, 0xff,
  0xff, 0xe7, 0xff, 0xf7, 0xff, 0x3f, 0xbf, 0xff, 0xff, 0xf9, 0xff, 0xf7, 0xff, 0x7f, 0x7f, 0xff,
  0xff, 0xfc, 0xff, 0xff, 0xff, 0xfc, 0xff, 0xff, 0xff, 0xfe, 0x7f, 0xff, 0xff, 0xc3, 0xff, 0xff,
  0xff, 0xff, 0x3f, 0xff, 0xff, 0x9f, 0xff, 0xff, 0xff, 0xff, 0x87, 0x9f, 0xff, 0x7f, 0xff, 0xff,
  0xff, 0xff, 0xe0, 0x03, 0xfc, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xf8, 0xf8, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xfe, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

//25x25
extern const unsigned char qr[] PROGMEM = {
  0xfe, 0x2a, 0x3f, 0x80, 0x82, 0x6a, 0xa0, 0x80, 0xba, 0x62, 0xae, 0x80, 0xba, 0xaf, 0xae, 0x80,
  0xba, 0x94, 0xae, 0x80, 0x82, 0x66, 0xa0, 0x80, 0xfe, 0xaa, 0xbf, 0x80, 0x00, 0x4b, 0x00, 0x00,
  0xc7, 0x6f, 0x0c, 0x00, 0xf9, 0x33, 0x1f, 0x00, 0x6a, 0xcb, 0x1d, 0x80, 0x31, 0xa1, 0xf4, 0x80,
  0xb7, 0x68, 0xb0, 0x80, 0xa8, 0x35, 0x51, 0x00, 0x8e, 0xaf, 0x7d, 0x80, 0x8d, 0xd1, 0xb6, 0x80,
  0xb2, 0xe7, 0xfa, 0x00, 0x00, 0x94, 0x88, 0x00, 0xfe, 0xfc, 0xa8, 0x80, 0x82, 0xa3, 0x88, 0x00,
  0xba, 0x3f, 0xfb, 0x00, 0xba, 0x37, 0xe1, 0x80, 0xba, 0x66, 0x06, 0x80, 0x82, 0xa1, 0x98, 0x80,
  0xfe, 0xf0, 0xe4, 0x80
};