//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////
// Air Quality Sensor
//
// An ESP8266 board is the base for gas and air quality related sensors
//
// Requirements:
//  LCD 20x4
//  MLX temperature sensor
// 
// Operation:
//
// Code Modifications:
// 
// Urs Utzinger
// Summer 2020
//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////


bool fastMode = false;       // true: Measure as fast as possible, false: operate in energy efficiency mode

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
                  WAIT_STABLE, HAS_ERROR};
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
unsigned long intervalLCD = 2000;                  // LCD refresh rate
bool lcd_avail = false;                            // is LCD attached?
unsigned long lastLCD;                             // last time LCD was modified
LiquidCrystal_I2C lcd(0x27,20,4);                  // set the LCD address to 0x27 for a 20 chars and 4 line display

// Particle
#define MLX_X              0
#define MLX_Y              0
#define MLXA_X             0
#define MLXA_Y             1
#define MLX_WARNING_X      4
#define MLX_WARNING_Y      0

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
// MLX
/******************************************************************************************************/
#include <SparkFunMLX90614.h>
#define fhDelta 0.5                                      // difference from forehad to oral temperature
#define mlxOffset 1.4                                    // offset to adjust for sensor inaccuracy
#define timeToStableMLX 250                              // 250ms
#define intervalMLXfast 250                              // 0.25 seconds
#define intervalMLXslow 1000                             // 1 second
bool therm_avail    = false;
const float emissivity = 0.98;                           // emissivity of skin
unsigned long intervalMLX;                               // readout intervall 250ms minimum
unsigned long lastMLX;                                   // last time we interacted with sensor
unsigned long sleepTimeMLX;
volatile SensorStates stateMLX = IS_IDLE; 
IRTherm therm;

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
  if (checkI2C(0x5A) == 1) {therm_avail = true;}  else {therm_avail = false;}   // MLX IR sensor

  Serial.print("LCD                  "); if (lcd_avail)    {Serial.println("available");} else {Serial.println("not available");}
  Serial.print("MLX temp             "); if (therm_avail)  {Serial.println("available");} else {Serial.println("not available");}

  /******************************************************************************************************/
  // Intervals
  /******************************************************************************************************/

  if (fastMode) {
    intervalLoop    =   100;  // 0.1 sec
    intervalLCD     =   250;  // 0.25 sec
    intervalRuntime = 60000;  // 1 minute
  } else{
    intervalLoop    =   1000; // 1 sec
    intervalLCD     =   1000; // 1 sec
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
  // Initialize MLX Sensor
  /******************************************************************************************************/
  if (therm_avail == true) {
    if (therm.begin() == 1) { 
      therm.setUnit(TEMP_C); // Set the library's units to Centigrade
      therm.setEmissivity(emissivity);
      Serial.print("MLX: Emissivity: ");
      Serial.println(therm.readEmissivity());
      stateMLX = IS_MEASURING;      
    } else {
      Serial.println("MLX: sensor not detected. Please check wiring."); 
      stateMLX = HAS_ERROR;
      therm_avail = false;
    }
    if (fastMode == true) { 
      intervalMLX  = intervalMLXfast; 
      sleepTimeMLX = 0;
    } else { 
      intervalMLX  = intervalMLXslow;
      sleepTimeMLX = intervalMLX - timeToStableMLX - 50;
    }
    Serial.print("MLX: sleep time is "); 
    Serial.print(sleepTimeMLX); 
    Serial.println("ms");
    Serial.print("MLX: interval time is "); 
    Serial.print(intervalMLX); 
    Serial.println("ms");
    Serial.println("MLX: Initialized");
  }
  /******************************************************************************************************/
  // Initialize Timing System
  /******************************************************************************************************/
  currentTime       = millis();
  lastTime          = currentTime;
  lastMLX           = currentTime;
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
  // MLX Contactless Thermal Sensor
  /******************************************************************************************************/
  // 
  // We should run this every 10ms if object is in proximity or temperature on MLX is > 35C 
  // otherwise once a minute

  if (therm_avail) {
    switch(stateMLX) {
      case IS_MEASURING : {
        if ((currentTime - lastMLX) > intervalMLX) {
          if (therm.read()) {
            Serial.println("MLX: temperature measured.");
            lastMLX = currentTime;
            if (fastMode == false) {
              therm.sleep();
              Serial.println("MLX: sent to sleep.");
              stateMLX = IS_SLEEPING;
            }
          }
        }
        break;
      }

      case IS_SLEEPING : {
        if ((currentTime - lastMLX) > sleepTimeMLX) {
          therm.wake(); // takes 250ms to wake up
          Serial.println("MLX: initiated wake up.");
          stateMLX = IS_MEASURING;
        }
        break;
      }
    } // switch
  } // if avail

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

void helpMenu() {
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
  if (therm_avail) {

    sprintf(lcdbuf,"%4.1f",(therm.object()+mlxOffset));
    lcd.setCursor(MLX_X, MLX_Y);
    lcd.print(lcdbuf);
    Serial.println(therm.object()+mlxOffset);

    sprintf(lcdbuf,"%4.1f",therm.ambient());
    lcd.setCursor(MLXA_X, MLXA_Y);
    lcd.print(lcdbuf);
    Serial.println(therm.ambient());

    // https://www.singlecare.com/blog/fever-temperature/
    // https://www.hopkinsmedicine.org/health/conditions-and-diseases/fever
    // https://www.ncbi.nlm.nih.gov/pmc/articles/PMC7115295/
    if (therm.object() < (35.0 - fhDelta - mlxOffset)) {
      sprintf(lcdbuf, "%s", "L");      
    } else if (therm.object() <= (36.4 - fhDelta - mlxOffset)) {
      sprintf(lcdbuf, "%s", "");
    } else if (therm.object() <  (37.2 - fhDelta - mlxOffset)) {
      sprintf(lcdbuf, "%s", "N");
    } else if (therm.object() <  (38.3 - fhDelta - mlxOffset)) {
      sprintf(lcdbuf, "%s", "T");
    } else if (therm.object() <  (41.5 - fhDelta - mlxOffset)) {
      sprintf(lcdbuf, "%s", "F");
    } else {
      sprintf(lcdbuf, "%s", "!");
    }
    lcd.setCursor(MLX_WARNING_X, MLX_WARNING_Y);
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
