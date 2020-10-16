//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////
// Air Quality Sensor
//
// An ESP8266 board is the base for gas and air quality related sensors
//
// Requirements:
//  LCD 20x4
//  SCD30 Senserion CO2
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
#
#define IAQ_X              9
#define IAQ_Y              3

#define IAQ_WARNING_X     12
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
// SCD30; Sensirion CO2 sensor
/******************************************************************************************************/
// Does not have sleep mode, but can set measurement intervals
// 2sec interval requires 19mA average current
// The maximum current during reading is 75mA
// Sensor is initilized
//   Measurement interval is set
//   Autocalibrate is enabled
//   Temperature offset to compenste self heating, improves humidity reading
//   Pressure is set
// Begin Measureing
// Check if Data Available
// Get CO2, Humidity, Temperature
// Update barometric pressure every once in a while
// Repeat at check
//
#include "SparkFun_SCD30_Arduino_Library.h"
float scd30_ppm;                                         // co2 concentration from sensor
float scd30_temp;                                        // temperature from sensor
float scd30_hum;                                         // humidity from sensor
bool scd30_avail = false;                                // do we have this sensor
// int lastPinStat;
// measure every 2sec ... 30minutes, default is 4 
#define intervalSCD30Fast 2000                           
#define intervalSCD30Slow 60000                          
unsigned long intervalSCD30;                             // will be either fast or slow(powersaving)
const unsigned long intervalPressureSCD30 = 60000;       // if we have new pressure data from other sensor we will provide 
                                                         // it to the co2 sensor to improve accuracy in this interval
unsigned long lastSCD30;                                 // last time we interacted with sensor
unsigned long lastPressureSCD30;                         // last time we updated pressure
volatile SensorStates stateSCD30 = IS_IDLE;
#define SCD30_RDY D5                                     // pin indicating data ready                                       
SCD30 scd30;

bool bme680_avail = true;

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////
// Support functions
//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////

uint8_t checkI2C(byte address) {
  Wire.beginTransmission(address);
  if (Wire.endTransmission() == 0) { return 1; } else { return 0; }
}

