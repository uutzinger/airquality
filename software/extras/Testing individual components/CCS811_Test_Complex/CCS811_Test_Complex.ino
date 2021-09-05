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

bool fastMode = true;       // true: Measure as fast as possible, false: operate in energy efficiency mode

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
  byte          baselineCCS811_valid;
  uint16_t      baselineCCS811;
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
                  HAS_ERROR};
// IS_IDLE        the sensor is powered up
// IS_MEASURING   the sensor is creating data autonomously
// IS_BUSY        the sensor is producing data and will not respond to commands
// DATA_AVAILABLE new data is available in sensor registers
// IS_SLEEPING    the sensor or parts of the sensot are in sleep mode
// IS_WAKINGUP    the sensor is getting out of sleep mode
// GET_BASELINE   read the baseline correction in sensor
// HAS_ERROR      the communication with the sensor failed

/******************************************************************************************************/
// LCD Display
/******************************************************************************************************/
// Power Consumption: ... have not measured
// 
#include <LiquidCrystal_I2C.h>
unsigned long intervalLCD;                  // LCD refresh rate
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
// CCS811; Airquality CO2 tVOC
/******************************************************************************************************/
// Sensor has 20min equilibrium time and 48hrs burnin time
// Sensor is read through interrupt handling (with this implementation):
//   Initilize with desired mode
//   When data is ready, iterrupt is triggered and data is obtained
// Power On 20ms
// Mode 0 – Idle (Measurements are disabled in this mode)
// Mode 1 – Constant power mode, IAQ measurement every second
// Mode 2 – Pulse heating mode IAQ measurement every 10 seconds
// Mode 3 – Low power pulse heating mode IAQ measurement every 60 seconds
// Mode 4 – Const power pulse heating mode IAQ measurement every 0.25 seconds
//
// baseline takes 24 hrs to establish, R_A is reference resistance
// Sleep mode: turns off i2c? 0.019 mA
// Operating Sequence:
// Setup turns on sensor and starts reading
// When measurement complete, interrupt is asserted and ISR is called
//  if fastMode = false
//    ISR wakes up i2c communication on wakeup pin
//    Sensor is given 1ms to weake up
//    Senor data is read and Sensor logic is switched back to sleep mode
//  if fastMode = true
//    Sensor data is read
// Status is sleeping
//   Waiting until data interrupt occurs
//
#include "SparkFunCCS811.h"
bool ccs811_avail = false;
unsigned long lastCCS811;                                // last time we interacted with sensor
unsigned long lastCCS811Baseline;                        // last time we obtained baseline
unsigned long lastCCS811Humidity;                        // last time we update Humidity on sensor
unsigned long lastCCS811Interrupt;                       // last time we update Humidity on sensor
unsigned long warmupCCS811;                              // sensor needs 20min conditioning 
unsigned long intervalCCS811Baseline;                    // Every 10 minutes
unsigned long intervalCCS811Humidity;                    // Every 1 minutes
#define baselineCCS811fast 300000                        // 5 mins
#define baselineCCS811slow 3600000                       // 1 hour
#define updateCCS811Humitityfast 60000                   // 1 min
#define updateCCS811Humitityslow 300000                  // 5 mins
#define stablebaseCCS811 86400000                        // sensor needs 24hr until baseline stable
#define burninCCS811    172800000                        // sensor needs 48hr burin
#define CCS811_INT D7                                    // active low interrupt, high to low at end of measurement
#define CCS811_WAKE D6                                   // active low wake to wake up sensor
#define ccs811ModeFast 1                                 // 1 per second
#define ccs811ModeSlow 3                                 // 1 per minute
uint8_t ccs811Mode;                                      // 4=0.25s, 3=60s, 2=10sec, 1=1sec.
volatile SensorStates stateCCS811 = IS_IDLE; 
CCS811 ccs811(0X5B);                                     // address pin will need to be set to high


void ICACHE_RAM_ATTR handleCCS811Interrupt() {
    if (fastMode == true) { 
      stateCCS811 = DATA_AVAILABLE;
    } else { 
      digitalWrite(CCS811_WAKE, LOW);   // set CCS811 to wake up 
      stateCCS811 = IS_WAKINGUP;
      lastCCS811Interrupt = millis();
    }
}

