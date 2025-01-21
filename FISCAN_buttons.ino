void setupButtons() {
  stalkUpButton.attachClick(singleClickUp);
  stalkUpButton.attachMultiClick(doubleClickUp);
  stalkUpButton.setPressMs(1000);  // that is the time when LongPressStart is called
  stalkUpButton.attachLongPressStart(pressStartUp);

  stalkDownButton.attachClick(singleClickDown);
  stalkUpButton.attachMultiClick(doubleClickDown);
  stalkDownButton.setPressMs(1000);  // that is the time when LongPressStart is called
  stalkDownButton.attachLongPressStart(pressStartDown);

  stalkResetButton.attachClick(singleClickReset);
  stalkResetButton.setPressMs(1000);  // that is the time when LongPressStart is called
  stalkResetButton.attachLongPressStart(pressStartReset);
}

void mimickStalkButtons() {
  digitalWrite(stalkPushUpReturn, digitalRead(stalkPushUp));
  digitalWrite(stalkPushDownReturn, digitalRead(stalkPushDown));
  digitalWrite(stalkPushResetReturn, digitalRead(stalkPushReset));
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

void doubleClickUp() {
  if (!fisDisable) {
    readBlock = readBlock + 5;
  }
  if (readBlock > 255) {
    readBlock = 0;
  }
}  // singleClick

void doubleClickDown() {
  if (!fisDisable) {
    readBlock = readBlock - 5;
  }
  if (readBlock < 0) {
    readBlock = 0;
  }
}  // singleClick

void singleClickReset() {
}  // singleClick

void pressStartUp() {
  if (hasHaldex) {
    lastHaldex = -1;
    lastBlock = -1;
    showHaldex = !showHaldex;
    for (int i = 0; i < 8; i++) {
      fisLine[i] = "";
    }
  }
}  // pressStart()

void pressStartDown() {
  readBlock = 1;
}  // pressStart()

void pressStartReset() {
  fisDisable = !fisDisable;  //flip-flop fisDisable
  mimickSet = true;
}  // pressStart()