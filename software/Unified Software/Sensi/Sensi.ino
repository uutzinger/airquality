//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////
// Air Quality Sensor
//
// An ESP8266 board is the base for gas and air quality related sensors
//
// Requirements:
//  - LCD 20x4
//  Any of the following sensors
//  - SCD30 Senserion CO2
//  - BME680 Bosch Temp, Humidity, Pressure, VOC
//  - SGP30 Senserione VOC, eCO2
//  - CCS811 Airquality eCO2 tVOC
//  - SPS30 Senserion particle
//  - MLX90614 Melex temp contactless
// NOT IMPEMENTED YET
//  - MAX30105 Maxim pulseox
// 
// WiFi and MQTT NOT IMPLEMENTED YET
// Operation:
//
// Code Modifications:
// 
// Urs Utzinger
// Summer 2020
//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////

bool fastMode = true;      // true: Measure as fast as possible, false: operate in energy efficiency mode
unsigned int dbglevel=2;   // 0 no output, 
                           // 1 boot info and errors 
                           // 2 measurement statements
                           // 3 additional runtime output 
                           // 4 detailed runtime status
/******************************************************************************************************/
// Sensor
/******************************************************************************************************/

#include <Wire.h>

enum SensorStates{IS_IDLE = 0, IS_MEASURING, IS_BUSY, DATA_AVAILABLE, GET_BASELINE, IS_SLEEPING, IS_WAKINGUP, WAIT_STABLE, HAS_ERROR};
// IS_IDLE        the sensor is powered up
// IS_MEASURING   the sensor is creating data autonomously
// IS_BUSY        the sensor is producing data and will not respond to commands
// DATA_AVAILABLE new data is available in sensor registers
// IS_SLEEPING    the sensor or parts of the sensot are in sleep mode
// IS_WAKINGUP    the sensor is getting out of sleep mode
// WAIT_STABLE    
// HAS_ERROR      the communication with the sensor failed

/******************************************************************************************************/
// WiFi and MQTT
/******************************************************************************************************/
// Not completed

#include <ESP8266WiFi.h>
#include <PubSubClient.h>
WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE  (50)
char msg[MSG_BUFFER_SIZE];

/******************************************************************************************************/
// SCD30; Sensirion CO2 sensor
/******************************************************************************************************/
// Response time 20sec
// Bootup time 2sec
// Atomatic Basline Correction might take up to 7 days
// Measurement Interval 2...1800 secs
// Does not have sleep mode, but can set measurement intervals
// 2sec interval requires 19mA average current
// The maximum current during reading is 75mA
//
// Operating sequence:
//
// Sensor is initilized
//   Measurement interval is set
//   Autocalibrate is enabled
//   Temperature offset to compenste self heating, improves humidity reading
//   Pressure is set
// Begin Measureing
// Check if Data Available
//  get CO2, Humidity, Temperature
// Update barometric pressure every once in a while if third party sensor is avaialble
// Repeat at Check if Data Available

#include "SparkFun_SCD30_Arduino_Library.h"
#define intervalSCD30Fast 2000              // measure every 2sec ... 30minutes, default is 4 
#define intervalSCD30Slow 60000             //
#define intervalSCD30Busy 500               // how often to read dataReady when sensor boots up
#define intervalPressureSCD30 60000         // if we have pressure data from other sensor we will provide it to the co2 sensor to improve accuracy in this interval
#define SCD30_RDY D5                        // pin indicating data ready                                       
float scd30_ppm;                            //  co2 concentration from sensor
float scd30_temp;                           // temperature from sensor
float scd30_hum;                            // humidity from sensor
bool scd30_avail = false;                   // do we have this sensor
unsigned long intervalSCD30;                // will bet set at initilization to either Fast or Slow
unsigned long lastSCD30;                    // last time we interacted with sensor
unsigned long lastPressureSCD30;            // last time we updated pressure
unsigned long lastSCD30Busy;
volatile SensorStates stateSCD30 = IS_IDLE;
SCD30 scd30;

void ICACHE_RAM_ATTR handleSCD30Interrupt() { // Interrupt service routine when data ready is signaled
  stateSCD30 = DATA_AVAILABLE;
  if (dbglevel > 3) {Serial.println(F("SCD30: interrupt occured"));}
}

/******************************************************************************************************/
// SPS30; Senserion Particle Sensor
/******************************************************************************************************/
// Sample interval min 1+/-0.04s
// After power up, sensor is idle
// Execute start command to start measurements
// Stop measurement, the sensor goes to idle
// Default cleaning interval is 604800 seconds (one week)
// Supply Current:
// Max              80.  mA
// Measurement Mode 65.  mA
// Idle Mode         0.36mA
// Sleep Mode        0.05mA
// Time to reach stable measurement: 8s for 200 – 3000 #/cm3 particles
//                                  16s for 100 – 200  #/cm3 particles
//                                  30s for  50 – 100  #/cm3 particles
// Sleep mode can be entered when sensor is idle but only with firmware 2.2 and greater
//
// Operation Mode:
//   Init, send start command
//   Read status
//   Read data
//   Determine time to stable
//   Wait until stable
//   Read data
// Slow Mode:
//   Got to idle
//   Got to sleep
//   Wait until sleep exceeded
//   Wakeup
//   Start
//   Goto wait until stable
//
// Fast Mode:
//   Wait until measurement interval exceeded
//   Goto read data
//

#include <sps30.h>
#define intervalSPS30Fast 1000              // minimum is 1 sec
#define intervalSPS30Slow 60000             // system will sleep for intervalSPS30Slow - timetoStable
#define SPS30Debug 0                        // define driver debug: 0 - no messages, 1 - request sending and receiving, 2 - request sending and receiving + show protocol errors */
unsigned long intervalSPS30;                // measurement interval
unsigned long timeSPS30Stable;              // time when readings are stable, is adjusted automatically based on particle counts
unsigned long lastSPS30;                    // last time we interacted with sensor
unsigned long wakeSPS30;                    // time when wakeup was issued
unsigned long wakeTimeSPS30;                // time when sensor is suppoed to be woken up
unsigned long timeToStableSPS30;            // how long it takes to get stable readings automatically pupulated based on total particles
char buf[64];
uint8_t ret, st;
float totalParticles;                       // we need to adjust measurement time depending on total particle concentration
uint32_t autoCleanIntervalSPS30;            // current cleaning settings in sensor
bool sps30_avail = false;                   // do we have this sensor
volatile SensorStates stateSPS30 = IS_BUSY; 
struct sps_values valSPS30;                 // readings from sensor, will hold any readings
SPS30 sps30;
SPS30_version v;

/******************************************************************************************************/
// SGP30; Senserion TVOC, eCO2
/******************************************************************************************************/
// Sampling rate: minimum 1s
// There is no recommended approach to put sensor into sleep mode. Once init command is issued, sensor remains in measurement mode.
// Current Consumption:
//   Measurement mode 49mA
//   Sleep mode 0.002..0.0010 mA
//
// Operation Sequence:
//
//   Power up, puts the sensor in sleep mode
//   Init Command (10ms to complete)
//   Read baseline from EEPROM
//   If valid set baseline
//   Humidity interval exceeded?
//     write humidity from third party sensors to sensor (10ms to complete)
//   Start Measure Airquality Command 
//   Unresponsive for 12ms
//   Initiate Get Airquality Command
//   Wait until measurement interval exceeded
//
// Read Baseline (10ms) periodically from sensors dynamic baseline calculations
// Set Humidity from third party sensor periodically to increase accuracy
//

#include "SparkFun_SGP30_Arduino_Library.h"
#define intervalSGP30Fast 1000              // as low as 10ms, ideal is 1s, gives best accuracy
#define intervalSGP30Slow 1000              // 
#define intervalSGP30Baseline 300000        // Obtain baseline every 5 minutes
#define intervalSGP30Humidity  60000        // Update once a minute
#define warmupSGP30_withbaseline 3600       // 60min 
#define warmupSGP30_withoutbaseline 43200   // 12hrs 
bool sgp30_avail  = false;
bool baslineSGP30_valid = false;
unsigned long lastSGP30;                    // last time we obtained data
unsigned long lastSGP30Humidity;            // last time we upated humidity
unsigned long lastSGP30Baseline;            // last time we obtained baseline
unsigned long intervalSGP30;                // Measure once a second
unsigned long warmupSGP30;
volatile SensorStates stateSGP30 = IS_IDLE; 
SGP30 sgp30;

/******************************************************************************************************/
// CCS811; Airquality CO2 tVOC
/******************************************************************************************************/
// Sensor has 20min equilibrium time and 48hrs burnin time
// Sensor is read through interrupt handling (in this software implementation):
// Power On 20ms
// Mode 0 – Idle (Measurements are disabled in this mode)
// Mode 1 – Constant power mode, IAQ measurement every second
// Mode 2 – Pulse heating mode IAQ measurement every 10 seconds
// Mode 3 – Low power pulse heating mode IAQ measurement every 60 seconds
// Mode 4 – Const power pulse heating mode IAQ measurement every 0.25 seconds
//
// baseline takes 24 hrs to establish, R_A is reference resistance
// Sleep mode: turns off i2c? 0.019 mA
//
// Operating Sequence:
//
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