/******************************************************************************************************/
// Other
/******************************************************************************************************/

int lastPinStat;
bool bme680_avail = false;
float bme680_temperature  = 25.0; 
float bme680_humidity = 30.0;

unsigned long currentTime;
unsigned long lastTime;            // last time we updated runtime;
unsigned long intervalLoop = 100;  // 10Hz
unsigned long intervalRuntime = 60000;
unsigned long tmpTime;


//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////
void setup() {
//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////

  Serial.begin(115200);
  Serial.println();
  Wire.begin();

  /******************************************************************************************************/
  // Wake and Interrupt Pin configuration
  /******************************************************************************************************/  
  // CCS811 address needs to be set to high
  // D7 is used to read CCS811 interrupt, low if end of measurement
  // D6 is used to  set CCS811 wake to low, to wake up sensor
  /******************************************************************************************************/

  pinMode(CCS811_WAKE, OUTPUT);                // CCS811 not Wake Pin
  pinMode(CCS811_INT, INPUT_PULLUP);           // CCS811 not Interrupt Pin
  digitalWrite(CCS811_WAKE, LOW);              // Set CCS811 to wake
  lastPinStat = digitalRead(CCS811_INT); 

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
  if (checkI2C(0x5B) == 1) {ccs811_avail =true;}  else {ccs811_avail = false;}  // Airquality CO2 tVOC

  Serial.print("LCD                  "); if (lcd_avail)    {Serial.println("available");} else {Serial.println("not available");}
  Serial.print("CCS811 eCO2, tVOC    "); if (ccs811_avail) {Serial.println("available");} else {Serial.println("not available");}
  
  if (fastMode) {
    intervalLoop    =    100;  // 0.1 sec
    intervalLCD     =   2000;  // 2 sec
    intervalRuntime =  60000;  // 1 minute
  } else{
    intervalLoop    =   1000;  // 1 sec
    intervalLCD     =  60000;  // 1 minute
    intervalRuntime = 600000;  // 10 minutes
  }

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
  // CCS811 Initialize 
  /******************************************************************************************************/
  if (ccs811_avail == true){

    CCS811Core::CCS811_Status_e css811Ret = ccs811.beginWithStatus();
    Serial.print("CCS811: begin exited with: ");
    Serial.println(ccs811.statusString(css811Ret));

    if (fastMode) { ccs811Mode = ccs811ModeFast; }
    else          { ccs811Mode = ccs811ModeSlow; }

    css811Ret = ccs811.setDriveMode(ccs811Mode);
    Serial.print("CCS811: mode request exited with: ");
    Serial.println(ccs811.statusString(css811Ret));

    //Configure and enable the interrupt line, then print error status
    css811Ret = ccs811.enableInterrupts();
    Serial.print("CCS811: interrupt configuation exited with: ");
    Serial.println(ccs811.statusString(css811Ret));
       
    if (mySettings.baselineCCS811_valid == 0xF0) {
      CCS811Core::CCS811_Status_e errorStatus = ccs811.setBaseline(mySettings.baselineCCS811);
      if (errorStatus == CCS811Core::CCS811_Stat_SUCCESS) { Serial.println("CCS811: baseline received"); }
      else {
        Serial.print("CCS811: error writing baseline: ");
        Serial.println(ccs811.statusString(errorStatus));
      }
    }

    if (fastMode == true) { 
      intervalCCS811Baseline =  baselineCCS811fast; 
      intervalCCS811Humidity =  updateCCS811Humitityfast;
    } else {
      intervalCCS811Baseline = baselineCCS811slow; 
      intervalCCS811Humidity =  updateCCS811Humitityslow;
      Serial.println("CCS811: it will take about 5minutes until readings are non zero.");
    }

    warmupCCS811 = currentTime + stablebaseCCS811;    
    
    attachInterrupt(digitalPinToInterrupt(CCS811_INT), handleCCS811Interrupt, FALLING);
    stateCCS811 = IS_IDLE; // IS Measuring?
  }

  /******************************************************************************************************/
  // Initialize Timing System
  /******************************************************************************************************/
  currentTime        = millis();
  lastTime           = currentTime;
  lastLCD            = currentTime;
  lastCCS811         = currentTime;
  lastCCS811Humidity = currentTime;
  lastCCS811Interrupt= currentTime;
  lastEEPROM         = currentTime;

  tmpTime = millis();
  updateLCD();
  lastLCD = currentTime;
  Serial.print("LCD updated in ");
  Serial.print((millis()-tmpTime));  
  Serial.println("ms");

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
      tmpTime = millis();
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
  // CCS811
  /******************************************************************************************************/

  if (ccs811_avail) {
    switch(stateCCS811) {

      case IS_WAKINGUP : { // ISR will activate this
        if ((currentTime - lastCCS811Interrupt) >= 1) { // wakeup time is 50micro seconds
          stateCCS811 = DATA_AVAILABLE;
          Serial.println("CCS811: wake up completed");
        }
        break;
      }

      case DATA_AVAILABLE : {
        unsigned long tmpTime = millis();
        ccs811.readAlgorithmResults(); //Calling this function updates the global tVOC and CO2 variables
        Serial.print("CCS811: readAlgorithmResults completed in ");
        Serial.print((millis()-tmpTime));  
        Serial.println("ms");
        uint8_t error = ccs811.getErrorRegister();
        if (error == 0xFF) { Serial.println("CCS811: failed to read ERROR_ID register."); }
        else  {
          if (error & 1 << 5) Serial.print("CCS811 Error: HeaterSupply");
          if (error & 1 << 4) Serial.print("CCS811 Error: HeaterFault");
          if (error & 1 << 3) Serial.print("CCS811 Error: MaxResistance");
          if (error & 1 << 2) Serial.print("CCS811 Error: MeasModeInvalid");
          if (error & 1 << 1) Serial.print("CCS811 Error: ReadRegInvalid");
          if (error & 1 << 0) Serial.print("CCS811 Error: MsgInvalid");
        }

        if ((currentTime - lastCCS811Baseline) >= intervalCCS811Baseline ) {
          tmpTime = millis();
          mySettings.baselineCCS811 = ccs811.getBaseline();
          lastCCS811Baseline = currentTime;
          Serial.print("CCS811: baseline obtained in ");
          Serial.print((millis()-tmpTime));  
          Serial.println("ms");
        }
        
        if ((currentTime - lastCCS811Humidity) > intervalCCS811Humidity ){
          lastCCS811Humidity = currentTime;
          if (bme680_avail == true) {
            tmpTime = millis();
            ccs811.setEnvironmentalData(bme680_humidity, bme680_temperature);
            Serial.print("CCS811: humidity and temperature compensation updated in ");
            Serial.print((millis()-tmpTime));  
            Serial.println("ms");
          }
        }  

        if (fastMode == false) { 
          digitalWrite(CCS811_WAKE, HIGH);     // put CCS811 to sleep
          Serial.println("CCS811: puttting sensor to sleep");
          stateCCS811 = IS_SLEEPING;
        } else {
          stateCCS811 = IS_IDLE;
        }
        
        lastCCS811 = currentTime;
        break;
      }

      case IS_IDLE : {
        // Continue Sleeping, we are waiting for interrupt
        // Serial.println("CCS811: is idle");
        break;
      }
      
      case IS_SLEEPING : {
        // Continue Sleeping, we are waiting for interrupt
        // Serial.println("CCS811: is sleeping");
        break;
      }
 
    } // end switch
  } // end if avail

 
  /******************************************************************************************************/
  // Time Management
  /******************************************************************************************************/
  // Update runtime every minute ***********************************************
  if ((currentTime - lastTime) >= intervalRuntime) {
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

  // Copy CCS811 basline to settings when warmup is finished
  if (currentTime >= warmupCCS811) {    
    mySettings.baselineCCS811 = ccs811.getBaseline();
    mySettings.baselineCCS811_valid = 0xF0;
    Serial.println("CCS811 baseline placed into settings");
    warmupCCS811 = warmupCCS811 + stablebaseCCS811; 
  }

  //  Put microcontroller to sleep **********************************************
  long sleepDuration = (long)millis() - (long)(currentTime + intervalLoop);
  if (sleepDuration > 0) {
   delay(sleepDuration);
  }
  
} // loop


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
  Serial.println("SGP30=eCO2============================");
  Serial.println("e: force eCO2, e400");
  Serial.println("t: force tVOC, t3000");  
  Serial.println("g: get baselines");
  Serial.println("SCD30=CO2=============================");
  Serial.println("f: force CO2, f400.0 in ppm");
  Serial.println("t: force temperature offset, t5.0 in C");
  Serial.println("CCS811=eCO2===========================");
  Serial.println("c: force basline, c400");
  Serial.println("b: get baseline");
  Serial.println("EEPROM================================");
  Serial.println("s: save to EEPROM");
  Serial.println("r: read from EEPROM");
  Serial.println("p: print current settings");
  Serial.println("d: create default settings");
  Serial.println("=========================Urs Utzinger=");
}

