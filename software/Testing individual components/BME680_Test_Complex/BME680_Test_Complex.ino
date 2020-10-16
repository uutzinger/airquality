//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////
// Air Quality Sensor
//
// An ESP8266 board is the base for gas and air quality related sensors
//
// Requirements:
//  LCD 20x4
//  BME680 Temp, Humidity, Pressure, VOC
// 
// Operation:
//
// Code Modifications:
// 
// Urs Utzinger
// Summer 2020
//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////

bool fastMode = true;              // true: Measure as fast as possible, false: operate in energy efficiency mode
unsigned long currentTime;
unsigned long lastTime;            // last time we updated runtime;
unsigned long intervalLoop = 100;  // 10Hz

/******************************************************************************************************/
// Store Sensor Baseline Data
/******************************************************************************************************/
#include <EEPROM.h>
#define EEPROM_SIZE 1024
int eeprom_address = 0;
struct EEPROMsettings {
  unsigned long runTime;
  byte          baselineSGP30_valid;
  uint16_t      baselineeCO2_SGP30;
  uint16_t      baselinetVOC_SGP30;
  byte          baslineCCS811_valid;
  uint16_t      baseline_CCS811;
  byte          tempOffset_SCD30_valid;
  float         tempOffset_SCD30;
  byte          forcedCalibration_SCD30_valid;
  float         forcedCalibration_SCD30;
  char          ssid1[33];  // 32 bytes max
  char          pw1[65];    // 64 chars max
  char          ssid2[33];
  char          pw2[65];
  char          ssid3[33];
  char          pw3[65];
  char          mqtt_server[255];
};

unsigned long lastEEPROM;    // last time we updated EEPROM, should occur every couple days
EEPROMsettings mySettings;

/******************************************************************************************************/
// Sensor
/******************************************************************************************************/

#include <Wire.h>

enum SensorStates{IS_IDLE = 0, IS_MEASURING, IS_BUSY, DATA_AVAILABLE, IS_SLEEPING, IS_WAKINGUP, 
                  WAIT_STABLE, UPDATE_DISPLAY, CHECK_EEPROM, GET_BASELINE, WRITE_EEPROM, HAS_ERROR};
// IS_IDLE        the sensor is powered up
// IS_MEASURING   the sensor is creating data autonomously
// IS_BUSY        the sensor is producing data and will not respond to commands
// DATA_AVAILABLE new data is available in sensor registers
// IS_SLEEPING    the sensor or parts of the sensot are in sleep mode
// IS_WAKINGUP    the sensor is getting out of sleep mode
// UPDATE_DISPLAY data was retrieved, now its time to update the display
// WAIT_STABLE    
// CHECK_EEPROM   if sensor is stable, initiate baseline retrival
// GET_BASELINE   read the baseline correction in sensor
// WRTIE_EEPROM   write baseline to EEPROM
// HAS_ERROR      the communication with the sensor failed

/******************************************************************************************************/
// LCD Display
/******************************************************************************************************/
// Power Consumption: ... have not measured
// 
#include <LiquidCrystal_I2C.h>
const unsigned long intervalLCD = 2000;            // LCD refresh rate
bool lcd_avail = false;                            // is LCD attached?
unsigned long lastLCD;                             // last time LCD was modified
LiquidCrystal_I2C lcd(0x27,20,4);                  // set the LCD address to 0x27 for a 20 chars and 4 line display

/******************************************************************************************************/
/* LCD Display Layout 20x4
/******************************************************************************************************/
// Particle
#define PM1_X              0
#define PM1_Y              0
#define PM2_X              0
#define PM2_Y              1
#define PM4_X              0
#define PM4_Y              2
#define PM10_X             0
#define PM10_Y             3

#define PM2_WARNING_X      3
#define PM2_WARNING_Y      1
#define PM10_WARNING_X     3
#define PM10_WARNING_Y     3

//CO2 & Pressure
#define CO2_X              4
#define CO2_Y              0
#define eCO2_X             4
#define eCO2_Y             1
#define eeCO2_X            4
#define eeCO2_Y            2
#define PRESSURE_X         4
#define PRESSURE_Y         3