#include "SparkFunCCS811.h"
#define ccs811ModeFast 1                    // 4=0.25s, 3=60s, 2=10sec, 1=1sec.
#define ccs811ModeSlow 3                    // 
#define baselineCCS811fast 300000           // 5 mins
#define baselineCCS811slow 3600000          // 1 hour
#define updateCCS811Humitityfast 60000      // 1 min
#define updateCCS811Humitityslow 300000     // 5 mins
//#define stablebaseCCS811 86400000           // sensor needs 24hr until baseline stable
#define stablebaseCCS811 43200000           // sensor needs 24hr until baseline stable
#define burninCCS811    172800000           // sensor needs 48hr burin
#define CCS811_INT D7                       // active low interrupt, high to low at end of measurement
#define CCS811_WAKE D6                      // active low wake to wake up sensor
bool ccs811_avail = false;
unsigned long lastCCS811;                   // last time we interacted with sensor
unsigned long lastCCS811Baseline;           // last time we obtained baseline
unsigned long lastCCS811Humidity;           // last time we update Humidity on sensor
unsigned long lastCCS811Interrupt;          // last time we update Humidity on sensor
unsigned long warmupCCS811;                 // sensor needs 20min conditioning 
unsigned long intervalCCS811Baseline;       // Every 10 minutes
unsigned long intervalCCS811Humidity;       // Every 1 minutes
uint8_t ccs811Mode;                                      
volatile SensorStates stateCCS811 = IS_IDLE; 
CCS811 ccs811(0X5B);                        // address pin will need to be set to high

void ICACHE_RAM_ATTR handleCCS811Interrupt() { // interrupt service routine to handle data ready signal
    if (fastMode == true) { 
      stateCCS811 = DATA_AVAILABLE;
    } else { 
      digitalWrite(CCS811_WAKE, LOW);       // set CCS811 to wake up 
      stateCCS811 = IS_WAKINGUP;
      lastCCS811Interrupt = millis();
    }
    if (dbglevel > 3) {Serial.println(F("CCS811: interrupt occured")); }
}

/******************************************************************************************************/
// BME680; Bosch Temp, Humidity, Pressure, VOC
/******************************************************************************************************/
// Continous 1Hz             1s
// Low Power 0.33 Hz         3s
// Ultra Low Power 0.0033 Hz 300s
#include <Adafruit_Sensor.h>
#include "Adafruit_BME680.h"
#define intervalBME680_Fast      1000                  // 1sec
#define intervalBME680_Slow     60000                  // 60sec
#define bme680_TempOversample     BME680_OS_8X         // 1,2,4,8,16x
#define bme680_HumOversample      BME680_OS_2X         // 1,2,4,8,16x
#define bme680_PressureOversample BME680_OS_4X         // 1,2,4,8,16x
#define bme680_FilterSize         BME680_FILTER_SIZE_3 // 0,1,3,7,15,31,63,127 
#define bme680_HeaterTemp         320                  // C
#define bme680_HeaterDuration     150                  // ms
bool bme680_avail = false;
float          bme680_ah = 0;                          // [gr/m^3]
unsigned long  intervalBME680;
unsigned long  lastBME680;                             // last time we interacted with sensor
unsigned long  endTimeBME680;                          // when data will be available
volatile SensorStates stateBME680 = IS_IDLE; 
Adafruit_BME680 bme680;

/******************************************************************************************************/
// MLX contact less tempreture sensor
/******************************************************************************************************/
#include <SparkFunMLX90614.h>
#define fhDelta 0.5                         // difference from forehad to oral temperature
#define timeToStableMLX 250                 // time until stable internal readings
bool therm_avail    = false;
const float emissivity = 0.98;              // emissivity of skin
unsigned long intervalMLX = 1000;           // readout intervall 250ms minimum
unsigned long lastMLX;                      // last time we interacted with sensor
unsigned long sleepTimeMLX;
float mlxOffset = 1.4;                      // offset to adjust for sensor inaccuracy
volatile SensorStates stateMLX = IS_IDLE; 
IRTherm therm;

/******************************************************************************************************/
// MAX30105; pulseox
/******************************************************************************************************/
// Not implemented yet
bool max_avail    = false;
const long intervalMAX30 = 10;              // readout intervall
unsigned long lastMAX;                      // last time we interacted with sensor
volatile SensorStates stateMAX = IS_IDLE; 

/******************************************************************************************************/
// Other
/******************************************************************************************************/
//
int lastPinStat;
unsigned long currentTime;
unsigned long lastTime;                     // last time we updated runtime
unsigned long intervalLoop;                 // how many times a second are we going to check status of devices
unsigned long intervalRuntime;              // how often to we update the uptime counter of the system
unsigned long tmpTime;                      // to measures execution times

/******************************************************************************************************/
// Store Sensor Baseline Data
/******************************************************************************************************/
#include <EEPROM.h>
#define EEPROM_SIZE 1024
//#define saveSettings 604800000              // 7 days
#define saveSettings 43200000               // 12 hrs
int eeprom_address = 0;

unsigned long lastEEPROM;                   // last time we updated EEPROM, should occur every couple days
struct EEPROMsettings {
  unsigned long runTime;
  unsigned int  debuglevel;                 // amount of debug output on serial port
  byte          baselineSGP30_valid;        // 0xF0 is the number for valid
  uint16_t      baselineeCO2_SGP30;         //
  uint16_t      baselinetVOC_SGP30;         //
  byte          baselineCCS811_valid;       //
  uint16_t      baselineCCS811;             // baseline is an internal value, not ppm
  byte          tempOffset_SCD30_valid;     //
  float         tempOffset_SCD30;           // in C
  byte          forcedCalibration_SCD30_valid;
  float         forcedCalibration_SCD30;    // in ppm
  byte          tempOffset_MLX_valid;       //
  float         tempOffset_MLX;             // in C
  char          ssid1[33];                  // 32 bytes max
  char          pw1[65];                    // 64 chars max
  char          ssid2[33];
  char          pw2[65];
  char          ssid3[33];
  char          pw3[65];
  char          mqtt_server[255];           // your mqtt server, requires internet/wifi
};

EEPROMsettings mySettings;

/******************************************************************************************************/
// LCD Display
/******************************************************************************************************/
// Power Consumption: ... have not yet measured it
// 
#include <LiquidCrystal_I2C.h>
unsigned long intervalLCD;                  // LCD refresh rate
bool lcd_avail = false;                     // is LCD attached?
unsigned long lastLCD;                      // last time LCD was modified
LiquidCrystal_I2C lcd(0x27,20,4);           // set the LCD address to 0x27 for a 20 chars and 4 line display

// For display layout see external file
//
// MLX Temperature (for bio display)
#define MLX_X              0
#define MLX_Y              0
#define MLXA_X             0
#define MLXA_Y             1
#define MLX_WARNING_X      4
#define MLX_WARNING_Y      0

// Particle (for air quality display)
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

//CO2 & Pressure (for air quality display)
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

// Humidity and IAQ  (for air quality display)
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

