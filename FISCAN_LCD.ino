void bootLCD() {
  lcd.begin(lcdColumn, lcdRow);
  lcd.backlight();
  // print FIS Line 3 on Row 0
  lcd.setCursor((lcdRow - fisLine[3].length()) / 2, 0);
  lcd.print(fisLine[3]);

  // print FIS Line 4 on Row 1
  lcd.setCursor((lcdRow - fisLine[4].length()) / 2, 1);
  lcd.print(fisLine[4]);

  // print FIS Line 6 on Row 2
  lcd.setCursor((lcdRow - fisLine[6].length()) / 2, 2);
  lcd.print(fisLine[6]);
}

void displayLCD() {
  for (uint8_t i = 0; i < 8; i++) {
    lcd.setCursor(0, i);
    lcd.print(fisLine[i]);
  }
}