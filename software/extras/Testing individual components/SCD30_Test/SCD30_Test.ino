/*
  Reading CO2, humidity and temperature from the SCD30
  By: Nathan Seidle
  SparkFun Electronics
  Date: May 22nd, 2018
  License: MIT. See license file for more information but you can
  basically do whatever you want with this code.

  Feel like supporting open source hardware?
  Buy a board from SparkFun! https://www.sparkfun.com/products/15112

  This example prints the current CO2 level, relative humidity, and temperature in C.

  Hardware Connections:
  Attach RedBoard to computer using a USB cable.
  Connect SCD30 to RedBoard using Qwiic cable.
  Open Serial Monitor at 115200 baud.
*/

#include <Wire.h>
TwoWire Wire_1 = TwoWire();

#include "SparkFun_SCD30_Arduino_Library.h" //Click here to get the library: http://librarymanager/All#SparkFun_SCD30
SCD30 airSensor;

void setup()
{
  Serial.begin(115200);
  Serial.println("");
  Serial.println("SCD30");
  Wire_1.begin(D4,D3);

  if (airSensor.begin(Wire_1,true) == false) //Wire interface and Autocalibrate is on
  {
    Serial.println("Air sensor not detected. Please check wiring. Freezing...");
    while (1)
      ;
  } else {
      airSensor.setMeasurementInterval(2); //Change number of seconds between measurements: 2 to 1800 seconds (30 minutes)
      airSensor.setAmbientPressure(1006); //Current ambient pressure in mBar: 700 to 1200  
      airSensor.setAltitudeCompensation(728); // Altitude in meters
      airSensor.setAutoSelfCalibration(true);
      float offset = airSensor.getTemperatureOffset();
      Serial.print("Current temp offset: ");
      Serial.print(offset, 2);
      Serial.println("C");
      // airSensor.setTemperatureOffset(5); //Optionally we can set temperature offset
  }

  //The SCD30 has data ready every two seconds
}

void loop()
{
  if (airSensor.dataAvailable())
  {
    Serial.print(airSensor.getCO2());
    Serial.print(",");
    Serial.print(airSensor.getTemperature(), 1); // make sure it fits on same plot
    Serial.print(",");
    Serial.print(airSensor.getHumidity(), 1); // make sure it fits on same plot
    Serial.println();
  }
  else
    // Serial.println("Waiting for new data");

  InputHandle();
  delay(500);

  
}


// ************************************************
// Handle serial input
// ************************************************
void InputHandle()
{
  char inBuff[ ] = "--------";
  uint16_t tmp;
  int bytesread;
  String value = "2000";
  String command = " ";
  
  if (Serial.available()) {
    Serial.setTimeout(1 ); // Serial read timeout
    bytesread=Serial.readBytesUntil('\n', inBuff, 9); // Read from serial until CR is read or timeout exceeded
    inBuff[bytesread]='\0';
    String instruction = String(inBuff);
    command = instruction.substring(0,1); 
    if (bytesread > 1) { // we have also a value
       value = instruction.substring(1,bytesread); 
    }

    if (command =="f") { // set setpoint
       tmp = value.toInt();
       if (((tmp > 400) & (tmp < 2000))) {
        airSensor.setForcedRecalibrationFactor(tmp);
       }
       else { 
         Serial.println("Calibration point out of valid Range"); 
       }
       Serial.print("Calibration point is:  ");
       Serial.println(tmp);
    } // end if 
  } // end serial available
} // end Input