// Temperatures and VOCs (for air quality display)
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
  // D5 is used to read SCD30 CO2 data ready (RDY)
  /******************************************************************************************************/
  // Configure here because I2C bus is turned off when sensor is asleep
  pinMode(CCS811_WAKE, OUTPUT);                // CCS811 not Wake Pin
  pinMode(CCS811_INT, INPUT_PULLUP);           // CCS811 not Interrupt Pin
  digitalWrite(CCS811_WAKE, LOW);              // Set CCS811 to wake
  lastPinStat = digitalRead(CCS811_INT); 
  pinMode(SCD30_RDY, INPUT);                   // interrupt scd30

  /******************************************************************************************************/
  // EEPROM setup and read 
  /******************************************************************************************************/  
  tmpTime = millis();
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(0,mySettings);
  Serial.print(F("EEPROM read in: "));
  Serial.print((millis()-tmpTime));
  Serial.println(F("ms"));

  /******************************************************************************************************/
  // Set Debug Output Level
  /******************************************************************************************************/  
  dbglevel =  mySettings.debuglevel;
  if (dbglevel > 0) { 
    Serial.print(F("Debug level is: "));
    Serial.println(dbglevel);
  }
  
  /******************************************************************************************************/
  // Check which devices are attached to I2C bus
  /******************************************************************************************************/
  if (checkI2C(0x27) == true) {lcd_avail = true;}    else {lcd_avail = false;}     // LCD display
  if (checkI2C(0x57) == true) {max_avail = true;}    else {max_avail = false;}     // MAX 30105 Pulseox & Particle
  if (checkI2C(0x58) == true) {sgp30_avail =true;}   else {sgp30_avail = false;}   // Senserion TVOC eCO2
  if (checkI2C(0x5A) == true) {therm_avail = true;}  else {therm_avail = false;}   // MLX IR sensor
  if (checkI2C(0x5B) == true) {ccs811_avail =true;}  else {ccs811_avail = false;}  // Airquality CO2 tVOC
  if (checkI2C(0x61) == true) {scd30_avail = true;}  else {scd30_avail = false;}   // Senserion CO2
  if (checkI2C(0x69) == true) {sps30_avail = true;}  else {sps30_avail = false;}   // Senserion Particle
  if (checkI2C(0x77) == true) {bme680_avail = true;} else {bme680_avail = false;}  // Bosch Temp, Humidity, Pressure, VOC

  if (dbglevel > 0) {
    Serial.print(F("LCD                  ")); if (lcd_avail)    {Serial.println(F("available"));} else {Serial.println(F("not available"));}
    Serial.print(F("MAX30105             ")); if (max_avail)    {Serial.println(F("available"));} else {Serial.println(F("not available"));}
    Serial.print(F("CCS811 eCO2, tVOC    ")); if (ccs811_avail) {Serial.println(F("available"));} else {Serial.println(F("not available"));}
    Serial.print(F("SGP30 eCO2, tVOC     ")); if (sgp30_avail)  {Serial.println(F("available"));} else {Serial.println(F("not available"));}
    Serial.print(F("MLX temp             ")); if (therm_avail)  {Serial.println(F("available"));} else {Serial.println(F("not available"));}
    Serial.print(F("SCD30 CO2, rH        ")); if (scd30_avail)  {Serial.println(F("available"));} else {Serial.println(F("not available"));}
    Serial.print(F("SPS30 PM             ")); if (sps30_avail)  {Serial.println(F("available"));} else {Serial.println(F("not available"));}
    Serial.print(F("BME680 T, rH, P tVOC ")); if (bme680_avail) {Serial.println(F("available"));} else {Serial.println(F("not available"));}
  }
  
  /******************************************************************************************************/
  // Intervals
  /******************************************************************************************************/

  if (fastMode == true) {
    intervalLoop    =    100;  // 0.1 sec, Main Loop is run 10 times per second
    intervalLCD     =   2000;  // 2 sec, LCD display is updated every 2 seconds
    intervalRuntime =  60000;  // 1 minute, Uptime is updted every minute
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
    if (dbglevel > 0) {
      Serial.println(F("LCD initialized"));
    }
  }

  /******************************************************************************************************/
  // Initialize SCD30 CO2 sensor 
  /******************************************************************************************************/
  if (scd30_avail == true) {
    if (fastMode) { intervalSCD30 = intervalSCD30Fast; } 
    else          { intervalSCD30 = intervalSCD30Slow; }
    if (dbglevel > 1) {
      Serial.print(F("SCD30 Interval: "));
      Serial.println(intervalSCD30);
    }

    if (scd30.begin(Wire, true)) {
      scd30.setMeasurementInterval(uint16_t(intervalSCD30/1000));  // Set number of seconds between measurements: 2 to 1800 seconds (30 minutes)
      scd30.setAutoSelfCalibration(true);                // 
      if (mySettings.tempOffset_SCD30_valid == 0xF0) { scd30.setTemperatureOffset(mySettings.tempOffset_SCD30); }
      mySettings.tempOffset_SCD30 = scd30.getTemperatureOffset();
      if (dbglevel > 1) {
        Serial.print(F("SCD30: current temp offset: "));
        Serial.print(mySettings.tempOffset_SCD30,2);
        Serial.println(F("C"));
      }
      attachInterrupt(digitalPinToInterrupt(SCD30_RDY), handleSCD30Interrupt, RISING);
      stateSCD30 = IS_BUSY;
    } else {
      scd30_avail = false;
      stateSCD30 = HAS_ERROR;
    }
    if (dbglevel > 0) { Serial.println("SCD30: initialized"); }
  }
  
  /******************************************************************************************************/
  // SGP30 Initialize
  /******************************************************************************************************/
  if (sgp30_avail == true){
    if (sgp30.begin() == false) {
      if (dbglevel > 0) { Serial.println(F("No SGP30 Detected. Check connections")); }
      sgp30_avail = false;
      stateSGP30 = HAS_ERROR;
    }
    if (fastMode) { intervalSGP30 = intervalSGP30Fast;}
    else          { intervalSGP30 = intervalSGP30Slow;}
    //Initializes sensor for air quality readings
    sgp30.initAirQuality();
    if (dbglevel > 0) { Serial.println(F("SGP30: measurements initialzed")); }
    stateSGP30 = IS_MEASURING;
    if (mySettings.baselineSGP30_valid == 0xF0) {
      sgp30.setBaseline((uint16_t)mySettings.baselineeCO2_SGP30, (uint16_t)mySettings.baselinetVOC_SGP30);
      if (dbglevel > 1) { Serial.println(F("SGP30: found valid baseline")); }
      warmupSGP30 = millis() + warmupSGP30_withbaseline;
    } else {
      warmupSGP30 = millis() + warmupSGP30_withoutbaseline;
    }
  }

  /******************************************************************************************************/
  // CCS811 Initialize 
  /******************************************************************************************************/
  if (ccs811_avail == true){

    CCS811Core::CCS811_Status_e css811Ret = ccs811.beginWithStatus();
    if (dbglevel > 1) {
      Serial.print(F("CCS811: begin - "));
      Serial.println(ccs811.statusString(css811Ret));
    }

    if (fastMode) { ccs811Mode = ccs811ModeFast; }
    else          { ccs811Mode = ccs811ModeSlow; }

    css811Ret = ccs811.setDriveMode(ccs811Mode);
    if (dbglevel > 1) {
      Serial.print(F("CCS811: mode request set - "));
      Serial.println(ccs811.statusString(css811Ret));
    }

    //Configure and enable the interrupt line, then print error status
    css811Ret = ccs811.enableInterrupts();
    if (dbglevel > 1) {
      Serial.print(F("CCS811: interrupt configuation - "));
      Serial.println(ccs811.statusString(css811Ret));
    }
           
    if (mySettings.baselineCCS811_valid == 0xF0) {
      CCS811Core::CCS811_Status_e errorStatus = ccs811.setBaseline(mySettings.baselineCCS811);
      if (errorStatus == CCS811Core::CCS811_Stat_SUCCESS) { 
        if (dbglevel > 1) { Serial.println(F("CCS811: baseline PROGRAMMED")); }
      } else {
        if (dbglevel > 0) {
          Serial.print(F("CCS811: error writing baseline - "));
          Serial.println(ccs811.statusString(errorStatus));
        }
      }
    }

    if (fastMode == true) {
      intervalCCS811Baseline = baselineCCS811fast; 
      intervalCCS811Humidity = updateCCS811Humitityfast;
    } else {
      intervalCCS811Baseline = baselineCCS811slow; 
      intervalCCS811Humidity = updateCCS811Humitityslow; 
      if (dbglevel > 1) { Serial.println(F("CCS811: it will take about 5 minutes until readings are non-zero.")); }
    }
    
    warmupCCS811 = currentTime + stablebaseCCS811;    
    
    attachInterrupt(digitalPinToInterrupt(CCS811_INT), handleCCS811Interrupt, FALLING);

    if (dbglevel > 0) { Serial.println(F("CCS811: initialized")); }
    stateCCS811 = IS_IDLE;
  }

  /******************************************************************************************************/
  // SPS30 Initialize Particle Sensor
  /******************************************************************************************************/
  if (sps30_avail == true) {
    
    sps30.EnableDebugging(SPS30Debug);

    if (sps30.begin(&Wire) == false) {
      if (dbglevel > 0) { Serial.println(F("SPS30: Sensor not detected in I2C. Please check wiring.")); }
      stateSPS30 = HAS_ERROR;
      sps30_avail = false;
    }

    if (sps30.probe() == false) { 
      if (dbglevel > 0) {
        Serial.println(F("SPS30: could not probe / connect")); 
        Serial.println(F("SGP30: powercycle the system and reopen serial console"));
      }
      stateSPS30 = HAS_ERROR;
      sps30_avail = false;
    } else { 
      if (dbglevel > 1) { Serial.println(F("SPS30: detected")); }
    }

    if (sps30.reset() == false) { 
      if (dbglevel > 0) { Serial.println(F("SPS30: could not reset.")); }
      stateSPS30 = HAS_ERROR;
      sps30_avail = false;
    }  

    // read device info
    if (dbglevel > 1) {
      ret = sps30.GetSerialNumber(buf, 32);
      if (ret == ERR_OK) {
        if (dbglevel > 1) { 
          Serial.print(F("SPS30: serial number : "));
          if(strlen(buf) > 0) { Serial.println(buf); }
          else { Serial.println(F("not available")); }
        }
      } 
      ret = sps30.GetProductName(buf, 32);
      if (ret == ERR_OK)  {
        if (dbglevel >1) {
          Serial.print(F("SPS30: product name  : "));
          if(strlen(buf) > 0)  {Serial.println(buf);}
          else {Serial.println(F("not available"));}
        }
      }
    }
    
    ret = sps30.GetVersion(&v);
    if (ret == ERR_OK) {
      if (dbglevel > 1) {
        Serial.print(F("SPS30: firmware level: ")); Serial.print(v.major);       Serial.print(F(".")); Serial.println(v.minor);
        Serial.print(F("SPS30: hardware level: ")); Serial.println(v.HW_version); 
        Serial.print(F("SPS30: SHDLC protocol: ")); Serial.print(v.SHDLC_major); Serial.print(F(".")); Serial.println(v.SHDLC_minor);
        Serial.print(F("SPS30: library level : ")); Serial.print(v.DRV_major);   Serial.print(F(".")); Serial.println(v.DRV_minor);
      }
    }
    
    // set autocleaning
    ret = sps30.GetAutoCleanInt(&autoCleanIntervalSPS30);
    if (ret == ERR_OK) {
      if (dbglevel > 1) { Serial.print(F("SPS30: current Auto Clean interval: ")); Serial.print(autoCleanIntervalSPS30); Serial.println(F("s")); }
    }

    if (fastMode == true) { 
      intervalSPS30 = intervalSPS30Fast;
    } else { 
      intervalSPS30 = intervalSPS30Slow; 
    }
    
    // start measurement
    if (sps30.start() == true) { 
      if (dbglevel > 0) { Serial.println(F("SPS30: measurement started")); }
      stateSPS30 = IS_BUSY; 
    } else { 
      if (dbglevel > 0) { Serial.println(F("SPS30: could NOT start measurement"));  }
      stateSPS30 = HAS_ERROR;
      sps30_avail = false;
    }

    // addresse issue with limited RAM on some microcontrollers
    if (dbglevel > 0) {
      if (sps30.I2C_expect() == 4) { Serial.println(F("SPS30: !!! Due to I2C buffersize only the SPS30 MASS concentration is available !!!")); }
    }
  } else {
    if (dbglevel > 0) { Serial.println("SPS30: not found!"); }
  } // end if SPS30 available

  /******************************************************************************************************/
  // Initialize BME680
  /******************************************************************************************************/
  if (bme680_avail == true){
    if (bme680.begin() == true) { 
      if (dbglevel > 1) { Serial.print(F("BME680: Setting oversampling for sensors\n")); }
      bme680.setTemperatureOversampling(bme680_TempOversample);
      bme680.setHumidityOversampling(bme680_HumOversample); 
      bme680.setPressureOversampling(bme680_PressureOversample); 
      if (dbglevel > 1) { Serial.print(F("BME680: Setting IIR filter to a value of 3 samples\n")); }
      bme680.setIIRFilterSize(bme680_FilterSize); 
      if (dbglevel > 1) { Serial.print(F("BME680: Setting gas measurement to 320\xC2\xB0\x43 for 150ms\n")); }
      bme680.setGasHeater(bme680_HeaterTemp,bme680_HeaterDuration); 
      stateBME680 = IS_IDLE;      
      if (fastMode == true) { intervalBME680 = intervalBME680_Fast; }
      else                  { intervalBME680 = intervalBME680_Slow; }
      if (dbglevel > 0) { Serial.println(F("BME680: Initialized")); }
    } else {
      if (dbglevel > 0) { Serial.println(F("BME680: Sensor not detected. Please check wiring.")); }
      stateBME680 = HAS_ERROR;
      bme680_avail = false;
    }   
  } // end if bme680 available

  /******************************************************************************************************/
  // Initialize MLX Sensor
  /******************************************************************************************************/
  if (therm_avail == true) {
    if (therm.begin() == 1) { 
      therm.setUnit(TEMP_C); // Set the library's units to Centigrade
      therm.setEmissivity(emissivity);
      if (dbglevel > 1) { Serial.print(F("MLX: Emissivity: ")); Serial.println(therm.readEmissivity()); }
      stateMLX = IS_MEASURING;      
    } else {
      if (dbglevel > 0) { Serial.println(F("MLX: sensor not detected. Please check wiring.")); }
      stateMLX = HAS_ERROR;
      therm_avail = false;
    }
    sleepTimeMLX = intervalMLX - timeToStableMLX - 50;
    if (dbglevel > 1) {
      Serial.print(F("MLX: sleep time is ")); Serial.print(sleepTimeMLX);  Serial.println(F("ms"));
      Serial.print(F("MLX: interval time is ")); Serial.print(intervalMLX); Serial.println(F("ms"));
    }

    if (mySettings.tempOffset_MLX_valid == 0xF0) {
      mlxOffset = mySettings.tempOffset_MLX;
      if (dbglevel > 1) { Serial.println(F("MLX: offset found"));}
    }

    if (dbglevel > 0) { Serial.println(F("MLX: Initialized")); }
  } // end if therm available
  
  /******************************************************************************************************/
  // Initialize MAX Pulse OX Sensor
  /******************************************************************************************************/
  if (max_avail == true) {
    //TO DO
  }

  /******************************************************************************************************/
  // Initialize Timing System
  /******************************************************************************************************/
  currentTime         = millis();
  lastTime            = currentTime;
  lastSCD30           = currentTime;
  lastLCD             = currentTime;
  lastPressureSCD30   = currentTime;
  lastEEPROM          = currentTime;
  lastMAX             = currentTime;
  lastMLX             = currentTime;
  lastCCS811          = currentTime;
  lastCCS811Humidity  = currentTime;
  lastCCS811Interrupt = currentTime;
  lastSCD30           = currentTime;
  lastSCD30Busy       = currentTime;
  lastSPS30           = currentTime;
  lastBME680          = currentTime;
  lastSGP30           = currentTime;
  lastSGP30Humidity   = currentTime;
  lastSGP30Baseline   = currentTime;

  /******************************************************************************************************/
  // Populate LCD screen
  /******************************************************************************************************/

  if (lcd_avail == true) {
    tmpTime = millis();
    updateLCD();
    lastLCD = currentTime;
    if (dbglevel > 0) { Serial.print(F("LCD updated in ")); Serial.print((millis()-tmpTime));  Serial.println(F("ms")); }
  }
  
  if (dbglevel > 0) { Serial.println(F("System initialized")); }
  
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
      if (dbglevel > 1) {
        Serial.print(F("LCD updated in "));
        Serial.print((millis()-tmpTime));  
        Serial.println(F("ms"));
      }
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
    if (dbglevel > 1) { 
      Serial.print("SCD30 RDY pin changed: ");
      Serial.print(lastPinStat);
      Serial.print(" - ");
      Serial.println(pinStat);
    }
    lastPinStat = pinStat;
  }
  */

  /******************************************************************************************************/
  // SCD30 Sensirion CO2 sensor
  /******************************************************************************************************/

  if (scd30_avail) {
    switch(stateSCD30) {
      case IS_MEASURING : { // used without RDY pin interrupt
        if ((currentTime - lastSCD30) >= intervalSCD30) {
          if (scd30.dataAvailable()) {
            scd30_ppm  = scd30.getCO2();
            scd30_temp = scd30.getTemperature();
            scd30_hum  = scd30.getHumidity();
            lastSCD30  = currentTime;
            if (dbglevel > 1) { Serial.println(F("SCD30: data read")); }
          } else {
            if (dbglevel > 1) { Serial.println(F("SCD30 data not yet available")); }
          }
        }
        break;
      } // is measuring

      case IS_BUSY: { // used to bootup sensor when RDY pin is used
        if ((currentTime - lastSCD30Busy) > intervalSCD30Busy) {
          scd30.dataAvailable(); // without checking status, RDY pin will not switch
          lastSCD30Busy = currentTime;
          if (dbglevel > 3) { Serial.println(F("SCD30: is busy")); }
        }
        break;
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
            scd30.setAmbientPressure(uint16_t(bme680.pressure/100.0)); 
            // update with value from pressure sensor, needs to be mbar
            lastPressureSCD30 = currentTime;
            if (dbglevel > 1) { Serial.print(F("SCD30: pressure updated to")); Serial.print(bme680.pressure/100.0); Serial.println(F("mbar")); }
          }
        }
        break;        
      }
      
      case HAS_ERROR : {
        if (dbglevel > 0) { Serial.println(F("SCD30 has error!")); }
        break;
      }
    } // switch state
  } // if available


  /******************************************************************************************************/
  // SGP30 Sensirion eCO2 sensor
  /******************************************************************************************************/

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
            if (dbglevel > 1) { Serial.println(F("SGP30: humidity updated for eCO2")); }
          }
          lastSGP30Humidity = currentTime;        
        } // end humidity update
        
        if ((currentTime - lastSGP30Baseline) > intervalSGP30Baseline) {
          sgp30.startGetBaseline(); // this has 10ms delay
          if (dbglevel > 2) { Serial.println(F("SGP30: getting baseline")); }
          lastSGP30Baseline= millis();
          stateSGP30 = GET_BASELINE;
          break;
        }

        if ((currentTime - lastSGP30) > intervalSGP30) {
          sgp30.startAirQuality();
          if (dbglevel > 3) { Serial.println(F("SGP30: airquality started")); }
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
          if (dbglevel > 1) { Serial.println(F("SGP30 eCO2 & tVOC measured")); }
        }
        break;
      }
      
      case GET_BASELINE : {
        if ((currentTime - lastSGP30Baseline) > 10) {
          if (sgp30.finishGetBaseline() == SUCCESS) {
            stateSGP30 = IS_MEASURING; 
            if (dbglevel > 1) { Serial.println(F("SGP30: baseline obtained")); }
          } else { stateSGP30 = HAS_ERROR; }
        }
        break;
      }
        
    } // switch state
  } // if available

  /******************************************************************************************************/
  // CCS811
  /******************************************************************************************************/

  if (ccs811_avail) {
    switch(stateCCS811) {

      case IS_WAKINGUP : { // ISR will activate this
        if ((currentTime - lastCCS811Interrupt) >= 1) { // wakeup time is 50micro seconds
          stateCCS811 = DATA_AVAILABLE;
          if (dbglevel > 3) { Serial.println(F("CCS811: wakeup completed")); }
        }
        break;
      }

      case DATA_AVAILABLE : {
        tmpTime = millis();
        ccs811.readAlgorithmResults(); //Calling this function updates the global tVOC and CO2 variables
        if (dbglevel > 1) { Serial.print(F("CCS811: readAlgorithmResults completed in ")); Serial.print((millis()-tmpTime)); Serial.println(F("ms")); }
        uint8_t error = ccs811.getErrorRegister();
        if (dbglevel > 0) {
          if (error == 0xFF) { Serial.println(F("CCS811: failed to read ERROR_ID register")); }
          else  {
            if (error & 1 << 5) Serial.print(F("CCS811 Error: HeaterSupply"));
            if (error & 1 << 4) Serial.print(F("CCS811 Error: HeaterFault"));
            if (error & 1 << 3) Serial.print(F("CCS811 Error: MaxResistance"));
            if (error & 1 << 2) Serial.print(F("CCS811 Error: MeasModeInvalid"));
            if (error & 1 << 1) Serial.print(F("CCS811 Error: ReadRegInvalid"));
            if (error & 1 << 0) Serial.print(F("CCS811 Error: MsgInvalid"));
          }
        }

        if ((currentTime - lastCCS811Baseline) >= intervalCCS811Baseline ) {
          tmpTime = millis();
          mySettings.baselineCCS811 = ccs811.getBaseline();
          lastCCS811Baseline = currentTime;
          if (dbglevel > 1) { Serial.print(F("CCS811: baseline obtained in")); Serial.print((millis()-tmpTime)); Serial.println(F("ms")); }
        }
        
        if ((currentTime - lastCCS811Humidity) > intervalCCS811Humidity ){
          lastCCS811Humidity = currentTime;
          if (bme680_avail == true) {
            tmpTime = millis();
            ccs811.setEnvironmentalData(bme680.humidity, bme680.temperature);
            if (dbglevel > 1) { Serial.print(F("CCS811: humidity and temperature compensation updated in "));  Serial.print((millis()-tmpTime)); Serial.println(F("ms")); }
          }
        }  

        if (fastMode == false) { 
          digitalWrite(CCS811_WAKE, HIGH);     // put CCS811 to sleep
          if (dbglevel > 3) { Serial.println(F("CCS811: puttting sensor to sleep")); }
          stateCCS811 = IS_SLEEPING;
        } else {
          stateCCS811 = IS_IDLE;
        }
        
        lastCCS811 = currentTime;
        break;
      }

      case IS_IDLE : {
        // Continue Sleeping, we are waiting for interrupt
        if (dbglevel > 3) { Serial.println(F("CCS811: is idle")); }
        break;
      }
      
      case IS_SLEEPING : {
        // Continue Sleeping, we are waiting for interrupt
        if (dbglevel > 3) { Serial.println(F("CCS811: is sleeping")); }
        break;
      }
 
    } // end switch
  } // end if avail ccs811

  /******************************************************************************************************/
  // SPS30 Sensirion Particle Sensor State Machine
  /******************************************************************************************************/

  if (sps30_avail) {
    
    switch(stateSPS30) { 
      
      case IS_BUSY: {
        if (dbglevel > 3) {  Serial.println(F("SPS30: is busy")); }
        if (((v.major==2) && (v.minor>=2)) || (v.major>2)) {
          ret = sps30.GetStatusReg(&st);         // takes 20ms
          if (ret == ERR_OK) {
            if (dbglevel > 2) { Serial.print(F("SPS30: reading status completed and ")); }
            if (st == STATUS_OK) { 
              if (dbglevel > 2) { Serial.println(F("SPS30 Status: ok")); }
            } else {
              if (dbglevel > 0) {
                if (st & STATUS_SPEED_ERROR) { Serial.println(F("SPS30 WARNING : Fan is turning too fast or too slow")); }
                if (st & STATUS_LASER_ERROR) { Serial.println(F("SPS30 ERROR: Laser failure")); }  
                if (st & STATUS_FAN_ERROR)   { Serial.println(F("SPS30 ERROR: Fan failure, fan is mechanically blocked or broken")); }
              }
              stateSPS30 = HAS_ERROR;
            } // status ok
          } else { 
            if (dbglevel > 0) { Serial.print(F("SPS30: could not read status")); } 
          }// read status
        } // Firmware >= 2.2
        tmpTime = millis();
        ret = sps30.GetValues(&valSPS30);               
        if (dbglevel > 1) { Serial.print(F("SPS30: values read in ")); Serial.print(millis() - tmpTime); Serial.println(F("ms")); }
        if (ret == ERR_DATALENGTH) { 
          if (dbglevel > 0) {  Serial.println(F("SPS30 ERROR: Data length during reading values")); }
        } else if (ret != ERR_OK) {
          if (dbglevel > 0) { Serial.print(F("SPS30 ERROR: reading values: ")); Serial.println(ret); }
        }
        lastSPS30 = currentTime; 
        // adjust time to get stable readings, it takes longer  with lower concentration to get a precise reading
        totalParticles = (valSPS30.NumPM0 + valSPS30.NumPM1 + valSPS30.NumPM2 + valSPS30.NumPM4 + valSPS30.NumPM10);
        if (totalParticles < 100)      { timeToStableSPS30 = 30000; } // hard coded from data sheet
        else if (totalParticles < 200) { timeToStableSPS30 = 16000; } // hard coded from data sheet
        else                           { timeToStableSPS30 =  8000; } // hard coded from data sheet
        timeSPS30Stable = currentTime +  timeToStableSPS30;
        if (dbglevel > 2) { Serial.print(F("SPS30: total particles - ")); Serial.println(totalParticles); }
        if (dbglevel > 3) {
          Serial.print(F("Current Time: ")); Serial.println(currentTime);
          Serial.print(F("SPS30: time when stable - ")); Serial.println(timeSPS30Stable);
        }
        stateSPS30 = WAIT_STABLE;            
        break; 
      } // is BUSY

      case WAIT_STABLE: {
        if (dbglevel > 3) {
          Serial.println(F("SPS30: wait until stable"));
          Serial.print(F("Current Time: ")); Serial.println(currentTime);
          Serial.print(F("SPS30: time when stable - ")); Serial.println(timeSPS30Stable);
        }
        if (currentTime >= timeSPS30Stable) {
          if (((v.major==2) && (v.minor>=2)) || (v.major>2)) {
            ret = sps30.GetStatusReg(&st);         // takes 20ms
            if (ret == ERR_OK) {
              if (dbglevel > 2) { Serial.print(F("SPS30: reading status completed and ")); }
              if (st == STATUS_OK) {
                if (dbglevel > 2) { Serial.println(F("status is ok"));}
              } else {
                if (dbglevel > 0) {
                  if (st & STATUS_SPEED_ERROR) { Serial.println(F("(WARNING) fan is turning too fast or too slow")); }
                  if (st & STATUS_LASER_ERROR) { Serial.println(F("(ERROR) laser failure")); }  
                  if (st & STATUS_FAN_ERROR)   { Serial.println(F("(ERROR) fan failure, fan is mechanically blocked or broken")); }
                }
              } // status ok
            } else {
              if (dbglevel > 0) { Serial.print(F("SPS30: could not read status")); }
            } // read status
          } // Firmware >= 2.2
          tmpTime = millis();
          ret = sps30.GetValues(&valSPS30);
          if (dbglevel > 1) {
            Serial.print(F("SPS30: values read in ")); 
            Serial.print(millis() - tmpTime); 
            Serial.println(F("ms"));
          }
          if (ret == ERR_DATALENGTH) { 
            if (dbglevel > 0) { Serial.println(F("SPS30 ERROR: Data length during reading values")); }
          } else if (ret != ERR_OK) {
            if (dbglevel > 0) { Serial.print(F("SPS ERROR: reading values: ")); Serial.println(ret); }
          }
          if (dbglevel > 3) { Serial.println(F("SPS30: going to idle")); }
          lastSPS30 = currentTime; 
          stateSPS30 = IS_IDLE;          
        } //if time stable
        break;
      } // wait stable
                  
      case IS_IDLE : {
        if (dbglevel > 3) { Serial.println("SPS30: is idle"); }
        if ((v.major<2) || (fastMode)) {
          timeSPS30Stable = (unsigned long) (lastSPS30 + intervalSPS30);
          if (dbglevel > 3) {
            Serial.print(F("Last SPS30: ")); Serial.println(lastSPS30);
            Serial.print(F("Current Time: ")); Serial.println(currentTime);
            Serial.print(F("SPS30: time when stable - ")); Serial.println(timeSPS30Stable);
            Serial.println(F("SPS30: going to wait until stable"));
          }
          stateSPS30 = WAIT_STABLE;             
        } else {
          wakeTimeSPS30 = (unsigned long) (lastSPS30 + intervalSPS30 - 50 - timeToStableSPS30);
          ret = sps30.sleep(); // 5ms execution time
          if (ret != ERR_OK) { 
            if (dbglevel > 0) { Serial.println(F("SPS30 ERROR: could not go to sleep")); }
            stateSPS30 = HAS_ERROR;
          }
          if (dbglevel > 3) { Serial.println(F("SPS30: going to sleep")); }
          stateSPS30 = IS_SLEEPING;
        }
        break;
      }
        
      case IS_SLEEPING : {
        if (dbglevel > 3) { Serial.println(F("SPS30: is sleepig")); }
        if (currentTime >= wakeTimeSPS30) { // Wake up if sleep time exceeded 
          if (dbglevel > 3) { Serial.println(F("SPS30 Status: waking up")); }
          ret = sps30.wakeup(); 
          if (ret != ERR_OK) {
            if (dbglevel > 0) { Serial.println(F("SPS30 ERROR: could not wakeup")); }
            stateSPS30 = HAS_ERROR; 
          } else {
            wakeSPS30 = currentTime;
            stateSPS30 = IS_WAKINGUP;
          }
        } // time interval
        break; 
      } // case is sleeping
        
      case IS_WAKINGUP : {  // this is only used in energy saving / slow mode
        if (dbglevel > 3) { Serial.println(F("SPS30: is waking up")); }
        if ((currentTime - wakeSPS30) >= 50) { // Give some time (50ms)to wake up 
          ret = sps30.start(); 
          if (ret != ERR_OK) { 
            if (dbglevel > 0) { Serial.println(F("SPS30 ERROR: Could not start SPS30 measurements")); }
            stateSPS30 = HAS_ERROR; 
          } else {
            if (dbglevel > 1) { Serial.println(F("SPS30 Status: started")); }
            stateSPS30 = IS_BUSY;
          }
        }        
        break;
      }

      case HAS_ERROR : {
        // What are we going to do about that? Reset
        if (dbglevel > 0) { Serial.println(F("SPS30 Status: error, going to reset")); }
        ret = sps30.reset(); 
        if (ret != ERR_OK) { 
          if (dbglevel > 0) { Serial.println(F("SPS30 ERROR: Could not reset")); }
          stateSPS30  = HAS_ERROR;
          sps30_avail = false; 
          break;
        }
        if (dbglevel > 1) { Serial.println(F("SPS30 Status: reset successful")); }
        if (sps30.start() == true) { 
          if (dbglevel > 1) { Serial.println(F("SPS30: measurement started")); }
          stateSPS30 = IS_BUSY; 
        } else { 
          if (dbglevel > 0) { Serial.println(F("SPS30: could NOT start measurement")); }
          stateSPS30 = HAS_ERROR;
          sps30_avail = false;
        }
        break; 
      }
          
    } // end cases
  } // end if available sps30


  /******************************************************************************************************/
  // BME680, Hum, Press, Temp, Gasresistance
  /******************************************************************************************************/

  if (bme680_avail == true) {
    switch(stateBME680) { 
      
      case IS_IDLE : {
        if ((currentTime - lastBME680) >= intervalBME680) {
          tmpTime = millis();
          endTimeBME680 = bme680.beginReading();
          lastBME680 = currentTime;
          if (endTimeBME680 == 0) {
            if (dbglevel > 0) { Serial.println(F("BME680: failed to begin reading")); }
            stateBME680 = HAS_ERROR; 
          } else {
            stateBME680 = IS_BUSY; 
          }
          if (dbglevel > 1) { Serial.print(F("BME680 reading started. Completes in ")); Serial.print(endTimeBME680-tmpTime); Serial.println(F("ms.")); }
        }
        break;
      }
      
      case IS_BUSY : {
        if (currentTime > endTimeBME680) {
          stateBME680 = DATA_AVAILABLE;
        }
        break;
      }

      case DATA_AVAILABLE : {
        if (bme680.endReading() ==  false) {
          if (dbglevel > 1) { Serial.println(F("BME680: Failed to complete reading")); }
        } else {
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
          float tmp = 273.15 + bme680.temperature;
          bme680_ah = bme680.humidity * 13.246 / tmp * exp(19.854 - 5423.0/tmp); // [gr/m^3]
          if ( (bme680_ah<0) | (bme680_ah>40.0) ) { bme680_ah = -1.0; } // make sure its reasonable
          // DewPoint
          // https://en.wikipedia.org/wiki/Dew_point
          // T Celsius
          // a = 6.1121 mbar, b = 18.678, c = 257.14 °C, d = 234.5 °C.
          // Tdp = c g_m / (b - g_m)
          // g_m = ln(RH/100) + (b-T/d)*(T/(c+T)))
          // NEED TO IMPLEMENT IF DEWPOINT is WANTED
          stateBME680 = IS_IDLE;
          if (dbglevel > 1) { Serial.println(F("BME680; readout completed")); }
        }
        break;
      }
                            
      case HAS_ERROR : {
        // What are we going to do about that?
        break; 
      }
     
    } // end cases
  } // end if available

  /******************************************************************************************************/
  // MLX Contactless Thermal Sensor
  /******************************************************************************************************/

  if (therm_avail) {
    switch(stateMLX) {
      case IS_MEASURING : {
        if ((currentTime - lastMLX) > intervalMLX) {
          if (therm.read()) {
            if (dbglevel > 1) { Serial.println(F("MLX: temperature measured")); }
            lastMLX = currentTime;
            if (fastMode == false) {
              therm.sleep();
              if (dbglevel > 3) { Serial.println(F("MLX: sent to sleep")); }
              stateMLX = IS_SLEEPING;
            }
          }
        }
        break;
      }

      case IS_SLEEPING : {
        if ((currentTime - lastMLX) > sleepTimeMLX) {
          therm.wake(); // takes 250ms to wake up
          if (dbglevel > 3) { Serial.println(F("MLX: initeated wake up")); }
          stateMLX = IS_MEASURING;
        }
        break;
      }
    } // switch
  } // if avail
  
  /******************************************************************************************************/
  // MAX Pulse OX Sensor
  /******************************************************************************************************/
  if (max_avail == true) {
    //TO DO
  }

  /******************************************************************************************************/
  // Time Management
  /******************************************************************************************************/
  // Update runtime every minute ***********************************************
  if ((currentTime - lastTime) >= intervalRuntime) {
    mySettings.runTime = mySettings.runTime + ((currentTime - lastTime) / 1000);
    lastTime = currentTime;
    if (dbglevel > 0) { Serial.println(F("Runtime updated")); }
  }
  // Update EEPROM once a week *************************************************
  if ((currentTime - lastEEPROM) >= saveSettings) {
    EEPROM.put(0,mySettings);
    if (EEPROM.commit()) {    
      lastEEPROM = currentTime;
      if (dbglevel > 0) { Serial.println(F("EEPROM updated")); }
    }
  } 
  // Copy CCS811 basline to settings when warmup is finished *******************
  if (currentTime >= warmupCCS811) {    
    mySettings.baselineCCS811 = ccs811.getBaseline();
    mySettings.baselineCCS811_valid = 0xF0;
    if (dbglevel > 1) { Serial.println(F("CCS811 baseline placed into settings")); }
    warmupCCS811 = warmupCCS811 + stablebaseCCS811; 
  }
  // Copy SGP30 basline to settings when warmup is finished *******************
  if (currentTime >=  warmupSGP30) {
    mySettings.baselinetVOC_SGP30 = sgp30.baselineTVOC;
    mySettings.baselineeCO2_SGP30 = sgp30.baselineCO2;
    if (dbglevel > 1) { Serial.println(F("SGP30 baseline setting updated")); }
    warmupSGP30 = warmupSGP30 + intervalSGP30Baseline; 
  }
  //  Put microcontroller to sleep **********************************************
  long sleepDuration = (long)millis() - (long)(currentTime + intervalLoop);
  if (sleepDuration > 0) {
    // Change this to enable low power sleep:
    delay(sleepDuration);
  }
  
} // loop

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////
// Support functions
//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////

