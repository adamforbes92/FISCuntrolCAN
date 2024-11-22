/* 
FISCuntrol V3.0 - a MK4 based FIS controller based on an ESP32.  
> Uses the stock buttons for toggling menus
> Supports CAN-BUS and K-line diagnostic protocols for gathering data
> Supports OpenHaldex 
> Optional RTC for special dates & custom messages
> Built-in Bluetooth for optional OpenHaldex 
> *** Boot logos to be added ***
*/
#include "FISCAN_config.h"

extern uint8_t readBlock = 1;
uint8_t measurements[29];

void sendFunctionFIS(uint8_t data) {
  SPI_INSTANCE.beginTransaction(SPISettings(125000, MSBFIRST, SPI_MODE3));
  SPI_INSTANCE.transfer(data);
  SPI_INSTANCE.endTransaction();
}
void beginFunctionFIS() {
  SPI_INSTANCE.begin();
}

// displays - LCD & FIS
extern LiquidCrystal_I2C lcd(0x27, lcdRow, lcdColumn);  // set the LCD address to 0x27 for a 16 chars and 2 line display
TLBFISLib FIS(fisENA, sendFunctionFIS, beginFunctionFIS);

// communication - k-line & CAN
ESP32_CAN<RX_SIZE_256, TX_SIZE_16> chassisCAN;
// Debugging can be enabled in "configuration.h" in order to print connection-related info on the Serial Monitor.
#if serialDebug
KLineKWP1281Lib diag(beginFunction, endFunction, sendFunction, receiveFunction, K_TX, is_full_duplex, &Serial);
#else
KLineKWP1281Lib diag(beginFunction, endFunction, sendFunction, receiveFunction, K_TX, is_full_duplex);
#endif

// Bluetooth for OpenHaldex etc...
//BluetoothSerial SerialBT;
openhaldexState state;

extern OneButton stalkUpButton(stalkPushUp, true);
extern OneButton stalkDownButton(stalkPushDown, true);
extern OneButton stalkResetButton(stalkPushReset, true);

// for repeated tasks (write to EEP & send status)
//TickTwo tickbtSendStatus(btSendStatus, btRefresh);
TickTwo tickSendOpenHaldex(broadcastOpenHaldex, canRefresh);

void IRAM_ATTR checkTicks() {
  // include all buttons here to be checked
  stalkUpButton.tick();     // just call tick() to check the state.
  stalkDownButton.tick();   // just call tick() to check the state.
  stalkResetButton.tick();  // just call tick() to check the state.
}

void setup() {
#if serialDebug
  Serial.begin(115200);
  Serial.println(F("ESP32 Initialisation..."));
#endif

  setupPins();     // set pin inputs/outputs, do output test if req.
  setupButtons();  // set de-bounce times, etc
  //launchBluetooth();  // begin the bluetooth connection (for OpenHaldex)

#if hasHaldex
  //tickbtSendStatus.start();  // begin ticker for BT Status
  tickSendOpenHaldex.start();  // begin ticker for BT Status
#endif
}

void loop() {
#if hasHaldex
  //tickbtSendStatus.update();  // refresh the BT Status ticker
  tickSendOpenHaldex.update();  // refresh the BT Status ticker
#endif
  ignitionState = HIGH;  //digitalRead(ignitionMonitorPin);  // check to see if the ignition has been turned on...
  stalkUpButton.tick();
  stalkDownButton.tick();
  stalkResetButton.tick();

  if (fisDisable) {
    mimickStalkButtons();
  }

  if (ignitionState == LOW) { ignitionStateRunOnce = false; }  // if ignition signal is 'low', reset the state

  if (ignitionState == HIGH && ignitionStateRunOnce == false && !fisDisable) {  //&& !fisBeenToggled)
    ignitionStateRunOnce = true;                                                // set ignStateRunOnce to stop redisplay of welcome message until ign. off.

    launchBoot();
    launchConnections();
  }

  if (!fisDisable) {
    // get data from ECU, either via. K-line or CAN
    if (hasK && !showHaldex) {
      if (millis() - lastDataRetrieve >= logFrequency) {
        lastDataRetrieve = millis();
        // read K-Line
        if (readBlock > 255) {
          readBlock = 1;
        }
        showMeasurements(readBlock);
      }
    }

    if (hasCAN || showHaldex) {
      parseCAN();
    }

    // display data from above capture, either via. FIS or LCD
    if (hasFIS) {
      if (hasK) {
        if (lastBlock != readBlock) {
          lastBlock = readBlock;
          FIS.clear();
        } else {
          displayFIS();
        }
      }

      if (hasCAN) {
        if (lastBlock != readBlock) {
          lastBlock = readBlock;
          FIS.clear();
        } else {
          displayFIS();
        }
      }

      if (hasHaldex) {
        if (lastHaldex != lastMode) {
          lastHaldex = lastMode;
          FIS.clear();
        } else {
          displayFIS();
        }
      }
    }

    if (hasLCD) {
      if (hasK) {
        if (lastBlock != readBlock) {
          lastBlock = readBlock;
          lcd.clear();
        } else {
          displayLCD();
        }
      }

      if (hasCAN) {
        if (lastBlock != readBlock) {
          lastBlock = readBlock;
          lcd.clear();
        } else {
          displayLCD();
        }
      }

      if (hasHaldex) {
        if (lastHaldex != lastMode) {
          lastHaldex = lastMode;
          lcd.clear();
        } else {
          displayLCD();
        }
      }
    }

    /*if (SerialBT.available()) {  // if anything comes in Bluetooth Serial
#if serialDebug
      Serial.println(F("Got serial data..."));
#endif
      btIncoming[10] = { 0 };
      incomingLen = SerialBT.readBytesUntil(serialPacketEnd, btIncoming, 10);
      if (incomingLen < 9) {
        runOnce = false;
      }
      btReceiveStatus();
    }*/
  }
}