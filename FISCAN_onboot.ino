void setupPins() {
#if serialDebug
  Serial.println(F("Defining Pin Inputs/Outputs..."));
#endif
  pinMode(stalkPushUpReturn, OUTPUT);     // Configure
  pinMode(stalkPushDownReturn, OUTPUT);   // Configure TX of K-line as Digital Output (only required for Slow Init)
  pinMode(stalkPushResetReturn, OUTPUT);  // Configure TX of K-line as Digital Output (only required for Slow Init)

  if (checkLED) {
    simulateOutputTest();
  }

  pinMode(ignitionMonitorPin, INPUT);  // Configure TX of K-line as Digital Output (only required for Slow Init)

  attachInterrupt(digitalPinToInterrupt(stalkPushUp), checkTicks, CHANGE);
  attachInterrupt(digitalPinToInterrupt(stalkPushDown), checkTicks, CHANGE);
  attachInterrupt(digitalPinToInterrupt(stalkPushReset), checkTicks, CHANGE);

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
//sprintf(combinedArray, "%s", combinedArray, "\n");  // with word space
#if serialDebug
      Serial.println(combinedArray);
#endif
      FIS.writeMultiLineText(0, 15, combinedArray, false);
      delay(bootTime);
    }

    if (hasLCD) {
      bootLCD();
      delay(bootTime);
    }
  }

  if (showBootScreen == 2) {
    if (hasFIS) {
      //bootFIS();
      FIS.clear();
      FIS.drawBitmap(0, 0, 64, 88, finger, false);
      delay(bootTime);
    }

    if (hasLCD) {
      bootLCD();
      delay(bootTime);
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
      FIS.setTextAlignment(TLBFISLib::LEFT);
      FIS.writeText(0, 1, "CONNECTING...");
    }

    if (hasLCD) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print("CONNECTED!");
    }

    if (diag.attemptConnect(connect_to_module, module_baud_rate) == KLineKWP1281Lib::SUCCESS) {
      isConnectedK = true;
    } else {
      isConnectedK = false;
    }

    //diag.connect(connect_to_module, module_baud_rate, false);

#if serialDebug
    Serial.println(diag.getPartNumber());
    Serial.println(diag.getIdentification());
#endif

    if (isConnectedK) {
      if (hasFIS) {
        // display conenecting to kline
        FIS.clear();
        FIS.setFont(TLBFISLib::COMPACT);
        FIS.setTextAlignment(TLBFISLib::LEFT);
        FIS.writeText(0, 30, diag.getPartNumber());
        FIS.writeText(0, 39, diag.getIdentification());
        delay(bootTime / 2);
      }

      if (hasLCD) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(diag.getPartNumber());
        lcd.setCursor(0, 1);
        lcd.print(diag.getIdentification());
      }
    }
  }

  if (hasCAN || hasHaldex) {
    // setup CAN
#if serialDebug
    Serial.println(F("Setting up CAN..."));
#endif
    if (hasFIS) {
      // display conenecting to kline
      FIS.setTextAlignment(TLBFISLib::LEFT);
      FIS.setFont(TLBFISLib::COMPACT);
      FIS.writeText(0, 0, "CONNECTING...");
    }

    if (hasLCD) {
      lcd.clear();
      lcd.setCursor(0, 0);
      lcd.print(connectingToCAN);
    }
    canInit();

#if serialDebug
    Serial.println(F("Set up CAN complete!"));
#endif
    if (isConnectedCAN) {
      if (hasFIS) {
        FIS.clear();
        FIS.setFont(TLBFISLib::COMPACT);
        FIS.setTextAlignment(TLBFISLib::LEFT);
        FIS.writeText(0, 1, "CONNECTED!");
      }
      if (hasLCD) {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print(connectedToCAN);
      }
    }
  }
}
