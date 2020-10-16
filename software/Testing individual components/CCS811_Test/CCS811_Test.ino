/******************************************************************************
  Wake from sleep and read interrupts

  Marshall Taylor @ SparkFun Electronics

  April 4, 2017

  https://github.com/sparkfun/CCS811_Air_Quality_Breakout
  https://github.com/sparkfun/SparkFun_CCS811_Arduino_Library

  This example configures the nWAKE and nINT pins.
  The interrupt pin is configured to pull low when the data is
  ready to be collected.
  The wake pin is configured to enable the sensor during I2C communications

  Hardware Connections (Breakoutboard to Arduino):
  3.3V to 3.3V pin
  GND to GND pin
  SDA to A4
  SCL to A5
  NOT_INT to D6
  NOT_WAKE to D5 (For 5V arduinos, use resistor divider)
    D5---
         |
         R1 = 4.7K
         |
         --------NOT_WAKE
         |
         R2 = 4.7K
         |
        GND

  Resources:
  Uses Wire.h for i2c operation

  Development environment specifics:
  Arduino IDE 1.8.1

  This code is released under the [MIT License](http://opensource.org/licenses/MIT).

  Please review the LICENSE.md file included with this example. If you have any questions
  or concerns with licensing, please contact techsupport@sparkfun.com.

  Distributed as-is; no warranty is given.
******************************************************************************/
#include <Wire.h>

#include <SparkFunCCS811.h> //Click here to get the library: http://librarymanager/All#SparkFun_CCS811

#define CCS811_ADDR 0x5B //Default I2C Address // or 0x5A 

#define CCS811_WAKE D6
#define CCS811_INT D7

CCS811 myCCS811(CCS811_ADDR);

//Global sensor object
//---------------------------------------------------------------
void setup()
{
  // Start the serial
  Serial.begin(115200);
  Serial.println();
  Serial.println("...");

  //Configure the wake line
  pinMode(CCS811_WAKE, OUTPUT);
  digitalWrite(CCS811_WAKE, LOW);
  pinMode(CCS811_INT, INPUT_PULLUP);
  delay(1000);
  
  Wire.begin();
  Wire.beginTransmission(0x5B);
  byte error = Wire.endTransmission();
  if (error == 0){ Serial.println("I2C device found at address 0x5B"); }
  Wire.beginTransmission(0x5A);
  error = Wire.endTransmission();
  if (error == 0){ Serial.println("I2C device found at address 0x5A"); }

  //This begins the CCS811 sensor and prints error status of .beginWithStatus()
  CCS811Core::CCS811_Status_e returnCode = myCCS811.beginWithStatus();
  Serial.print("CCS811 begin exited with: ");
  //Pass the error code to a function to print the results
  Serial.println(myCCS811.statusString(returnCode));

  // Mode 4 This sets the mode to 0.25 second reads, and prints returned error status.
  // Mode 3 This sets the mode to 60 second reads
  // Mode 2 This sets the mode to 10 second reads
  // Mode 1 This sets the mode to 1 second reads
  returnCode = myCCS811.setDriveMode(2);
  Serial.print("Mode request exited with: ");
  Serial.println(myCCS811.statusString(returnCode));

  //Configure and enable the interrupt line,
  //then print error status
  returnCode = myCCS811.enableInterrupts();
  Serial.print("Interrupt configuation exited with: ");
  Serial.println(myCCS811.statusString(returnCode));

}
//---------------------------------------------------------------
void loop()
{
  //Look for interrupt request from CCS811
  if (digitalRead(CCS811_INT) == LOW)
  {
    //Wake up the CCS811 logic engine
    digitalWrite(CCS811_WAKE, LOW);
    //Need to wait at least 50 us
    delay(1);
    //Interrupt signal caught, so cause the CCS811 to run its algorithm
    myCCS811.readAlgorithmResults(); //Calling this function updates the global tVOC and CO2 variables

    printSensorError();
    
    Serial.print("CO2[");
    Serial.print(myCCS811.getCO2());
    Serial.print("] tVOC[");
    Serial.print(myCCS811.getTVOC());
    Serial.print("] millis[");
    Serial.print(millis());
    Serial.print("]");
    Serial.println();

    //Now put the CCS811's logic engine to sleep
    digitalWrite(CCS811_WAKE, HIGH);
    //Need to be asleep for at least 20 us
    delay(1);
  }
  delay(1); //cycle kinda fast
}

//printSensorError gets, clears, then prints the errors
//saved within the error register.
void printSensorError()
{
  uint8_t error = myCCS811.getErrorRegister();

  if (error == 0xFF) //comm error
  {
    Serial.println("Failed to get ERROR_ID register.");
  }
  else
  {
    if (error & 1 << 5) Serial.print("Error: HeaterSupply");
    if (error & 1 << 4) Serial.print("Error: HeaterFault");
    if (error & 1 << 3) Serial.print("Error: MaxResistance");
    if (error & 1 << 2) Serial.print("Error: MeasModeInvalid");
    if (error & 1 << 1) Serial.print("Error: ReadRegInvalid");
    if (error & 1 << 0) Serial.print("Error: MsgInvalid");
  }
}
