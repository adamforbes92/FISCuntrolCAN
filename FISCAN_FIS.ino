void bootFIS() {
#if serialDebug
  Serial.println(F("Booting FIS..."));
#endif
  FIS.begin();
  FIS.initScreen(screenSize);  // defined in config
#if serialDebug
  Serial.println(F("Booting FIS Complete!"));
#endif
}

void displayFIS() {
  FIS.update();
  FIS.setTextAlignment(TLBFISLib::CENTER);
  FIS.setFont(TLBFISLib::COMPACT);

  if (!showHaldex && isConnectedK) {
    char buf[16];
    sprintf(buf, "BLOCK %d", readBlock);
    FIS.writeText(0, 1, buf);
    FIS.drawLine(0, 9, 64);
  } else {
    FIS.writeText(0, 1, "OPENHALDEX");
    FIS.drawLine(0, 9, 64);
  }

  FIS.setTextAlignment(TLBFISLib::LEFT);
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
  FIS.writeMultiLineText(0, 35, combinedArray, false);
}

void drawScreen() {
  //Draw the logo bitmap at position X0, Y2, with its width of 64px and a height of 45px.
  FIS.drawBitmap(0, 0, 64, 88, MK4Golf);

  //Set a workspace in the middle of the FIS and clear it.
  //The size of the QR code is 25x25px, so a rectangle of 27x27px should be cleared "behind it" to make the code readable.
  //FIS.setWorkspace((64 - 27) / 2, (48 - 27) / 2, 27, 27, true);  //"true" = also clear

  //Draw the QR code bitmap at position X1, Y1 (inside the workspace defined above), with its width and height of 25px.
  //FIS.drawBitmap(1, 1, 25, 25, qr);

  //It is not necessary to reset the workspace, because an "error" event will automatically clear all custom workspaces.
}