bool checkI2C(byte address) {
  Wire.beginTransmission(address);
  if (Wire.endTransmission() == 0) { return true; } else { return false; }
}

/**************************************************************************************/
// Serial Input: Support Routines
/**************************************************************************************/
void helpMenu() {
  Serial.println(F("All Sensors==========================="));
  Serial.println(F("z: print all sensor data"));
  Serial.println(F("Debug Level==========================="));
  Serial.println(F("l: set debug level 0..4, l1"));
  Serial.println(F("SGP30=eCO2============================"));
  Serial.println(F("e: force eCO2, e400"));
  Serial.println(F("t: force tVOC, t3000"));  
  Serial.println(F("g: get baselines"));
  Serial.println(F("SCD30=CO2============================="));
  Serial.println(F("f: force CO2, f400.0 in ppm"));
  Serial.println(F("t: force temperature offset, t5.0 in C"));
  Serial.println(F("CCS811=eCO2==========================="));
  Serial.println(F("c: force basline, c400"));
  Serial.println(F("b: get baseline"));
  Serial.println(F("MLX================================"));
  Serial.println(F("m: force temp, m400"));
  Serial.println(F("EEPROM================================"));
  Serial.println(F("s: save to EEPROM"));
  Serial.println(F("r: read from EEPROM"));
  Serial.println(F("p: print current settings"));
  Serial.println(F("d: create default settings"));
  Serial.println(F("1: SSID 1, 1myssid"));
  Serial.println(F("2: SSID 2, 2myssid"));
  Serial.println(F("3: SSID 3, 3myssid"));
  Serial.println(F("4: password SSID 1, 1mypas"));
  Serial.println(F("5: password SSID 2, 2mypas"));
  Serial.println(F("6: password SSID 3, 3mypas")); 
  Serial.println(F("9: mqtt server, 9my,mqtt.com")); 
  Serial.println(F("=========================Urs Utzinger="));
}