#define CO2_WARNING_X      8
#define CO2_WARNING_Y      0
#define eCO2_WARNING_X     8
#define eCO2_WARNING_Y     1
#define eeCO2_WARNING_X    8
#define eeCO2_WARNING_Y    2
#define PRESSURE_WARNING_X 8
#define PRESSURE_WARNING_Y 3

// Humidity and IAQ
#define HUM1_X             9
#define HUM1_Y             0
#define HUM2_X             9
#define HUM2_Y             1
#define HUM3_X             9
#define HUM3_Y             2
#
#define IAQ_X              9
#define IAQ_Y              3

#define IAQ_WARNING_X     14
#define IAQ_WARNING_Y      3

// Temperatures and VOCs
#define TEMP1_X           14
#define TEMP1_Y            0
#define TEMP2_X           14
#define TEMP2_Y            1
#define TEMP3_X           14
#define TEMP3_Y            2
#
#define TVOC_X            15
#define TVOC_Y             3
#define TTVOC_X           15
#define TTVOC_Y            2

#define TEMP3_WARNING_X   19
#define TEMP3_WARNING_Y    2
#define TVOC_WARNING_X    19
#define TVOC_WARNING_Y     3
#define TTVOC_WARNING_X   19
#define TTVOC_WARNING_Y    2

/******************************************************************************************************/
// BME680; Bosch Temp, Humidity, Pressure, VOC
/******************************************************************************************************/
#include <Adafruit_Sensor.h>
#include "Adafruit_BME680.h"
bool bme680_avail = false;
const uint8_t  bme680_TempOversample     = BME680_OS_8X;         // 1,2,4,8,16x
const uint8_t  bme680_HumOversample      = BME680_OS_2X;         // 1,2,4,8,16x
const uint8_t  bme680_PressureOversample = BME680_OS_4X;         // 1,2,4,8,16x
const uint8_t  bme680_FilterSize         = BME680_FILTER_SIZE_3; // 0,1,3,7,15,31,63,127 
const uint16_t bme680_HeaterTemp         = 320;                  // C
const uint16_t bme680_HeaterDuration     = 150;                  // ms
float          bme680_pressure = 0;
float          bme680_temperature = 0;                           // [C]
float          bme680_humidity;                                  // [%]
uint32_t       bme680_gas = 0;                                   // [Ohm]
float          bme680_ah = 0;                                    // [gr/m^3]
#define intervalBME680_Fast 1000                                 // 1sec
#define intervalBME680_Slow 60000                                // 60sec
unsigned long  intervalBME680;
unsigned long  lastBME680;                                       // last time we interacted with sensor
unsigned long  endTimeBME680;                                    // when data will be available
volatile SensorStates stateBME680 = IS_IDLE; 
Adafruit_BME680 bme680;
// Continous 1Hz             1s
// Low Power 0.33 Hz         3s
// Ultra Low Power 0.0033 Hz 300s

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////
// Support functions
//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////

uint8_t checkI2C(byte address) {
  Wire.beginTransmission(address);
  if (Wire.endTransmission() == 0) { return 1; } else { return 0; }
}


void helpMenu() 
{
  Serial.println("SGP30=eCO2=========================");
  Serial.println("e: eCO2, e400");
  Serial.println("t: tVOC, t3000");  
  Serial.println("SCD30=CO2==========================");
  Serial.println("f: force calibration, f400.0 in ppm");
  Serial.println("t: set temp offset, t5.0 in C");
  Serial.println("CCS811=eCO2========================");
  Serial.println("c: basline, c400");
  Serial.println("EEPROM=============================");
  Serial.println("s: save to EEPROM");
  Serial.println("r: read from EEPROM");
  Serial.println("p: print current settings");
  Serial.println("d: create default settings");
  Serial.println("=====================Urs Utzinger=");
}

