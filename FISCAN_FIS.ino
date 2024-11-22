void bootFIS() {
  FIS.begin();
  FIS.initScreen(screenSize);  // defined in config
  //All commands have been moved to the drawScreen() function (defined below), so that the custom functions can also execute it.
}

void displayFIS() {
  FIS.update();
  FIS.setTextAlignment(TLBFISLib::CENTER);
  FIS.setFont(TLBFISLib::COMPACT);

  if (!showHaldex) {
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
  FIS.drawBitmap(0, 0, 64, 88, finger);

  //Set a workspace in the middle of the FIS and clear it.
  //The size of the QR code is 25x25px, so a rectangle of 27x27px should be cleared "behind it" to make the code readable.
  //FIS.setWorkspace((64 - 27) / 2, (48 - 27) / 2, 27, 27, true);  //"true" = also clear

  //Draw the QR code bitmap at position X1, Y1 (inside the workspace defined above), with its width and height of 25px.
  //FIS.drawBitmap(1, 1, 25, 25, qr);

  //It is not necessary to reset the workspace, because an "error" event will automatically clear all custom workspaces.
}

const PROGMEM uint8_t avant[138] = {
  0x07, 0xfe, 0x00, 0x3f, 0xff, 0xc0, 0x78, 0x01, 0xe0, 0xc8, 0x01, 0x30, 0xb4, 0x02, 0xd0, 0xc4, 0x02, 0x30, 0x84, 0x02, 0x10, 0x84, 0x02, 0x10, 0x84, 0x02, 0x10, 0x84, 0x02, 0x10, 0x84, 0x02, 0x10, 0x9f, 0xff, 0x90, 0xb0, 0x00, 0xd0, 0xe0, 0x00, 0x70, 0x80, 0x00, 0x10, 0x80, 0x00, 0x10, 0x80, 0x00, 0x10, 0xe0, 0x00, 0x70, 0xb7, 0xfe, 0xd0, 0x98, 0x01, 0x90, 0x90, 0x00, 0x90, 0x90, 0x00, 0x90, 0x90, 0x00, 0x90, 0x90, 0x00, 0x90, 0x90, 0x00, 0x90, 0xf0, 0x00, 0xf0, 0x90, 0x00, 0x90, 0x90, 0x00, 0x90, 0x90, 0x00, 0x90, 0x90, 0x00, 0x90, 0x90, 0x00, 0x90, 0x90, 0x00, 0x90, 0x90, 0x00, 0x90, 0xb0, 0x00, 0xd0, 0xd0, 0x00, 0xb0, 0x90, 0x00, 0x90, 0x90, 0x00, 0x90, 0x90, 0x00, 0x90, 0x90, 0x00, 0x90, 0x98, 0x01, 0x90, 0x97, 0xfe, 0x90, 0xa0, 0x00, 0x50, 0xe0, 0x00, 0x70, 0x90, 0x00, 0x90, 0x7f, 0xff, 0xe0, 0x1f, 0xff, 0x80
};

const PROGMEM uint8_t sedan[138] = {
  0x07, 0xfe, 0x00, 0x3f, 0xff, 0xc0, 0x78, 0x01, 0xe0, 0xc8, 0x01, 0x30, 0xb4, 0x02, 0xd0, 0xc4, 0x02, 0x30, 0x84, 0x02, 0x10, 0x84, 0x02, 0x10, 0x84, 0x02, 0x10, 0x84, 0x02, 0x10, 0x84, 0x02, 0x10, 0x9f, 0xff, 0x90, 0xb0, 0x00, 0xd0, 0xe0, 0x00, 0x70, 0x80, 0x00, 0x10, 0x80, 0x00, 0x10, 0x80, 0x00, 0x10, 0xe0, 0x00, 0x70, 0xb7, 0xfe, 0xd0, 0x98, 0x01, 0x90, 0x90, 0x00, 0x90, 0x90, 0x00, 0x90, 0x90, 0x00, 0x90, 0x90, 0x00, 0x90, 0x90, 0x00, 0x90, 0xf0, 0x00, 0xf0, 0x90, 0x00, 0x90, 0x90, 0x00, 0x90, 0x90, 0x00, 0x90, 0x90, 0x00, 0x90, 0x90, 0x00, 0x90, 0x90, 0x00, 0x90, 0x90, 0x00, 0x90, 0xb8, 0x01, 0xd0, 0xe7, 0xfe, 0x70, 0x80, 0x00, 0x10, 0xc0, 0x00, 0x30, 0xa0, 0x00, 0x50, 0x90, 0x00, 0x90, 0x8c, 0x03, 0x10, 0x83, 0xfc, 0x10, 0xc0, 0x00, 0x30, 0x60, 0x00, 0x60, 0x50, 0x00, 0xa0, 0x3f, 0xff, 0xc0, 0x07, 0xfe, 0x00
};

const PROGMEM uint8_t left_door[8] = {
  0x02, 0x07, 0x0c, 0x18, 0x30, 0x63, 0xc5, 0x79
};

const PROGMEM uint8_t right_door[8]{
  0xc0, 0xe0, 0x30, 0x18, 0x0c, 0xc6, 0xa3, 0x9e
};

const PROGMEM uint8_t avant_trunc[8]{
  0x40, 0x08, 0xff, 0xfc, 0xff, 0xfc, 0x7f, 0xf8
};

const PROGMEM uint8_t sedan_trunc[21]{
  0x80, 0x00, 0x40, 0xc0, 0x00, 0xc0, 0xe0, 0x01, 0xc0, 0xf8, 0x07, 0xc0, 0x7f, 0xff, 0x80, 0x3f, 0xff, 0x00, 0x1f, 0xfe, 0x00
};

const PROGMEM uint8_t b5f[704]{
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x80, 0x1f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x80, 0x1f, 0xff, 0xf0, 0x00, 0x1f, 0xff, 0xf1, 0x80, 0x1f, 0xff, 0xc0, 0x00, 0x07, 0xff, 0x01, 0x80, 0x1f, 0xff, 0x80, 0x00, 0x03, 0xf0, 0x01, 0x80, 0x1c, 0x0f, 0x80, 0x00, 0x01, 0xe0, 0x01, 0x80, 0x1c, 0x0f, 0x00, 0x00, 0x00, 0xe0, 0x01, 0x80, 0x1c, 0x0f, 0x00, 0x00, 0x00, 0xe0, 0x07, 0x80, 0x1c, 0x0e, 0x00, 0x00, 0x00, 0xe0, 0xc7, 0x80, 0x1c, 0x0e, 0x00, 0x00, 0x00, 0x60, 0x03, 0x80, 0x1c, 0x0e, 0x00, 0x00, 0x00, 0x60, 0x01, 0x80, 0x1c, 0x0e, 0x00, 0x00, 0x00, 0x60, 0x01, 0x80, 0x1c, 0x0e, 0x00, 0x00, 0x00, 0x78, 0x01, 0x80, 0x1c, 0x0e, 0x00, 0x00, 0x00, 0x7f, 0x81, 0x80, 0x1c, 0x0e, 0x00, 0x00, 0x00, 0x3f, 0xff, 0x80, 0x1c, 0x0e, 0x00, 0x00, 0x00, 0x20, 0x03, 0x80, 0x1c, 0x0e, 0x00, 0x00, 0x00, 0x20, 0x01, 0x80, 0x1c, 0x0e, 0x03, 0xff, 0xe0, 0x20, 0x01, 0x80, 0x1c, 0x0e, 0x07, 0xff, 0xf0, 0x20, 0x01, 0x80, 0x1c, 0x0e, 0x07, 0xff, 0xf0, 0x20, 0x01, 0x80, 0x1c, 0x0f, 0x07, 0xff, 0xf0, 0x3f, 0xff, 0x80, 0x1c, 0x0f, 0x07, 0xff, 0xe0, 0x3f, 0xf3, 0x80, 0x1c, 0x0f, 0x81, 0xf0, 0x00, 0x3f, 0xf1, 0x80, 0x1c, 0x00, 0x18, 0xe0, 0x00, 0x3f, 0xf1, 0x80, 0x1f, 0xe0, 0xd6, 0xe0, 0x00, 0x60, 0x01, 0x80, 0x1f, 0xfc, 0xf8, 0xe0, 0x00, 0x60, 0x01, 0x80, 0x1f, 0xfe, 0x78, 0xe0, 0x00, 0x60, 0x01, 0x80, 0x1f, 0xff, 0x5e, 0xe0, 0x00, 0x60, 0x01, 0x80, 0x1d, 0xff, 0x31, 0xe0, 0x00, 0x60, 0x01, 0x80, 0x1c, 0xff, 0x83, 0xe0, 0x00, 0x7f, 0xff, 0x80, 0x1c, 0x7f, 0xfe, 0xe0, 0x00, 0xff, 0xff, 0x80, 0x1c, 0x1f, 0xff, 0xe0, 0x00, 0xe0, 0x03, 0x80, 0x1c, 0x00, 0xff, 0xe0, 0x01, 0xe0, 0x01, 0x80, 0x1c, 0x00, 0xff, 0xe0, 0x01, 0xe0, 0x01, 0x80, 0x1c, 0x00, 0xff, 0xf0, 0x03, 0xe0, 0x01, 0x80, 0x1c, 0x00, 0x7f, 0xf8, 0x07, 0xe0, 0x01, 0x80, 0x1c, 0x00, 0x77, 0xfe, 0x0f, 0xff, 0xff, 0x80, 0x1f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x80, 0x1f, 0xff, 0xff, 0xff, 0xdf, 0xe0, 0x01, 0x80, 0x1f, 0xff, 0xff, 0xff, 0xc3, 0xe0, 0x01, 0x80, 0x1f, 0xe3, 0x8f, 0x1f, 0xe1, 0xe0, 0x01, 0x80, 0x1f, 0xdf, 0x86, 0x0f, 0xe1, 0xe0, 0x01, 0x80, 0x1f, 0x8a, 0x66, 0x03, 0xe0, 0xe0, 0x03, 0x80, 0x1f, 0x07, 0x06, 0x3f, 0xf1, 0xe0, 0x07, 0x80, 0x1e, 0x00, 0x00, 0x7f, 0xff, 0xf8, 0x01, 0x80, 0x1e, 0x00, 0x00, 0x27, 0xff, 0xff, 0x41, 0x80, 0x1e, 0x00, 0x03, 0x21, 0xfc, 0x75, 0x41, 0x80, 0x1c, 0x00, 0x00, 0xf0, 0x78, 0x65, 0xb3, 0x80, 0x1c, 0x00, 0x01, 0xf0, 0x38, 0x67, 0xff, 0x80, 0x1c, 0x00, 0x0c, 0x58, 0x18, 0x63, 0xc1, 0x80, 0x1c, 0x00, 0x00, 0x48, 0x18, 0x60, 0x31, 0x80, 0x1c, 0x00, 0x00, 0xc0, 0x1c, 0x61, 0x01, 0x80, 0x1c, 0x00, 0x00, 0x00, 0x0c, 0x61, 0x01, 0x80, 0x1c, 0x00, 0x00, 0x00, 0x0c, 0x7f, 0xff, 0x80, 0x1c, 0x07, 0xc0, 0x3f, 0x8c, 0x7f, 0xf1, 0x80, 0x1c, 0x07, 0xe0, 0x3f, 0xcc, 0x7f, 0x01, 0x80, 0x1c, 0x0f, 0xe0, 0x3f, 0xcc, 0x70, 0x01, 0x80, 0x1c, 0x0f, 0xe0, 0x7f, 0xec, 0x60, 0x01, 0x80, 0x1c, 0x0f, 0xe0, 0x7f, 0xcc, 0x60, 0x01, 0x80, 0x1c, 0x00, 0x00, 0x00, 0x0c, 0x60, 0x07, 0x80, 0x1c, 0x00, 0x00, 0x00, 0x08, 0x60, 0xc7, 0x80, 0x1c, 0x00, 0x00, 0x00, 0x08, 0x60, 0x03, 0x80, 0x1c, 0x00, 0x00, 0x00, 0x08, 0x60, 0x01, 0x80, 0x1c, 0x00, 0x00, 0x00, 0x0c, 0x60, 0x01, 0x80, 0x1c, 0x00, 0x00, 0x00, 0x04, 0x78, 0x01, 0x80, 0x1c, 0x00, 0x00, 0x00, 0x06, 0x7f, 0x81, 0x80, 0x1c, 0x00, 0x00, 0x00, 0x03, 0x62, 0x3f, 0x80, 0x1c, 0x00, 0x00, 0x00, 0x01, 0xe2, 0x3f, 0x80, 0x1c, 0x00, 0x00, 0x00, 0x00, 0xe2, 0x3f, 0x80, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x60, 0x03, 0x80, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x70, 0x01, 0x80, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x78, 0x01, 0x80, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x68, 0x01, 0x80, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x6c, 0x01, 0x80, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x6c, 0x03, 0x80, 0x1f, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

const PROGMEM uint8_t Q[520]{
  0x00, 0x00, 0x00, 0x7f, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x00, 0x07, 0xff, 0xff, 0xc0, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x7f, 0xff, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x07, 0xff, 0xff, 0xff, 0xff, 0xc0, 0x00, 0x00, 0x0f, 0xff, 0xff, 0xff, 0xff, 0xe0, 0x00, 0x00, 0x3f, 0xff, 0xff, 0xff, 0xff, 0xf0, 0x00, 0x00, 0x7f, 0xff, 0xff, 0xff, 0xff, 0xf8, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x03, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x80, 0x07, 0xff, 0xff, 0xfc, 0x3f, 0xff, 0xff, 0xc0, 0x07, 0xff, 0xff, 0xc0, 0x00, 0xff, 0xff, 0xe0, 0x0f, 0xff, 0xfe, 0x00, 0x00, 0x3f, 0xff, 0xf0, 0x1f, 0xff, 0xf8, 0x00, 0x00, 0x1f, 0xff, 0xf0, 0x1f, 0xff, 0xf0, 0x00, 0x00, 0x0f, 0xff, 0xf0, 0x3f, 0xff, 0xe0, 0x00, 0x00, 0x07, 0xff, 0xf8, 0x3f, 0xff, 0xc0, 0x00, 0x00, 0x03, 0xff, 0xf8, 0x3f, 0xff, 0x80, 0x00, 0x00, 0x01, 0xff, 0xfc, 0x7f, 0xff, 0x00, 0x00, 0x00, 0x00, 0xff, 0xfc, 0x7f, 0xff, 0x00, 0x00, 0x00, 0x00, 0xff, 0xfc, 0x7f, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x7f, 0xfe, 0xff, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x7f, 0xfe, 0xff, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xfe, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xfe, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xff, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x1f, 0xff, 0xff, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x1f, 0xff, 0xff, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x1f, 0xff, 0xff, 0xf8, 0x00, 0x00, 0x00, 0x00, 0x1f, 0xff, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x1f, 0xff, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x1f, 0xff, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xfe, 0xff, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xfe, 0x7f, 0xfe, 0x00, 0x00, 0x00, 0x00, 0x7f, 0xfe, 0x7f, 0xff, 0x00, 0x00, 0x00, 0x00, 0x7f, 0xfe, 0x7f, 0xff, 0x00, 0x00, 0x00, 0x00, 0xff, 0xfc, 0x3f, 0xff, 0x00, 0x00, 0x00, 0x00, 0xff, 0xfc, 0x3f, 0xff, 0x80, 0x00, 0x00, 0x01, 0xff, 0xfc, 0x3f, 0xff, 0xc0, 0x00, 0x00, 0x03, 0xff, 0xf8, 0x1f, 0xff, 0xe0, 0x00, 0x00, 0x07, 0xff, 0xf8, 0x1f, 0xff, 0xf0, 0x00, 0x00, 0x0f, 0xff, 0xf8, 0x0f, 0xff, 0xf8, 0x00, 0x00, 0x1f, 0xff, 0xf0, 0x07, 0xff, 0xfe, 0x00, 0x00, 0x7f, 0xff, 0xf0, 0x07, 0xff, 0xff, 0x80, 0x00, 0xff, 0xff, 0xe0, 0x03, 0xff, 0xff, 0xe0, 0x03, 0xff, 0xff, 0xc0, 0x01, 0xff, 0xff, 0xfe, 0x0f, 0xff, 0xff, 0xc0, 0x00, 0xff, 0xff, 0xff, 0xc3, 0xff, 0xff, 0x80, 0x00, 0x7f, 0xff, 0xff, 0xe0, 0x7f, 0xff, 0x00, 0x00, 0x3f, 0xff, 0xff, 0xfe, 0x07, 0xfe, 0x00, 0x00, 0x1f, 0xff, 0xff, 0xff, 0xc0, 0x7c, 0x00, 0x00, 0x07, 0xff, 0xff, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x03, 0xff, 0xff, 0xff, 0xff, 0xc0, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x3f, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x0f, 0xff, 0xff, 0xff, 0xff, 0xf8, 0x00, 0x00, 0x01, 0xff, 0xff, 0xff, 0xff, 0xfe, 0x00, 0x00, 0x00, 0x1f, 0xff, 0xff, 0xff, 0xfe, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0xff, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f, 0xff, 0xf0, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3f, 0xe0
};

const PROGMEM uint8_t QBSW[128]{
  0x07, 0xe0, 0x1f, 0xc0, 0x3e, 0x38, 0x0c, 0x07, 0x1f, 0xf8, 0x1f, 0xf0, 0x7f, 0xbc, 0x1e, 0x0f, 0x3f, 0xfc, 0x1f, 0xf8, 0xff, 0x9c, 0x1e, 0x0e, 0x7c, 0x1e, 0x1c, 0x3d, 0xe1, 0x1c, 0x3e, 0x1e, 0x78, 0x0f, 0x1c, 0x1d, 0xc0, 0x1c, 0x3e, 0x1c, 0xf0, 0x07, 0x1c, 0x3d, 0xe0, 0x1c, 0x7e, 0x3c, 0xe0, 0x07, 0x9c, 0x78, 0xf8, 0x1c, 0x76, 0x38, 0xe0, 0x07, 0x9f, 0xf0, 0xfe, 0x1c, 0xe7, 0x78, 0xe0, 0x07, 0x9f, 0xf8, 0x3f, 0x9c, 0xe7, 0x70, 0xe0, 0x07, 0x1c, 0x3c, 0x07, 0x9d, 0xc7, 0x70, 0xf0, 0x07, 0x1c, 0x1c, 0x03, 0xdf, 0x87, 0xe0, 0x78, 0x0f, 0x1c, 0x1c, 0x01, 0xdf, 0x87, 0xe0, 0x7c, 0x3e, 0x1c, 0x3c, 0x83, 0xcf, 0x07, 0xc0, 0x3f, 0xff, 0xdf, 0xfd, 0xff, 0x8f, 0x07, 0xc0, 0x0f, 0xff, 0xdf, 0xf9, 0xff, 0x0e, 0x03, 0x80, 0x01, 0xff, 0x9f, 0xc0, 0x7e, 0x0e, 0x03, 0x80
};