void printSettings() {
  Serial.print(F("Debug level: .................. ")); Serial.println((unsigned int)mySettings.debuglevel);
  Serial.print(F("Runtime [min]: ................ ")); Serial.println((unsigned long)mySettings.runTime/60);
  Serial.print(F("Base SGP30 valid: ............. ")); Serial.println((int)mySettings.baselineSGP30_valid);
  Serial.print(F("Base eCO2 SGP30: [ppm]......... ")); Serial.println((int)mySettings.baselineeCO2_SGP30);
  Serial.print(F("Base tVOC SGP30: [ppm]......... ")); Serial.println((int)mySettings.baselinetVOC_SGP30);
  Serial.print(F("Base CCS811 valid: ............ ")); Serial.println((int)mySettings.baselineCCS811_valid);
  Serial.print(F("Base CCS811: [Ohm]............. ")); Serial.println((int)mySettings.baselineCCS811);
  Serial.print(F("Temp Offset SCD30 valid: ...... ")); Serial.println((int)mySettings.tempOffset_SCD30_valid);
  Serial.print(F("Temp Offset SCD30: [C]......... ")); Serial.println((float)mySettings.tempOffset_SCD30);
  Serial.print(F("Forced Calibration SCD30 valid: ")); Serial.println((int)mySettings.forcedCalibration_SCD30_valid);
  Serial.print(F("Forced Calibration SCD30:[ppm]. ")); Serial.println((float)mySettings.forcedCalibration_SCD30);
  Serial.print(F("MLX Temp Offset valid: ........ ")); Serial.println((int)mySettings.tempOffset_MLX_valid);
  Serial.print(F("MLX Temp Offset: .............. ")); Serial.println((float)mySettings.tempOffset_MLX);
  Serial.print(F("SSID: ......................... ")); Serial.println(mySettings.ssid1);
  Serial.print(F("PW: ........................... ")); Serial.println(mySettings.pw1);
  Serial.print(F("SSID: ......................... ")); Serial.println(mySettings.ssid2);
  Serial.print(F("PW: ........................... ")); Serial.println(mySettings.pw2);
  Serial.print(F("SSID: ......................... ")); Serial.println(mySettings.ssid3);
  Serial.print(F("PW: ........................... ")); Serial.println(mySettings.pw3);
  Serial.print(F("MQTT: ......................... ")); Serial.println(mySettings.mqtt_server);
}

