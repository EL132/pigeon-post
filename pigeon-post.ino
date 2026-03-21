#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Try 0x27 first (most common)
LiquidCrystal_I2C lcd(0x27, 16, 2);

void setup() {
  Wire.begin(21, 22);   // SDA = 21, SCL = 22

  lcd.init();           // initialize LCD
  lcd.backlight();      // turn on backlight

  lcd.setCursor(0, 0);  // column 0, row 0
  lcd.print("i miss you");
}

void loop() {
  // nothing needed here
}