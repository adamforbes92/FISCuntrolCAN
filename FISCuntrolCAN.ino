/* 
FISCuntrol V3.0 - a MK4 based FIS controller based on an ESP32.  
> Uses the stock buttons for toggling menus
> Supports CAN-BUS and K-line diagnostic protocols for gathering data
> Supports OpenHaldex 
> Optional RTC for special dates & custom messages
> *** Boot logos to be added ***
    *Bitmaps can easily be generated with the https://javl.github.io/image2cpp/ tool.
    *For halfscreen, the max visible size is 64x48, and for fullscreen it is 64x88.
> Revised code and PCBs for smaller footprints
> V1.01 - Added debug statements
> V1.02 - Edited PCB, missing pullup on K-Line (won't connect to ECU if missing!), added 1k pullDOWN on ENA ESP side, added 10k pullUP on ENA cluster side(!)
> V1.03 - Added CAN details, sorted RPM.  Left shift all text.  Confirmed working.
*/

#include "FISCAN_config.h"

extern uint8_t readBlock = 1;

void sendFunctionFIS(uint8_t data) {
  SPI_INSTANCE.beginTransaction(SPISettings(125000, MSBFIRST, SPI_MODE3));
  SPI_INSTANCE.transfer(data);
  SPI_INSTANCE.endTransaction();
}
void beginFunctionFIS() {
  SPI_INSTANCE.begin();
}

// displays - LCD & FIS
TLBFISLib FIS(fisENA, sendFunctionFIS, beginFunctionFIS);

// communication - k-line & CAN
ESP32_CAN<RX_SIZE_256, TX_SIZE_16> chassisCAN;
#if serialDebug
KLineKWP1281Lib diag(beginFunction, endFunction, sendFunction, receiveFunction, K_TX, is_full_duplex, &Serial);
#else
KLineKWP1281Lib diag(beginFunction, endFunction, sendFunction, receiveFunction, K_TX, is_full_duplex);
#endif

openhaldexState state;

extern OneButton stalkUpButton(stalkPushUp, true);
extern OneButton stalkDownButton(stalkPushDown, true);
extern OneButton stalkResetButton(stalkPushReset, true);

// for repeated tasks (write to EEP & send status)
TickTwo tickSendOpenHaldex(broadcastOpenHaldex, canRefresh);

void IRAM_ATTR checkTicks() {
  // include all buttons here to be checked
  stalkUpButton.tick();     // just call tick() to check the state.
  stalkDownButton.tick();   // just call tick() to check the state.
  stalkResetButton.tick();  // just call tick() to check the state.
}

void setup() {
#if serialDebug || ChassisCANDebug
  Serial.begin(serialBaud);
  DEBUG_PRINTLN("ESP32 Initialisation...");
#endif

  setupPins();     // set pin inputs/outputs, do output test if req.
  setupButtons();  // set de-bounce times, etc
  launchBoot();
  launchConnections();

#if hasHaldex
  tickSendOpenHaldex.start();  // begin ticker for BT Status
#endif
}

void loop() {
#if hasHaldex
  tickSendOpenHaldex.update();  // refresh the BT Status ticker
#endif

  if (digitalRead(ignitionMonitorPin)) {
    ignitionState = true;
    triggerShutdown = true;
  } else {
    ignitionState = false;
  }

  if (!ignitionState && triggerShutdown) {
    beginShutdown();  // in '_onboot.ino'
  }

  // tick over the buttons
  stalkUpButton.tick();
  stalkDownButton.tick();
  stalkResetButton.tick();

  if (mimickSet) {
    DEBUG_PRINTLN("Turning off FIS...");
    fisDisablePrep();  // in '_onboot.ino'
  }

  if (fisDisable) {
    mimickStalkButtons();
  }

  if (ignitionState && !ignitionStateRunOnce && !fisDisable) {
    ignitionStateRunOnce = true;  // set ignStateRunOnce to stop redisplay of welcome message until ign. off.

    launchBoot();
    launchConnections();
  }

  if (ignitionState && !fisDisable) {
    // Get data from ECU, either via. K-line or CAN //
    if (hasK && !showHaldex && isConnectedK) {
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
    // Get data from ECU, either via. K-line or CAN //

    // Display data from above capture
    //noInterrupts();
    if (hasFIS) {
      if (hasK && isConnectedK) {
        if (lastBlock != readBlock) {
          lastBlock = readBlock;
          FIS.clear();
          delay(fisWakeDelay);
        } else {
          displayFIS();
        }
      }

      if (hasCAN) {
        if (lastBlock != readBlock) {
          lastBlock = readBlock;
          FIS.clear();
          delay(fisWakeDelay);
        } else {
          displayFIS();
        }
      }

      if (hasHaldex) {
        if (lastHaldex != lastMode) {
          lastHaldex = lastMode;
          FIS.clear();
          delay(fisWakeDelay);
        } else {
          displayFIS();
        }
      }
    }
    // Display data from above capture
  }
}