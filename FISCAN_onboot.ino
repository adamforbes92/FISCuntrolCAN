void setupPins() {
  DEBUG_PRINTLN("Defining Pin Inputs/Outputs...");

  // setup output pins for stalk buttons
  pinMode(stalkPushUpReturn, OUTPUT);
  pinMode(stalkPushDownReturn, OUTPUT);
  pinMode(stalkPushResetReturn, OUTPUT);

  if (checkLED) {
    simulateOutputTest();
  }

  // set outputs HIGH to keep them 'off'
  digitalWrite(stalkPushUpReturn, HIGH);
  digitalWrite(stalkPushDownReturn, HIGH);
  digitalWrite(stalkPushResetReturn, HIGH);

  pinMode(ignitionMonitorPin, INPUT);  // ignition signal - could be an interrupt, necessary?

  attachInterrupt(digitalPinToInterrupt(stalkPushUp), checkTicks, FALLING);
  attachInterrupt(digitalPinToInterrupt(stalkPushDown), checkTicks, FALLING);
  attachInterrupt(digitalPinToInterrupt(stalkPushReset), checkTicks, FALLING);
  //attachInterrupt(digitalPinToInterrupt(ignitionMonitorPin), ignitionStateISR, CHANGE);

  DEBUG_PRINTLN("Completed defining Pin Inputs/Outputs!");
}

void blinkLED(int duration, int pinRef) {
  digitalWrite(pinRef, LOW);
  delay(duration);
  digitalWrite(pinRef, HIGH);
  delay(duration);
}

void simulateOutputTest() {
  DEBUG_PRINTLN("Simulate Outputs / check LEDs...");

  blinkLED(150, stalkPushUpReturn);
  blinkLED(150, stalkPushDownReturn);
  blinkLED(150, stalkPushResetReturn);

  DEBUG_PRINTLN("Simulate Outputs / check LEDs complete!");
}

void launchBoot() {
  DEBUG_PRINTLN("Beginning Boot Sequence...");

  if (hasFIS) {
    bootFIS();

    if (showBootScreen == 1) {
      returnBootMessage();
      if (hasFIS) {
        FIS.clear();
        FIS.setTextAlignment(TLBFISLib::CENTER);
        FIS.setFont(TLBFISLib::COMPACT);
        char combinedArray[500];

        for (uint8_t i = 0; i < 8; i++) {
          char buf1[fisLine[i].length() + 1];
          fisLine[i].toUpperCase();
          fisLine[i].toCharArray(buf1, fisLine[i].length() + 1);
          if (i == 0) {
            sprintf(combinedArray, "%s", buf1);  // with word space
          } else {
            sprintf(combinedArray, "%s\n%s", combinedArray, buf1);  // with word space
          }
        }

        DEBUG_PRINTLN(combinedArray);

        FIS.writeMultiLineText(0, 15, combinedArray, false);
        delay(bootScreenDuration);
      }
    }

    if (showBootScreen == 2) {
      if (hasFIS) {
        FIS.clear();
        FIS.drawBitmap(0, 0, 64, 88, MK4Golf, true);  // draw MK4 Golf FROM PROGMEM (true)!
        delay(bootScreenDuration);
      }
    }

    for (int i = 0; i < 8; i++) {
      fisLine[i] = "";
    }
  }
  DEBUG_PRINTLN("Boot Sequence Complete!");
}

void launchConnections() {
  if (hasK || hasCAN || hasHaldex) {
    DEBUG_PRINTLN("Beginning connection launch...");
  }

  if (hasK) {
    if (hasFIS) {
      // display conenecting to kline
      FIS.clear();
      FIS.setFont(TLBFISLib::COMPACT);
      FIS.setTextAlignment(globalTextAlignment);
      FIS.writeText(0, 1, "CONNECTING TO");
      FIS.writeText(0, 9, "K-LINE!");
    }

    if (diag.attemptConnect(K_Module, K_Baud) == KLineKWP1281Lib::SUCCESS) {
      isConnectedK = true;
      if (connectionDelayDuration > 0) {
        FIS.clear();
        FIS.setFont(TLBFISLib::COMPACT);
        FIS.setTextAlignment(globalTextAlignment);
        FIS.writeText(0, 1, "CONNECTED");
        FIS.writeText(0, 9, "K-LINE!");
        DEBUG_PRINTLN("Connected k-line!");
        delay(connectionDelayDuration);
      }
    } else {
      isConnectedK = false;
      if (connectionDelayDuration > 0) {
        FIS.clear();
        FIS.setFont(TLBFISLib::COMPACT);
        FIS.setTextAlignment(globalTextAlignment);
        FIS.writeText(0, 1, "FAILED");
        FIS.writeText(0, 9, "K-LINE!");
        DEBUG_PRINTLN("Failed k-line!");
        delay(connectionDelayDuration);
      }
    }

    DEBUG_PRINTLN(diag.getPartNumber());
    DEBUG_PRINTLN(diag.getIdentification());

    if (isConnectedK) {
      if (hasFIS && displayECUonBoot) {
        // display ECU details from k
        FIS.clear();
        FIS.setFont(TLBFISLib::COMPACT);
        FIS.setTextAlignment(globalTextAlignment);
        FIS.writeText(0, 30, diag.getPartNumber());
        FIS.writeText(0, 39, diag.getIdentification());
        delay(bootScreenDuration / 2);
      }
    }
  }

  if (hasCAN || hasHaldex) {
    // setup CAN
    DEBUG_PRINTLN("Setting up CAN...");
    canInit();
    DEBUG_PRINTLN("Set up CAN complete!");
  }
  if (!isConnectedCAN && !isConnectedK) {
    DEBUG_PRINTLN("No connections available, defaulting to OEM");
    pressStartReset();
  }
}

void fisDisablePrep() {
  if (fisDisable) {
    FIS.turnOff();
    diag.disconnect(false);
    //chassisCAN.reset();
  }
  if (!fisDisable) {
    ignitionStateRunOnce = false;

    FIS.begin();
    FIS.initScreen(screenSize);  // defined in config
    for (int i = 0; i < 8; i++) {
      fisLine[i] = "";
    }
  }
  if (mimickSet) {
    mimickSet = false;

    digitalWrite(stalkPushUpReturn, HIGH);
    digitalWrite(stalkPushDownReturn, HIGH);
    digitalWrite(stalkPushResetReturn, HIGH);
  }
}

void beginShutdown() {
  if (hasFIS) {
    FIS.end();
  }
  if (hasK) {
    diag.disconnect(false);
  }
  if(hasCAN){
    //chassisCAN.reset();
  }
  ignitionStateRunOnce = false;
  fisDisable = false;
  isConnectedK = false;
  lastBlock = -1;
  lastHaldex = -1;
  triggerShutdown = false;
  hasOpenHaldex = false;
}
