/*
 Demonstration sketch for Adafruit i2c/SPI LCD backpack
 using MCP23008 I2C expander
 ( http://www.ladyada.net/products/i2cspilcdbackpack/index.html )

 This sketch prints "Hello World!" to the LCD
 and shows the time.
 
  The circuit:
  You will need a 3.3 to 5V level shifter
 * 5V  to Level Shifter HV to ESP8266 5V pin 
 * Level Shiftern LV to ESP8266 3.3V pin
 * GND to Level Shifter GND to ESP8266 GND pin
 * CLK to Level Shifter HL, Level Shifter LL to ESP8266 D1 pin
 * DAT to Level Shifter HL, Level Shifter LL to ESP9266 D2 pin
*/

// include the library code:
#include "Wire.h"
#include "Adafruit_LiquidCrystal.h"
TwoWire Wire_1 = TwoWire();              // Create Wire interface

Adafruit_LiquidCrystal lcd(0);           // Connect via i2c, default address #0 (A0-A2 not jumpered)

void setup() {
  Wire_1.begin(D2, D1);                  // Standard i2c interface on ESP8266, can also use other pints e.g. D3 & D4
  Wire_1.setClock(100000);               // Standard 100kHz speed

  lcd.begin(20, 4, LCD_5x8DOTS, Wire_1); // set up the LCD's number of rows and columns: 
  lcd.print("hello, world!");            // Print a message to the LCD.
}

void loop() {
  lcd.setCursor(0, 1);                   // set the cursor to column 0, line 1, note: line 1 is the second row, since counting begins with 0):
  lcd.print(millis()/1000);              // print the number of seconds since reset:

  lcd.setBacklight(HIGH);
  delay(500);
  lcd.setBacklight(LOW);
  delay(500);
}
