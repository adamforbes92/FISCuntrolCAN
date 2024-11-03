void setupPins() {
#if serialDebug
  Serial.println(F("Defining Pin Inputs/Outputs..."));
#endif
  pinMode(stalkPushUpReturn, OUTPUT);     // Configure TX of K-line as Digital Output (only required for Slow Init)
  pinMode(stalkPushDownReturn, OUTPUT);   // Configure TX of K-line as Digital Output (only required for Slow Init)
  pinMode(stalkPushResetReturn, OUTPUT);  // Configure TX of K-line as Digital Output (only required for Slow Init)

  if (checkLED) {
    simulateOutputTest();
  }

  pinMode(stalkPushUp, INPUT);     // Configure TX of K-line as Digital Output (only required for Slow Init)
  pinMode(stalkPushDown, INPUT);   // Configure TX of K-line as Digital Output (only required for Slow Init)
  pinMode(stalkPushReset, INPUT);  // Configure TX of K-line as Digital Output (only required for Slow Init)
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
  if (showBootScreen) {
    returnBootMessage();
    if (hasFIS) {
      bootFIS();
    }

    if (hasLCD) {
      bootLCD();
      delay(2000);
    }
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
    }

    if (hasLCD) {
      lcd.setCursor(0, 0);
      lcd.clear();
      lcd.print("Connecting to K-Line...");
    }

    diag.connect(connect_to_module, module_baud_rate, false);
#if debug_traffic
    diag.KWP1281debugFunction(KWP1281debugFunction);
#endif
  }

  if (hasCAN) {
    // setup CAN
  }
}