void defaultSettings() {
  mySettings.runTime                       = (unsigned long)   0;
  mySettings.debuglevel                    = (unsigned int)    1;
  mySettings.baselineSGP30_valid           = (byte)         0x00;
  mySettings.baselineeCO2_SGP30            = (uint16_t)        0;
  mySettings.baselinetVOC_SGP30            = (uint16_t)        0;
  mySettings.baselineCCS811_valid          = (byte)         0x00;
  mySettings.baselineCCS811                = (uint16_t)        0;
  mySettings.tempOffset_SCD30_valid        = (byte)         0x00;
  mySettings.tempOffset_SCD30              = (float)        0.01;
  mySettings.forcedCalibration_SCD30_valid = (byte)         0x00;
  mySettings.forcedCalibration_SCD30       = (float)         0.0;
  mySettings.tempOffset_MLX_valid          = (byte)         0x00;
  mySettings.tempOffset_MLX                = (float)         0.0;
  strcpy(mySettings.ssid1,                 "meddev");
  strcpy(mySettings.pw1,                   "");
  strcpy(mySettings.ssid2,                 "AMARIE-HP_Network");
  strcpy(mySettings.pw2,                   "");
  strcpy(mySettings.ssid3,                 "MuchoCoolioG");
  strcpy(mySettings.pw3,                   "");
  strcpy(mySettings.mqtt_server,           "my.mqqtt.server.org");
}