void inputHandle()
{
  char inBuff[] = "0123456789ABCDEF";
  uint16_t tmpI;
  float tmpF;
  unsigned long tmpTime;
  int bytesread;
  String value = "20000.0";
  String command = " ";
  
  if (Serial.available()) {
    Serial.setTimeout(1 ); // Serial read timeout
    bytesread=Serial.readBytesUntil('\n', inBuff, 17);               // Read from serial until CR is read or timeout exceeded
    inBuff[bytesread]='\0';
    String instruction = String(inBuff);
    command = instruction.substring(0,1); 
    if (bytesread > 1) { // we have also a value
      value = instruction.substring(1,bytesread); 
    }

    
    if (command == "e") {                                            // forced CO2 set setpoint
      tmpI = value.toInt();
      if ((tmpI > 400) && (tmpI < 2000)) {
        // sgp30.xxx((uint16_t)tmpI);
        mySettings.baselineSGP30_valid = 0xF0;
        mySettings.baselineeCO2_SGP30 = (uint16_t)tmpI;
        Serial.print("Calibration point is:  ");
        Serial.println(tmpI);
      } else { Serial.println("Calibration point out of valid Range"); }
    } 

    else if (command == "t") {                                       // forced tVOC set setpoint
      tmpI = value.toInt();
      if ((tmpI > 400) && (tmpI < 2000)) {
        // sgp30.xxx((uint16_t)tmpI);
        mySettings.baselineSGP30_valid = 0xF0;
        mySettings.baselinetVOC_SGP30 = (uint16_t)tmpI;
        Serial.print("Calibration point is:  ");
        Serial.println(tmpI);
      } else { Serial.println("Calibration point out of valid Range"); }
    } 

    else if (command == "c") {                                       // forced tVOC set setpoint
      tmpI = value.toInt();
      if ((tmpI > 400) && (tmpI < 2000)) {
        // sgp30.xxx((uint16_t)tmpI);
        mySettings.baslineCCS811_valid = 0xF0;
        mySettings.baseline_CCS811 = (uint16_t)tmpI;
        Serial.print("Calibration point is:  ");
        Serial.println(tmpI);
      } else { Serial.println("Calibration point out of valid Range"); }
    } 

    else if (command == "f") {                                       // forced CO2 set setpoint
      tmpI = value.toInt();
      if ((tmpI > 400) && (tmpI < 2000)) {
        //scd30.setForcedRecalibrationFactor((uint16_t)tmpI);
        mySettings.forcedCalibration_SCD30_valid = 0xF0;
        mySettings.forcedCalibration_SCD30 = (uint16_t)tmpI;
        Serial.print("Calibration point is:  ");
        Serial.println(tmpI);
      } else { Serial.println("Calibration point out of valid Range"); }
    } 
    
    else if (command == "t") {                                       // forced CO2 set setpoint
      tmpF = value.toFloat();
      if ((tmpF > 0.0) && (tmpF < 20.0)) {
        //scd30.setTemperatureOffset(tmpF);
        mySettings.tempOffset_SCD30_valid = 0xF0;
        //mySettings.tempOffset_SCD30 = scd30.getTemperatureOffset();
        Serial.print("Temperature offset set to:  ");
        Serial.println(mySettings.tempOffset_SCD30);
      } else { Serial.println("Offset point out of valid Range"); }
    } 

    else if (command == "s") {                                  // save EEPROM
      tmpTime=millis();
      EEPROM.put(0,mySettings);
      if (EEPROM.commit()) {
        Serial.print("EEPROM saved in: ");
        Serial.print(millis()-tmpTime);
        Serial.println("ms");
      }
    }

    else if (command == "r") {                                  // read EEPROM
      tmpTime=millis();
      EEPROM.get(0,mySettings);
      Serial.print("EEPROM read in: ");
      Serial.print(millis()-tmpTime);
      Serial.println("ms");
      printSettings();
    }

    else if (command == "p") {                                  // display settings
      printSettings();
    } 

    else if (command == "d") {                                  // default settings
      defaultSettings();
      printSettings();
    }
     
    else {                                                     // unrecognized command
      helpMenu();
    } // end if

  } // end serial available
} // end Input