void printSettings() {
  Serial.print("Runtime [min]: ................ "); Serial.println((unsigned long)mySettings.runTime/60);
  Serial.print("Base SGP30 valid: ............. "); Serial.println((int)mySettings.baselineSGP30_valid);
  Serial.print("Base eCO2 SGP30: .............. "); Serial.println((int)mySettings.baselineeCO2_SGP30);
  Serial.print("Base tVOC SGP30: .............. "); Serial.println((int)mySettings.baselinetVOC_SGP30);
  Serial.print("Base CCS811 valid: ............ "); Serial.println((int)mySettings.baselineCCS811_valid);
  Serial.print("Base CCS811: .................. "); Serial.println((int)mySettings.baselineCCS811);
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
  mySettings.baselineCCS811_valid          = (byte)         0x00;
  mySettings.baselineCCS811                = (uint16_t)        0;
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

    ///////////////////////////////////////////////////////////////////
    // SGP30
    ///////////////////////////////////////////////////////////////////
    if (command == "e") {                                            // forced CO2 set setpoint
      tmpI = value.toInt();
      if ((tmpI >= 400) && (tmpI <= 2000)) {
        //sgp30.getBaseline();
        //mySettings.baselinetVOC_SGP30 = sgp30.baselineTVOC;
        //sgp30.setBaseline((uint16_t)tmpI, (uint16_t)mySettings.baselinetVOC_SGP30);
        mySettings.baselineSGP30_valid = 0xF0;
        mySettings.baselineeCO2_SGP30 = (uint16_t)tmpI;
        Serial.print("Calibration point is:  ");
        Serial.println(tmpI);
      } else { Serial.println("Calibration point out of valid Range"); }
    } 

    else if (command == "t") {                                       // forced tVOC set setpoint
      tmpI = value.toInt();
      if ((tmpI >= 0) && (tmpI <= 60000)) {
        //sgp30.getBaseline();
        //mySettings.baselineeCO2_SGP30 = sgp30.baselineCO2;
        //sgp30.setBaseline((uint16_t)mySettings.baselineeCO2_SGP30, (uint16_t)tmpI);
        mySettings.baselineSGP30_valid = 0xF0;
        mySettings.baselinetVOC_SGP30 = (uint16_t)tmpI;
        Serial.print("Calibration point is:  ");
        Serial.println(tmpI);
      } else { Serial.println("Calibration point out of valid Range"); }
    } 
    
    else if (command == "g") {                                       // get setpoints
      //sgp30.getBaseline();
      mySettings.baselineSGP30_valid = 0xF0;
      //mySettings.baselinetVOC_SGP30 = sgp30.baselineTVOC;
      //mySettings.baselineeCO2_SGP30 = sgp30.baselineCO2;
    } 

    ///////////////////////////////////////////////////////////////////
    // SGP30
    ///////////////////////////////////////////////////////////////////

    else if (command == "f") {                                       // forced CO2 set setpoint
      tmpI = value.toInt();
      if ((tmpI >= 400) && (tmpI <= 2000)) {
        //scd30.setForcedRecalibrationFactor((uint16_t)tmpI);
        mySettings.forcedCalibration_SCD30_valid = 0xF0;
        mySettings.forcedCalibration_SCD30 = (uint16_t)tmpI;
        Serial.print("Calibration point is:  ");
        Serial.println(tmpI);
      } else { Serial.println("Calibration point out of valid Range"); }
    } 
    
    else if (command == "t") {                                       // forced CO2 set setpoint
      tmpF = value.toFloat();
      if ((tmpF >= 0.0) && (tmpF <= 20.0)) {
        //scd30.setTemperatureOffset(tmpF);
        mySettings.tempOffset_SCD30_valid = 0xF0;
        //mySettings.tempOffset_SCD30 = scd30.getTemperatureOffset();
        Serial.print("Temperature offset set to:  ");
        Serial.println(mySettings.tempOffset_SCD30);
      } else { Serial.println("Offset point out of valid Range"); }
    } 

    ///////////////////////////////////////////////////////////////////
    // CCS811
    ///////////////////////////////////////////////////////////////////

    else if (command == "c") {                                       // forced baseline
      tmpI = value.toInt();
      if ((tmpI >= 0) && (tmpI <= 100000)) {
        ccs811.setBaseline((uint16_t)tmpI);
        mySettings.baselineCCS811_valid = 0xF0;
        mySettings.baselineCCS811 = (uint16_t)tmpI;
        Serial.print("Calibration point is:  ");
        Serial.println(tmpI);
      } else { Serial.println("Calibration point out of valid Range"); }
    } 

    else if (command == "b") {                                      // getbaseline
        mySettings.baselineCCS811 = ccs811.getBaseline();
        Serial.print("CCS811: baseline is  ");
        Serial.println(mySettings.baselineCCS811);
    } 
    ///////////////////////////////////////////////////////////////////
    // EEPROM abd settings
    ///////////////////////////////////////////////////////////////////

    else if (command == "s") {                                       // save EEPROM
      tmpTime=millis();
      EEPROM.put(0,mySettings);
      if (EEPROM.commit()) {
        Serial.print("EEPROM saved in: ");
        Serial.print(millis()-tmpTime);
        Serial.println("ms");
      }
    }

    else if (command == "r") {                                       // read EEPROM
      tmpTime=millis();
      EEPROM.get(0,mySettings);
      Serial.print("EEPROM read in: ");
      Serial.print(millis()-tmpTime);
      Serial.println("ms");
      printSettings();
    }

    else if (command == "p") {                                       // display settings
      printSettings();
    } 

    else if (command == "d") {                                       // default settings
      defaultSettings();
      printSettings();
    }
     
    else {                                                           // unrecognized command
      helpMenu();
    } // end if

  } // end serial available
} // end Input