/**************************************************************************************/
// Serial Input: Handle Commmands
/**************************************************************************************/
void inputHandle()
{
  char inBuff[] = "0123456789ABCDEF0123456789ABCDEF";
  uint16_t tmpI;
  float tmpF;
  int bytesread;
  String value = "20000.0";
  String command = " ";
  
  if (Serial.available()) {
    Serial.setTimeout(1 ); // Serial read timeout
    bytesread=Serial.readBytesUntil('\n', inBuff, 33);               // Read from serial until CR is read or timeout exceeded
    inBuff[bytesread]='\0';
    String instruction = String(inBuff);
    command = instruction.substring(0,1); 
    if (bytesread > 1) { // we have also a value
      value = instruction.substring(1,bytesread); 
    }

    if (command == "l") {                                            // set debug level
      tmpI = value.toInt();
      if ((tmpI >= 0) && (tmpI <= 10)) {
        dbglevel = (unsigned long)tmpI;
        mySettings.debuglevel = dbglevel;
        Serial.print(F("Debugl level set to:  "));
        Serial.println(mySettings.debuglevel);
      } else { Serial.println(F("Debug level out of valid Range")); }
    } 

    else if (command == "z") {                                       // dump all sensor data
      printSensors();
    } 

    ///////////////////////////////////////////////////////////////////
    // SGP30
    ///////////////////////////////////////////////////////////////////
    else if (command == "e") {                                        // forced CO2 set setpoint
      tmpI = value.toInt();
      if ((tmpI >= 400) && (tmpI <= 2000)) {
        sgp30.getBaseline();
        mySettings.baselinetVOC_SGP30 = sgp30.baselineTVOC;
        sgp30.setBaseline((uint16_t)tmpI, (uint16_t)mySettings.baselinetVOC_SGP30);
        mySettings.baselineSGP30_valid = 0xF0;
        mySettings.baselineeCO2_SGP30 = (uint16_t)tmpI;
        Serial.print(F("Calibration point is:  "));
        Serial.println(tmpI);
      } else { Serial.println(F("Calibration point out of valid Range")); }
    } 

    else if (command == "t") {                                       // forced tVOC set setpoint
      tmpI = value.toInt();
      if ((tmpI > 400) && (tmpI < 2000)) {
        sgp30.getBaseline();
        mySettings.baselineeCO2_SGP30 = sgp30.baselineCO2;
        sgp30.setBaseline((uint16_t)mySettings.baselineeCO2_SGP30, (uint16_t)tmpI);
        mySettings.baselineSGP30_valid = 0xF0;
        mySettings.baselinetVOC_SGP30 = (uint16_t)tmpI;
        Serial.print(F("Calibration point is:  "));
        Serial.println(tmpI);
      } else { Serial.println(F("Calibration point out of valid Range")); }
    } 
    
    else if (command == "g") {                                       // forced tVOC set setpoint
      sgp30.getBaseline();
      mySettings.baselineSGP30_valid = 0xF0;
      mySettings.baselinetVOC_SGP30 = sgp30.baselineTVOC;
      mySettings.baselineeCO2_SGP30 = sgp30.baselineCO2;
    } 

    ///////////////////////////////////////////////////////////////////
    // SGP30
    ///////////////////////////////////////////////////////////////////
    else if (command == "f") {                                       // forced CO2 set setpoint
      tmpI = value.toInt();
      if ((tmpI >= 400) && (tmpI <= 2000)) {
        scd30.setForcedRecalibrationFactor((uint16_t)tmpI);
        mySettings.forcedCalibration_SCD30_valid = 0xF0;
        mySettings.forcedCalibration_SCD30 = (uint16_t)tmpI;
        Serial.print(F("Calibration point is:  "));
        Serial.println(tmpI);
      } else { Serial.println(F("Calibration point out of valid Range")); }
    } 
    
    else if (command == "t") {                                       // forced CO2 set setpoint
      tmpF = value.toFloat();
      if ((tmpF >= 0.0) && (tmpF <= 20.0)) {
        scd30.setTemperatureOffset(tmpF);
        mySettings.tempOffset_SCD30_valid = 0xF0;
        mySettings.tempOffset_SCD30 = scd30.getTemperatureOffset();
        Serial.print(F("Temperature offset set to:  "));
        Serial.println(mySettings.tempOffset_SCD30);
      } else { Serial.println(F("Offset point out of valid Range")); }
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
        Serial.print(F("Calibration point is:  "));
        Serial.println(tmpI);
      } else { Serial.println(F("Calibration point out of valid Range")); }
    } 

    else if (command == "b") {                                      // getbaseline
        mySettings.baselineCCS811 = ccs811.getBaseline();
        Serial.print(F("CCS811: baseline is  "));
        Serial.println(mySettings.baselineCCS811);
    } 

    ///////////////////////////////////////////////////////////////////
    // MLX settings
    ///////////////////////////////////////////////////////////////////    
    else if (command == "m") {                                       // forced MLX temp offset
      tmpF = value.toFloat();
      if ((tmpF >= -20.0) && (tmpF <= 20.0)) {
        mlxOffset = tmpF;
        mySettings.tempOffset_MLX_valid = 0xF0;
        mySettings.tempOffset_MLX = tmpF;
        Serial.print(F("Temperature offset set to:  "));
        Serial.println(mySettings.tempOffset_MLX);
      } else { Serial.println(F("Offset point out of valid Range")); }
    } 

    ///////////////////////////////////////////////////////////////////
    // EEPROM settings
    ///////////////////////////////////////////////////////////////////
    else if (command == "s") {                                       // save EEPROM
      tmpTime=millis();
      EEPROM.put(0,mySettings);
      if (EEPROM.commit()) {
        Serial.print(F("EEPROM saved in: "));
        Serial.print(millis()-tmpTime);
        Serial.println(F("ms"));
      }
    }

    else if (command == "r") {                                       // read EEPROM
      tmpTime=millis();
      EEPROM.get(0,mySettings);
      Serial.print(F("EEPROM read in: "));
      Serial.print(millis()-tmpTime);
      Serial.println(F("ms"));
      printSettings();
    }

    else if (command == "p") {                                       // display settings
      printSettings();
    } 

    else if (command == "d") {                                       // default settings
      defaultSettings();
      printSettings();
    }

    else if (command == "1") {                                       // SSID 1
      value.toCharArray(mySettings.ssid1,value.length());
      Serial.print(F("SSID 1 is:  "));
      Serial.println(mySettings.ssid1);
    } 

    else if (command == "2") {                                       // SSID 2
      value.toCharArray(mySettings.ssid2,value.length());
      Serial.print(F("SSID 2 is:  "));
      Serial.println(mySettings.ssid2);
    } 
    
    else if (command == "3") {                                       // SSID 3
      value.toCharArray(mySettings.ssid3,value.length());
      Serial.print(F("SSID 3 is:  "));
      Serial.println(mySettings.ssid3);
    } 

    else if (command == "4") {                                       // Password 1
      value.toCharArray(mySettings.pw1,value.length());
      Serial.print(F("Password 1 is:  "));
      Serial.println(mySettings.pw1);
    } 

    else if (command == "5") {                                       // Password 2
      value.toCharArray(mySettings.pw2,value.length());
      Serial.print(F("Password 2 is:  "));
      Serial.println(mySettings.pw2);
    } 

    else if (command == "6") {                                       // Password 3
      value.toCharArray(mySettings.pw3,value.length());
      Serial.print(F("Password 3 is:  "));
      Serial.println(mySettings.pw3);
    } 

    else if (command == "9") {                                       // MQTT Server
      value.toCharArray(mySettings.mqtt_server,value.length());
      Serial.print(F("MQTT server is:  "));
      Serial.println(mySettings.mqtt_server);
    } 
    
    else {                                                           // unrecognized command
      helpMenu();
    } // end if
    
  } // end serial available
} // end Input