void updateLCD() {
  char lcdbuf[16];
  lcd.clear(); // 2ms
  if (bme680_avail == true) {
    
    sprintf(lcdbuf,"%4d",(int)(bme680_pressure/100.0));
    lcd.setCursor(PRESSURE_X, PRESSURE_Y);
    lcd.print(lcdbuf);
  
    sprintf(lcdbuf,"%4.1f%%",bme680_humidity);
    lcd.setCursor(HUM2_X, HUM2_Y);
    lcd.print(lcdbuf);
  
    sprintf(lcdbuf,"%4.1fg",bme680_ah);
    lcd.setCursor(HUM3_X, HUM3_Y);
    lcd.print(lcdbuf);

    if ((bme680_humidity >= 45) && (bme680_humidity <= 55)) {
      sprintf(lcdbuf, "%s", "N");
    } else if ((bme680_humidity >= 30) && (bme680_humidity < 45)) {
      sprintf(lcdbuf, "%s", "T");
    } else if ((bme680_humidity >= 55) && (bme680_humidity < 60)) {
      sprintf(lcdbuf, "%s", "T");
    } else if ((bme680_humidity >= 60) && (bme680_humidity < 80)) {
      sprintf(lcdbuf, "%s", "H");
    } else if ((bme680_humidity >  15) && (bme680_humidity < 30)) {
      sprintf(lcdbuf, "%s", "L");
    } else {
      sprintf(lcdbuf, "%s", "!");
    }
    //lcd.setCursor(IAQ_WARNING_X, IAQ_WARNING_Y);
    //lcd.print(lcdbuf);
  
    sprintf(lcdbuf,"%+5.1fC",bme680_temperature);
    lcd.setCursor(TEMP2_X, TEMP2_Y);
    lcd.print(lcdbuf);
  
    sprintf(lcdbuf,"%4.1f",(float(bme680_gas)/1000.0));
    lcd.setCursor(IAQ_X, IAQ_Y);
    lcd.print(lcdbuf);
  
    if (bme680_gas < 5000) {
      sprintf(lcdbuf, "%s", "N");
    } else if (bme680_gas < 10000) {
      sprintf(lcdbuf, "%s", "T");
    } else if (bme680_gas < 300000) {
      sprintf(lcdbuf, "%s", "P");
    } else {
      sprintf(lcdbuf, "%s", "!");
    }
    lcd.setCursor(IAQ_WARNING_X, IAQ_WARNING_Y);
    lcd.print(lcdbuf);
  } // end if avail
} // end update LCD

void printSettings() {
  Serial.print("Runtime [min]: ................ "); Serial.println((unsigned long)mySettings.runTime/60);
  Serial.print("Base SGP30 valid: ............. "); Serial.println((int)mySettings.baselineSGP30_valid);
  Serial.print("Base eCO2 SGP30: .............. "); Serial.println((int)mySettings.baselineeCO2_SGP30);
  Serial.print("Base tVOC SGP30: .............. "); Serial.println((int)mySettings.baselinetVOC_SGP30);
  Serial.print("Base CCS811 valid: ............ "); Serial.println((int)mySettings.baslineCCS811_valid);
  Serial.print("Base CCS811: .................. "); Serial.println((int)mySettings.baseline_CCS811);
  Serial.print("Temp Offset SCD30 valid: ...... "); Serial.println((int)mySettings.tempOffset_SCD30_valid);
  Serial.print("Temp Offset SCD30: ............ "); Serial.println((float)mySettings.tempOffset_SCD30);
  Serial.print("Forced Calibration SCD30 valid: "); Serial.println((int)mySettings.forcedCalibration_SCD30_valid);
  Serial.print("Forced Calibration SCD30: ..... "); Serial.println((float)mySettings.forcedCalibration_SCD30);
  Serial.print("SSID: ......................... "); Serial.println(mySettings.ssid1);
  Serial.print("PW: ........................... "); Serial.println(mySettings.pw1);
  Serial.print("SSID: ......................... "); Serial.println(mySettings.ssid2);
  Serial.print("PW: ........................... "); Serial.println(mySettings.pw2);
  Serial.print("SSID: ......................... "); Serial.println(mySettings.ssid3);
  Serial.print("PW: ........................... "); Serial.println(mySettings.pw3);
  Serial.print("MQTT: ......................... "); Serial.println(mySettings.mqtt_server);
}

