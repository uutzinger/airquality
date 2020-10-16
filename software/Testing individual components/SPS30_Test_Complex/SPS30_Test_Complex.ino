//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////
// Air Quality Sensor
//
// An ESP8266 board is the base for gas and air quality related sensors
//
// Requirements:
//  LCD 20x4
//  SPS30 Senserion particle
// 
// Operation:
//
// Code Modifications:
// 
// Urs Utzinger
// Summer 2020
//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////


bool fastMode = true;             // true: Measure as fast as possible, false: operate in energy efficiency mode

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

enum SensorStates{IS_IDLE = 0, IS_BUSY, DATA_AVAILABLE, IS_SLEEPING, IS_WAKINGUP, 
                  WAIT_STABLE, HAS_ERROR};
// IS_IDLE        the sensor is powered up
// IS_MEASURING   the sensor is creating data autonomously
// IS_BUSY        the sensor is producing data or is getting setup
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
unsigned long intervalLCD = 2000;                  // LCD refresh rate
bool lcd_avail = false;                            // is LCD attached?
unsigned long lastLCD;                             // last time LCD was modified
LiquidCrystal_I2C lcd(0x27,20,4);                  // set the LCD address to 0x27 for a 20 chars and 4 line display

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
// SPS30; Senserion Particle Sensor
/******************************************************************************************************/
// Sample interval min 1+/-0.04s
// After power up, sensor is idle
// Execute start command to start measurements
// Stop measurement goes to idle
// Default cleaning interval is 604800 seconds (one week)
// Supply Current:
// Max              80mA
// Measurement Mode 65mA
// Idle Mode         0.36mA
// Sleep Mode        0.05mA
// Time to reach stable measurement: 8s for 200 – 3000 #/cm3 particles
//                                  16s for 100 – 200  #/cm3 particles
//                                  30s for  50 – 100  #/cm3 particles
// Sleep mode can be entered from idle mode
// Operation Mode Slow:
//   Init, send start command
//   if version >=2.2 Read Status
//   Read Data
//   Determine Time to Stable
//   Wait until Stable
//   Read table Data
//   Got to Idle
//   if version >2.2 
//     Got to Sleep
//     Wait until Sleep Exceeded
//     Wakeup
//   Goto Read Data above
// NEED TO UPDATE THIS
// Operation Mode Fast:
//   Init, send start command
//   Read Status
//   Read Data
//   Determine Time to Stable
//   Read Data until Stable
//   Update Stable Data
//   Wait until Interval Expired
//   Goto Read Data above
//
#include <sps30.h>
char buf[64];
uint8_t ret, st;
float totalParticles;                                    // we need to adjust measurement time depending on total particle concentration
uint32_t autoCleanIntervalSPS30;                         // current cleaning settings in sensor
bool sps30_avail = false;                                // do we have this sensor
#define intervalSPS30Fast 1000                           // minimum is 1 sec
#define intervalSPS30Slow 60000                          // system will sleep for intervalSPS30Slow - timetoStable
unsigned long intervalSPS30;                             // measurement interval
unsigned long timeSPS30Stable;                           // time when readings are stable, is adjusted automatically based on particle counts
unsigned long lastSPS30;                                 // last time we interacted with sensor
unsigned long wakeSPS30;                                 // time when wakeup was issued
unsigned long wakeTimeSPS30;                             // time when sensor is suppoed to be woken up
unsigned long timeToStableSPS30;                         // how long it takes to get stable readings automatically pupulated based on total particles
volatile SensorStates stateSPS30 = IS_BUSY; 
struct sps_values valSPS30;                              // readings from sensor, will hold any readings
SPS30 sps30;
SPS30_version v;