void printSensors() {
  
  if (scd30_avail) {
    Serial.print(F("SCD30 CO2:             ")); Serial.print(scd30_ppm);       Serial.println(F("[ppm]"));
    Serial.print(F("SCD30 rH:              ")); Serial.print(scd30_hum);       Serial.println(F("[%]"));
    Serial.print(F("SCD30 Temperature:     ")); Serial.print(scd30_temp);      Serial.println(F("[degC]"));
    scd30.getTemperatureOffset();
  }
  
  if (bme680_avail) {
    Serial.print(F("BME680 Temperature:    ")); Serial.print(bme680.temperature); Serial.println(F("[degC]"));
    Serial.print(F("BME680 Pressure:       ")); Serial.print(bme680.pressure); Serial.println(F("[Pa]"));
    Serial.print(F("BME680 Humidity:       ")); Serial.print(bme680.humidity); Serial.println(F("[%]"));
    Serial.print(F("BME680 Gas Resistance: ")); Serial.print(bme680.gas_resistance); Serial.println(F("[Ohm]"));
    Serial.print(F("BME680 aH:             ")); Serial.print(bme680_ah);       Serial.println(F("[g/m^3]"));

  }

  if (sgp30_avail == true) {
    Serial.print(F("SGP30 CO2:             ")); Serial.print(sgp30.CO2);        Serial.println(F("[ppm]"));
    Serial.print(F("SGP30 tVOC:            ")); Serial.print(sgp30.TVOC);       Serial.println(F("[ppm]"));
    Serial.print(F("SGP30 baseline CO2:    ")); Serial.println(sgp30.baselineCO2);
    Serial.print(F("SGP30 baseline tVOC:   ")); Serial.println(sgp30.baselineTVOC);
    Serial.print(F("SGP30 H2:              ")); Serial.print(sgp30.H2);         Serial.println(F("[ppm]"));
    Serial.print(F("SGP30 Ethanol:         ")); Serial.print(sgp30.ethanol);    Serial.println(F("[ppm]"));
  }
  
  if (ccs811_avail == true) {
    uint16_t tmpI;
    tmpI = ccs811.getCO2();
    Serial.print(F("CCS811 CO2:            ")); Serial.print(tmpI);  Serial.println(F("[ppm]"));
    tmpI = ccs811.getTVOC();
    Serial.print(F("CCS811 tVOC:           ")); Serial.print(tmpI); Serial.println(F("[ppm]"));
    tmpI = ccs811.getBaseline();
    Serial.print(F("CCS811 baseline:       ")); Serial.print(tmpI);
  }
  
  if (sps30_avail) {
    Serial.print(F("SPS30 P1.0:            ")); Serial.print(valSPS30.MassPM1);  Serial.println(F("[μg/m3]"));
    Serial.print(F("SPS30 P2.5:            ")); Serial.print(valSPS30.MassPM2);  Serial.println(F("[μg/m3]"));
    Serial.print(F("SPS30 P4.0:            ")); Serial.print(valSPS30.MassPM4);  Serial.println(F("[μg/m3]"));
    Serial.print(F("SPS30 P10:             ")); Serial.print(valSPS30.MassPM10); Serial.println(F("[μg/m3]"));
    Serial.print(F("SPS30 P1.0:            ")); Serial.print(valSPS30.NumPM0);   Serial.println(F("[#/cm3]"));
    Serial.print(F("SPS30 P2.5:            ")); Serial.print(valSPS30.NumPM2);   Serial.println(F("[#/cm3]"));
    Serial.print(F("SPS30 P4.0:            ")); Serial.print(valSPS30.NumPM4);   Serial.println(F("[#/cm3]"));
    Serial.print(F("SPS30 P10:             ")); Serial.print(valSPS30.NumPM10);  Serial.println(F("[#/cm3]"));
    Serial.print(F("SPS30 Average:         ")); Serial.print(valSPS30.PartSize); Serial.println(F("[μm]"));
  }

  if (therm_avail) {
    float tmpF;
    tmpF = therm.object();
    Serial.print(F("MLX Object:            ")); Serial.print(tmpF);              Serial.println(F("[degC]"));
    tmpF = therm.ambient();
    Serial.print(F("MLX Ambient:           ")); Serial.print(tmpF);              Serial.println(F("[degC]"));
    tmpF = therm.readEmissivity();
    Serial.print(F("MLX Emissivity:        ")); Serial.println(tmpF);
    Serial.print(F("MLX Offset:            ")); Serial.println(mlxOffset);  
  }
  
}

/**************************************************************************************/
// Update LCD
/**************************************************************************************/
void updateLCD() {
  char lcdbuf[16];
  lcd.clear();
  byte charsPrinted;
  
  if (scd30_avail) { // =============================================================
    
    sprintf(lcdbuf, "%4d", int(scd30_ppm));
    lcd.setCursor(CO2_X, CO2_Y);
    lcd.print(lcdbuf);

    sprintf(lcdbuf, "%4.1f%%", scd30_hum);
    lcd.setCursor(HUM1_X, HUM1_Y);
    lcd.print(lcdbuf);
    //Serial.println(lcdbuf);
    
    sprintf(lcdbuf,"%+5.1fC",scd30_temp);
    lcd.setCursor(TEMP1_X, TEMP1_Y);
    lcd.print(lcdbuf);
    //Serial.println(lcdbuf);

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
  }  // end if avail scd30
  
  if (bme680_avail == true) { // ====================================================
    
    sprintf(lcdbuf,"%4d",(int)(bme680.pressure/100.0));
    lcd.setCursor(PRESSURE_X, PRESSURE_Y);
    lcd.print(lcdbuf);
  
    sprintf(lcdbuf,"%4.1f%%",bme680.humidity);
    lcd.setCursor(HUM2_X, HUM2_Y);
    lcd.print(lcdbuf);
  
    sprintf(lcdbuf,"%4.1fg",bme680_ah);
    lcd.setCursor(HUM3_X, HUM3_Y);
    lcd.print(lcdbuf);

    if ((bme680.humidity >= 45) && (bme680.humidity <= 55)) {
      sprintf(lcdbuf, "%s", "N");
    } else if ((bme680.humidity >= 30) && (bme680.humidity < 45)) {
      sprintf(lcdbuf, "%s", "T");
    } else if ((bme680.humidity >= 55) && (bme680.humidity < 60)) {
      sprintf(lcdbuf, "%s", "T");
    } else if ((bme680.humidity >= 60) && (bme680.humidity < 80)) {
      sprintf(lcdbuf, "%s", "H");
    } else if ((bme680.humidity >  15) && (bme680.humidity < 30)) {
      sprintf(lcdbuf, "%s", "L");
    } else {
      sprintf(lcdbuf, "%s", "!");
    }
    
    //===
    // Humidity WARNING, no location identified for display.
    //===
    //lcd.setCursor(IAQ_WARNING_X, IAQ_WARNING_Y);
    //lcd.print(lcdbuf);
  
    sprintf(lcdbuf,"%+5.1fC",bme680.temperature);
    lcd.setCursor(TEMP2_X, TEMP2_Y);
    lcd.print(lcdbuf);
  
    sprintf(lcdbuf,"%4.1f",(float(bme680.gas_resistance)/1000.0));
    lcd.setCursor(IAQ_X, IAQ_Y);
    lcd.print(lcdbuf);
  
    if (bme680.gas_resistance < 5000) {
      sprintf(lcdbuf, "%s", "N");
    } else if (bme680.gas_resistance < 10000) {
      sprintf(lcdbuf, "%s", "T");
    } else if (bme680.gas_resistance < 300000) {
      sprintf(lcdbuf, "%s", "P");
    } else {
      sprintf(lcdbuf, "%s", "!");
    }
    lcd.setCursor(IAQ_WARNING_X, IAQ_WARNING_Y);
    lcd.print(lcdbuf);
  } // end if avail bme680
  
  if (sgp30_avail == true) { // ====================================================
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
  } // end if avail sgp30

  if (ccs811_avail == true) { // ====================================================

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
  } // end if avail ccs811
  
  if (sps30_avail) { // ====================================================
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
  }// end if avail SPS30

  if (therm_avail) { // ====================================================

    sprintf(lcdbuf,"%4.1f",(therm.object()+mlxOffset));
    //lcd.setCursor(MLX_X, MLX_Y);
    //lcd.print(lcdbuf);
    //Serial.print("MLX: object temperature: "); Serial.print(therm.object()+mlxOffset); Serial.println("C");

    sprintf(lcdbuf,"%4.1f",therm.ambient());
    //lcd.setCursor(MLXA_X, MLXA_Y);
    //lcd.print(lcdbuf);
    //Serial.print("MLX: ambient temperature: "); Serial.print(therm.ambient()); Serial.println("C");

    // https://www.singlecare.com/blog/fever-temperature/
    // https://www.hopkinsmedicine.org/health/conditions-and-diseases/fever
    // https://www.ncbi.nlm.nih.gov/pmc/articles/PMC7115295/
    if (therm.object() < (35.0 - fhDelta)) {
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
    //lcd.setCursor(MLX_WARNING_X, MLX_WARNING_Y);
    //lcd.print(lcdbuf);
    //Serial.print("MLX: "); Serial.println(lcdbuf);

  }// end if avail  MLX
  
} // update display


/**************************************************************************************/
// Wait for serial input
/**************************************************************************************/
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
