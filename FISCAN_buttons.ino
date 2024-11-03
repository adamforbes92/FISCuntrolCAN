void setupButtons() {
  stalkUpButton.debounceTime = 20;     // Debounce timer in ms
  stalkUpButton.multiclickTime = 350;  // Time limit for multi clicks
  stalkUpButton.longClickTime = 2500;  // time until "held-down clicks" register

  stalkDownButton.debounceTime = 20;     // Debounce timer in ms
  stalkDownButton.multiclickTime = 350;  // Time limit for multi clicks
  stalkDownButton.longClickTime = 2500;  // time until "held-down clicks" register

  stalkResetButton.debounceTime = 20;     // Debounce timer in ms
  stalkResetButton.multiclickTime = 350;  // Time limit for multi clicks
  stalkResetButton.longClickTime = 2500;  // time until "held-down clicks" register
}

void reviewButtons() {
  stalkUpButton.Update();
  stalkDownButton.Update();
  stalkResetButton.Update();

  // reset held LOW
  if (stalkResetButton.clicks == -1) {
    fisDisable = !fisDisable;  //flip-flop disDisable
    fisBeenToggled = false;
  }

  // up OR down held LOW, reset 'read block' to zero
  if (stalkUpButton.clicks == -1) {
    showHaldex = !showHaldex;
  }

  if (stalkDownButton.clicks == -1) {
    readBlock = 1;
  }

  // if up clicked more than once, add one onto readBlock
  if (stalkUpButton.clicks > 0) {
    readBlock++;
  }

  // if up clicked more than once, subtract one onto readBlock
  if (stalkDownButton.clicks > 0) {
    readBlock--;
  }
}

void mimickStalkButtons() {
  digitalWrite(stalkPushUpReturn, LOW);
  digitalWrite(stalkPushDownReturn, LOW);
  digitalWrite(stalkPushResetReturn, LOW);

  if (stalkUpButton.clicks == 1) { digitalWrite(stalkPushUpReturn, HIGH); }
  if (stalkDownButton.clicks == 1) { digitalWrite(stalkPushDownReturn, HIGH); }
  if (stalkResetButton.clicks == 1) { digitalWrite(stalkPushResetReturn, HIGH); }

  if (stalkResetButton.clicks == -1) {
    fisDisable = !fisDisable;  //flip-flop disDisable
    fisBeenToggled = true;
  }
}