void defaultSettings() {
  mySettings.runTime                       = (unsigned long)   0;
  mySettings.baselineSGP30_valid           = (byte)         0x00;
  mySettings.baselineeCO2_SGP30            = (uint16_t)        0;
  mySettings.baselinetVOC_SGP30            = (uint16_t)        0;
  mySettings.baslineCCS811_valid           = (byte)         0x00;
  mySettings.baseline_CCS811               = (uint16_t)        0;
  mySettings.tempOffset_SCD30_valid        = (byte)         0x00;
  mySettings.tempOffset_SCD30              = (float)        0.01;
  mySettings.forcedCalibration_SCD30_valid = (byte)         0x00;
  mySettings.forcedCalibration_SCD30       = (float)         0.0;
  strcpy(mySettings.ssid1,                 "meddev");
  strcpy(mySettings.pw1,                   "w1ldc8ts");
  strcpy(mySettings.ssid2,                 "AMARIE-HP_Network");
  strcpy(mySettings.pw2,                   "catfudcasserol");
  strcpy(mySettings.ssid3,                 "MuchoCoolioG");
  strcpy(mySettings.pw3,                   "fluorolog");
  strcpy(mySettings.mqtt_server,           "my.mqqtt.server.org");
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////
void setup() {
//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////

  Serial.begin(115200);
  Serial.println();
  Wire.begin();

  /******************************************************************************************************/
  // EEPROM setup and read 
  /******************************************************************************************************/  
  unsigned long tmpTime = millis();
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(0,mySettings);
  Serial.print("EEPROM read in: ");
  Serial.print(millis()-tmpTime);
  Serial.println("ms");
  
  /******************************************************************************************************/
  // Check which devices are attached to I2C bus
  /******************************************************************************************************/
  if (checkI2C(0x27) == 1) {lcd_avail = true;}    else {lcd_avail = false;}     // LCD display
  if (checkI2C(0x77) == 1) {bme680_avail = true;} else {bme680_avail = false;}  // Bosch Temp, Humidity, Pressure, VOC

  /******************************************************************************************************/
  // Initialize LCD Screen
  /******************************************************************************************************/
  if (lcd_avail == true) {
    lcd.begin();
    lcd.backlight();
    lcd.setCursor(0, 0);
    Serial.println("LCD Initialized");
  }

  /******************************************************************************************************/
  // Initialize BME680
  /******************************************************************************************************/
  if (bme680_avail == true){
    if (bme680.begin() == true) { 
      Serial.print(F("- Setting oversampling for sensors\n"));
      bme680.setTemperatureOversampling(bme680_TempOversample);
      bme680.setHumidityOversampling(bme680_HumOversample); 
      bme680.setPressureOversampling(bme680_PressureOversample); 
      Serial.print(F("- Setting IIR filter to a value of 3 samples\n"));
      bme680.setIIRFilterSize(bme680_FilterSize); 
      Serial.print(F("- Setting gas measurement to 320\xC2\xB0\x43 for 150ms\n")); // "°C" symbols
      bme680.setGasHeater(bme680_HeaterTemp,bme680_HeaterDuration); 
      stateBME680 = IS_IDLE;      
      Serial.println("BME680 Initialized");
    } else {
      Serial.println("BME680 sensor not detected. Please check wiring."); 
      stateBME680 = HAS_ERROR;
      bme680_avail = false;
    }   
  }

  /******************************************************************************************************/
  // Initialize Timing System
  /******************************************************************************************************/
  currentTime       = millis();
  lastTime          = currentTime;
  lastLCD           = currentTime;
  lastBME680        = currentTime;
  lastEEPROM        = currentTime;

  Serial.println("System Initialized");

} // end setup

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////
void loop() {
//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////
  currentTime = millis();

  /******************************************************************************************************/
  // LCD update display
  /******************************************************************************************************/
  if (lcd_avail == true) {
    if ((currentTime - lastLCD) >= intervalLCD) {
      unsigned long tmpTime = millis();
      updateLCD();
      lastLCD = currentTime;
      Serial.print("LCD updated in ");
      Serial.print((millis()-tmpTime));  
      Serial.println("ms");
    }
  }

  /******************************************************************************************************/
  // BME680, Hum, Press, Temp, Gasresistance
  /******************************************************************************************************/

  if (bme680_avail == true) {
    switch(stateBME680) { 
      
      case IS_IDLE : {
        if ((currentTime - lastBME680) >= intervalBME680) {
          unsigned long tmpTime = millis();
          endTimeBME680 = bme680.beginReading();
          lastBME680 = currentTime;
          if (endTimeBME680 == 0) { 
            Serial.println(F("Failed to begin reading :("));
            stateBME680 = HAS_ERROR; 
          } else {
            stateBME680 = IS_BUSY; 
          }
          Serial.print("BME680 reading started. Completes in ");
          Serial.print(endTimeBME680-tmpTime);
          Serial.println("ms");
        }
      }
      
      case IS_BUSY : {
        if (currentTime > endTimeBME680) {
          stateBME680 = DATA_AVAILABLE;
        }
      }

      case DATA_AVAILABLE : {
        if (!bme680.endReading()) {
          Serial.println(F("Failed to complete reading :("));        
        } else {
          bme680_temperature = bme680.temperature;
          bme680_pressure =    bme680.pressure;
          bme680_humidity =    bme680.humidity;
          bme680_gas =         bme680.gas_resistance;
          Serial.print("BME680 Temperature:");
          Serial.print(bme680_temperature);
          Serial.println("degC");
          Serial.print("BME680 Pressure:");
          Serial.print(bme680_pressure);
          Serial.println("Pa");
          Serial.print("BME680 Humidity:");
          Serial.print(bme680_humidity);
          Serial.println("%");
          Serial.print("BME680 Gas Resistance:");
          Serial.print(bme680_gas);
          Serial.println("Ohm");
          //
          // Absolute Humidity
          // https://www.eoas.ubc.ca/books/Practical_Meteorology/prmet102/Ch04-watervapor-v102b.pdf
          //
          // 1) Input: Temperature -> Water Saturtion Vapor Pressure
          // 2) Input: Relative Humidity, Saturation Vapor Pressure -> Mass/Volume Ratio
          // 
          // T [K]
          // T0 = 273.15 [K]
          // Rv = R*/Mv = 461.5 J K-1 kg-1
          // L= 2.83*10^6 J/kg
          // Saturation Vapor Pressure: es = 611.3 exp (L/Rv *(1/T0 - 1/T))) [P]
          // Saturation Vapor Pressure: es = 611.3 exp (5423 [K] *(1/273.15[K] - 1/T[K]))) [P]
          // Relative Humidity = RH [%] = e/es * 100
          // Absolute Humidity = mass/volume = rho_v 
          // = e / (Rv * T)
          // = RH / 100 * 611.3 / (461.5 * T) * exp(5423 (1/273.15 - 1/T)) [kgm^-3]
          float tmp = 273.15 + bme680_temperature;
          bme680_ah = bme680_humidity * 13.246 / tmp * exp(19.854 - 5423.0/tmp); // [gr/m^3]
          if ( (bme680_ah<0) | (bme680_ah>40.0) ) {bme680_ah = -1.0;} // make sure its reasonable

          // DewPoint
          // https://en.wikipedia.org/wiki/Dew_point
          // T Celsius
          // a = 6.1121 mbar, b = 18.678, c = 257.14 °C, d = 234.5 °C.
          // Tdp = c g_m / (b - g_m)
          // g_m = ln(RH/100) + (b-T/d)*(T/(c+T)))

          stateBME680 = IS_IDLE;
          Serial.println("BME680 readout completed");
        }
      }
                            
      case HAS_ERROR : {
        // What are we going to do about that?
        break; 
      }
     
    } // end cases
  } // end if available

  
  /******************************************************************************************************/
  // Serial input handling
  /******************************************************************************************************/
  inputHandle();

  /******************************************************************************************************/
  // DEBUG
  /******************************************************************************************************/
  /* 
  int pinStat = digitalRead(SCD30_RDY);
  if (lastPinStat != pinStat) {
    Serial.print("SCD30 RDY pin changed: ");
    Serial.print(lastPinStat);
    Serial.print(" - ");
    Serial.println(pinStat);
    lastPinStat = pinStat;
  }
  */
 
  /******************************************************************************************************/
  // Time Management
  /******************************************************************************************************/
  // Update runtime every minute ***********************************************
  if ((currentTime - lastTime) >= 60000) {
    mySettings.runTime = mySettings.runTime + ((currentTime - lastTime) / 1000);
    lastTime = currentTime;
    Serial.println("Runtime updated");
  }
  // Update EEPROM once a week *************************************************
  if ((currentTime - lastEEPROM) >= 604800000) {
    EEPROM.put(0,mySettings);
    if (EEPROM.commit()) {    
      lastEEPROM = currentTime;
      Serial.println("EEPROM updated");
    }
  } 
  //  Put sensor to sleep *****************************************************
  long sleepDuration = long(millis()) - long(currentTime + intervalLoop);
  if (sleepDuration > 0) {
   delay(sleepDuration);
  }
  
} // loop
