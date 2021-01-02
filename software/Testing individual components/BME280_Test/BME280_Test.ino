/*
  Get basic environmental readings from the BME280
  By: Nathan Seidle
  SparkFun Electronics
  Date: March 9th, 2018
  License: This code is public domain but you buy me a beer if you use this and we meet someday (Beerware license).

  Feel like supporting our work? Buy a board from SparkFun!
  https://www.sparkfun.com/products/14348 - Qwiic Combo Board
  https://www.sparkfun.com/products/13676 - BME280 Breakout Board
  
  This example shows how to read humidity, pressure, and current temperature from the BME280 over I2C.

  Hardware connections:
  BME280 -> Arduino
  GND -> GND
  3.3 -> 3.3
  SDA -> A4
  SCL -> A5
*/

#include <Wire.h>

#include "SparkFunBME280.h"
BME280 mySensor;

void setup()
{
  Serial.begin(115200);
  Serial.println("Reading basic values from BME280");

  Wire.begin(D1, D2);
  Wire.setClock(100000); // 400kHz speed

  mySensor.settings.commInterface   = I2C_MODE;
  mySensor.settings.I2CAddress      = 0x76;
  mySensor.settings.runMode         = MODE_NORMAL;
  mySensor.settings.tStandby        = 5;
  mySensor.settings.filter          = 4;
  mySensor.settings.tempOverSample  = 5; 
  mySensor.settings.pressOverSample = 5;
  mySensor.settings.humidOverSample = 5;

  if (mySensor.beginI2C(Wire) == false) //Begin communication over I2C
  {
    Serial.println("The sensor did not respond. Please check wiring.");
    while(1); //Freeze
  }
}

void loop()
{
  long startTime = millis();
  while(mySensor.isMeasuring() == false) ; //Wait for sensor to start measurment
  while(mySensor.isMeasuring() == true) ; //Hang out while sensor completes the reading    
  long endTime = millis();
  Serial.print(" Measure time(ms): ");
  Serial.print(endTime - startTime);

  Serial.print("Humidity: ");
  Serial.print(mySensor.readFloatHumidity(), 1);
  Serial.print(",");

  Serial.print(" Pressure: ");
  Serial.print(mySensor.readFloatPressure()/100.0, 2);
  Serial.print(",");

  //Serial.print(" Alt: ");
  //Serial.print(mySensor.readFloatAltitudeMeters(), 1);
  //Serial.print(mySensor.readFloatAltitudeFeet(), 1);

  Serial.print(" Temp: ");
  Serial.print(mySensor.readTempC(), 2);
  //Serial.print(mySensor.readTempF(), 2);

  Serial.println();

  // delay(50);
}