void ICACHE_RAM_ATTR handleSCD30Interrupt() { 
  stateSCD30 = DATA_AVAILABLE;
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
  char inBuff[] = "--------";
  uint16_t tmpI;
  float tmpF;
  unsigned long tmpTime;
  int bytesread;
  String value = "2000";
  String command = " ";
  
  if (Serial.available()) {
    Serial.setTimeout(1 ); // Serial read timeout
    bytesread=Serial.readBytesUntil('\n', inBuff, 9);          // Read from serial until CR is read or timeout exceeded
    inBuff[bytesread]='\0';
    String instruction = String(inBuff);
    command = instruction.substring(0,1); 
    if (bytesread > 1) { // we have also a value
      value = instruction.substring(1,bytesread); 
    }

    
    if (command =="e") {                                       // forced CO2 set setpoint
      tmpI = value.toInt();
      if (((tmpI > 400) & (tmpI < 2000))) {
        // sgp30.xxx((uint16_t)tmpI);
        mySettings.baselineSGP30_valid = 0xF0;
        mySettings.baselineeCO2_SGP30 = (uint16_t)tmpI;
        Serial.print("Calibration point is:  ");
        Serial.println(tmpI);
      } else { Serial.println("Calibration point out of valid Range"); }
    } 

    else if (command =="t") {                                       // forced tVOC set setpoint
      tmpI = value.toInt();
      if (((tmpI > 400) & (tmpI < 2000))) {
        // sgp30.xxx((uint16_t)tmpI);
        mySettings.baselineSGP30_valid = 0xF0;
        mySettings.baselinetVOC_SGP30 = (uint16_t)tmpI;
        Serial.print("Calibration point is:  ");
        Serial.println(tmpI);
      } else { Serial.println("Calibration point out of valid Range"); }
    } 

    else if (command =="c") {                                       // forced tVOC set setpoint
      tmpI = value.toInt();
      if (((tmpI > 400) & (tmpI < 2000))) {
        // sgp30.xxx((uint16_t)tmpI);
        mySettings.baslineCCS811_valid = 0xF0;
        mySettings.baseline_CCS811 = (uint16_t)tmpI;
        Serial.print("Calibration point is:  ");
        Serial.println(tmpI);
      } else { Serial.println("Calibration point out of valid Range"); }
    } 

    else if (command =="f") {                                       // forced CO2 set setpoint
      tmpI = value.toInt();
      if (((tmpI > 400) & (tmpI < 2000))) {
        scd30.setForcedRecalibrationFactor((uint16_t)tmpI);
        mySettings.forcedCalibration_SCD30_valid = 0xF0;
        mySettings.forcedCalibration_SCD30 = (uint16_t)tmpI;
        Serial.print("Calibration point is:  ");
        Serial.println(tmpI);
      } else { Serial.println("Calibration point out of valid Range"); }
    } 
    
    else if (command =="t") {                                       // forced CO2 set setpoint
      tmpF = value.toFloat();
      if (((tmpF > 0.0) & (tmpF < 20.0))) {
        scd30.setTemperatureOffset(tmpF);
        mySettings.tempOffset_SCD30_valid = 0xF0;
        mySettings.tempOffset_SCD30 = scd30.getTemperatureOffset();
        Serial.print("Temperature offset set to:  ");
        Serial.println(mySettings.tempOffset_SCD30);
      } else { Serial.println("Offset point out of valid Range"); }
    } 
    else if (command =="s") {                                  // save EEPROM
      tmpTime=millis();
      EEPROM.put(0,mySettings);
      if (EEPROM.commit()) {
        Serial.print("EEPROM saved in: ");
        Serial.print(millis()-tmpTime);
        Serial.println("ms");
      }
    }
    else if (command =="r") {                                  // read EEPROM
      tmpTime=millis();
      EEPROM.get(0,mySettings);
      Serial.print("EEPROM read in: ");
      Serial.print(millis()-tmpTime);
      Serial.println("ms");
      printSettings();
    }
    else if (command =="p") {                                  // display settings
      printSettings();
    } 
    else if (command =="d") {                                  // default settings
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
  if (scd30_avail) {
    sprintf(lcdbuf, "%4d", int(scd30_ppm));
    lcd.setCursor(CO2_X, CO2_Y);
    lcd.print(lcdbuf);
    Serial.println(lcdbuf);

    sprintf(lcdbuf, "%4.1f%%", scd30_hum);
    lcd.setCursor(HUM1_X, HUM1_Y);
    lcd.print(lcdbuf);
    Serial.println(lcdbuf);
    
    sprintf(lcdbuf,"%+5.1fC",scd30_temp);
    lcd.setCursor(TEMP1_X, TEMP1_Y);
    lcd.print(lcdbuf);
    Serial.println(lcdbuf);

    if (scd30_ppm < 800) {
      sprintf(lcdbuf, "%s", "N");
    } else if (scd30_ppm < 1000) {
      sprintf(lcdbuf, "%s", "T");
    } else if (scd30_ppm < 5000) {
      sprintf(lcdbuf, "%s", "P");
    } else {
      sprintf(lcdbuf, "%s", "!");
    }
    lcd.setCursor(CO2_WARNING_X, CO2_WARNING_Y);
    lcd.print(lcdbuf);
  }
}

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
  // Set sensor configuration lines
  // D5 is used to read SCD30 CO2 data ready (RDY)
  /******************************************************************************************************/
  pinMode(SCD30_RDY, INPUT);      // interrupt scd30
  // lastPinStat = digitalRead(SCD30_RDY);

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
  if (checkI2C(0x61) == 1) {scd30_avail = true;}  else {scd30_avail = false;}   // Senserion CO2

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
  // Initialize SCD30 CO2 sensor 
  /******************************************************************************************************/
  if (scd30_avail == true) {
    Serial.print("SCD30 Interval: ");
    if (fastMode) { intervalSCD30 = intervalSCD30Fast; } 
    else          { intervalSCD30 = intervalSCD30Slow; }
    Serial.println(intervalSCD30);

    if (scd30.begin(Wire, true)) {
      scd30.setMeasurementInterval(uint16_t(intervalSCD30/1000));  // Change number of seconds between measurements: 2 to 1800 seconds (30 minutes)
      scd30.setAutoSelfCalibration(true);                // 
      mySettings.tempOffset_SCD30 = scd30.getTemperatureOffset();
      Serial.print("Current temp offset: ");
      Serial.print(mySettings.tempOffset_SCD30,2);
      Serial.println("C");
      if (mySettings.tempOffset_SCD30_valid == 0xF0) { 
        scd30.setTemperatureOffset(mySettings.tempOffset_SCD30);
      }
      mySettings.tempOffset_SCD30 = scd30.getTemperatureOffset();
      Serial.print("Current temp offset: ");
      Serial.print(mySettings.tempOffset_SCD30,2);
      Serial.println("C");
      attachInterrupt(digitalPinToInterrupt(SCD30_RDY), handleSCD30Interrupt, RISING);
      stateSCD30 = IS_BUSY;
    } else {
      scd30_avail = false;
      stateSCD30 = HAS_ERROR;
    }
    Serial.println("SCD30 Initialized");
  }

  /******************************************************************************************************/
  // Initialize Timing System
  /******************************************************************************************************/
  currentTime       = millis();
  lastTime          = currentTime;
  lastSCD30         = currentTime;
  lastLCD           = currentTime;
  lastPressureSCD30 = currentTime;
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
  // SCD30 Sensirion CO2 sensor
  /******************************************************************************************************/
  // Response time 20sec
  // Bootup time 2sec
  // Atomatic Basline Correction might take up to 7 days
  // Measurement Interval 2...1800 
  // Is_Measureing
  //  check if new data available,
  //    read data from sensor
  //  update pressure if available

  if (scd30_avail) {
    switch(stateSCD30) {
      case IS_MEASURING : { // used without RDY pin interrupt
        if ((currentTime - lastSCD30) >= intervalSCD30) {
          if (scd30.dataAvailable()) {
            scd30_ppm  = scd30.getCO2();
            scd30_temp = scd30.getTemperature();
            scd30_hum  = scd30.getHumidity();
            lastSCD30  = currentTime;
            Serial.println("SCD30 data read");
          } else {
            Serial.println("SCD30 data not yet available");
          }
        }
        if (bme680_avail) { // update pressure settings
          if ((currentTime - lastPressureSCD30) >= intervalPressureSCD30) {
            scd30.setAmbientPressure(uint16_t(1015)); 
            // update with value from pressure sensor, needs to be mbar
            lastPressureSCD30 = currentTime;
            Serial.println("SCD30 pressure updated");
          }
        }
        break;
      } // is measuring

      case IS_BUSY: { // used to bootup sensor when RDY pin is used
        scd30.dataAvailable(); // without checking status, RDY pin will not switch
      }
      case DATA_AVAILABLE : { // used to obtain data when RDY pin is used
        scd30_ppm  = scd30.getCO2();
        scd30_temp = scd30.getTemperature();
        scd30_hum  = scd30.getHumidity();
        lastSCD30  = currentTime;
        stateSCD30 = IS_IDLE; 
        break;
      }
      
      case IS_IDLE : { // used when RDY pin is used
        if (bme680_avail) { // update pressure settings
          if ((currentTime - lastPressureSCD30) >= intervalPressureSCD30) {
            scd30.setAmbientPressure(uint16_t(1015)); 
            // update with value from pressure sensor, needs to be mbar
            lastPressureSCD30 = currentTime;
            Serial.println("SCD30 pressure updated");
          }
        }
        break;        
      }
      
      case HAS_ERROR : {
        Serial.println("SCD30 has error!");
        break;
      }
    } // switch state
  } // if available

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
  // Update runtime every minute
  if ((currentTime - lastTime) >= 60000) {
    mySettings.runTime = mySettings.runTime + ((currentTime - lastTime) / 1000);
    lastTime = currentTime;
    Serial.println("Runtime updated");
  }

  // Update EEPROM once a week
  if ((currentTime - lastEEPROM) >= 604800000) {
    EEPROM.put(0,mySettings);
    if (EEPROM.commit()) {    
      lastEEPROM = currentTime;
      Serial.println("EEPROM updated");
    }
  }

  long sleepDuration = long(millis()) - long(currentTime + intervalLoop);
  if (sleepDuration > 0) {
   delay(sleepDuration);
  }
  
} // loop
