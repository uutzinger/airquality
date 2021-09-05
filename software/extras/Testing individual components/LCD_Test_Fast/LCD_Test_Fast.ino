#include "LiquidCrystal_PCF8574.h"
#include <Wire.h>

// #include <LiquidCrystal_I2C.h>
// LiquidCrystal_I2C lcd(0x27,20,4);  // set the LCD address to 0x27 for a 20 chars and 4 line display

TwoWire Wire_3 = TwoWire();

LiquidCrystal_PCF8574 lcd(0x27);  // set the LCD address to 0x27 
char lcdDisplay[4][20];           // 4 lines of 20 characters

void setup()
{
  Wire_3.begin(D2, D1);
  Wire_3.setClock(100000); // standard 100kHz speed
  Wire_3.setClockStretchLimit(200000); //SCD30, CCS811 requires clock stretching

  lcd.begin(20,4, Wire_3);
  lcd.setBacklight(255);
  //lcd.backlight();
}

void loop()
{
  char lcdbuf[21];
  const char clearLine[] = "0123456789ABCDEF1234";  // 20 spaces
  
  strncpy(&lcdDisplay[0][0], clearLine , 20);
  strncpy(&lcdDisplay[1][0], clearLine , 20);
  strncpy(&lcdDisplay[2][0], clearLine , 20);
  strncpy(&lcdDisplay[3][0], clearLine , 20);
  lcd.home();
  lcd.clear();
  lcd.setCursor(0, 0);   
  // 1st line continues at 3d line
  // 2nd line continues at 4th line
  strncpy(lcdbuf, &lcdDisplay[0][0], 20); lcdbuf[20] = '\0'; // create a one line c-string
  lcd.print(lcdbuf);                                         // print one line at a time
  strncpy(lcdbuf, &lcdDisplay[2][0], 20); lcdbuf[20] = '\0';
  lcd.print(lcdbuf); 
  strncpy(lcdbuf, &lcdDisplay[1][0], 20); lcdbuf[20] = '\0';
  lcd.print(lcdbuf); 
  strncpy(lcdbuf, &lcdDisplay[3][0], 20); lcdbuf[20] = '\0';
  lcd.print(lcdbuf);

  delay(50);
}
