//////////////////////////////////////////////////////////////////////////////////////////////////////////
// Air Quality Sensor
//////////////////////////////////////////////////////////////////////////////////////////////////////////
// Urs Utzinger
// Winter 2020, fixing the i2c issues
//////////////////////////////////////////////////////////////////////////////////////////////////////////

#define BAUDRATE 115200          // serial communicaiton speed, terminal settings need to match

// Create 3 i2c interfaces:
#include <Wire.h>
TwoWire myWire = TwoWire();

bool scd30_avail = false;                   // do we have this sensor?
bool sps30_avail = false;                   // do we have this sensor?
bool sgp30_avail  = false;                  // do we have this sensor
bool ccs811_avail = false;                  // do we have this sensor?
bool bme680_avail = false;                  // do we hace the sensor?
bool bme280_avail = false;                  // do we hace the sensor?
bool therm_avail    = false;                // do we hav e the sensor?
bool max_avail    = false;                  // do we have sensor?
bool lcd_avail = false;                     // is LCD attached?

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////
void setup() {
//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////
  
  Serial.begin(BAUDRATE);
  Serial.println(F(""));;

  // I2C_1 D1 & D2
  myWire.setClock(100000); // 100kHz or 400kHz speed
  myWire.setClockStretchLimit(200000);
  myWire.begin(D1, D2);
  
  /******************************************************************************************************/
  // Check which devices are attached to the three I2C buses
  /******************************************************************************************************/
  if (checkI2C(0x20, &myWire) == true) {lcd_avail    = true; } // LCD display
  if (checkI2C(0x57, &myWire) == true) {max_avail    = true; } // MAX 30105 Pulseox & Particle
  if (checkI2C(0x58, &myWire) == true) {sgp30_avail  = true; } // Senserion TVOC eCO2
  if (checkI2C(0x5A, &myWire) == true) {therm_avail  = true; } // MLX IR sensor
  if (checkI2C(0x5B, &myWire) == true) {ccs811_avail = true; } // Airquality CO2 tVOC
  if (checkI2C(0x61, &myWire) == true) {scd30_avail  = true; } // Senserion CO2
  if (checkI2C(0x69, &myWire) == true) {sps30_avail  = true; } // Senserion Particle
  if (checkI2C(0x76, &myWire) == true) {bme280_avail = true; } // Bosch Temp, Humidity, Pressure
  if (checkI2C(0x77, &myWire) == true) {bme680_avail = true; } // Bosch Temp, Humidity, Pressure, VOC

  //I2C_2 D3 & D4
  myWire.begin(D3, D4);

  if (checkI2C(0x20, &myWire) == true) {lcd_avail    = true; } // LCD display
  if (checkI2C(0x57, &myWire) == true) {max_avail    = true; } // MAX 30105 Pulseox & Particle
  if (checkI2C(0x58, &myWire) == true) {sgp30_avail  = true; } // Senserion TVOC eCO2
  if (checkI2C(0x5A, &myWire) == true) {therm_avail  = true; } // MLX IR sensor
  if (checkI2C(0x5B, &myWire) == true) {ccs811_avail = true; } // Airquality CO2 tVOC
  if (checkI2C(0x61, &myWire) == true) {scd30_avail  = true; } // Senserion CO2
  if (checkI2C(0x69, &myWire) == true) {sps30_avail  = true; } // Senserion Particle
  if (checkI2C(0x76, &myWire) == true) {bme280_avail = true; } // Bosch Temp, Humidity, Pressure
  if (checkI2C(0x77, &myWire) == true) {bme680_avail = true; } // Bosch Temp, Humidity, Pressure, VOC

  // I2C_3 D5 & D6
  myWire.begin(D5, D6);

  if (checkI2C(0x20, &myWire) == true) {lcd_avail    = true; } // LCD display
  if (checkI2C(0x57, &myWire) == true) {max_avail    = true; } // MAX 30105 Pulseox & Particle
  if (checkI2C(0x58, &myWire) == true) {sgp30_avail  = true; } // Senserion TVOC eCO2
  if (checkI2C(0x5A, &myWire) == true) {therm_avail  = true; } // MLX IR sensor
  if (checkI2C(0x5B, &myWire) == true) {ccs811_avail = true; } // Airquality CO2 tVOC
  if (checkI2C(0x61, &myWire) == true) {scd30_avail  = true; } // Senserion CO2
  if (checkI2C(0x69, &myWire) == true) {sps30_avail  = true; } // Senserion Particle
  if (checkI2C(0x76, &myWire) == true) {bme280_avail = true; } // Bosch Temp, Humidity, Pressure
  if (checkI2C(0x77, &myWire) == true) {bme680_avail = true; } // Bosch Temp, Humidity, Pressure, VOC

  Serial.print(F("LCD                  ")); if (lcd_avail)    {Serial.println(F("available ")); } else {Serial.println(F("not available"));}
  Serial.print(F("MAX30105             ")); if (max_avail)    {Serial.println(F("available ")); } else {Serial.println(F("not available"));}
  Serial.print(F("CCS811 eCO2, tVOC    ")); if (ccs811_avail) {Serial.println(F("available ")); } else {Serial.println(F("not available"));}
  Serial.print(F("SGP30 eCO2, tVOC     ")); if (sgp30_avail)  {Serial.println(F("available ")); } else {Serial.println(F("not available"));}
  Serial.print(F("MLX temp             ")); if (therm_avail)  {Serial.println(F("available ")); } else {Serial.println(F("not available"));}
  Serial.print(F("SCD30 CO2, rH        ")); if (scd30_avail)  {Serial.println(F("available ")); } else {Serial.println(F("not available"));}
  Serial.print(F("SPS30 PM             ")); if (sps30_avail)  {Serial.println(F("available ")); } else {Serial.println(F("not available"));}
  Serial.print(F("BM[E/P]280 T, P[, rH]")); if (bme280_avail) {Serial.println(F("available ")); } else {Serial.println(F("not available"));}
  Serial.print(F("BME680 T, rH, P tVOC ")); if (bme680_avail) {Serial.println(F("available ")); } else {Serial.println(F("not available"));}

} // end setup

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////
void loop() {
//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////
  
  delay(1000); 
  long startTime = micros();
  // 10 micro seconds
  myWire.begin(D1, D2); 
  long endTime = micros();
  Serial.println(endTime-startTime);
  startTime = micros();
  // 15 micro seconds
  myWire.begin(D1, D2);
  myWire.setClock(100000); 
  myWire.setClockStretchLimit(200000);
  endTime = micros();
  Serial.println(endTime-startTime);
    
} // loop

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////
// Support functions
//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////

bool checkI2C(byte address, TwoWire *myWire) {
  myWire->beginTransmission(address);
  if (myWire->endTransmission() == 0) { return true; } else { return false; }
}