void updateLCD() {
  char lcdbuf[16];
  lcd.clear(); // 2ms
  if (ccs811_avail == true) {

    lcd.setCursor(eeCO2_X, eeCO2_Y);
    sprintf(lcdbuf, "%4d", ccs811.getCO2());
    lcd.print(lcdbuf);

    lcd.setCursor(TTVOC_X, TTVOC_Y);
    sprintf(lcdbuf, "%4d", ccs811.getTVOC());
    lcd.print(lcdbuf);

    if (ccs811.getCO2() < 800) {
      sprintf(lcdbuf, "%s", "N");
    } else if (ccs811.getCO2() < 1000) {
      sprintf(lcdbuf, "%s", "T");
    } else if (ccs811.getCO2() < 5000) {
      sprintf(lcdbuf, "%s", "P");
    } else {
      sprintf(lcdbuf, "%s", "!");
    }
    lcd.setCursor(eeCO2_WARNING_X, eeCO2_WARNING_Y);
    lcd.print(lcdbuf);

    if (ccs811.getTVOC() < 220) {
      sprintf(lcdbuf, "%s", "N");
    } else if (ccs811.getTVOC() < 660) {
      sprintf(lcdbuf, "%s", "T");
    } else if (ccs811.getTVOC() < 2200) {
      sprintf(lcdbuf, "%s", "P");
    } else {
      sprintf(lcdbuf, "%s", "!");
    }
    lcd.setCursor(TTVOC_WARNING_X, TTVOC_WARNING_Y);
    lcd.print(lcdbuf);
    
  } // end if avail
} // end update LCD
