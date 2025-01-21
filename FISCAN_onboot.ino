void setupPins() {
#if serialDebug
  Serial.println(F("Defining Pin Inputs/Outputs..."));
#endif

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

#if serialDebug
  Serial.println(F("Completed defining Pin Inputs/Outputs!"));
#endif
}

void blinkLED(int duration, int pinRef) {
  digitalWrite(pinRef, LOW);
  delay(duration);
  digitalWrite(pinRef, HIGH);
  delay(duration);
}

void simulateOutputTest() {
#if serialDebug
  Serial.println(F("Simulate Outputs / check LEDs..."));
#endif
  blinkLED(150, stalkPushUpReturn);
  blinkLED(150, stalkPushDownReturn);
  blinkLED(150, stalkPushResetReturn);
#if serialDebug
  Serial.println(F("Simulate Outputs / check LEDs complete!"));
#endif
}

void launchBoot() {
#if serialDebug
  Serial.println(F("Beginning Boot Sequence..."));
#endif
  if (hasFIS) {
    bootFIS();
  }

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

#if serialDebug
      Serial.println(combinedArray);
#endif
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
#if serialDebug
  Serial.println(F("Boot Sequence Complete!"));
#endif
}

void launchConnections() {
#if serialDebug
  Serial.println(F("Beginning connection launch..."));
#endif
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
        delay(connectionDelayDuration);
      }
      pressStartReset();
    }

#if serialDebug
    Serial.println(diag.getPartNumber());
    Serial.println(diag.getIdentification());
#endif

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
#if serialDebug
    Serial.println(F("Setting up CAN..."));
#endif
    canInit();

#if serialDebug
    Serial.println(F("Set up CAN complete!"));
#endif
  }
}

void fisDisablePrep() {
  if (fisDisable) {
    FIS.turnOff();
    //diag.disconnect(false);
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
