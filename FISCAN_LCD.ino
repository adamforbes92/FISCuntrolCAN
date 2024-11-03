void bootLCD() {
  lcd.init();
  lcd.backlight();
  // print FIS Line 3 on Row 0
  lcd.setCursor((lcdRow - fisLine3.length()) / 2, 0);
  lcd.print(fisLine3);

  // print FIS Line 4 on Row 1
  lcd.setCursor((lcdRow - fisLine4.length()) / 2, 1);
  lcd.print(fisLine4);

  // print FIS Line 6 on Row 2
  lcd.setCursor((lcdRow - fisLine6.length()) / 2, 2);
  lcd.print(fisLine6);
}

void displayLCD() {
  // print FIS Line 3 on Row 0
  lcd.setCursor((lcdRow - fisLine3.length()) / 2, 0);
  lcd.print(fisLine3);

  // print FIS Line 4 on Row 1
  lcd.setCursor((lcdRow - fisLine4.length()) / 2, 1);
  lcd.print(fisLine4);

  // print FIS Line 6 on Row 2
  lcd.setCursor((lcdRow - fisLine6.length()) / 2, 2);
  lcd.print(fisLine6);
}