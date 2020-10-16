/****************************************************************************** 
MLX90614_Set_Emissivity
How to read and set the MLX90614's emissivity coefficient

Emissivity is a relative measure of how efficiently an object
radiates heat. A perfect thermal emitter has a emissivity of
1.0, but most real-world objects range from 0 to 1.

For example, human skin has an emissivity of about 0.98.
Shiny metals have a very low emissivity, for example polished
aluminum has an emissivity of 0.05. Painted aluminum has an
emissivity of 0.45. Here's a good reference:
http://www.thermoworks.com/emissivity_table.html

If you get really wacky values after setting the emissivity,
try cycling power to the MLX90614.

To set the emissivity, change the value of the 
newEmissivity variable above setup(). Allowable values are
between 0.1 and 1.0.

Hardware Hookup (if you're not using the eval board):
MLX90614 ------------- Arduino
  VDD ------------------ 3.3V
  VSS ------------------ GND
  SDA ------------------ SDA (A4 on older boards)
  SCL ------------------ SCL (A5 on older boards)
  
An LED can be attached to pin 8 to monitor for any read errors.

Jim Lindblom @ SparkFun Electronics
October 23, 2015
https://github.com/sparkfun/SparkFun_MLX90614_Arduino_Library

Development environment specifics:
Arduino 1.6.5
SparkFun IR Thermometer Evaluation Board - MLX90614
******************************************************************************/

#include <Arduino.h>
#include <Wire.h> // I2C library, required for MLX90614

#include <SparkFunMLX90614.h> // SparkFunMLX90614 Arduino library

IRTherm therm; // Create an IRTherm object to interact with throughout

const byte LED = LED_BUILTIN; // Optional LED attached to pin 8 (active low)

float newEmissivity = 1.0;    // Skin is 0.98

void setup() 
{
  Wire.begin();

  Serial.begin(115200); // Initialize Serial to log output

  therm.begin(); // Initialize thermal IR sensor
  therm.setUnit(TEMP_F); // Set the library's units to Farenheit

  // Call setEmissivity() to configure the MLX90614's 
  // emissivity compensation:
  therm.setEmissivity(newEmissivity);
  delay(2000);   // Give time to startup

  // readEmissivity() can be called to read the device's
  // configured emissivity -- it'll return a value between
  // 0.1 and 1.0.
  Serial.println("Emissivity: " + String(therm.readEmissivity()));
  pinMode(LED, OUTPUT); // LED pin as output
  setLED(LOW); // LED OFF
}

void loop() 
{
  
  // Call therm.read() to read object and ambient temperatures from the sensor.
  if (therm.read()) // On success, read() will return 1, on fail 0.
  {
    setLED(HIGH); //LED on
    // Use the object() and ambient() functions to grab the object and ambient
    // temperatures.
    // They'll be floats, calculated out to the unit you set with setUnit().
    Serial.print(String(therm.object(), 2));
    Serial.print(','); // Degree Symbol
    Serial.print(String(therm.ambient(), 2));
    Serial.println();
    setLED(LOW);
  }
  delay(100); // output MLX valid usually after 0.25s
}

void setLED(bool on)
{
  if (on)
    digitalWrite(LED, LOW);
  else
    digitalWrite(LED, HIGH);
}
