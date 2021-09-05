// LCD with i2c backpack test
// This replaces the contents of an LCD screen with the contents of a buffer
// Urs Utzinger, 2020

#include "LiquidCrystal_PCF8574.h"
#include <Wire.h>

TwoWire myWire = TwoWire();

LiquidCrystal_PCF8574 lcd(0x27);  // set the LCD address to 0x27 
char lcdDisplay[4][20];           // 4 lines of 20 character buffer

void setup()
{
  Serial.begin(115200);
  myWire.begin(D6, D5);
  myWire.setClock(100000); // 100kHz or 400kHz speed
  myWire.setClockStretchLimit(200000);
  Wire.beginTransmission(0x27);
  int error = Wire.endTransmission();
  if (error !=0) {Serial.println(F("Sensor not found")); }  
  lcd.begin(20,4, myWire);
  lcd.setBacklight(255);
}

void loop()
{
  char lcdbuf[21];
  const char someLine[] = "0123456789ABCDEF1234";  // 20 chars
  
  // fill the screen buffer
  strncpy(&lcdDisplay[0][0], someLine , 20);
  strncpy(&lcdDisplay[1][0], someLine , 20);
  strncpy(&lcdDisplay[2][0], someLine , 20);
  strncpy(&lcdDisplay[3][0], someLine , 20);
  // prepare screen
  lcd.home();
  lcd.clear();
  lcd.setCursor(0, 0);   
  // copy the buffer to the screen
  // 1st line continues at 3d line
  // 2nd line continues at 4th line
  lcd.setBacklight(0);
  strncpy(lcdbuf, &lcdDisplay[0][0], 20); lcdbuf[20] = '\0'; // create a termineted text line 
  lcd.print(lcdbuf);                                         // print the line to screen
  strncpy(lcdbuf, &lcdDisplay[2][0], 20); lcdbuf[20] = '\0';
  lcd.print(lcdbuf); 
  strncpy(lcdbuf, &lcdDisplay[1][0], 20); lcdbuf[20] = '\0';
  lcd.print(lcdbuf); 
  strncpy(lcdbuf, &lcdDisplay[3][0], 20); lcdbuf[20] = '\0';
  lcd.print(lcdbuf);
  lcd.setBacklight(255);
  delay(50); // keep artitically short to test if screen corrupts with frequent writes
}
