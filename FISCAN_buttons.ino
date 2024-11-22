void setupButtons() {
  stalkUpButton.attachClick(singleClickUp);
  stalkUpButton.setPressMs(1000);  // that is the time when LongPressStart is called
  stalkUpButton.attachLongPressStart(pressStartUp);

  stalkDownButton.attachClick(singleClickDown);
  stalkDownButton.setPressMs(1000);  // that is the time when LongPressStart is called
  stalkDownButton.attachLongPressStart(pressStartDown);

  stalkResetButton.attachClick(singleClickReset);
  stalkResetButton.setPressMs(1000);  // that is the time when LongPressStart is called
  stalkResetButton.attachLongPressStart(pressStartReset);
}

void mimickStalkButtons() {
  digitalWrite(stalkPushUpReturn, !digitalRead(stalkPushUp));
  digitalWrite(stalkPushDownReturn, !digitalRead(stalkPushDown));
  digitalWrite(stalkPushResetReturn, !digitalRead(stalkPushReset));
}

// this function will be called when the button was pressed 1 time only.
void singleClickUp() {
  if (!fisDisable) {
    if (showHaldex && hasHaldex) {
      lastMode++;
      if (lastMode > 2) {
        lastMode = 0;
      }
      switch (lastMode) {
        case 0:
          state.mode = MODE_STOCK;
          break;
        case 1:
          state.mode = MODE_FWD;
          break;
        case 2:
          state.mode = MODE_5050;
          break;
      }
    } else {
      readBlock++;
    }
  }
}  // singleClick

void singleClickDown() {
  if (!fisDisable) {
    if (showHaldex && hasHaldex) {
      lastMode--;
      if (lastMode < 0) {
        lastMode = 2;
      }
      switch (lastMode) {
        case 0:
          state.mode = MODE_STOCK;
          break;
        case 1:
          state.mode = MODE_FWD;
          break;
        case 2:
          state.mode = MODE_5050;
          break;
      }
    } else {
      readBlock--;
    }
  }
}  // singleClick
void singleClickReset() {
}  // singleClick

void pressStartUp() {
  if (hasHaldex) {
    showHaldex = !showHaldex;
    for (int i = 0; i < 8; i++) {
      fisLine[i] = "";
    }
    if (hasFIS) {
      FIS.clear();
    }
    if (hasLCD) {
      lcd.clear();
    }
  }
}  // pressStart()

void pressStartDown() {
  readBlock = 1;
}  // pressStart()

void pressStartReset() {
  fisDisable = !fisDisable;  //flip-flop disDisable
  ignitionStateRunOnce = false;
  if (fisDisable) {
    FIS.turnOff();
    //FIS.end();
    diag.disconnect(false);
  } else {
    FIS.initScreen(screenSize);  // defined in config
    for (int i = 0; i < 8; i++) {
      fisLine[i] = "";
    }
  }
}  // pressStart()

// this function will be called when the button was pressed 2 times in a short timeframe.
void doubleClick() {
}  // doubleClick


// this function will be called when the button was pressed multiple times in a short timeframe.
void multiClick() {
#if serialDebug
  Serial.print("multiClick(");
  Serial.println(") detected.");
#endif
}  // multiClick