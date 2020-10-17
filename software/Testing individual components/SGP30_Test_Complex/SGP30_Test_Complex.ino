//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////
// Air Quality Sensor
//
// An ESP8266 board is the base for gas and air quality related sensors
//
// Requirements:
//  LCD 20x4
//  SGP30 eCO2
// 
// Operation:
//
// Code Modifications:
// 
// Urs Utzinger
// Summer 2020
//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////

bool fastMode = false;              // true: Measure as fast as possible, false: operate in energy efficiency mode
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
                  WAIT_STABLE, GET_BASELINE, HAS_ERROR};
// IS_IDLE        the sensor is powered up
// IS_MEASURING   the sensor is creating data autonomously
// IS_BUSY        the sensor is producing data and will not respond to commands
// DATA_AVAILABLE new data is available in sensor registers
// IS_SLEEPING    the sensor or parts of the sensot are in sleep mode
// IS_WAKINGUP    the sensor is getting out of sleep mode
// WAIT_STABLE    
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
// SGP30; Senserion TVOC, eCO2
/******************************************************************************************************/
// Sampling rate: minimum 1s
// Current Consumption:
//   Measurement mode 49mA
//   Sleep mode        0.002..0.0010 mA
// There is no recommended approach to put sensor into sleep mode. Once init command is issued, 
//   sensor remains in measurement mode.
// Operation Sequence:
//   Power up, puts the sensor in sleep mode
//   Init Command (10ms to complete)
//   Read baseline from EEPROM
//   If valid set baseline
//   Set Humidity Compensation 10ms
//   Start Measure Airquality Command 
//   Unresponsive for 12ms
//   Initiate Get Airquality Command
//   Wait until measurement interval exceeded
//   Warmup time exceeded?
//     reliable measurement
//     get baseline and save to EEPROM
//   Humidity interval exceeded?
//     write humidity to sensor
//
// Write Baseline 10ms from EEPROM to decrease  time to stable readings
// Read Baseline (10ms) periodically from sensors dynamic baseline calculations
// Set Humidity from third party sensor periodically to increase accuracy
//
#include "SparkFun_SGP30_Arduino_Library.h"
bool sgp30_avail  = false;
bool baslineSGP30_valid = false;
unsigned long lastSGP30;                                 // last time we obtained data
unsigned long lastSGP30Humidity;                         // last time we upated humidity
unsigned long lastSGP30Baseline;                         // last time we obtained baseline
#define intervalSGP30Fast 1000                           // as low as 10ms, ideal is 1s, gives best accuracy
#define intervalSGP30Slow 1000                           //
#define intervalSGP30Baseline 300000                     // Every 5 minutes
#define intervalSGP30Humidity  60000                     // Update once a minute
#define warmupSGP30_withbaseline 3600                    // 60min 
#define warmupSGP30_withoutbaseline 43200                // 12hrs 
unsigned long intervalSGP30;                              // Measure once a second
unsigned long warmupSGP30;
volatile SensorStates stateSGP30 = IS_IDLE; 
SGP30 sgp30;