/******************************************************************************************************/
// Other
/******************************************************************************************************/
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
  serialTrigger((char *) "SPS30: press <enter> to start");

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
  if (checkI2C(0x69) == 1) {sps30_avail = true;}  else {sps30_avail = false;}   // Senserion Particle

  if (lcd_avail)    {Serial.println("LCD available");}
  if (sps30_avail)  {Serial.println("SPS30 particle available");}

  /******************************************************************************************************/
  // Intervals
  /******************************************************************************************************/

  if (fastMode) {
    intervalLoop = 100;       // 0.1 sec
    intervalLCD = 2000;       // 2 sec
    intervalRuntime = 60000;  // 1 minute
  } else{
    intervalLoop = 1000;      // 1 sec
    intervalLCD = 60000;      // 1 minute
    intervalRuntime = 600000; // 10 minutes
  }

  /******************************************************************************************************/
  // Initialize LCD Screen
  /******************************************************************************************************/
  if (lcd_avail == true) {
    lcd.begin();
    lcd.backlight();
    lcd.setCursor(0, 0);
    Serial.println("LCD Initialized");
  } else {
    Serial.println("LCD not found!");
  }
  
  /******************************************************************************************************/
  // SPS30 Initialize Particle Sensor
  /******************************************************************************************************/
  if (sps30_avail == true) {
    /* define driver debug
     * 0 : no messages
     * 1 : request sending and receiving
     * 2 : request sending and receiving + show protocol errors */
    sps30.EnableDebugging(0);
    if (sps30.begin(&Wire) == false) {
      Serial.println("SPS30: Sensor not detected in I2C. Please check wiring.");
      stateSPS30 = HAS_ERROR;
      sps30_avail = false;
    }
    if (sps30.probe() == false) { 
      Serial.println("SPS30: could not probe / connect."); 
      stateSPS30 = HAS_ERROR;
      sps30_avail = false;
    } else { 
      Serial.println("SPS30: detected"); 
    }

    if (sps30.reset() == false) { 
      Serial.println("SPS30: could not reset."); 
      stateSPS30 = HAS_ERROR;
      sps30_avail = false;
    }  

    // read device info
    ret = sps30.GetSerialNumber(buf, 32);
    if (ret == ERR_OK) {
      Serial.print(F("SPS30: serial number : "));
      if(strlen(buf) > 0)  Serial.println(buf);
      else { Serial.println(F("SPS30: not available"));}
    } 
    ret = sps30.GetProductName(buf, 32);
    if (ret == ERR_OK)  {
      Serial.print(F("SPS30: product name  : "));
      if(strlen(buf) > 0)  {Serial.println(buf);}
      else {Serial.println(F("not available"));}
    }
    ret = sps30.GetVersion(&v);
    if (ret == ERR_OK) {
      Serial.print("SPS30: firmware level: ");
      Serial.print(v.major);
      Serial.print(".");
      Serial.println(v.minor);
      Serial.print("SPS30: hardware level: ");
      Serial.println(v.HW_version);
      Serial.print("SPS30: SHDLC protocol: ");
      Serial.print(v.SHDLC_major);
      Serial.print(".");
      Serial.println(v.SHDLC_minor);
      Serial.print("SPS30: library level : ");
      Serial.print(v.DRV_major);
      Serial.print(".");
      Serial.println(v.DRV_minor);
    }
    ret = sps30.GetAutoCleanInt(&autoCleanIntervalSPS30);
    if (ret == ERR_OK) {
      Serial.print(F("SPS30: current Auto Clean interval: "));
      Serial.print(autoCleanIntervalSPS30);
      Serial.println(F("s"));
    }

    // Start Measurement
    if (sps30.start() == true) { 
      Serial.println("SPS30: measurement started");
      stateSPS30 = IS_BUSY; 
    } else { 
      Serial.println("SPS30: could NOT start measurement"); 
      stateSPS30 = HAS_ERROR;
      sps30_avail = false;
    }

    // This addresses issue with limited RAM on microcontrollers
    if (sps30.I2C_expect() == 4) { Serial.println("SPS30: !!! Due to I2C buffersize only the SPS30 MASS concentration is available !!!"); }
  } else {
    Serial.println("SPS30: not found!");
  }

  /******************************************************************************************************/
  // Initialize Timing System
  /******************************************************************************************************/
  currentTime       = millis();
  lastTime          = currentTime;
  lastSPS30         = currentTime;
  lastLCD           = currentTime;

  if (lcd_avail == true) {
    updateLCD();
    lastLCD = currentTime;
    Serial.print("LCD updated in ");
    Serial.print((millis()-tmpTime));  
    Serial.println("ms");
  }
  
  Serial.println("System initialized");

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
  // SPS30 Sensirion Particle Sensor State Machine
  /******************************************************************************************************/
  //
  // Operation Mode:
  //   Start (at setup)
  //   Read Status (IS_BUSY state)
  //   Read Data (IS_BUSY state)
  //   Determine Time to Stable 
  //   Wait Until Stable (WAIT_STABLE)
  //   Read Data
  //
  // if fastMode == false want energy saving with sleeping
  //   Got to Sleep
  //   Wait until Sleep Exceeded
  //   Wakeup
  //   Start
  //   Goto Wait Until Stable
  //
  // else fastMode == true, want maximal update rate
  //   Wait until MeasurementInterval Expired
  //   Gotto Read Data
  //
  /******************************************************************************************************/
  if (sps30_avail) {
    
    switch(stateSPS30) { 
      case IS_BUSY: { 
        if (((v.major==2) && (v.minor>=2)) || (v.major>2)) {
          ret = sps30.GetStatusReg(&st);         // takes 20ms
          if (ret == ERR_OK) {
            Serial.print("SPS30: reading status completed and ");
            if (st == STATUS_OK) {            Serial.println("SPS30 Status: ok");
            } else {
              if (st & STATUS_SPEED_ERROR) { Serial.println("SPS30 WARNING : Fan is turning too fast or too slow"); }
              if (st & STATUS_LASER_ERROR) { Serial.println("SPS30 ERROR: Laser failure"); }  
              if (st & STATUS_FAN_ERROR)   { Serial.println("SPS30 ERROR: Fan failure, fan is mechanically blocked or broken"); }
              stateSPS30 = HAS_ERROR;
            } // status ok
          } else { Serial.print("SPS30: could not read status"); }// read status
        } // Firmware >= 2.2
        ret = sps30.GetValues(&valSPS30);               
        Serial.println("SPS30: values read");
        if (ret == ERR_DATALENGTH) { Serial.println("SPS30 ERROR: Data length during reading values"); }
        else if (ret != ERR_OK) {
          sprintf(buf, "SPS30 ERROR: reading values: %d", ret) ;
          Serial.println(buf); }
        lastSPS30 = currentTime; 
        // adjust time to get stable readings, it takes longer  with lower concentration to get a precise reading
        totalParticles = (valSPS30.NumPM0 + valSPS30.NumPM1 + valSPS30.NumPM2 + valSPS30.NumPM4 + valSPS30.NumPM10);
        if (totalParticles < 100)      { timeToStableSPS30 = 30000; }
        else if (totalParticles < 200) { timeToStableSPS30 = 16000; }
        else                           { timeToStableSPS30 =  8000; }
        timeSPS30Stable = currentTime +  timeToStableSPS30;
        stateSPS30 = WAIT_STABLE;            
        break; 
      } // BUSY

      case WAIT_STABLE: {
          if (currentTime >= timeSPS30Stable) {
          if (((v.major==2) && (v.minor>=2)) || (v.major>2)) {
            ret = sps30.GetStatusReg(&st);         // takes 20ms
            if (ret == ERR_OK) {
              Serial.print("SPS30: reading status completed and ");
              if (st == STATUS_OK) {            Serial.println("SPS30 Status: ok");
              } else {
                if (st & STATUS_SPEED_ERROR) { Serial.println("SPS30 WARNING : Fan is turning too fast or too slow"); }
                if (st & STATUS_LASER_ERROR) { Serial.println("SPS30 ERROR: Laser failure"); }  
                if (st & STATUS_FAN_ERROR)   { Serial.println("SPS30 ERROR: Fan failure, fan is mechanically blocked or broken"); }
              } // status ok
            } else { Serial.print("SPS30: could not read status"); }// read status
          } // Firmware >= 2.2
          ret = sps30.GetValues(&valSPS30);               
          Serial.println("SPS30: values read");
          if (ret == ERR_DATALENGTH) { Serial.println("SPS30 ERROR: Data length during reading values"); }
          else if (ret != ERR_OK) {
            sprintf(buf, "SPS ERROR: reading values: %d", ret) ;
            Serial.println(buf); }
          lastSPS30 = currentTime; 
          stateSPS30 = IS_IDLE;          
        } //if time stable
      } // wait stable
                  
      case IS_IDLE : { 
        if ((v.major<2) || (fastMode)) {
          timeSPS30Stable = (unsigned long) (lastSPS30 + intervalSPS30);
          stateSPS30 = WAIT_STABLE;             
        } else {
          wakeTimeSPS30 = (unsigned long) (lastSPS30 + intervalSPS30 - 50 - timeToStableSPS30);
          ret = sps30.sleep(); // 5ms execution time
          if (ret != ERR_OK) { 
            Serial.println("SPS30 ERROR: Could not go to sleep"); 
            stateSPS30 = HAS_ERROR;
          }
          stateSPS30 = IS_SLEEPING;
        }
        break;
      }
        
      case IS_SLEEPING : {
        if (currentTime >= wakeTimeSPS30) { // Wake up if sleep time stam exceeded 
          Serial.println("SPS30 Status: waking up");
          ret = sps30.wakeup(); 
          if (ret != ERR_OK) { 
            Serial.println("SPS30 ERROR: Could not wakeup");
            stateSPS30 = HAS_ERROR; 
          } else {
            wakeSPS30 = currentTime;
            stateSPS30 = IS_WAKINGUP;
          }
        } // time interval
        break; 
      } // case is sleeping
        
      case IS_WAKINGUP : {  // this is only used in energy saving / slow mode
        if ((currentTime - wakeSPS30) >= 50) { // Give some time (50ms)to wake up 
          ret = sps30.start(); 
          if (ret != ERR_OK) { 
            Serial.println("SPS30 ERROR: Could not start SPS30 measurements");
            stateSPS30 = HAS_ERROR; 
          } else {
            Serial.println("SPS30 Status: started");
            stateSPS30 = IS_BUSY;
          }
        }        
        break;
      }

      case HAS_ERROR : {
        // What are we going to do about that? Reset
        Serial.println("SPS30 Status: error, resetting");
        ret = sps30.reset(); 
        if (ret != ERR_OK) { 
          Serial.println("SPS30 ERROR: Could not reset" ); 
          stateSPS30  = HAS_ERROR;
          sps30_avail = false; 
          break;
        }
        Serial.println("SPS30 Status: reset successful");
        if (sps30.start() == true) { 
          Serial.println("SPS30: measurement started");
          stateSPS30 = IS_BUSY; 
        } else { 
          Serial.println("SPS30: could NOT start measurement"); 
          stateSPS30 = HAS_ERROR;
          sps30_avail = false;
        }
        break; 
      }
     
    } // end cases
  } // end if available sps30

 
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
      if ((tmpI > 400) && (tmpI < 2000)) {
        //sgp30.getBaseline();
        //mySettings.baselineeCO2_SGP30 = sgp30.baselineCO2;
        //sgp30.setBaseline((uint16_t)mySettings.baselineeCO2_SGP30, (uint16_t)tmpI);
        mySettings.baselineSGP30_valid = 0xF0;
        mySettings.baselinetVOC_SGP30 = (uint16_t)tmpI;
        Serial.print("Calibration point is:  ");
        Serial.println(tmpI);
      } else { Serial.println("Calibration point out of valid Range"); }
    } 
    
    else if (command == "g") {                                       // forced tVOC set setpoint
      //sgp30.getBaseline();
      mySettings.baselineSGP30_valid = 0xF0;
      //mySettings.baselinetVOC_SGP30 = sgp30.baselineTVOC;
      //mySettings.baselineeCO2_SGP30 = sgp30.baselineCO2;
    } 

    ///////////////////////////////////////////////////////////////////
    // SCD30
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
        //ccs811.setBaseline((uint16_t)tmpI);
        mySettings.baselineCCS811_valid = 0xF0;
        mySettings.baselineCCS811 = (uint16_t)tmpI;
        Serial.print("Calibration point is:  ");
        Serial.println(tmpI);
      } else { Serial.println("Calibration point out of valid Range"); }
    } 

    else if (command == "b") {                                      // getbaseline
        //mySettings.baselineCCS811 = ccs811.getBaseline();
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
  lcd.clear();
  if (sps30_avail) {
    sprintf(lcdbuf,"%3.0f",valSPS30.MassPM1);
    lcd.setCursor(PM1_X, PM1_Y);
    lcd.print(lcdbuf);
    
    sprintf(lcdbuf,"%3.0f",valSPS30.MassPM2);
    lcd.setCursor(PM2_X, PM2_Y);
    lcd.print(lcdbuf);
    
    sprintf(lcdbuf,"%3.0f",valSPS30.MassPM4);
    lcd.setCursor(PM4_X, PM4_Y);
    lcd.print(lcdbuf);
    
    sprintf(lcdbuf,"%3.0f",valSPS30.MassPM10);
    lcd.setCursor(PM10_X, PM10_Y);
    lcd.print(lcdbuf);

    if (valSPS30.MassPM2 < 10.0) {
      sprintf(lcdbuf, "%s", "N");
    } else if (valSPS30.MassPM2 < 25.0) {
      sprintf(lcdbuf, "%s", "T");
    } else if (valSPS30.MassPM2 < 65.0) {
      sprintf(lcdbuf, "%s", "P");
    } else {
      sprintf(lcdbuf, "%s", "!");
    }
    lcd.setCursor(PM2_WARNING_X, PM2_WARNING_Y);
    lcd.print(lcdbuf);

    if (valSPS30.MassPM10 < 20.0) {
      sprintf(lcdbuf, "%s", "N");
    } else if (valSPS30.MassPM10 < 50.0) {
      sprintf(lcdbuf, "%s", "T");
    } else if (valSPS30.MassPM10 < 150.0) {
      sprintf(lcdbuf, "%s", "P");
    } else {
      sprintf(lcdbuf, "%s", "!");
    }
    lcd.setCursor(PM10_WARNING_X, PM10_WARNING_Y);
    lcd.print(lcdbuf);
  }// end if avail  
}

/**
 * serialTrigger prints repeated message, then waits for enter
 * to come in from the serial port.
 */
void serialTrigger(char * mess)
{
  Serial.println();

  while (!Serial.available()) {
    Serial.println(mess);
    delay(2000);
  }

  while (Serial.available())
    Serial.read();
}
