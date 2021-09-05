/*
  Put the BME280 into low power mode (aka Forced Read)
  Nathan Seidle @ SparkFun Electronics
  March 23, 2018

  Feel like supporting our work? Buy a board from SparkFun!
  https://www.sparkfun.com/products/14348 - Qwiic Combo Board
  https://www.sparkfun.com/products/13676 - BME280 Breakout Board

  This example shows how used the 'Forced Mode' to obtain a reading then put the
  sensor to sleep between readings.
*/

#include <Wire.h>
TwoWire Wire_1 = TwoWire();

#include "SparkFunBME280.h"
BME280 mySensor;

void setup()
{
  Serial.begin(115200);
  Serial.println("Example showing alternate I2C addresses");

  Wire_1.begin(D5, D6);
  Wire_1.setClock(400000); //Increase to fast I2C speed!

  mySensor.settings.commInterface   = I2C_MODE;
  mySensor.settings.I2CAddress      = 0x76;
  mySensor.settings.runMode         = MODE_FORCED;
  mySensor.settings.tStandby        = 0;
  mySensor.settings.filter          = 4;
  mySensor.settings.tempOverSample  = 1; 
  mySensor.settings.pressOverSample = 1;
  mySensor.settings.humidOverSample = 1;


  uint8_t chipID=mySensor.beginI2C(Wire_1);
  Serial.println(chipID);
  if (chipID == 0x58) {
    Serial.println("Its a BMP");
  } else if (chipID == 0x60) {
    Serial.println("Its a BME");
  }

  mySensor.setMode(MODE_SLEEP); //Sleep for now
}

void loop()
{
  mySensor.setMode(MODE_FORCED); //Wake up sensor and take reading

  long startTime = millis();
  while(mySensor.isMeasuring() == false) ; //Wait for sensor to start measurment
  while(mySensor.isMeasuring() == true) ; //Hang out while sensor completes the reading    
  long endTime = millis();

  //Sensor is now back asleep but we get get the data

  Serial.print(" Measure time(ms): ");
  Serial.print(endTime - startTime);

  Serial.print(" Humidity: ");
  Serial.print(mySensor.readFloatHumidity(), 0);

  Serial.print(" Pressure: ");
  Serial.print(mySensor.readFloatPressure(), 0);

  //Serial.print(" Alt: ");
  //Serial.print(mySensor.readFloatAltitudeMeters(), 1);
  //Serial.print(mySensor.readFloatAltitudeFeet(), 1);

  Serial.print(" Temp: ");
  Serial.print(mySensor.readTempC(), 2);
  //Serial.print(mySensor.readTempF(), 2);

  Serial.println();

  delay(50);
}