bool bme680_avail = false;
float bme680_ah = 0.0;

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
  Serial.println("g: get baselines");
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
        sgp30.getBaseline();
        mySettings.baselinetVOC_SGP30 = sgp30.baselineTVOC;
        sgp30.setBaseline((uint16_t)tmpI, (uint16_t)mySettings.baselinetVOC_SGP30);
        mySettings.baselineSGP30_valid = 0xF0;
        mySettings.baselineeCO2_SGP30 = (uint16_t)tmpI;
        Serial.print("Calibration point is:  ");
        Serial.println(tmpI);
      } else { Serial.println("Calibration point out of valid Range"); }
    } 

    else if (command == "t") {                                       // forced tVOC set setpoint
      tmpI = value.toInt();
      if ((tmpI > 400) && (tmpI < 2000)) {
        sgp30.getBaseline();
        mySettings.baselineeCO2_SGP30 = sgp30.baselineCO2;
        sgp30.setBaseline((uint16_t)mySettings.baselineeCO2_SGP30, (uint16_t)tmpI);
        mySettings.baselineSGP30_valid = 0xF0;
        mySettings.baselinetVOC_SGP30 = (uint16_t)tmpI;
        Serial.print("Calibration point is:  ");
        Serial.println(tmpI);
      } else { Serial.println("Calibration point out of valid Range"); }
    } 
    
    else if (command == "g") {                                       // forced tVOC set setpoint
      sgp30.getBaseline();
      mySettings.baselineSGP30_valid = 0xF0;
      mySettings.baselinetVOC_SGP30 = sgp30.baselineTVOC;
      mySettings.baselineeCO2_SGP30 = sgp30.baselineCO2;
    } 

    else if (command == "c") {                                       // forced tVOC set setpoint
      tmpI = value.toInt();
      if ((tmpI > 400) && (tmpI < 2000)) {
        // ccs811.xxx((uint16_t)tmpI);
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
  if (sgp30_avail == true) {
    lcd.setCursor(eCO2_X, eCO2_Y);
    sprintf(lcdbuf, "%4d", sgp30.CO2);
    lcd.print(lcdbuf);

    lcd.setCursor(TVOC_X, TVOC_Y);
    sprintf(lcdbuf, "%4d", sgp30.TVOC);
    lcd.print(lcdbuf);

    if (sgp30.CO2 < 800) {
      sprintf(lcdbuf, "%s", "N");
    } else if (sgp30.CO2 < 1000) {
      sprintf(lcdbuf, "%s", "T");
    } else if (sgp30.CO2 < 5000) {
      sprintf(lcdbuf, "%s", "P");
    } else {
      sprintf(lcdbuf, "%s", "!");
    }
    lcd.setCursor(eCO2_WARNING_X, eCO2_WARNING_Y);
    lcd.print(lcdbuf);

    if (sgp30.TVOC < 220) {
      sprintf(lcdbuf, "%s", "N");
    } else if (sgp30.TVOC < 660) {
      sprintf(lcdbuf, "%s", "T");
    } else if (sgp30.TVOC < 2200) {
      sprintf(lcdbuf, "%s", "P");
    } else {
      sprintf(lcdbuf, "%s", "!");
    }
    lcd.setCursor(TVOC_WARNING_X, TVOC_WARNING_Y);
    lcd.print(lcdbuf);
    //
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
  strcpy(mySettings.pw1,                   "");
  strcpy(mySettings.ssid2,                 "AMARIE-HP_Network");
  strcpy(mySettings.pw2,                   "");
  strcpy(mySettings.ssid3,                 "MuchoCoolioG");
  strcpy(mySettings.pw3,                   "");
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
  if (checkI2C(0x58) == 1) {sgp30_avail =true;}   else {sgp30_avail = false;}   // Senserion TVOC eCO2

  Serial.print("LCD                  "); if (lcd_avail)    {Serial.println("available");} else {Serial.println("not available");}
  Serial.print("SGP30 eCO2, tVOC     "); if (sgp30_avail)  {Serial.println("available");} else {Serial.println("not available");}

  /******************************************************************************************************/
  // Initialize LCD Screen
  /******************************************************************************************************/
  if (lcd_avail == true) {
    lcd.begin();
    lcd.backlight();
    lcd.setCursor(0, 0);
    Serial.println("LCD initialized");
  } else {
    Serial.println("LCD not found!");
  }
  
  /******************************************************************************************************/
  // SGP30 Initialize
  /******************************************************************************************************/

  if (sgp30_avail == true){
    if (sgp30.begin() == false) {
      Serial.println("No SGP30 Detected. Check connections.");
      sgp30_avail = false;
      stateSGP30 = HAS_ERROR;
    }
    if (fastMode) { intervalSGP30 = intervalSGP30Fast;}
    else          { intervalSGP30 = intervalSGP30Slow;}
    //Initializes sensor for air quality readings
    sgp30.initAirQuality();
    stateSGP30 = IS_MEASURING;
    if (mySettings.baselineSGP30_valid == 0xF0) {
      sgp30.setBaseline((uint16_t)mySettings.baselineeCO2_SGP30, (uint16_t)mySettings.baselinetVOC_SGP30);
      warmupSGP30 = millis() + warmupSGP30_withbaseline;
    } else {
      warmupSGP30 = millis() + warmupSGP30_withoutbaseline;
    }
  }

  /******************************************************************************************************/
  // Initialize Timing System
  /******************************************************************************************************/
  currentTime       = millis();
  lastTime          = currentTime;
  lastLCD           = currentTime;
  lastSGP30         = currentTime;
  lastEEPROM        = currentTime;
  lastSGP30Humidity   = currentTime;
  lastSGP30Baseline   = currentTime;

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
  // SGP30 Sensirion eCO2 sensor
  /******************************************************************************************************/
  // Operation Sequence:
  //   Humidity interval exceeded?
  //     write humidity to sensor
  //   Start Measure Airquality Command 
  //   Unresponsive for 12ms
  //   Initiate Get Airquality Command
  //   Wait until measurement interval exceeded

  if (sgp30_avail) {
    switch(stateSGP30) {
      case IS_MEASURING : {
        if ((currentTime - lastSGP30Humidity) > intervalSGP30Humidity) {
          if (bme680_avail) {
            // Humidity correction, 8.8 bit number
            // 0x0F80 = 15.5 g/m^3
            // 0x0001 = 1/256 g/m^3
            // 0xFFFF = 256 +256/256 g/m^3
            sgp30.setHumidity(uint16_t(bme680_ah * 256.0 + 0.5)); 
            Serial.println("Humidity updated for SGP30 eCO2");
          }
          lastSGP30Humidity = currentTime;        
        } // end humidity update
        
        if ((currentTime - lastSGP30Baseline) > intervalSGP30Baseline) {
          sgp30.startGetBaseline(); // this has 10ms delay
          lastSGP30Baseline= millis();
          stateSGP30 = GET_BASELINE;
          break;
        }

        if ((currentTime - lastSGP30) > intervalSGP30) {
          sgp30.startAirQuality();
          lastSGP30 = currentTime;
          stateSGP30 = IS_BUSY;
        }
        break;
      }

      case IS_BUSY : {
        if ((currentTime - lastSGP30) >= 12) {
          sgp30.getAirQuality();
          lastSGP30 = currentTime;
          stateSGP30 = IS_MEASURING;
          Serial.println("SGP30 eCO2 & tVOC measured");
        }
        break;
      }
      
      case GET_BASELINE : {
        if ((currentTime - lastSGP30Baseline) > 10) {
          if (sgp30.finishGetBaseline() == SUCCESS) {
            stateSGP30 = IS_MEASURING; 
            Serial.println("SGP30 Baseline obtained");
          } else { stateSGP30 = HAS_ERROR; }
        }
        break;
      }
        
    } // switch state
  } // if available

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
  // Copy SGP30 basline to settings when warmup is finished
  if (currentTime >=  warmupSGP30) {
    mySettings.baselinetVOC_SGP30 = sgp30.baselineTVOC;
    mySettings.baselineeCO2_SGP30 = sgp30.baselineCO2;
    mySettings.baselineSGP30_valid = 0xF0;
    Serial.println("SGP30 baseline setting updated");
    warmupSGP30 = warmupSGP30 + intervalSGP30Baseline; 
  }

  //  Put sensor to sleep *****************************************************
  long sleepDuration = (long)millis() - (long)(currentTime + intervalLoop);
  if (sleepDuration > 0) {
   delay(sleepDuration);
  }
  
} // loop
