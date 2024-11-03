/* 
FISCuntrol V3.0 - a MK4 based FIS controller based on an ESP32
> Uses the stock buttons for toggling menus
> Supports CAN-BUS and K-line diag protocols for gathering data
> Supports OpenHaldex 
> Optional RTC for special dates & custom messages
> *** Boot logos to be added ***
*/
#include "FISCAN_config.h"

#define SCREEN_SIZE TLBFISLib::FULLSCREEN  //try changing this to TLBFISLib::FULLSCREEN
uint8_t measurements[3 * 4];

void sendFunctionFIS(uint8_t data) {
  SPI_INSTANCE.beginTransaction(SPISettings(125000, MSBFIRST, SPI_MODE3));
  SPI_INSTANCE.transfer(data);
  SPI_INSTANCE.endTransaction();
}
void beginFunctionFIS() {
  SPI_INSTANCE.begin();
}

//extern TLBFISLib FIS(ENA_PIN, sendFunctionFIS, beginFunctionFIS);  //optionally, an "endFunction" can also be provided; it would be called when executing end()
extern LiquidCrystal_I2C lcd(0x27, lcdRow, lcdColumn);  // set the LCD address to 0x27 for a 16 chars and 2 line display
ESP32_CAN<RX_SIZE_256, TX_SIZE_16> chassisCAN;
BluetoothSerial SerialBT;
openhaldexState state;

#if serialDebug
extern KLineKWP1281Lib diag(beginFunction, endFunction, sendFunction, receiveFunction, TX_pin, is_full_duplex, &Serial);
#else
extern KLineKWP1281Lib diag(beginFunction, endFunction, sendFunction, receiveFunction, TX_pin, is_full_duplex);
#endif

extern ClickButton stalkUpButton(stalkPushUp, LOW, CLICKBTN_PULLUP);
extern ClickButton stalkDownButton(stalkPushDown, LOW, CLICKBTN_PULLUP);
extern ClickButton stalkResetButton(stalkPushReset, LOW, CLICKBTN_PULLUP);

// for repeated tasks (write to EEP & send status)
TickTwo tickbtSendStatus(btSendStatus, btRefresh);

void setup() {
#if serialDebug
  Serial.begin(115200);
  Serial.println(F("ESP32 Init..."));
#endif
  tickbtSendStatus.start();  // begin ticker for BT Status

  setupPins();               // set pin inputs/outputs, do output test if req.
  setupButtons();            // set de-bounce times, etc
  launchBoot();              // get boot screen and display (LCD or FIS)
  launchConnections();       // begin connections (either k-line or CAN)
  launchBluetooth();         // begin the bluetooth connection (for OpenHaldex)
}

void loop() {
  tickbtSendStatus.update();  // refresh the BT Status ticker
  ignitionState = digitalRead(ignitionMonitorPin); // check to see if the ignition has been turned on...
  reviewButtons(); // check the stalk, see if they've been pressed...

  if (fisDisable) { mimickStalkButtons(); } // if FIS is disabled, do nothing but mimick the stalk buttons

  if (ignitionState == LOW) { ignitionStateRunOnce = false; }  // if ignition signal is 'low', reset the state

  if (ignitionState == HIGH && ignitionStateRunOnce == false) {  //&& !fisBeenToggled)
    ignitionStateRunOnce = true;                                 // set ignStateRunOnce to stop redisplay of welcome message until ign. off.

    launchBoot();
    launchConnections();
  }

  // get data from ECU, either via. K-line or CAN
  if (hasK) {
    // read K-Line
    showMeasurements(readBlock);
  }

  if (hasCAN) {
    // read CAN
    parseCAN();
  }

  // display data from above capture, either via. FIS or LCD
  if (hasFIS) {
    if (showHaldex) {

    } else {
      displayFIS();
    }
  }

  if (hasLCD) {
    if (showHaldex) {

    } else {
      displayLCD();
    }
  }

  if (SerialBT.available()) {  // if anything comes in Bluetooth Serial
#if serialDebug
    Serial.println(F("Got serial data..."));
#endif
    btIncoming[10] = { 0 };
    incomingLen = SerialBT.readBytesUntil(serialPacketEnd, btIncoming, 10);
    if (incomingLen < 10) {
      runOnce = false;
    }
    btReceiveStatus();
  }
}