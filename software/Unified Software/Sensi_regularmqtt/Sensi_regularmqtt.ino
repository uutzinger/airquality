//////////////////////////////////////////////////////////////////////////////////////////////////////////
// Air Quality Sensor
//////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Supported:
//  - ESP8266 micro controller
//  - SPS30 Senserion particle, Paul Vah library
//  - SCD30 Senserion CO2, Sparkfun library, using interrupt from data ready pin
//  - SGP30 Senserion VOC, eCO2, Sparkfun library
//  - BME680 Bosch Temp, Humidity, Pressure, VOC, Adafruit library
//  - BME280 Bosch Temp, Humidity, Pressure  Sparkfun library
//  - CCS811 Airquality eCO2 tVOC, Spakrfun library, using interrupt from data ready pin
//  - MLX90614 Melex temp contactless, Spakrfun library 
//  - MAX30105 Maxim pulseox, not implemented yet, Spakfun library
//
// Data display through:
//  - LCD 20x4, LiquidCrystal_PCF8574 or Adafruit_LCD library
//  - MQTT publication to broker using PubSubClient library
// 
// Operation:
//  Fast Mode: read often, minimal power saving, some sensors have higher sampling rate than others
//  Slow Mode: read at about 1 sample per minute, where possible enable power saving features, 
//  Some sensorS require 1 sec sample interval regardless of Fast or Slow Mode.
//
//  Air quality sensors consume more power because they have heated sensos elements and are not 
//  well suited for battery operation.
//
// Settings:
//  EEPROM, check help-screen (send ? in terminal)
//  3 wifi passwords and 2 mqtt servers.
//  Hardcoded settings in the text below.
//  Intervals are in milli seconds, when using long data type, that can represent > 500 days.
//
// Wifi:
//  Scans network for known ssid, attempts connection with username password, if disconnected attempts reconnection.
//  MQTT server is contacted, if it can be reached, a backup server is contected. username password is supported.
//
// Wire/I2C:
//  Multiple i2c pins are supported, ESP8266 Arduino supports only one active wire interface, however the pins can
//  changed prior to each communication.
//  At startup all pin combinations are scanned for possible scl & sda connections of the attached sensors.
//  Pins of supported sensors are registered.  
//
//  Some breakout boards do not work well together with others.
//  Senosors might enage in excessive clockstretching and some have pull up resistor requirements beyond the one provided by the microcontroller.
//  For example CCS911 and SCD30 require long clock stretching. They are "slow" sensors. Its best to place them on separate i2c bus.
//  The LCD display requires 5V logic and corrupts regulalry when the i2c bus is shared, it should be placed on separate bus.
//  The MLX sensor might report negative or excessive high temperature when combined with arbitrary sensors.
//
// Improvements:
//  Some sensor drivers were modified to have no or almost no delay functions in the code sections that 
//  are repeatedly called. Some drivers were modivied to allow for user specified i2c port instead of standard "Wire" port.
//
// Software implementation:
//  The program sections are divided into intializations and updates. 
//  The updates are written as state machines and called on regular basis through the main loop.

// Quality Assessments:
// The LCD screen background light is blinking when a sensor is outside its recommended range.
//  Pressure:
//    A change of 5mbar within in 24hrs can cause headaches in suseptible subjects
//    The program comptes the avaraging filter coefficients based on the sensor sample interval for a 24hr smoothing filter y = (1-a)*y + a*x
//  CO2:
//    >1000ppm is poor
//  Temperature:
//    20-25.5C is normal range
//  Humidity:
//    30-60% is normal range
//  Particles
//    P2.5: >25ug/m3 is poor
//    P10: >50ug/m3 is poor 
//  tVOC:
//    >660ppb is poor
//////////////////////////////////////////////////////////////////////////////////////////////////////////
// This software is provided as is, no warranty to is proper opertion is implied. Use it at your own risk.
// Urs Utzinger
// January 2021, i2c scanning and pin switching, OTA flashing
// Winter 2020, fixing the i2c issues
// Fall 2020; MQTT and Wifi
// Summer 2020; first release
//////////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Wiring configuration of the sensors
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Wemos D1_mini ESP8266
// ---------------------
// There is a limitted number of pins available and some affect boot behaviour of the system.
// https://randomnerdtutorials.com/esp8266-pinout-reference-gpios/
// Symbol below: 
//  2 can be use for i2c, pin is usually pulled high
//  l indictes pin is usually low, 
//  c can use for control, 
//  i can use for interrupt.
//  RX,TX, is needed if you want serial debug and USB terminal (serial.begin()), 
//  therefore they are not available if you user serial user input over the terminal.
// Code, Pinnumber, Pinname, Usual Function, behaviour:
// lc ,16, "D0",     , no pull up, wake from sleep, no PWM or I2C, no interrupt, high at boot
// 2  , 5, "D1", SCL , avail
// 2  , 4, "D2", SDA , avail
// 2  , 0, "D3",     , pulled up, connected to flash, boor fails if pulled low
// 2  , 2, "D4",     , pulled up, high at boot, boot fails if pulled low
// 2  ,14, "D5", SCK , avail
// 2  ,12, "D6", MISO, avail
// 2  ,13, "D7", MOSI, avail
// li ,15, "D8", SS, , pulled low, cannot use pull up, boot fails if pulled high
// 2  , 3, "RX",     , high at boot, goes to USB
// 2  , 1, "TX",     , high at boot, goes to USB, boot failure if pulled low
//
// Wakte and Interrupt Pin assignements
// ------------------------------------
// Use D8 for SCD30_RDY interrupt, goes high when data available, remains high until data is read, 
//   this requires the sensors to be power switched after programming, 
//   if the sensor has data available, the pin is high and the mcu can not be programmed
// Use D0 to wake CCS811_Wake, needs to be low when communicating with sensors, most of the time its high
// Use D7 for CCS811_INT, goes low when data is available, most of the time its high, needs pull up configuration via software
// CCS811 address needs to be set to high which is accomplished with connecting it to 3.3V signal
//
// Suggested i2c interfaces
// ------------------------
// I2C_1 D1 & D2
// I2C_2 D3 & D4
// I2C_3 D5 & D6
//
// Sensor characteristics
// Power Sup, Clock Speed, Clock Stretch, Pull ups on breakout board, Board name
//------------------------------------------------------------------------------------------
//       5V, 100kHz,   ?, 10k, LCD 20x4
//                    No,  5k, Level Shifter 
//   3.3&5V, 100kHz,  No, Inf, SPS30 Senserion particle
//   3.3-5V, 100kHz, 150, Inf, SCD30 Senserion CO2
// 1.8-3.3V, 400kHz,  No, 10k, SGP30 Senserion VOC, eCO2
// 1.8-3.3V, 400kHz,  No, 10k, BME680 Bosch Temp, Humidity, Pressure, VOC    
// 1.8-3.6V, 100kHz, Yes,  5k, CCS811 Airquality eCO2 tVOC                   
//    3or5V, 100kHz,  No, Inf, MLX90614 Melex temp contactless               
// 1.8&3.3V, 400kHz,  No,  5k, MAX30105 Maxim pulseox 
//
// Sensirion sensors without pullups need an external 10k pullup on SDA and SCL according Paul Vah instructions.
// ESP8266 has builtin pullups on standard i2c pins, but it appears addditional external ones are needed.
// If another sensor board on the same bus aledady has pullup resistors, there is no additional pull up needed. 
// All pullups will add in parallel and lower the total pullup resistor value, therfore requiring more current to operate the i2c bus.
//
// Suggested Configuration
// ==============================================================================================
//
// I2C_1 400kHz no clock stretch
// -----------------------------
// SGP30
// BME680
// MAX30105
//
// I2C_2 100kHz with extra clock stretch
// -------------------------------------
// SPS30 with 5V power and pullup to 3.3V
// SCD30
// CCS811 
//
// I2C_3 100kHz no extra clock stretch
// -----------------------------------
// LCD after 5V Level Shifter, this sensor affects operation of other sensors
// MLX90614 to 3.3V side, this sensor might be affected by other breakout boards and erroniously 
// reporting negative temp and temp above 500C.
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////

#include <ArduinoOTA.h>

//////////////////////////////////////////////////////////////////////////////////////////////////////////

bool fastMode = true;                           // true:  Measure with about 5 sec intervals, 
                                                // false: Measure in about 1 min intervals and enable sleeping and low energy modes if available
                                                // slow mode has not been tested thorougly
volatile unsigned int dbglevel;                 // 0 no output over USB terminal
                                                // 1 boot info and errors 
                                                // 2..xx turn on verbose debug mode for specific device, see help screen
#define BAUDRATE 115200                         // serial communicaiton speed, terminal settings need to match

#include <Wire.h>
TwoWire myWire = TwoWire();

// You can define multipl i2c ports with repeating statements above, unfortunately ESP8266 Arduino only supports one because
// the wire library calls twi.h for which only one instance can run. 
// We will switch pins each time we communicate to different i2c bus and use same wire instance.

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Hard Coded Settings
// It is not recommeended to change settings others than the ones in the section marked "user" below. 
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////

/******************************************************************************************************/
// Sensor
/******************************************************************************************************/
enum SensorStates{IS_IDLE = 0, IS_MEASURING, IS_BUSY, DATA_AVAILABLE, GET_BASELINE, IS_SLEEPING, IS_WAKINGUP, WAIT_STABLE, HAS_ERROR};
                                                           // IS_IDLE        the sensor is powered up
                                                           // IS_MEASURING   the sensor is creating data autonomously
                                                           // IS_BUSY        the sensor is producing data and will not respond to commands
                                                           // DATA_AVAILABLE new data is available in sensor registers
                                                           // IS_SLEEPING    the sensor or parts of the sensot are in sleep mode
                                                           // IS_WAKINGUP    the sensor is getting out of sleep mode
                                                           // WAIT_STABLE    readings are not stable yet    
                                                           // HAS_ERROR      the communication with the sensor failed

#define SCD30_RDY D8                                       // normal low, active high pin indicating data ready fcom SCD30 sensor (raising)
#define CCS811_INT D7                                      // normal high, active low interrupt, high to low (falling) at end of measurement
#define CCS811_WAKE D0                                     // normal high, active low to wake up sensor, might need to pull low and high w resistor

bool allGood = true;                                       // a sensor is outside recommended limits
bool lastBlinkInten = true;                                // dark or bright?
unsigned long lastBlink;                                   // last time we switched LCD intensity
#define intervalBlinkOff 1000                              // blink off time
#define intervalBlinkOn 9000                               // blink on time
unsigned long intervalBlink = 1000;                        // 

/******************************************************************************************************/
// WiFi and MQTT
/******************************************************************************************************/
/////// User defined ========================================
#define MQTT_PORT 1883                                     // default mqtt port
#define intervalWiFi      5000                             // We check if WiFi is connected every 5 sec, if disconnected we attempted reconnection every 5sec
#define intervalMQTTFast  5000                             // send MQTT sensor data every 5sec, not relevant if send data immediately is set in EEPROM
#define intervalMQTTSlow 60000                             // send MQTT sensor dta 60sec, not relevant if send data immediately is set in EEPROM
/////// =====================================================
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
enum WiFiStates{START_UP = 0, CHECK_CONNECTION, IS_SCANNING, IS_CONNECTING, IS_WAITING};
bool wifi_avail = false;                                   // do we have wifi?
bool ssid_found = false;                                   // known SSID is in range
bool wifi_connected = false;                               // is wifi connected to known ssid?
bool mqtt_connected = false;                               // is mqtt server connected?
bool mqtt_sent = false;                                    // did we publish data?
bool connectSuccess = false;                               // temporary for connection testing
bool sendMQTTonce = true;                                  // send system constants to broker in beginning
char ssid[33];                                             // 32 bytes max, options are stored in EEPROM 
char pw[65];                                               // 64 chars max options are stored in EEPROM and loaded at startup
int nNetworks;                                             // number of WiFi networks found during scanning
int connectionAttempts;                                    // if too many connection attempts, restart wifi scan
unsigned long intervalMQTT = 0;                            // automatically set during setup
unsigned long lastMQTTPublish;                             // last time we published mqtt data
unsigned long lastWiFi;                                    // last time we checked if we are connected
unsigned long lastMQTT;                                    // last time we checked if mqtt is connected
char payloadStr[1024];                                     // String allocated for MQTT message
char topicStr[64];                                         // String allocated for MQTT topic 
char qualityMessage[32];                                   // short String allocated to represent low, normal, high, threshold, excessive, fever etc.
WiFiClient WiFiClient;                                     // The WiFi interface
PubSubClient mqttClient(WiFiClient);                       // The MQTT interface
volatile WiFiStates stateWiFi = START_UP;                  // keeping track of WiFi state
volatile WiFiStates stateMQTT = IS_WAITING;                // keeping track of MQTT state

/******************************************************************************************************/
// Stored System settings
/******************************************************************************************************/
#include <EEPROM.h>
#define EEPROM_SIZE 1024                                   // make sure this value is larger than the space required by the settings below!
//#define saveSettings 604800000                           // 7 days
#define saveSettings 43200000                              // 12 hrs, this might impact longevity of flash memory, I dont know max number write cycles
int eepromAddress = 0;                                     // So far this location is not oeverwritten by uploading other programs
unsigned long lastEEPROM;                                  // last time we updated EEPROM, should occur every couple days
// ========================================================// The EEPROM section
// This table has grown over time. So its not in order.
// Appending new settingswill keeps the already stored settings.
// Boolean settings are stored in as a byte.
struct EEPROMsettings {
  unsigned long runTime;                                   // keep track of total sensor run time
  unsigned int  debuglevel;                                // amount of debug output on serial port
  byte          baselineSGP30_valid;                       // 0xF0 = valid
  uint16_t      baselineeCO2_SGP30;                        //
  uint16_t      baselinetVOC_SGP30;                        //
  byte          baselineCCS811_valid;                      // 0xF0 = valid
  uint16_t      baselineCCS811;                            // baseline is an internal value, not ppm
  byte          tempOffset_SCD30_valid;                    // 0xF0 = valid
  float         tempOffset_SCD30;                          // in C
  byte          forcedCalibration_SCD30_valid;             // 0xF0 = valid, not used as the sensor is not designed to prepoluate its internal calibration
  float         forcedCalibration_SCD30;                   // not used
  byte          tempOffset_MLX_valid;                      // 0xF0 = valid
  float         tempOffset_MLX;                            // in C
  char          ssid1[33];                                 // WiFi SSID 32 bytes max
  char          pw1[65];                                   // WiFi passwrod 64 chars max
  char          ssid2[33];                                 // 2nd set of WiFi credentials
  char          pw2[65];                                   //
  char          ssid3[33];                                 // 3rd set of WiFi credentials
  char          pw3[65];                                   //
  char          mqtt_server[255];                          // your mqtt server
  char          mqtt_username[32];                         // username for MQTT server, leave blank if no password
  char          mqtt_password[32];                         // password for MQTT server
  bool          useLCD;                                    // use/not use LCD even if it is connected
  bool          useWiFi;                                   // use/not use WiFi and MQTT
  bool          useSCD30;                                  // ...
  bool          useSPS30;                                  // ...
  bool          useSGP30;                                  // ...
  bool          useMAX30;                                  // ...
  bool          useMLX;                                    // ...
  bool          useBME680;                                 // ...
  bool          useCCS811;                                 // ...
  bool          sendMQTTimmediate;                         // true: update MQTT right away when new data is availablk, otherwise send one unified message
  char          mqtt_fallback[255];                        // your fallback mqtt server if initial server fails, useful when on private home network
  char          mqtt_mainTopic[32];                        // name of this sensing device for mqtt broker
  bool          consumerLCD;                               // simplified display
  bool          useBME280;                                 //
};
EEPROMsettings mySettings;                                 // the settings

/******************************************************************************************************/
// SCD30; Sensirion CO2 sensor
/******************************************************************************************************/
// Response time is 20sec
// Bootup time 2sec
// Atomatic Basline Correction might take up to 7 days
// Measurement Interval 2...1800 secs
// Does not have sleep mode, but can set measurement intervals
// 2sec interval requires 19mA average current
// The maximum current during reading is 75mA
//
#include <SparkFun_SCD30_Arduino_Library.h>
/////// User defined ========================================
#define intervalSCD30Fast  4000                            // measure every 2sec ... 30minutes, default is 4 
#define intervalSCD30Slow 60000                            // once a minute
#define intervalSCD30Busy 500                              // how frequently to read dataReady when sensor boots up, this is needed to clear the dataready signal for the interrupt
#define intervalPressureSCD30 120000                       // if we have pressure data from other sensor we will provide it to the co2 sensor to improve accuracy in this interval
//////// End user defined ===================================
float scd30_ppm;                                           // co2 concentration from sensor
float scd30_temp;                                          // temperature from sensor
float scd30_hum;                                           // humidity from sensor
float scd30_ah;                                            // absolute humidity, calculated
bool scd30_avail = false;                                  // do we have this sensor?
bool scd30NewData = false;                                 // do we have new data?
TwoWire *scd30_port =0;                                    // pointer to the i2c port, might be useful for other microcontrollers
uint8_t scd30_i2c[2];                                      // the pins for the i2c port, set during initialization
unsigned long intervalSCD30 = 0;                           // will bet set at initilization to either values for Fast or Slow opertion
unsigned long lastSCD30;                                   // last time we interacted with sensor
unsigned long lastPressureSCD30;                           // last time we updated pressure
unsigned long lastSCD30Busy;                               // for the statemachine
const byte SCD30interruptPin = SCD30_RDY;                  // 
volatile SensorStates stateSCD30 = IS_IDLE;                // keeping track of sensor state
SCD30 scd30;                                               // the sensor

void ICACHE_RAM_ATTR handleSCD30Interrupt() {              // Interrupt service routine when data ready is signaled
  stateSCD30 = DATA_AVAILABLE;                             // advance the sensor state
  if (dbglevel == 4) {                                     // for debugging
    Serial.println(F("SCD30: interrupt occured")); }
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
// Sleep Mode        0.05mA (needs firmware 2.2 and newer)
// Time to reach stable measurement: 8s for 200 – 3000 #/cm3 particles
//                                  16s for 100 – 200  #/cm3 particles
//                                  30s for  50 – 100  #/cm3 particles
// Sleep mode can be entered when sensor is idle but only with firmware 2.2 or greater.
// There appears to be no firmware updater on Sensirion support website.
//
#include <sps30.h>
//////// User Defined =======================================
#define intervalSPS30Fast  5000                            // minimum is 1 sec
#define intervalSPS30Slow 60000                            // system will sleep for intervalSPS30Slow - timetoStable if sleep function is available
#define SPS30Debug 0                                       // define driver debug: 
                                                           //   0 - no messages, 
                                                           //   1 - request sending and receiving, 
                                                           //   2 - request sending and receiving + show protocol errors */
//////// End user defined ===================================
unsigned long intervalSPS30 = 0;                           // measurement interval
unsigned long timeSPS30Stable;                             // time when readings are stable, is adjusted automatically based on particle counts
unsigned long lastSPS30;                                   // last time we interacted with sensor
unsigned long wakeSPS30;                                   // time when wakeup was issued
unsigned long wakeTimeSPS30;                               // time when sensor is supposed to be woken up
unsigned long timeToStableSPS30;                           // how long it takes to get stable readings, automatically pupulated based on total particles
char buf[64];                                              // messaging buffer
uint8_t ret, st;                                           // return variables
float totalParticles;                                      // we need to calculate time to stable readings depending on total particle concentration
uint32_t autoCleanIntervalSPS30;                           // current cleaning interval setting in sensor
bool sps30_avail = false;                                  // do we have this sensor?
bool sps30NewData = false;                                 // do we have new data?
TwoWire *sps30_port =0;                                    // pointer to the i2c port, might be useful for other microcontrollers
uint8_t sps30_i2c[2];                                      // the pins for the i2c port, set during initialization
volatile SensorStates stateSPS30 = IS_BUSY;                // sensor state
struct sps_values valSPS30;                                // will hold the readings from sensor
SPS30 sps30;                                               // the sensor
SPS30_version v;                                           // version structure of sensor

/******************************************************************************************************/
// SGP30; Senserion TVOC, eCO2
/******************************************************************************************************/
// Sampling rate: minimum is 1s
// There is no recommended approach to put sensor into sleep mode. 
// Once init command is issued, sensor remains in measurement mode.
// Current Consumption:
//   Measurement mode 49mA
//   Sleep mode 0.002..0.0010 mA
//
#include <SparkFun_SGP30_Arduino_Library.h>
//////// User defined =======================================
#define intervalSGP30Fast 1000                             // as low as 10ms, ideal is 1s, gives best accuracy
#define intervalSGP30Slow 1000                             // recommended interval is 1s, no slow version
#define intervalSGP30Baseline 300000                       // obtain baseline every 5 minutes
#define intervalSGP30Humidity  60000                       // update humidity once a minute if available from other sensor
#define warmupSGP30_withbaseline 3600000                   // 60min 
#define warmupSGP30_withoutbaseline 43200000                // 12hrs 
//////// End user defined ===================================
bool sgp30_avail  = false;                                 // do we have this sensor
bool sgp30NewData = false;                                 // dow have new data
TwoWire *sgp30_port =0;                                    // pointer to the i2c port, might be useful for other microcontrollers
uint8_t sgp30_i2c[2];                                      // the pins for the i2c port, set during initialization
bool baslineSGP30_valid = false;
unsigned long lastSGP30;                                   // last time we obtained data
unsigned long lastSGP30Humidity;                           // last time we upated humidity
unsigned long lastSGP30Baseline;                           // last time we obtained baseline
unsigned long intervalSGP30 = 0;                           // populated during setup
unsigned long warmupSGP30;                                 // populated during setup
volatile SensorStates stateSGP30 = IS_IDLE; 
SGP30 sgp30;

/******************************************************************************************************/
// CCS811; Airquality CO2 tVOC
/******************************************************************************************************/
// Sensor has 20min equilibrium time and 48hrs burn in time
// Sensor is read through interrupt handling (in this software implementation):
// Power on takes 20ms
// Mode 0 – Idle (Measurements are disabled in this mode)
// Mode 1 – Constant power mode, IAQ measurement every second
// Mode 2 – Pulse heating mode IAQ measurement every 10 seconds
// Mode 3 – Low power pulse heating mode IAQ measurement every 60 seconds
// Mode 4 – Const power pulse heating mode IAQ measurement every 0.25 seconds
//
// Baseline takes 24 hrs to establish, R_A is reference resistance
// Sleep mode: turns off i2c, 0.019 mA power consumption
//    
#include <SparkFunCCS811.h>
//////// User defined =======================================
#define ccs811ModeFast 2                                   // 4=0.25s, 3=60s, 2=10sec, 1=1sec.
#define ccs811ModeSlow 3                                   // 
#define baselineCCS811Fast       300000                    // 5 mins
#define baselineCCS811Slow      3600000                    // 1 hour
#define updateCCS811HumitityFast 120000                    // 2 min
#define updateCCS811HumititySlow 300000                    // 5 mins
//#define stablebaseCCS811     86400000                    // sensor needs 24hr until baseline stable
#define stablebaseCCS811       43200000                    // sensor needs 12hr until baseline stable
#define burninCCS811          172800000                    // sensor needs 48hr burn in
//////// End user defined ===================================
bool ccs811_avail = false;                                 // do we have this sensor?
bool ccs811NewData = false;                                // do we have new data
TwoWire *ccs811_port =0;                                   // pointer to the i2c port, might be useful for other microcontrollers
uint8_t ccs811_i2c[2];                                     // the pins for the i2c port, set during initialization
unsigned long lastCCS811;                                  // last time we interacted with sensor
unsigned long lastCCS811Baseline;                          // last time we obtained baseline
unsigned long lastCCS811Humidity;                          // last time we update Humidity on sensor
volatile unsigned long lastCCS811Interrupt;                // last time we update Humidity on sensor
unsigned long warmupCCS811;                                // sensor needs 20min conditioning 
unsigned long intervalCCS811Baseline;                      // get the baseline every few minutes
unsigned long intervalCCS811Humidity;                      // update the humidity every few minutes
uint8_t ccs811Mode;                                        // operation mode, see above 
const byte CCS811interruptPin = CCS811_INT;                // CCS811 not Interrupt Pin
volatile SensorStates stateCCS811 = IS_IDLE;               // sensor state
CCS811 ccs811(0X5B);                                       // the sensor, if alternative address is used, the address pin will need to be set to high

void ICACHE_RAM_ATTR handleCCS811Interrupt() {             // interrupt service routine to handle data ready signal
    if (fastMode == true) { 
      stateCCS811 = DATA_AVAILABLE;                        // update the sensor state
    } else { 
      digitalWrite(CCS811_WAKE, LOW);                      // set CCS811 to wake up 
      stateCCS811 = IS_WAKINGUP;                           // update the sensor state
      lastCCS811Interrupt = millis();
    }
    if (dbglevel == 10) {Serial.println(F("CCS811: interrupt occured")); }
}

/******************************************************************************************************/
// BME680; Bosch Temp, Humidity, Pressure, VOC
/******************************************************************************************************/
// Continous 1Hz             1s   12000uA
// Low Power 0.33 Hz         3s     900uA
// Ultra Low Power 0.0033 Hz 300s    90uA
// 
#include <Adafruit_Sensor.h>
#include "Adafruit_BME680.h"
//////// User defined =======================================
#define intervalBME680Fast             5000                // 5sec, mimimum 1s 
#define intervalBME680Slow            60000                // 60sec
// The (*) marked settings are from example programs, 
// highest accuracy is when oversampling is largest and IIR filter longest
// IIR filter is for temp and pressure only
// Fast measurements: high power, high accuracy
#define bme680_TempOversampleFast     BME680_OS_16X        // 1,2,4,8*,16x
#define bme680_HumOversampleFast      BME680_OS_16X        // 1,2*,4,8,16x
#define bme680_PressureOversampleFast BME680_OS_16X        // 1,2,4*,8,16x
#define bme680_FilterSizeFast         BME680_FILTER_SIZE_15 // 0,1,3*,7,15,31,63,127 
// Slow measurements; assuming you want a few datapoints every once in a while 
#define bme680_TempOversampleSlow     BME680_OS_1X         // 1,2,4,8*,16x
#define bme680_HumOversampleSlow      BME680_OS_1X         // 1,2*,4,8,16x
#define bme680_PressureOversampleSlow BME680_OS_1X         // 1,2,4*,8,16x
#define bme680_FilterSizeSlow         BME680_FILTER_SIZE_0 // 0,1,3*,7,15,31,63,127 
// AirQuality Index is Bosch proprietary software and not used here. We only measure sensor resistance.
#define bme680_HeaterTemp             320                  // C
#define bme680_HeaterDuration         150                  // ms, time it takes for stable reading
//////// End user defined ===================================
bool bme680_avail = false;                                 // do we hace the sensor?
bool bme680NewData = false;                                // do we have new data?
TwoWire *bme680_port =0;                                   // pointer to the i2c port
uint8_t bme680_i2c[2];                                     // the pins for the i2c port, set during initialization
float          bme680_ah = 0;                              // [gr/m^3]
unsigned long  intervalBME680 = 0;                         // filled automatically during setup
unsigned long  lastBME680;                                 // last time we interacted with sensor
unsigned long  endTimeBME680;                              // when data will be available
float bme680_pressure24hrs = 0.0;                          // average pressure last 24hrs
float alphaBME680;                                         // poorman's low pass filter for f_cutoff = 1 day
volatile SensorStates stateBME680 = IS_IDLE;               // sensor state
Adafruit_BME680 bme680;                                    // the sensor (i2c connected

/******************************************************************************************************/
// BME280; Bosch Temp, Humidity, Pressure
/******************************************************************************************************/
// Continous 1Hz             1s
// Low Power 0.33 Hz         3s
// Ultra Low Power 0.0033 Hz 300s
// 3.6microA H,P,T
// --------------------------------------
// runMode  0=SLEEP 1=FORCED 2=FORCED 3=NORMAL
// tStandby 0=0.5ms 1=62.5ms 2=125ms 3=250ms 4=500 ms 5=1000ms 6=10ms 7=20ms   
// filter   0=off,1(75% response) 1=2,2 2=4,5 3=8,11 4=16,22 
// tempOverSample & pressOverSample & humidOverSample can be: 
// 0=off 1=1x 2=2x 3=4x 4=8x 5=16x
// 
// oversampling does not seem to affect noise, nor measurement time
// filter reduces noise, does not affect mesurement time
// standby time = 5, measurements completes in about 800ms
// stabdby time = 0, with all three sensors seems to take about 5-6ms, with only one sensor 3ms
// humidity sensor is broken and reports 0
#include <SparkFunBME280.h>
// No User Defined Section =================================
// Fast measurements: high power consmpution and high accuracy
//   Uses standbytime
#define bme280_TempOversampleFast     5                    // 16x
#define bme280_HumOversampleFast      5                    // 16x
#define bme280_PressureOversampleFast 5                    // 16x
#define bme280_FilterFast             4                    // 16 data poinrts
#define bme280_ModeFast               MODE_NORMAL          // run autonomously
#define bme280_StandbyTimeFast        5                    // 1000ms
#define intervalBME280Fast            0                    // this will use the calculated interval time 
// Slow measurements: low energy, low accuracy
//   MODE_FORCED does not use standby time, sensor goes back to sleep right after reading
#define bme280_TempOversampleSlow     1                    // 1x
#define bme280_HumOversampleSlow      1                    // 1x
#define bme280_PressureOversampleSlow 1                    // 1x
#define bme280_FilterSlow             4                    // 16 data points
#define bme280_ModeSlow               MODE_FORCED          // measure manually
#define bme280_StandbyTimeSlow        0                    // is not relevant
#define intervalBME280Slow            60000                // 1 minute
//////// ===================================================
float bme280_pressure;                                     // pressure from sensor
float bme280_temp;                                         // temperature from sensor
float bme280_hum;                                          // humidity from sensor
bool bme280_avail = false;                                 // do we hace the sensor?
bool bme280NewData = false;                                // is there new data
long bme280_measuretime = 0;                               // computed time it takes to complete the measurements and to finish standby
bool BMEhum_avail = false;                                 // no humidity sensor if we have BMP instead of BME
TwoWire *bme280_port =0;                                   // pointer to the i2c port, might be useful for other microcontrollers
uint8_t bme280_i2c[2];                                     // the pins for the i2c port, set during initialization
float bme280_ah = 0;                                       // [gr/m^3]
unsigned long  intervalBME280 = 0;                         // filled automatically during setup
unsigned long  lastBME280;                                 // last time we interacted with sensor
unsigned long  endTimeBME280;                              // when data will be available
float bme280_pressure24hrs = 0.0;                          // average pressure last 24hrs
float alphaBME280;                                         // poorman's low pass filter for f_cutoff = 1 day
volatile SensorStates stateBME280 = IS_IDLE;               // sensor state
BME280 bme280;                                             // the sensor

/******************************************************************************************************/
// MLX contact less tempreture sensor
/******************************************************************************************************/
// The MLX sensor has a sleep mode.
// It is possible that traffic on the I2C bus by other sensors wakes it up though.
#include <SparkFunMLX90614.h>
/////// User defined ========================================
#define fhDelta 0.5                                        // difference from forehad to oral temperature
const float emissivity = 0.98;                             // emissivity of skin
unsigned long intervalMLX = 5000;                          // readout intervall in ms, 250ms minimum
float mlxOffset = 1.4;                                     // offset to adjust for sensor inaccuracy, 
                                                           // reading a black surface of an object should give the same value
                                                           // as current room temperature, measuring wall or ceiling differs from room temp
/////// End user defined ====================================
#define timeToStableMLX 250                                // time until stable internal readings in ms
bool therm_avail    = false;                               // do we hav e the sensor?
bool mlxNewData = false;                                   // do we have new data
TwoWire *mlx_port =0;                                      // pointer to the i2c port, might be useful for other microcontrollers
uint8_t mlx_i2c[2];                                        // the pins for the i2c port, set during initialization
unsigned long lastMLX;                                     // last time we interacted with sensor
unsigned long sleepTimeMLX;                                // computed internally
volatile SensorStates stateMLX = IS_IDLE;                  // sensor state
IRTherm therm;                                             // the sensor

/******************************************************************************************************/
// MAX30105; pulseox
/******************************************************************************************************/
// Not implemented yet
//#include ...
//// User defined ===========================================
const long intervalMAX30 = 10;                             // readout intervall
//// End user defined =======================================
bool max_avail    = false;                                 // do we have sensor?
bool maxNewData   = false;                                 // do we have new data?
TwoWire *max_port =0;                                      // pointer to the i2c port, might be useful for other microcontrollers
uint8_t max_i2c[2];                                        // the pins for the i2c port, set during initialization
unsigned long lastMAX;                                     // last time we interacted with sensor
volatile SensorStates stateMAX = IS_IDLE;                  // sensor state

/******************************************************************************************************/
// Time Keeping
/******************************************************************************************************/
//
unsigned long currentTime;                                 // Populated at beginning of main loop
unsigned long lastTime;                                    // last time we updated runtime
unsigned long intervalLoop;                                // determines desired frequency of check the devices
unsigned long intervalRuntime;                             // how often do we update the uptime counter of the system
unsigned long tmpTime;                                     // temporary variable to measures execution times
long          myDelay;                                     // main loop delay, automatically computed
float         myDelayAvg = -99;                            // average delay used in each loop

/******************************************************************************************************/
// LCD Display
/******************************************************************************************************/
// Power Consumption: ... not yet measured
// LCD setup uses long delays during initialization:
//  clear 2ms delay
//  setcursor no delay
//  print dont know delays, also dont know how this works as it uses arduino specific streaming
//  begin calls init and therfore has long delay
//  init >1000ms delay
//  backlight no delay
//
/////User Setting ===========================================
#define LCDResetTime 43200000                              // 12 hrs, in case we want to reset and attempt cleaning corrupted display, no longer used
#define intervalLCDFast  5000                              // 5sec
#define intervalLCDSlow 60000                              // 1min
#define ADALCD                                             // we have Adafruit i2c to LCD driver
//#undef ADALCD                                            // we have non-Adafruit i2c to LCD driver
//// End user defined =======================================
#if defined ADALCD
  #include "Adafruit_LiquidCrystal.h"
#else
  #include "LiquidCrystal_PCF8574.h"
#endif
unsigned long intervalLCD = 0;                             // LCD refresh rate, is set depending on fastmode during setup
bool lcd_avail = false;                                    // is LCD attached?
unsigned long lastLCD;                                     // last time LCD was modified
unsigned long lastLCDReset;                                // last time LCD was reset
char lcdDisplay[4][20];                                    // 4 lines of 20 characters, Display 0
char lcdDisplayAlt[4][20];                                 // 4 lines of 20 characters, Display 1
bool altDisplay = false;                                   // Alternate between sensors, when not enough space on display
TwoWire *lcd_port = 0;                                     // Pointer to the i2c port
uint8_t lcd_i2c[2];                                        // the pins for the i2c port, set during initialization
#if defined ADALCD
Adafruit_LiquidCrystal lcd(0);                             // 0x20 is added inside driver to create 0x20 as address
#else
LiquidCrystal_PCF8574 lcd(0x27);                           // set the LCD address to 0x27 
#endif
// For display layout see excel file in project folder
#include "LCDlayout.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////
void setup() {
//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////
  
  Serial.begin(BAUDRATE);
  Serial.println(F(""));;
  const char waitmsg[] = "Waiting 10seconds or skip";      // Allows user to open serial terminal to onserve the debug output
  serialTrigger((char *) waitmsg, 10);
  
  myWire.begin(D1, D2);
  myWire.setClock(100000); // 100kHz or 400kHz speed
  myWire.setClockStretchLimit(200000);

  /******************************************************************************************************/
  // EEPROM setup and read 
  /******************************************************************************************************/  
  tmpTime = millis();
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(eepromAddress, mySettings);
  if (mySettings.debuglevel > 0) { Serial.print(F("EEPROM read in: ")); Serial.print((millis()-tmpTime)); Serial.println(F("ms")); }

  /******************************************************************************************************/
  // Debug EEPROM override; force systems settings
  // When you add a boolean setting it is stored as a byte and toggetling it switches from 255 to 245
  // If one does not want to reset all settings through user input, it can be helpful to create clean
  // setting here, save it to EEPROM and then reupload code
  /******************************************************************************************************/  
  //mySettings.debuglevel  = 1;   //
  //mySettings.useLCD    = true;  // enable/disable sensors even if they are connected
  //mySettings.useWiFi   = true;  // 
  //mySettings.useSCD30  = true;  //
  //mySettings.useSPS30  = true;  //
  //mySettings.useSGP30  = true;  //
  //mySettings.useMAX30  = true;  //
  //mySettings.useMLX    = true;  //
  //mySettings.useBME680 = true;  //
  //mySettings.useBME280 = true;  //
  //mySettings.useCCS811 = true;  //
  //mySettings.consumerLCD = true;  //
  //mySettings.sendMQTTimmediate = true; //
  //strcpy(mySettings.mqtt_mainTopic, "Sensi");
  
  /******************************************************************************************************/
  // Set Debug Output Level
  /******************************************************************************************************/  
  dbglevel =  mySettings.debuglevel;
  if (dbglevel > 0) { Serial.print(F("Debug level is: ")); Serial.println(dbglevel); }

  /******************************************************************************************************/
  // Check which devices are attached to the I2C pins, this self configures the connections
  /******************************************************************************************************/
  uint8_t portArray[] = {16, 5, 4, 0, 2, 14, 12, 13};
  String portMap[] = {"D0", "D1", "D2", "D3", "D4", "D5", "D6", "D7"}; //for Wemos
  byte error, address;
  char tmpStr[48];
  
  for (uint8_t i = 0; i < sizeof(portArray); i++) {
    for (uint8_t j = 0; j < sizeof(portArray); j++) {
      if (i != j){
        if (dbglevel > 0) {Serial.println("Scanning (SDA : SCL) - " + portMap[i] + " : " + portMap[j] + " - ");}
        Wire.begin(portArray[i], portArray[j]);
        for (address = 1; address < 127; address++ )  {
          Wire.beginTransmission(address);
          error = Wire.endTransmission();
          if (error == 0){
            // found a device
            if      (address == 0x20) {lcd_avail    = true; lcd_port   = &myWire; lcd_i2c[0]   = portArray[i];    lcd_i2c[1]= portArray[j]; } // LCD display
            else if (address == 0x57) {max_avail    = true; max_port   = &myWire; max_i2c[0]   = portArray[i];    max_i2c[1]= portArray[j]; } // MAX 30105 Pulseox & Particle
            else if (address == 0x58) {sgp30_avail  = true; sgp30_port = &myWire; sgp30_i2c[0] = portArray[i];  sgp30_i2c[1]= portArray[j]; } // Senserion TVOC eCO2
            else if (address == 0x5A) {therm_avail  = true; mlx_port   = &myWire; mlx_i2c[0]   = portArray[i];    mlx_i2c[1]= portArray[j]; } // MLX IR sensor
            else if (address == 0x5B) {ccs811_avail = true; ccs811_port= &myWire; ccs811_i2c[0]= portArray[i]; ccs811_i2c[1]= portArray[j]; } // Airquality CO2 tVOC
            else if (address == 0x61) {scd30_avail  = true; scd30_port = &myWire; scd30_i2c[0] = portArray[i];  scd30_i2c[1]= portArray[j]; } // Senserion CO2
            else if (address == 0x69) {sps30_avail  = true; sps30_port = &myWire; sps30_i2c[0] = portArray[i];  sps30_i2c[1]= portArray[j]; } // Senserion Particle
            else if (address == 0x76) {bme280_avail = true; bme280_port= &myWire; bme280_i2c[0]= portArray[i]; bme280_i2c[1]= portArray[j]; } // Bosch Temp, Humidity, Pressure
            else if (address == 0x77) {bme680_avail = true; bme680_port= &myWire; bme680_i2c[0]= portArray[i]; bme680_i2c[1]= portArray[j]; } // Bosch Temp, Humidity, Pressure, VOC
            else if (dbglevel > 0) {Serial.print("Found unknonw device - "); Serial.print(address); Serial.println(" !"); }
          }
        }
      }
    }
  }

  Serial.println("");
  
  if (dbglevel > 0) {
    Serial.print(F("LCD                  "));  if (lcd_avail)    { sprintf(tmpStr, "available:  %d SDA %d SCL %d", uint32_t(lcd_port),   lcd_i2c[0],     lcd_i2c[1]);     Serial.println(tmpStr); } else {Serial.println(F("not available"));}
    Serial.print(F("MAX30105             "));  if (max_avail)    { sprintf(tmpStr, "available:  %d SDA %d SCL %d", uint32_t(max_port),    max_i2c[0],    max_i2c[1]);     Serial.println(tmpStr); } else {Serial.println(F("not available"));}
    Serial.print(F("CCS811 eCO2, tVOC    "));  if (ccs811_avail) { sprintf(tmpStr, "available:  %d SDA %d SCL %d", uint32_t(ccs811_port), ccs811_i2c[0], ccs811_i2c[1]);  Serial.println(tmpStr); } else {Serial.println(F("not available"));}
    Serial.print(F("SGP30 eCO2, tVOC     "));  if (sgp30_avail)  { sprintf(tmpStr, "available:  %d SDA %d SCL %d", uint32_t(sgp30_port),  sgp30_i2c[0],  sgp30_i2c[1]);   Serial.println(tmpStr); } else {Serial.println(F("not available"));}
    Serial.print(F("MLX temp             "));  if (therm_avail)  { sprintf(tmpStr, "available:  %d SDA %d SCL %d", uint32_t(mlx_port),    mlx_i2c[0],    mlx_i2c[1]);     Serial.println(tmpStr); } else {Serial.println(F("not available"));}
    Serial.print(F("SCD30 CO2, rH        "));  if (scd30_avail)  { sprintf(tmpStr, "available:  %d SDA %d SCL %d", uint32_t(scd30_port),  scd30_i2c[0],  scd30_i2c[1]);   Serial.println(tmpStr); } else {Serial.println(F("not available"));}
    Serial.print(F("SPS30 PM             "));  if (sps30_avail)  { sprintf(tmpStr, "available:  %d SDA %d SCL %d", uint32_t(sps30_port),  sps30_i2c[0],  sps30_i2c[1]);   Serial.println(tmpStr); } else {Serial.println(F("not available"));}
    Serial.print(F("BM[E/P]280 T, P[, rH]"));  if (bme280_avail) { sprintf(tmpStr, "available:  %d SDA %d SCL %d", uint32_t(bme280_port), bme280_i2c[0], bme280_i2c[1]);  Serial.println(tmpStr); } else {Serial.println(F("not available"));}
    Serial.print(F("BME680 T, rH, P tVOC "));  if (bme680_avail) { sprintf(tmpStr, "available:  %d SDA %d SCL %d", uint32_t(bme680_port), bme680_i2c[0], bme680_i2c[1]);  Serial.println(tmpStr); } else {Serial.println(F("not available"));}
  }
  
  serialTrigger((char *) waitmsg, 10);

  /******************************************************************************************************/
  // Time Intervals for Loop, Display and MQTT publishing, keeping track of runtime
  /******************************************************************************************************/

  if (fastMode == true) {                                  // fast mode -----------------------------------
    intervalLoop    =                100;                  // 0.1 sec, Main Loop is run 10 times per second
    intervalLCD     =    intervalLCDFast;                  // LCD display is updated every 10 seconds
    intervalMQTT    =   intervalMQTTFast;                  // 
    intervalRuntime =              60000;                  // 1 minute, uptime is updted every minute
  } else{                                                  // slow mode -----------------------------------
    intervalLoop    =               1000;                  // 1 sec
    intervalLCD     =    intervalLCDSlow;                  // 1 minute
    intervalMQTT    =   intervalMQTTSlow;                  //
    intervalRuntime =             600000;                  // 10 minutes
  }

  /******************************************************************************************************/
  // Initialize all devices
  /******************************************************************************************************/
  // Initialize LCD Screen
  if (lcd_avail && mySettings.useLCD)       { initializeLCD();   }   else { lcd_avail    = false; }
  // Initialize SCD30 CO2 sensor 
  if (scd30_avail && mySettings.useSCD30)   { initializeSCD30(); }   else { scd30_avail  = false; } 
  // SGP30 Initialize
  if (sgp30_avail && mySettings.useSGP30)   { initializeSGP30();  }  else { sgp30_avail  = false; }
  // CCS811 Initialize 
  if (ccs811_avail && mySettings.useCCS811) { initializeCCS811();  } else { ccs811_avail = false; }
  // SPS30 Initialize Particle Sensor
  if (sps30_avail && mySettings.useSPS30)   { initializeSPS30();  }  else { sps30_avail  = false; }
  // Initialize BME680
  if (bme680_avail && mySettings.useBME680) { initializeBME680();  } else { bme680_avail = false; }
  // Initialize BME280
  if (bme280_avail && mySettings.useBME280) { initializeBME280();  } else { bme280_avail = false; }
  // Initialize MLX Sensor
  if (therm_avail && mySettings.useMLX)     { initializeMLX();  }    else { therm_avail  = false; }
  // Initialize MAX Pulse OX Sensor
  if (max_avail && mySettings.useMAX30)     {   }                    else { max_avail    = false; }

  /******************************************************************************************************/
  // Make sure i2c bus was not changed
  // I did not check each driver to verify wether it modifies the wire settings
  // The settings below will work for all sensors supported
  // If one has only one wire instance, it does not make sense to switch clock speed.
  /******************************************************************************************************/
  myWire.setClock(100000);
  myWire.setClockStretchLimit(200000);

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
  lastBME280          = currentTime;
  lastBME680          = currentTime;
  lastSGP30           = currentTime;
  lastSGP30Humidity   = currentTime;
  lastSGP30Baseline   = currentTime;
  lastLCDReset        = currentTime;
  lastWiFi            = currentTime; 
  lastMQTT            = currentTime;
  lastMQTTPublish     = currentTime;
  lastBlink           = currentTime;
  
  /******************************************************************************************************/
  // Populate LCD screen, start with clean LCD
  /******************************************************************************************************/
  if (lcd_avail && mySettings.useLCD) {
    tmpTime = millis();
    if (mySettings.consumerLCD) { updateConsumerLCD(); } else {updateLCD();}
    lastLCD = currentTime;
    if (dbglevel > 0) { Serial.print(F("LCD updated in ")); Serial.print((millis()-tmpTime));  Serial.println(F("ms")); }
  }

  /******************************************************************************************************/
  // Connect to WiFi
  /******************************************************************************************************/
  initializeWiFi(); // this will turn off wifi if user settings want it off

  /******************************************************************************************************/
  // ESP8266 over the Air firmware update
  /******************************************************************************************************/  
  if (wifi_avail && mySettings.useWiFi) {
    ArduinoOTA.setHostname(mySettings.mqtt_mainTopic); // That would be appropriate host name
    ArduinoOTA.setPassword("w1ldc8ts");                // That should be set by programmer and not user 
    ArduinoOTA.onStart([]() { if (mySettings.debuglevel > 0) { Serial.println(F("Start")); } });
    ArduinoOTA.onEnd([]()   { if (mySettings.debuglevel > 0) { Serial.println(F("\nEnd")); } });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) { if (mySettings.debuglevel > 0) { Serial.printf("Progress: %u%%\r", (progress / (total / 100))); } });
    ArduinoOTA.onError([](ota_error_t error) {
      if (mySettings.debuglevel > 0) {
        Serial.printf("Error[%u]: ", error);
        if (error == OTA_AUTH_ERROR) Serial.println(F("Auth Failed"));
        else if (error == OTA_BEGIN_ERROR) Serial.println(F("Begin Failed"));
        else if (error == OTA_CONNECT_ERROR) Serial.println(F("Connect Failed"));
        else if (error == OTA_RECEIVE_ERROR) Serial.println(F("Receive Failed"));
        else if (error == OTA_END_ERROR) Serial.println(F("End Failed"));
      }
    });
    ArduinoOTA.begin();
    if (mySettings.debuglevel > 0) { Serial.println(F("OTA ready")); }
  }
  
  /******************************************************************************************************/
  // System is initialized
  /******************************************************************************************************/
  if (dbglevel > 0) { Serial.println(F("System initialized")); }
  
} // end setup

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////
void loop() {
//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////
  currentTime = millis();
  
  /******************************************************************************************************/
  // Update the State Machines for all Devices
  /******************************************************************************************************/
  if (wifi_avail && mySettings.useWiFi)     { updateWiFi(); }        // WiFi update, this will reconnect if disconnected
  if (wifi_avail && mySettings.useWiFi)     { updateMQTT(); }        // MQTT update
  if (scd30_avail && mySettings.useSCD30)   { updateSCD30(); }       // SCD30 Sensirion CO2 sensor
  if (sgp30_avail && mySettings.useSGP30)   { updateSGP30(); }       // SGP30 Sensirion eCO2 sensor
  if (ccs811_avail && mySettings.useCCS811) { updateCCS811(); }      // CCS811
  if (sps30_avail && mySettings.useSPS30)   { updateSPS30(); }       // SPS30 Sensirion Particle Sensor State Machine
  if (bme280_avail && mySettings.useBME280) { updateBME280(); }      // BME280, Hum, Press, Temp
  if (bme680_avail && mySettings.useBME680) { updateBME680(); }      // BME680, Hum, Press, Temp, Gasresistance
  if (therm_avail && mySettings.useMLX)     { updateMLX(); }         // MLX Contactless Thermal Sensor
  if (max_avail && mySettings.useMAX30)     {   }                    // MAX Pulse Ox Sensor
  if (wifi_avail && mySettings.useWiFi)     { ArduinoOTA.handle(); } // Over the air flashing
  /***************************************************F***************************************************/

  /******************************************************************************************************/
  // MQTT: Publish Sensor Data
  /******************************************************************************************************/
  if (mqtt_connected && mySettings.useWiFi) { updateMQTTMessage(); } // MQTT send sensor data

  /******************************************************************************************************/
  // LCD: Display Sensor Data
  /******************************************************************************************************/
  if (lcd_avail && mySettings.useLCD) { 
    if ((currentTime - lastLCD) >= intervalLCD) {
      tmpTime = millis();
      if (mySettings.consumerLCD) { updateConsumerLCD(); } else { updateLCD(); }
      lastLCD = currentTime;
      if (dbglevel > 0) { Serial.print(F("LCD updated in ")); Serial.print((millis()-tmpTime));  Serial.println(F("ms")); }
    }
  }
  
  /******************************************************************************************************/
  // Serial User Input
  /******************************************************************************************************/
  inputHandle();                                                     // Serial input handling

  /******************************************************************************************************/
  // Time Managed Events
  /******************************************************************************************************/
  
  // Update runtime every minute -------------------------------------
  if ((currentTime - lastTime) >= intervalRuntime) {
    mySettings.runTime = mySettings.runTime + ((currentTime - lastTime) / 1000);
    lastTime = currentTime;
    if (dbglevel > 0) { Serial.println(F("Runtime updated")); }
  }

  // Update EEPROM once a week ---------------------------------------
  if ((currentTime - lastEEPROM) >= saveSettings) {
    EEPROM.put(0,mySettings);
    if (EEPROM.commit()) {    
      lastEEPROM = currentTime;
      if (dbglevel > 0) { Serial.println(F("EEPROM updated")); }
    }
  }
  
  // Copy CCS811 basline to settings when warmup is finished ---------
  if (ccs811_avail && mySettings.useCCS811) {
    if (currentTime >= warmupCCS811) {    
      
      ccs811_port->begin(ccs811_i2c[0], ccs811_i2c[1]);
      
      mySettings.baselineCCS811 = ccs811.getBaseline();
      mySettings.baselineCCS811_valid = 0xF0;
      if (dbglevel == 10) { Serial.println(F("CCS811 baseline placed into settings")); }
      warmupCCS811 = warmupCCS811 + stablebaseCCS811; 
    }
  }
  
  // Copy SGP30 basline to settings when warmup is finished ----------
  if (sgp30_avail && mySettings.useSGP30) {  
    if (currentTime >=  warmupSGP30) {
      mySettings.baselinetVOC_SGP30 = sgp30.baselineTVOC;
      mySettings.baselineeCO2_SGP30 = sgp30.baselineCO2;
      mySettings.baselineSGP30_valid = 0xF0;
      if (dbglevel == 6) { Serial.println(F("SGP30 baseline setting updated")); }
      warmupSGP30 = warmupSGP30 + intervalSGP30Baseline; 
    }
  }

  // Update Warning --------------------------------------------------
  if ((currentTime - lastBlink) > intervalBlink) {
    if (allGood == false) { // blink the LCD backlight
      if (dbglevel > 0) { Serial.println(F("Sensors out of normal range!")); }
      lcd_port->begin(lcd_i2c[0], lcd_i2c[1]);
      lastBlink = currentTime;
      lastBlinkInten = !lastBlinkInten; 
      #if defined ADALCD  
      if (lastBlinkInten) {lcd.setBacklight(HIGH); intervalBlink=intervalBlinkOn; } else {lcd.setBacklight(LOW); intervalBlink=intervalBlinkOff; }
      #else
      if (lastBlinkInten) {lcd.setBacklight(255);  intervalBlink=intervalBlinkOn; } else {lcd.setBacklight(200); intervalBlink=intervalBlinkOff; }
      #endif
    } else {
      if (lastBlinkInten == false) { // make sure backlight is on
        lcd_port->begin(lcd_i2c[0], lcd_i2c[1]);
        #if defined ADALCD  
        lcd.setBacklight(HIGH); lastBlinkInten = true; intervalBlink=intervalBlinkOn;
        #else
        lcd.setBacklight(255);  lastBlinkInten = true; intervalBlink=intervalBlinkOn;
        #endif
      }
    } 
  }

  //  Free microcontroller for background tasks  ---------------------
  // while (millis() < currentTime + intervalLoop) yield();  // Andreas Spiess approach
  myDelay = long(currentTime+intervalLoop) - long(millis()); // my approach
  if (myDelay > 0) { delay(myDelay); }
  // can we keepup with handling the sensors?
  if (myDelayAvg == -99) {myDelayAvg = float(myDelay);}
  myDelayAvg = 0.9*myDelayAvg + 0.1*float(myDelay);
  
} // loop

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////
// Sensor Initializations
//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////

/******************************************************************************************************/
// LCD
/******************************************************************************************************/
void initializeLCD() {
  
  lcd_port->begin(lcd_i2c[0], lcd_i2c[1]);

#if defined ADALCD  
  lcd.begin(20, 4, LCD_5x8DOTS, *lcd_port);
  lcd.setBacklight(HIGH);
#else
  lcd.begin(20, 4, lcd_port);
  lcd.setBacklight(255);
#endif
  if (dbglevel > 0) { Serial.println(F("LCD initialized")); }
  delay(50);
} 

/******************************************************************************************************/
// SCD30
/******************************************************************************************************/
void initializeSCD30() {
  if (fastMode) { intervalSCD30 = intervalSCD30Fast; }               // set fast or slow mode
  else          { intervalSCD30 = intervalSCD30Slow; }
  if (dbglevel >0) { Serial.print(F("SCD30 Interval: ")); Serial.println(intervalSCD30); }

  if (dbglevel > 0) { Serial.println(F("SCD30: configuring interrupt")); }
  pinMode(SCD30interruptPin , INPUT);                      // interrupt scd30
  attachInterrupt(digitalPinToInterrupt(SCD30interruptPin),  handleSCD30Interrupt,  RISING);

  scd30_port->begin(scd30_i2c[0], scd30_i2c[1]);

  if (scd30.begin(*scd30_port, true)) {                              // start with autocalibration
    scd30.setMeasurementInterval(uint16_t(intervalSCD30/1000));
    scd30.setAutoSelfCalibration(true); 
    if (mySettings.tempOffset_SCD30_valid == 0xF0) { scd30.setTemperatureOffset(mySettings.tempOffset_SCD30); }
    mySettings.tempOffset_SCD30 = scd30.getTemperatureOffset();
    if (dbglevel > 0) { Serial.print(F("SCD30: current temp offset: ")); Serial.print(mySettings.tempOffset_SCD30,2); Serial.println(F("C")); }
    stateSCD30 = IS_BUSY;
  } else {
    scd30_avail = false;
    stateSCD30 = HAS_ERROR;
  }
  if (dbglevel > 0) { Serial.println("SCD30: initialized"); }
  delay(50);
}

/******************************************************************************************************/
// SGP30
/******************************************************************************************************/
void initializeSGP30() {
  if (fastMode) { intervalSGP30 = intervalSGP30Fast;}
  else          { intervalSGP30 = intervalSGP30Slow;}

  sgp30_port->begin(sgp30_i2c[0], sgp30_i2c[1]);

  if (sgp30.begin(*sgp30_port) == false) {
    if (dbglevel > 0) { Serial.println(F("No SGP30 Detected. Check connections")); }
    sgp30_avail = false;
    stateSGP30 = HAS_ERROR;
  }
  
  //Initializes sensor for air quality readings
  sgp30.initAirQuality();
  if (dbglevel > 0) { Serial.println(F("SGP30: measurements initialzed")); }
  stateSGP30 = IS_MEASURING;
  if (mySettings.baselineSGP30_valid == 0xF0) {
    sgp30.setBaseline((uint16_t)mySettings.baselineeCO2_SGP30, (uint16_t)mySettings.baselinetVOC_SGP30);
    if (dbglevel > 0) { Serial.println(F("SGP30: found valid baseline")); }
    warmupSGP30 = millis() + warmupSGP30_withbaseline;
  } else {
    warmupSGP30 = millis() + warmupSGP30_withoutbaseline;
  }
  delay(50);
}

/******************************************************************************************************/
// CCS811
/******************************************************************************************************/
void initializeCCS811() {

  if (fastMode == true) {
    ccs811Mode = ccs811ModeFast;                                     // 
    intervalCCS811Baseline = baselineCCS811Fast; 
    intervalCCS811Humidity = updateCCS811HumitityFast;
  } else {
    ccs811Mode = ccs811ModeSlow;                                     // 
    intervalCCS811Baseline = baselineCCS811Slow; 
    intervalCCS811Humidity = updateCCS811HumititySlow; 
    if (dbglevel > 0) { Serial.println(F("CCS811: it will take about 5 minutes until readings are non-zero.")); }
  }
  warmupCCS811 = currentTime + stablebaseCCS811;    

  if (dbglevel > 0) { Serial.println(F("CCS811: configuring interrupt")); }
  pinMode(CCS811_WAKE, OUTPUT);                            // CCS811 not Wake Pin
  pinMode(CCS811interruptPin, INPUT_PULLUP);               // CCS811 not Interrupt Pin
  attachInterrupt(digitalPinToInterrupt(CCS811interruptPin), handleCCS811Interrupt, FALLING);

  if (dbglevel > 0) { Serial.println(F("CCS811: wakingup sensor.")); }
  digitalWrite(CCS811_WAKE, LOW);                                    // Set CCS811 to wake
  delay(1);                                                          // wakeup takes 50 microseconds
  
  ccs811_port->begin(ccs811_i2c[0], ccs811_i2c[1]);  
  
  CCS811Core::CCS811_Status_e css811Ret = ccs811.beginWithStatus(*ccs811_port); // has delays and wait loops
  if (dbglevel > 0) { Serial.print(F("CCS811: begin - ")); Serial.println(ccs811.statusString(css811Ret)); }
  css811Ret = ccs811.setDriveMode(ccs811Mode);
  if (dbglevel > 0) { Serial.print(F("CCS811: mode request set - ")); Serial.println(ccs811.statusString(css811Ret)); }
  css811Ret = ccs811.enableInterrupts();                             // Configure and enable the interrupt line, then print error status
  if (dbglevel > 0) { Serial.print(F("CCS811: interrupt configuation - ")); Serial.println(ccs811.statusString(css811Ret)); }
  if (mySettings.baselineCCS811_valid == 0xF0) {
    CCS811Core::CCS811_Status_e errorStatus = ccs811.setBaseline(mySettings.baselineCCS811);
    if (errorStatus == CCS811Core::CCS811_Stat_SUCCESS) { 
      if (dbglevel > 0) { Serial.println(F("CCS811: baseline PROGRAMMED")); }
    } else {
      if (dbglevel > 0) { Serial.print(F("CCS811: error writing baseline - ")); Serial.println(ccs811.statusString(errorStatus)); }
    }
  }  
  if (dbglevel > 0) { Serial.println(F("CCS811: initialized")); }
  stateCCS811 = IS_IDLE;
  delay(50);
}

/******************************************************************************************************/
// SPS30
/******************************************************************************************************/
void initializeSPS30() {    
  if (fastMode == true) { intervalSPS30 = intervalSPS30Fast; }
  else                  { intervalSPS30 = intervalSPS30Slow; }

  sps30_port->begin(sps30_i2c[0], sps30_i2c[1]);  

  sps30.EnableDebugging(SPS30Debug);

  if (sps30.begin(sps30_port) == false) {
    if (dbglevel > 0) { Serial.println(F("SPS30: Sensor not detected in I2C. Please check wiring.")); }
    stateSPS30 = HAS_ERROR;
    sps30_avail = false;
  }

  if (sps30.probe() == false) { 
    // attempt reset and retry, I dont know how to restart sps30 without physical access.
    sps30.reset();
    delay(100);
    if (sps30.probe() == false) {
      if (dbglevel > 0) {
        Serial.println(F("SPS30: could not probe / connect")); 
        Serial.println(F("SPS30: powercycle the system and reopen serial console"));
      }
      stateSPS30 = HAS_ERROR;
      sps30_avail = false;
    }
  } else { 
    if (dbglevel > 0) { Serial.println(F("SPS30: detected")); }
  }

  if (sps30.reset() == false) { 
    if (dbglevel > 0) { Serial.println(F("SPS30: could not reset.")); }
    stateSPS30 = HAS_ERROR;
    sps30_avail = false;
  }  

  // read device info
  if (dbglevel > 0) {
    Serial.println(F("SPS30: obtaining device information:"));
    ret = sps30.GetSerialNumber(buf, 32);
    if (ret == ERR_OK) {
      if (dbglevel > 0) { 
        Serial.print(F("SPS30: serial number : "));
        if(strlen(buf) > 0) { Serial.println(buf); }
        else { Serial.println(F("not available")); }
      }
    }  else { Serial.println(F("SPS30: could not obtain serial number")); }

    ret = sps30.GetProductName(buf, 32);
    if (ret == ERR_OK)  {
        Serial.print(F("SPS30: product name  : "));
        if(strlen(buf) > 0)  { Serial.println(buf); }
        else { Serial.println(F("not available")); }
    } else { if (dbglevel > 0) { Serial.println(F("SPS30: could not obtain product name")); } }
  }
  
  ret = sps30.GetVersion(&v);
  if (ret != ERR_OK) { // I have sensor that reports v.minor = 255 and CRC error when reading version information
    if (dbglevel >0 ) { Serial.println(F("SPS30: error when reading version information")); } 
    if (v.major > 0 && v.major < 3)  { // is reasonable
      if (v.minor == 255) { v.minor = 0;} // is not reasonable
    }
  }
  if (dbglevel >0 ) {
    Serial.print(F("SPS30: firmware level: ")); Serial.print(v.major);        Serial.print(F(".")); Serial.println(v.minor);
    Serial.print(F("SPS30: hardware level: ")); Serial.println(v.HW_version); 
    Serial.print(F("SPS30: SHDLC protocol: ")); Serial.print(v.SHDLC_major);  Serial.print(F(".")); Serial.println(v.SHDLC_minor);
    Serial.print(F("SPS30: library level : ")); Serial.print(v.DRV_major);    Serial.print(F(".")); Serial.println(v.DRV_minor);
  }
  
  ret = sps30.GetAutoCleanInt(&autoCleanIntervalSPS30);
  if (ret == ERR_OK) {
    if (dbglevel > 0) { Serial.print(F("SPS30: current Auto Clean interval: ")); Serial.print(autoCleanIntervalSPS30); Serial.println(F("s")); }
  }  else { Serial.println(F("SPS30: coulnd not obtain autoclean information")); }
  
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
  delay(50);
}

/******************************************************************************************************/
// BME680
/******************************************************************************************************/
void initializeBME680() {
  
  bme680_port->begin(bme680_i2c[0], bme680_i2c[1]);  
  
  if (bme680.begin(0x77, true, bme680_port) == true) { 
    stateBME680 = IS_IDLE;
    if (dbglevel > 0) { Serial.print(F("BME680: Setting oversampling for sensors\n")); }
    if (fastMode == true) { 
      intervalBME680 = intervalBME680Fast; 
      bme680.setTemperatureOversampling(bme680_TempOversampleFast);
      bme680.setHumidityOversampling(bme680_HumOversampleFast); 
      bme680.setPressureOversampling(bme680_PressureOversampleFast); 
      if (dbglevel > 0) { Serial.print(F("BME680: Setting IIR filter for fast measurements\n")); }
      bme680.setIIRFilterSize(bme680_FilterSizeFast); 
    } else { 
      intervalBME680 = intervalBME680Slow; 
      bme680.setTemperatureOversampling(bme680_TempOversampleSlow);
      bme680.setHumidityOversampling(bme680_HumOversampleSlow); 
      bme680.setPressureOversampling(bme680_PressureOversampleSlow); 
      if (dbglevel > 0) { Serial.print(F("BME680: Setting IIR filter for slow measurements\n")); }
      bme680.setIIRFilterSize(bme680_FilterSizeSlow); 
    }      
    if (dbglevel > 0) { Serial.print(F("BME680: Setting gas measurement to 320\xC2\xB0\x43 for 150ms\n")); }
    bme680.setGasHeater(bme680_HeaterTemp,bme680_HeaterDuration); 
    BMEhum_avail = true;
    if (dbglevel > 0) { Serial.println(F("BME680: Initialized")); }
  } else {
    if (dbglevel > 0) { Serial.println(F("BME680: Sensor not detected. Please check wiring.")); }
    stateBME680 = HAS_ERROR;
    bme680_avail = false;
  }   
  
  // Construct poor man s lowpass filter y = (1-alpha) * y + alpha * x
  // https://dsp.stackexchange.com/questions/54086/single-pole-iir-low-pass-filter-which-is-the-correct-formula-for-the-decay-coe
  // filter cut off frequency is 1/day
  float   w_c = 2.0 * 3.141 / (24.0 * 3600.0);                       // = cut off frequency [radians / seconds]
  float   f_s = 1.0 / (intervalBME680 / 1000.0);                     // = sampling frenecy [1/s] 
          w_c = w_c / f_s;                                           // = normalize cut off frequency [radians]
  float     y = 1 - cos(w_c);                                        //
  alphaBME680 = -y + sqrt( y*y + 2 * y);                             // 
  delay(50);
}

/******************************************************************************************************/
// BME280
/******************************************************************************************************/
void initializeBME280() {

  bme280_port->begin(bme280_i2c[0], bme280_i2c[1]);  
  
  bme280.settings.commInterface = I2C_MODE;
  bme280.settings.I2CAddress = 0x76;
  if (dbglevel > 0) { Serial.print(F("BME280: setting oversampling for sensors\n")); }
  if (fastMode == true) { 
    intervalBME280                  = intervalBME280Fast;
    bme280.settings.runMode         = bme280_ModeFast;
    bme280.settings.tStandby        = bme280_StandbyTimeFast;
    bme280.settings.filter          = bme280_FilterFast;
    bme280.settings.tempOverSample  = bme280_TempOversampleFast; 
    bme280.settings.pressOverSample = bme280_PressureOversampleFast;
    bme280.settings.humidOverSample = bme280_HumOversampleFast;
  } else { 
    intervalBME280                  = intervalBME280Slow;
    bme280.settings.runMode         = bme280_ModeSlow;
    bme280.settings.tStandby        = bme280_StandbyTimeSlow;
    bme280.settings.filter          = bme280_FilterSlow;
    bme280.settings.tempOverSample  = bme280_TempOversampleSlow; 
    bme280.settings.pressOverSample = bme280_PressureOversampleSlow;
    bme280.settings.humidOverSample = bme280_HumOversampleSlow;
  }
  
  uint8_t chipID=bme280.beginI2C(*bme280_port);
  if (chipID == 0x58) {
    BMEhum_avail = false;
    if (dbglevel > 0) { Serial.println(F("BMP280: initialized")); }
  } else if(chipID == 0x60) {
    BMEhum_avail = true;
    if (dbglevel > 0) { Serial.println(F("BME280: initialized")); }
  } else {
    BMEhum_avail = false;
    if (dbglevel > 0) { Serial.println(F("BMx280: Sensor not detected. Please check wiring.")); }    
    stateBME280 = HAS_ERROR;
    bme280_avail = false;
  }

  // Calculate the time it takes to complete a measurement cycle
  // -----------------------------------------------------------
  float tmp = 1.0;
  if      (bme280.settings.tempOverSample == 1) {tmp = tmp + 2.;}
  else if (bme280.settings.tempOverSample == 2) {tmp = tmp + 4.;}
  else if (bme280.settings.tempOverSample == 3) {tmp = tmp + 8.;}
  else if (bme280.settings.tempOverSample == 4) {tmp = tmp + 16.;}
  else if (bme280.settings.tempOverSample == 5) {tmp = tmp + 32.;}

  if      (bme280.settings.pressOverSample == 1) {tmp = tmp + 2.5;}
  else if (bme280.settings.pressOverSample == 2) {tmp = tmp + 4.5;}
  else if (bme280.settings.pressOverSample == 3) {tmp = tmp + 8.5;}
  else if (bme280.settings.pressOverSample == 4) {tmp = tmp + 16.5;}
  else if (bme280.settings.pressOverSample == 5) {tmp = tmp + 32.5;}

  if (BMEhum_avail) {
    if      (bme280.settings.humidOverSample == 1) {tmp = tmp + 2.5;}
    else if (bme280.settings.humidOverSample == 2) {tmp = tmp + 4.5;}
    else if (bme280.settings.humidOverSample == 3) {tmp = tmp + 8.5;}
    else if (bme280.settings.humidOverSample == 4) {tmp = tmp + 16.5;}
    else if (bme280.settings.humidOverSample == 5) {tmp = tmp + 32.5;}
  }

  if (bme280.settings.runMode == 3) {
    if      (bme280.settings.tStandby == 0) { tmp = tmp + 0.5; }
    else if (bme280.settings.tStandby == 1) { tmp = tmp + 62.5; }
    else if (bme280.settings.tStandby == 2) { tmp = tmp + 125.0; }
    else if (bme280.settings.tStandby == 3) { tmp = tmp + 250.0; }
    else if (bme280.settings.tStandby == 4) { tmp = tmp + 500.0; }
    else if (bme280.settings.tStandby == 5) { tmp = tmp + 1000.0; }
    else if (bme280.settings.tStandby == 6) { tmp = tmp + 10.0; }
    else if (bme280.settings.tStandby == 7) { tmp = tmp + 20.0; }
  }
  bme280_measuretime = long(tmp);

  // make sure we dont attempt reading faster than it takes the sensor to complete a reading
  if (bme280_measuretime > intervalBME280) {intervalBME280 = bme280_measuretime;}

  // Construct poor man's lowpass filter y = (1-alpha) * y + alpha * x
  // https://dsp.stackexchange.com/questions/54086/single-pole-iir-low-pass-filter-which-is-the-correct-formula-for-the-decay-coe
  // filter cut off frequency is 1/day
  float   w_c = 2.0 * 3.141 / (24.0 * 3600.0);                        // = cut off frequency [radians / seconds]
  float   f_s = 1.0 / (intervalBME280 / 1000.0);                      // = sampling frenecy [1/s] 
          w_c = w_c / f_s;                                            // = normalize cut off frequency [radians]
  float     y = 1 - cos(w_c);                                         // compute alpha for 3dB attenuation at cut off frequency
  alphaBME280 = -y + sqrt( y*y + 2 * y);                              // 

  if ((chipID == 0x58) || (chipID == 0x60))  {                        //
    if (bme280.settings.runMode == MODE_NORMAL) {                     // for normal mode we obtain readings periodically
        bme280.setMode(MODE_NORMAL);                                  //
        stateBME280 = IS_BUSY;                                        //
    } else if (bme280.settings.runMode == MODE_FORCED) {              // for forced mode we initiate reading manually
      bme280.setMode(MODE_SLEEP);                                     // sleep for now
      stateBME280 = IS_SLEEPING;                                      //
    } else if (bme280.settings.runMode == MODE_SLEEP){                //
       bme280.setMode(MODE_SLEEP);                                    //
       stateBME280 = IS_SLEEPING;                                     //
    }
  }
  delay(50);

} // end bme280

/******************************************************************************************************/
// MLX
/******************************************************************************************************/
void initializeMLX(){

  mlx_port->begin(mlx_i2c[0], mlx_i2c[1]);  
  
  if (therm.begin(0x5A, *mlx_port) == true) { 
    therm.setUnit(TEMP_C); // Set the library's units to Centigrade
    therm.setEmissivity(emissivity);
    if (dbglevel > 0) { Serial.print(F("MLX: Emissivity: ")); Serial.println(therm.readEmissivity()); }
    stateMLX = IS_MEASURING;      
  } else {
    if (dbglevel > 0) { Serial.println(F("0MLX: sensor not detected. Please check wiring.")); }
    stateMLX = HAS_ERROR;
    therm_avail = false;
  }
  sleepTimeMLX = intervalMLX - timeToStableMLX - 50;
  if (dbglevel > 0) {
    Serial.print(F("MLX: sleep time is ")); Serial.print(sleepTimeMLX);  Serial.println(F("ms"));
    Serial.print(F("MLX: interval time is ")); Serial.print(intervalMLX); Serial.println(F("ms"));
  }

  if (mySettings.tempOffset_MLX_valid == 0xF0) {
    mlxOffset = mySettings.tempOffset_MLX;
    if (dbglevel > 0) { Serial.println(F("MLX: offset found"));}
  }

  if (dbglevel > 0) { Serial.println(F("MLX: Initialized")); }
  delay(50);
}

/******************************************************************************************************/
// WiFi & MQTT
/******************************************************************************************************/
void initializeWiFi() {    
  if (WiFi.status() == WL_NO_SHIELD) {
    wifi_avail = false;
    stateWiFi = IS_WAITING;
    stateMQTT = IS_WAITING;
    if (dbglevel > 0) { Serial.println(F("WiFi is not available")); }
  } else {
    wifi_avail = true;
    if (dbglevel > 0) { Serial.println(F("WiFi is available")); }
    if (dbglevel > 0) { Serial.print("MAC: "); Serial.println(WiFi.macAddress()); }
    if (WiFi.status() == WL_CONNECTED) { WiFi.disconnect(); } // make sure we are not connected
    if (mySettings.useWiFi == true) {
      WiFi.mode(WIFI_STA);
      stateWiFi = START_UP;
      stateMQTT = IS_WAITING;
      mqttClient.setCallback(mqttCallback);
      if (dbglevel > 0) { Serial.println(F("WiFi mode set to client")); }
    } else {
      wifi_avail = false;
      stateWiFi = IS_WAITING;
      WiFi.mode(WIFI_OFF);
      if (dbglevel > 0) { Serial.println(F("WiFi turned off")); }
    }
    delay(50);
  }
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////
// Sensor state machines, update routines for the main loop
//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////

/******************************************************************************************************/
// WiFi
/******************************************************************************************************/
void updateWiFi() {  
  // Operation: 
  //   set nework to scan mode
  //   check if scan completed
  //   if scan is not completed continue scanning
  //   if no networks found go back to start scan mode
  //   if networks found check if ssid match any of our three options stored in EEPROM
  //   if no networks found go back to start scan mode
  //   if network found attempt connecting
  //   if connecting was not successful got back to start scanning
  //   if connection successfull go to check connection status
  //   remain in connection status until connection lost
  //   if connection lost go bach to start scanning
  
  switch(stateWiFi) {
    
    case IS_WAITING : { //---------------------
      break;
    }

    case START_UP : { //---------------------
      WiFi.scanNetworks(true, false); // async scanning, dont scan for hidden SSID
      if (dbglevel == 3) { Serial.println(F("Scanning Networks.")); }
      stateWiFi = IS_SCANNING;
      stateMQTT = IS_WAITING;        
    }
    
    case IS_SCANNING : { //---------------------
      if ((currentTime - lastWiFi) >= intervalWiFi) {
        nNetworks = WiFi.scanComplete();
        if (nNetworks == -1) {
          if (dbglevel == 3) { Serial.println(F("WiFi Scanning not complete.")); }
        } else if (nNetworks == 0) { // end still scanning
          if (dbglevel == 3) { Serial.println(F("No SSID found.")); }
          WiFi.scanDelete();
        } else if (nNetworks > 0) { // end no networks found       
          strcpy(ssid, "");
          ssid_found = false;
          for (int i=0; i<nNetworks; i++) {
            if (WiFi.SSID(i) == String(mySettings.ssid1)) {
              strcpy(ssid, mySettings.ssid1);
              strcpy(pw, mySettings.pw1);
              ssid_found=true;
            }
            if (WiFi.SSID(i) == String(mySettings.ssid2)) {
              strcpy(ssid, mySettings.ssid2);
              strcpy(pw, mySettings.pw2);
              ssid_found=true;
            } 
            if (WiFi.SSID(i) == String(mySettings.ssid3)) {
              strcpy(ssid, mySettings.ssid3);
              strcpy(pw, mySettings.pw3);
              ssid_found=true;
            }
          } // end checking for known SSIDs
          if (ssid_found == true) {
            if (dbglevel > 0) { Serial.print(F("Connecting to: ")); Serial.println(ssid); }
            if (strlen(ssid) > 0) {
              if (strlen(pw) > 0) { 
                WiFi.begin(ssid, pw); // regular password enabled conenction
              } else { 
                WiFi.begin(ssid);  // my corporate network allows to connect without authentication for devices with preregistered MAC
              } 
            } else { WiFi.begin(); }
            connectionAttempts = 0;
            stateWiFi = IS_CONNECTING;
          } else { 
            if (dbglevel == 3) { Serial.println(F("SSID not found.")); }
            stateWiFi = START_UP;
          } // end ssid found
          WiFi.scanDelete();
        } // end networks found
        lastWiFi = currentTime;
      }
      break;
    } // end of scanning
    
    case IS_CONNECTING : { //---------------------
      if ((currentTime - lastWiFi) >= intervalWiFi) {
        connectionAttempts++;
        if (WiFi.status() == WL_CONNECTED) {
          if (dbglevel == 3) { Serial.println(F("WiFi Connecting: Success.")); }
          wifi_connected = true;
          stateWiFi = CHECK_CONNECTION;
          stateMQTT = START_UP;
        }
        if (connectionAttempts > 10) {
          if (dbglevel > 0) { Serial.println(F("WiFi Connecting: Failure.")); }
          stateWiFi = START_UP;
        }
        lastWiFi = currentTime;
      }
      break;        
    } // end is connecting
    
    case CHECK_CONNECTION : { //---------------------
      if ((currentTime - lastWiFi) >= intervalWiFi) {
        if (WiFi.status() != WL_CONNECTED) {
          if (dbglevel == 3) { Serial.println(F("WiFi is disconnected.")); }
          stateWiFi = START_UP;
          stateMQTT = IS_WAITING;        
          wifi_connected = false;
          mqtt_connected = false;
        }
        lastWiFi = currentTime;
      }
      break;
    }
  
  } // end switch case
}

/******************************************************************************************************/
// MQTT
/******************************************************************************************************/
void updateMQTT() {
  // Operation:
  //   Wait until WiFi connection is established
  //   Set the MQTT server
  //   Connect with either username or no username
  //   If connection did not work, attempt connection to fallback server
  //   Once connection is stablished, periodically check if still connected
  //   If connection lost go back to setting up MQTT server

  switch(stateMQTT) {
    
    case IS_WAITING : { //---------------------
      // just wait...
      if ((currentTime - lastMQTT) >= intervalWiFi) {
        if (dbglevel == 3) { Serial.println(F("MQTT is waiting for network to come up.")); }          
        lastMQTT = currentTime;
      }
       break;
    }
    
    case START_UP : { //---------------------
      if ((currentTime - lastMQTT) >= intervalWiFi) {
        mqttClient.setServer(mySettings.mqtt_server, MQTT_PORT);
        if (dbglevel > 0) { Serial.print(F("Connecting to MQTT: ")); Serial.println(mySettings.mqtt_server); }
        connectSuccess = false;
        if (strlen(mySettings.mqtt_username) > 0) {
          connectSuccess = mqttClient.connect("SensiEPS8266Client", mySettings.mqtt_username, mySettings.mqtt_password);
        } else {
          connectSuccess = mqttClient.connect("SensiEPS8266Client");
        }
        if (connectSuccess == true) {
          strcpy(payloadStr, "Sensi up");
          strcpy(topicStr, mySettings.mqtt_mainTopic);
          strcat(topicStr, "/status");
          mqttClient.publish(topicStr, payloadStr);
          mqtt_connected = true;
          stateMQTT = CHECK_CONNECTION;
          if (dbglevel == 3) { Serial.println(F("Connecting to MQTT: success")); }
        } else { // connection did not work, try fallback server
          mqttClient.setServer(mySettings.mqtt_fallback, MQTT_PORT);
          if (dbglevel > 0) { Serial.print(F("Connecting to MQTT: ")); Serial.println(mySettings.mqtt_fallback); }
          if (strlen(mySettings.mqtt_username) > 0) {
            connectSuccess = mqttClient.connect("SensiEPS8266Client", mySettings.mqtt_username, mySettings.mqtt_password);
          } else {
            connectSuccess = mqttClient.connect("SensiEPS8266Client");
          }  
          if (connectSuccess == true) {  
            strcpy(payloadStr, "Sensi up");
            strcpy(topicStr, mySettings.mqtt_mainTopic);
            strcat(topicStr, "/status");
            mqttClient.publish(topicStr, payloadStr);
            if (dbglevel == 3) { Serial.println(F("Connecting to MQTT: success")); }
            mqtt_connected = true;
            stateMQTT = CHECK_CONNECTION;
          } else {  // connection failed
            if (dbglevel > 0) { Serial.println(F("Connecting to MQTT: failed")); }
            mqtt_connected = false;
            // repeat starting up
          }
        }
        lastMQTT = currentTime;
      }
      break;
    }
    
    case CHECK_CONNECTION : { //---------------------
      if ((currentTime - lastMQTT) >= intervalWiFi) {
        if (mqttClient.loop() != true) {
          mqtt_connected = false;
          stateMQTT = START_UP;
          if (dbglevel == 3) { Serial.print(F("MQTT is disconnected. Reconnect to server.")); }
        }
        lastMQTT = currentTime;
      }
      break;          
    }
          
  } // end switch state
}

/******************************************************************************************************/
// SCD30
/******************************************************************************************************/
void updateSCD30 () {
  // Operation:
  //   Sensor is initilized
  //     Measurement interval is set
  //     Autocalibrate is enabled
  //     Temperature offset to compensate self heating, improves humidity reading
  //     Pressure is set
  //   Begin Measureing
  //   If sensor has completed measurement ISR is triggered
  //   ISR advances statemachine
  //    Check if Data Available
  //     get CO2, Humidity, Temperature
  //    Update barometric pressure every once in a while if third party sensor is avaialble
  //    Wait until next ISR
  
  switch(stateSCD30) {
    
    case IS_MEASURING : { // used when RDY pin interrupt is not enabled
      if ((currentTime - lastSCD30) >= intervalSCD30) {
        
        scd30_port->begin(scd30_i2c[0], scd30_i2c[1]);  
        
        if (scd30.dataAvailable()) {
          scd30_ppm  = scd30.getCO2();
          scd30_temp = scd30.getTemperature();
          scd30_hum  = scd30.getHumidity();
          lastSCD30  = currentTime;
          if (dbglevel == 4) { Serial.println(F("SCD30: data read")); }
          scd30NewData = true;
        } else {
          if (dbglevel == 4) { Serial.println(F("SCD30 data not yet available")); }
        }
      }
      break;
    } // is measuring
    
    case IS_BUSY: { // used to bootup sensor when RDY pin interrupt is enabled
      if ((currentTime - lastSCD30Busy) > intervalSCD30Busy) {
        
        scd30_port->begin(scd30_i2c[0], scd30_i2c[1]);  
        
        if (scd30.dataAvailable()) {scd30.readMeasurement();} // without reading data, RDY pin will remain high and no interrupt will occur
        lastSCD30Busy = currentTime;
        if (dbglevel == 4) { Serial.println(F("SCD30: is busy")); }
      }
      break;
    }
    
    case DATA_AVAILABLE : { // used to obtain data when RDY pin interrupt is used
      
      scd30_port->begin(scd30_i2c[0], scd30_i2c[1]);  
      
      scd30_ppm  = scd30.getCO2();
      scd30_temp = scd30.getTemperature();
      scd30_hum  = scd30.getHumidity();
      lastSCD30  = currentTime;
      scd30NewData = true;
      stateSCD30 = IS_IDLE; 
      break;
    }
    
    case IS_IDLE : { // used when RDY pin is used with ISR
      // wait for intrrupt to occur, 
      // update pressure if available
      if (bme680_avail && mySettings.useBME680) { // update pressure settings
        if ((currentTime - lastPressureSCD30) >= intervalPressureSCD30) {

          scd30_port->begin(scd30_i2c[0], scd30_i2c[1]);  
          
          scd30.setAmbientPressure(uint16_t(bme680.pressure/100.0));  // update with value from pressure sensor, needs to be mbar
          lastPressureSCD30 = currentTime;
          if (dbglevel == 4) { Serial.print(F("SCD30: pressure updated to")); Serial.print(bme680.pressure/100.0); Serial.println(F("mbar")); }
        }
      } else if ((bme280_avail && mySettings.useBME280)) {
        if ((currentTime - lastPressureSCD30) >= intervalPressureSCD30) {
          
          scd30_port->begin(scd30_i2c[0], scd30_i2c[1]);  
          
          scd30.setAmbientPressure(uint16_t(bme280_pressure/100.0));  // pressure is in Pa and scd30 needs mBar
          lastPressureSCD30 = currentTime;
          if (dbglevel == 4) { Serial.print(F("SCD30: pressure updated to")); Serial.print(bme280_pressure/100.0); Serial.println(F("mbar")); }
        }
      }
      break;        
    }
    
    case HAS_ERROR : {
      if (dbglevel > 0) { Serial.println(F("SCD30 has error!")); }
      break;
    }
    
  } // switch state
}

/******************************************************************************************************/
// SGP30
/******************************************************************************************************/
void updateSGP30() {
  // Operation Sequence:
  //  Setup
  //   Power up, puts the sensor in sleep mode
  //   Init Command (10ms to complete)
  //   Read baseline from EEPROM
  //   If valid set baseline
  //  Update
  //   Humidity interval exceeded?
  //     write humidity from third party sensors to sensor (10ms to complete)
  //   Start Measure Airquality Command 
  //   Unresponsive for 12ms
  //   Initiate Get Airquality Command
  //   Wait until measurement interval exceeded
  //
  // Read Baseline (10ms) periodically from sensors dynamic baseline calculations
  // Set Humidity from third party sensor periodically to increase accuracy

  switch(stateSGP30) {
    
    case IS_MEASURING : { //---------------------
      if ((currentTime - lastSGP30Humidity) > intervalSGP30Humidity) {
        if (bme680_avail && mySettings.useBME680) {
          
          sgp30_port->begin(sgp30_i2c[0], sgp30_i2c[1]);  
          
          // Humidity correction, 8.8 bit number
          // 0x0F80 = 15.5 g/m^3
          // 0x0001 = 1/256 g/m^3
          // 0xFFFF = 256 +256/256 g/m^3
          sgp30.setHumidity(uint16_t(bme680_ah * 256.0 + 0.5)); 
          if (dbglevel == 6) { Serial.println(F("SGP30: humidity updated for eCO2")); }
        }
        lastSGP30Humidity = currentTime;        
      } // end humidity update 
      if ((currentTime - lastSGP30Baseline) > intervalSGP30Baseline) {

        sgp30_port->begin(sgp30_i2c[0], sgp30_i2c[1]);  
        
        sgp30.startGetBaseline(); // this has 10ms delay
        if (dbglevel == 6) { Serial.println(F("SGP30: obtaining internal baseline")); }
        lastSGP30Baseline= millis();
        stateSGP30 = GET_BASELINE;
        break;
      }
      if ((currentTime - lastSGP30) > intervalSGP30) {
        
        sgp30_port->begin(sgp30_i2c[0], sgp30_i2c[1]);  
        
        sgp30.startAirQuality();
        if (dbglevel == 6) { Serial.println(F("SGP30: airquality measurement initiated")); }
        lastSGP30 = currentTime;
        stateSGP30 = IS_BUSY;
      }
      break;
    }
    
    case IS_BUSY : { //---------------------
      if ((currentTime - lastSGP30) >= 12) {
        
        sgp30_port->begin(sgp30_i2c[0], sgp30_i2c[1]);  
        
        sgp30.getAirQuality();
        lastSGP30 = currentTime;
        stateSGP30 = IS_MEASURING;
        if (dbglevel == 6) { Serial.println(F("SGP30: eCO2 & tVOC measured")); }
        sgp30NewData = true;
      }
      break;
    }
    
    case GET_BASELINE : { //---------------------
      if ((currentTime - lastSGP30Baseline) > 10) {
        
        sgp30_port->begin(sgp30_i2c[0], sgp30_i2c[1]);  
        
        if (sgp30.finishGetBaseline() == SUCCESS) {
          stateSGP30 = IS_MEASURING; 
          if (dbglevel == 6) { Serial.println(F("SGP30: baseline obtained")); }
        } else { stateSGP30 = HAS_ERROR; }
      }
      break;
    }
    
  } // switch state
}

/******************************************************************************************************/
// CCS811
/******************************************************************************************************/
void updateCCS811() {
  // Operation:
  //  Setup 
  //   turn on sensor and starts reading
  //  When measurement complete, interrupt is asserted and ISR is called
  //  if fastMode = false
  //    ISR wakes up i2c communication on wakeup pin
  //    Sensor is given 1ms to wake up
  //    Senor data is read and sensor logic is switched back to sleep mode
  //  if fastMode = true
  //    Sensor data is read
  //  Status is sleeping
  //    Waiting until data interrupt occurs

  switch(stateCCS811) {
    
    case IS_WAKINGUP : { // ISR will activate this when getting out of sleeping
      if ((currentTime - lastCCS811Interrupt) >= 1) { // wakeup time is 50micro seconds
        stateCCS811 = DATA_AVAILABLE;
        if (dbglevel == 10) { Serial.println(F("CCS811: wakeup completed")); }
      }
      break;
    }
    
    case DATA_AVAILABLE : { // executed either after sleeping or ideling
      tmpTime = millis();
      
      ccs811_port->begin(ccs811_i2c[0], ccs811_i2c[1]);  
      
      ccs811.readAlgorithmResults(); //Calling this function updates the global tVOC and CO2 variables
      if (dbglevel == 10) { Serial.print(F("CCS811: readAlgorithmResults completed in ")); Serial.print((millis()-tmpTime)); Serial.println(F("ms")); }
      ccs811NewData=true;
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
        if (dbglevel == 10) { Serial.print(F("CCS811: baseline obtained in")); Serial.print((millis()-tmpTime)); Serial.println(F("ms")); }
      }
      
      if ((currentTime - lastCCS811Humidity) > intervalCCS811Humidity ) {
        lastCCS811Humidity = currentTime;
        if (bme680_avail && mySettings.useBME680) {
          tmpTime = millis();
          ccs811.setEnvironmentalData(bme680.humidity, bme680.temperature);
          if (dbglevel == 10) { Serial.print(F("CCS811: humidity and temperature compensation updated in "));  Serial.print((millis()-tmpTime)); Serial.println(F("ms")); }
        }
      }
      
      if (fastMode == false) { 
        digitalWrite(CCS811_WAKE, HIGH);     // put CCS811 to sleep
        if (dbglevel == 10) { Serial.println(F("CCS811: puttting sensor to sleep")); }
        stateCCS811 = IS_SLEEPING;
      } else {
        stateCCS811 = IS_IDLE;
      }
      lastCCS811 = currentTime;
      break;
    }
    
    case IS_IDLE : { //---------------------
      // Continue ideling, we are waiting for interrupt
      if (dbglevel == 10) { Serial.println(F("CCS811: is idle")); }
      // if not use ISR use this
      //if (digitalRead(CCS811_INT) == 0) {
      //  stateCCS811 = DATA_AVAILABLE;          // update the sensor state
      //  if (dbglevel == 10) {Serial.println(F("CCS811: interrupt occured")); }
      //} // end if not use ISR
      break;
    }
    
    case IS_SLEEPING : { //---------------------
      // Continue Sleeping, we are waiting for interrupt
      if (dbglevel == 10) { Serial.println(F("CCS811: is sleeping")); }
      // if we dont want to use ISR method use this:
      //if (digitalRead(CCS811_INT) == 0) {
      //  digitalWrite(CCS811_WAKE, LOW);        // set CCS811 to wake up 
      //  stateCCS811 = IS_WAKINGUP;             // update the sensor state
      //  lastCCS811Interrupt = millis();
      //  if (dbglevel == 10) {Serial.println(F("CCS811: interrupt occured")); }
      //} // end if not use ISR
      break;
    }
    
  } // end switch
}

/******************************************************************************************************/
// SPS30
/******************************************************************************************************/
void updateSPS30() {   
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
  //   Start measurement
  //   Goto wait until stable
  // Fast Mode:
  //   Wait until measurement interval exceeded
  //   Goto read data
  //

  switch(stateSPS30) { 
    
    case IS_BUSY: { //---------------------
      if (dbglevel == 5) {  Serial.println(F("SPS30: is busy")); }
      
      sps30_port->begin(sps30_i2c[0], sps30_i2c[1]);  
      
      if (((v.major==2) && (v.minor>=2)) || (v.major>2)) {
        ret = sps30.GetStatusReg(&st);         // takes 20ms
        if (ret == ERR_OK) {
          if (dbglevel == 5) { Serial.print(F("SPS30: reading status completed and ")); }
          if (st == STATUS_OK) { 
            if (dbglevel == 5) { Serial.println(F("SPS30 Status: ok")); }
          } else {
            if (dbglevel > 0) {
              if (st & STATUS_SPEED_ERROR) { Serial.println(F("SPS30 WARNING : Fan is turning too fast or too slow")); }
              if (st & STATUS_LASER_ERROR) { Serial.println(F("SPS30 ERROR: Laser failure")); }  
              if (st & STATUS_FAN_ERROR)   { Serial.println(F("SPS30 ERROR: Fan failure, fan is mechanically blocked or broken")); }
            }
            stateSPS30 = HAS_ERROR;
          } // status ok
        } else { 
          if (dbglevel > 0) { Serial.println(F("SPS30: could not read status")); } 
        }// read status
      } // Firmware >= 2.2
      tmpTime = millis();
      ret = sps30.GetValues(&valSPS30);               
      if (dbglevel == 5) { Serial.print(F("SPS30: values read in ")); Serial.print(millis() - tmpTime); Serial.println(F("ms")); }
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
      if (dbglevel == 5) { Serial.print(F("SPS30: total particles - ")); Serial.println(totalParticles); }
      if (dbglevel == 5) {
        Serial.print(F("Current Time: ")); Serial.println(currentTime);
        Serial.print(F("SPS30: time when stable - ")); Serial.println(timeSPS30Stable);
      }
      stateSPS30 = WAIT_STABLE;            
      break; 
    } // is BUSY

    case WAIT_STABLE: { //---------------------
      if (dbglevel == 5) {
        Serial.println(F("SPS30: wait until stable"));
        Serial.print(F("Current Time: ")); Serial.println(currentTime);
        Serial.print(F("SPS30: time when stable - ")); Serial.println(timeSPS30Stable);
      }
      if (currentTime >= timeSPS30Stable) {
        
        sps30_port->begin(sps30_i2c[0], sps30_i2c[1]);  
        
        if (((v.major==2) && (v.minor>=2)) || (v.major>2)) {
          ret = sps30.GetStatusReg(&st);         // takes 20ms
          if (ret == ERR_OK) {
            if (dbglevel == 5) { Serial.print(F("SPS30: reading status completed and ")); }
            if (st == STATUS_OK) {
              if (dbglevel == 5) { Serial.println(F("status is ok"));}
            } else {
              if (dbglevel > 0) {
                if (st & STATUS_SPEED_ERROR) { Serial.println(F("(WARNING) fan is turning too fast or too slow")); }
                if (st & STATUS_LASER_ERROR) { Serial.println(F("(ERROR) laser failure")); }  
                if (st & STATUS_FAN_ERROR)   { Serial.println(F("(ERROR) fan failure, fan is mechanically blocked or broken")); }
              }
            } // status ok
          } else {
            if (dbglevel > 0) { Serial.println(F("SPS30: could not read status")); }
          } // read status
        } // Firmware >= 2.2
        tmpTime = millis();
        ret = sps30.GetValues(&valSPS30);
        if (dbglevel == 5) {
          Serial.print(F("SPS30: values read in ")); 
          Serial.print(millis() - tmpTime); 
          Serial.println(F("ms"));
        }
        if (ret == ERR_DATALENGTH) { 
          if (dbglevel > 0) { Serial.println(F("SPS30 ERROR: Data length during reading values")); }
        } else if (ret != ERR_OK) {
          if (dbglevel > 0) { Serial.print(F("SPS ERROR: reading values: ")); Serial.println(ret); }
        }
        sps30NewData = true;
        if (dbglevel == 5) { Serial.println(F("SPS30: going to idle")); }
        lastSPS30 = currentTime; 
        stateSPS30 = IS_IDLE;          
      } //if time stable
      break;
    } // wait stable
                
    case IS_IDLE : { //---------------------
      if (dbglevel == 5) { Serial.println("SPS30: is idle"); }
      if ((v.major<2) || (fastMode)) {
        timeSPS30Stable = (unsigned long) (lastSPS30 + intervalSPS30);
        if (dbglevel == 5) {
          Serial.print(F("Last SPS30: ")); Serial.println(lastSPS30);
          Serial.print(F("Current Time: ")); Serial.println(currentTime);
          Serial.print(F("SPS30: time when stable - ")); Serial.println(timeSPS30Stable);
          Serial.println(F("SPS30: going to wait until stable"));
        }
        stateSPS30 = WAIT_STABLE;             
      } else {
        wakeTimeSPS30 = (unsigned long) (lastSPS30 + intervalSPS30 - 50 - timeToStableSPS30);

        sps30_port->begin(sps30_i2c[0], sps30_i2c[1]);  
        
        ret = sps30.sleep(); // 5ms execution time
        if (ret != ERR_OK) { 
          if (dbglevel > 0) { Serial.println(F("SPS30 ERROR: could not go to sleep")); }
          stateSPS30 = HAS_ERROR;
        }
        if (dbglevel == 5) { Serial.println(F("SPS30: going to sleep")); }
        stateSPS30 = IS_SLEEPING;
      }
      break;
    }
      
    case IS_SLEEPING : { //---------------------
      if (dbglevel == 5) { Serial.println(F("SPS30: is sleepig")); }
      if (currentTime >= wakeTimeSPS30) { // Wake up if sleep time exceeded
         
        sps30_port->begin(sps30_i2c[0], sps30_i2c[1]);  
        
        if (dbglevel == 5) { Serial.println(F("SPS30 Status: waking up")); }
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
      if (dbglevel == 5) { Serial.println(F("SPS30: is waking up")); }
      if ((currentTime - wakeSPS30) >= 50) { // Give some time (50ms)to wake up 
        
        sps30_port->begin(sps30_i2c[0], sps30_i2c[1]);  
        
        ret = sps30.start(); 
        if (ret != ERR_OK) { 
          if (dbglevel > 0) { Serial.println(F("SPS30 ERROR: Could not start SPS30 measurements")); }
          stateSPS30 = HAS_ERROR; 
        } else {
          if (dbglevel == 5) { Serial.println(F("SPS30 Status: started")); }
          stateSPS30 = IS_BUSY;
        }
      }        
      break;
    }

    case HAS_ERROR : { //---------------------
      // What are we going to do about that? Reset
      if (dbglevel > 0) { Serial.println(F("SPS30 Status: error, going to reset")); }
      
      sps30_port->begin(sps30_i2c[0], sps30_i2c[1]);  
      
      ret = sps30.reset(); 
      if (ret != ERR_OK) { 
        if (dbglevel > 0) { Serial.println(F("SPS30 ERROR: Could not reset")); }
        stateSPS30  = HAS_ERROR;
        sps30_avail = false; 
        break;
      }
      if (dbglevel == 5) { Serial.println(F("SPS30 Status: reset successful")); }
      if (sps30.start() == true) { 
        if (dbglevel == 5) { Serial.println(F("SPS30: measurement started")); }
        stateSPS30 = IS_BUSY; 
      } else { 
        if (dbglevel > 0) { Serial.println(F("SPS30: could NOT start measurement")); }
        stateSPS30 = HAS_ERROR;
        sps30_avail = false;
      }
      break; 
    }
        
  } // end cases
}

/******************************************************************************************************/
// BME680
/******************************************************************************************************/
void updateBME680() {

  switch(stateBME680) { 
    
    case IS_IDLE : { //---------------------
      if ((currentTime - lastBME680) >= intervalBME680) {
        
        bme680_port->begin(bme680_i2c[0], bme680_i2c[1]);  
        
        tmpTime = millis();
        endTimeBME680 = bme680.beginReading();
        lastBME680 = currentTime;
        if (endTimeBME680 == 0) {
          if (dbglevel > 0) { Serial.println(F("BME680: failed to begin reading")); }
          stateBME680 = HAS_ERROR; 
        } else {
          stateBME680 = IS_BUSY; 
        }
        if (dbglevel == 9) { Serial.print(F("BME680 reading started. Completes in ")); Serial.print(endTimeBME680-tmpTime); Serial.println(F("ms.")); }
      }
      break;
    }
    
    case IS_BUSY : { //---------------------
      if (currentTime > endTimeBME680) {
        stateBME680 = DATA_AVAILABLE;
      }
      break;
    }

    case DATA_AVAILABLE : { //---------------------
      
      bme680_port->begin(bme680_i2c[0], bme680_i2c[1]);  
      
      if (bme680.endReading() ==  false) {
        if (dbglevel == 9) { Serial.println(F("BME680: Failed to complete reading")); }
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

        // NEED TO IMPLEMENT IF DEWPOINT is WANTED
        // DewPoint
        // https://en.wikipedia.org/wiki/Dew_point
        // T Celsius
        // a = 6.1121 mbar, b = 18.678, c = 257.14 °C, d = 234.5 °C.
        // Tdp = c g_m / (b - g_m)
        // g_m = ln(RH/100) + (b-T/d)*(T/(c+T)))
        
        if (bme680_pressure24hrs == 0.0) {bme680_pressure24hrs = bme680.pressure; } 
        else {bme680_pressure24hrs = (1.0-alphaBME680) * bme680_pressure24hrs + alphaBME680* bme680.pressure; }
        if (dbglevel == 9) { Serial.println(F("BME680; readout completed")); }
        bme680NewData = true;
      }
      stateBME680 = IS_IDLE;
      break;
    }
                          
    case HAS_ERROR : {
      // What are we going to do about that?
      if (dbglevel == 9) { Serial.println(F("BME680 has Error!")); }
      break; 
    }
   
  } // end cases
}

/******************************************************************************************************/
// BME280
/******************************************************************************************************/
void updateBME280() {

  switch(stateBME280) { 

    case IS_SLEEPING: { // ------------------ Slow and Energy Saving Mode: Wait until time to start measurement
      if ((currentTime - lastBME280) >= intervalBME280) {
        
        bme280_port->begin(bme280_i2c[0], bme280_i2c[1]);  
        
        bme280.setMode(MODE_FORCED); // Start reading
        lastBME280 = currentTime;
        stateBME280 = IS_BUSY;
      }
      break;
    }

    case IS_BUSY: {  // ---------------------- Slow and Energy Saving Mode: Wait until measurement complete
      if ((currentTime - lastBME280) >= bme280_measuretime) { // wait until measurement completed
        
        bme280_port->begin(bme280_i2c[0], bme280_i2c[1]);  
        
        if (bme280.isMeasuring() == true) { 
          lastBME280 = currentTime; 
          if (dbglevel == 9) { Serial.println(F("BM[E/P]280: Failed to complete reading")); }
          } // check if measurement is actually completed, if not wait some longer
        else                                { stateBME280 = DATA_AVAILABLE; } // yes its completed
      }
      break;
    }
    
    case IS_MEASURING: { // ------------------ Fast and Accurate Mode: Wait until mesaurement complete
      if ((currentTime - lastBME280) >= intervalBME280) {
        stateBME280 = DATA_AVAILABLE;
      }
      break;
    }

    case DATA_AVAILABLE : { //--------------- Data is available either from slow or fast mode
      
      bme280_port->begin(bme280_i2c[0], bme280_i2c[1]);  
      
      bme280_temp     = bme280.readTempC();
      bme280_pressure = bme280.readFloatPressure();
      if (BMEhum_avail) { 
        bme280_hum = bme280.readFloatHumidity(); 
        float tmp = 273.15 + bme280_temp;
        bme280_ah = bme280_hum * 13.246 / tmp * exp(19.854 - 5423.0/tmp); // [gr/m^3]
      } else {
        bme280_hum = -1.0;
        bme280_ah =-1.0;
      }
      if (dbglevel == 9) { Serial.println(F("BM[E/P]280; readout completed")); }
      bme280NewData = true;
      lastBME280 = currentTime;
      
      if (bme280_pressure24hrs == 0.0) {bme280_pressure24hrs = bme280_pressure;} 
      else { bme280_pressure24hrs = (1.0-alphaBME280) * bme280_pressure24hrs + alphaBME280 * bme280_pressure; } 

      if (fastMode) { stateBME280 = IS_MEASURING; }
      else {          stateBME280 = IS_SLEEPING; }
      break;
    }

    case HAS_ERROR : {
      // What are we going to do about that?
      if (dbglevel == 9) { Serial.println(F("BME280 has Error!")); }
      break; 
    }
   
  } // end cases
}


/******************************************************************************************************/
// MLX
/******************************************************************************************************/
void updateMLX() {
  
  switch(stateMLX) {
    
    case IS_MEASURING : { //---------------------
      if ((currentTime - lastMLX) > intervalMLX) {
        
        mlx_port->begin(mlx_i2c[0], mlx_i2c[1]);  
        
        if (therm.read()) {
          if (dbglevel == 8) { Serial.println(F("MLX: temperature measured")); }
          lastMLX = currentTime;
          mlxNewData = true;
          if (fastMode == false) {
            therm.sleep();
            if (dbglevel == 8) { Serial.println(F("MLX: sent to sleep")); }
            stateMLX = IS_SLEEPING;
          }
        }
      }
      break;
    }

    case IS_SLEEPING : { //---------------------
      if ((currentTime - lastMLX) > sleepTimeMLX) {
        
        mlx_port->begin(mlx_i2c[0], mlx_i2c[1]);  
        
        therm.wake(); // takes 250ms to wake up, sleepTime is shorter than intervalMLX to accomodate that
        if (dbglevel == 8) { Serial.println(F("MLX: initiated wake up")); }
        stateMLX = IS_MEASURING;
      }
      break;
    }
  } // switch
}

/******************************************************************************************************/
// MQTT
/******************************************************************************************************/
void updateMQTTMessage() {

  mqtt_sent = false;
  // --------------------- this sends new data to mqtt server as soon as its available ----------------
  if (mySettings.sendMQTTimmediate) {
    if (scd30NewData)  {
      sprintf(payloadStr, "%4dppm", int(scd30_ppm));                   strcpy(topicStr, mySettings.mqtt_mainTopic);  strcat(topicStr, "/data/scd30_CO2");              mqttClient.publish(topicStr, payloadStr); delay(1);
      sprintf(payloadStr, "%4.1f%%", scd30_hum);                       strcpy(topicStr, mySettings.mqtt_mainTopic);  strcat(topicStr, "/data/scd30_rH");               mqttClient.publish(topicStr, payloadStr); delay(1);
      sprintf(payloadStr, "%+5.1fC",scd30_temp);                       strcpy(topicStr, mySettings.mqtt_mainTopic);  strcat(topicStr, "/data/scd30_T");                mqttClient.publish(topicStr, payloadStr); delay(1);
      checkCO2(float(scd30_ppm), qualityMessage);                      strcpy(topicStr, mySettings.mqtt_mainTopic);  strcat(topicStr, "/data/scd30_CO2_airquality");   mqttClient.publish(topicStr, qualityMessage); delay(1);
      checkHumidity(scd30_hum, qualityMessage);                        strcpy(topicStr, mySettings.mqtt_mainTopic);  strcat(topicStr, "/data/scd30_rH_airquality");    mqttClient.publish(topicStr, qualityMessage); delay(1);
      scd30NewData = false;
      if (dbglevel == 3) { Serial.println("SCD30 MQTT updated.");}
      mqtt_sent = true;
    }
    if (sgp30NewData)  {
      sprintf(payloadStr, "%4dppm", sgp30.CO2);                        strcpy(topicStr, mySettings.mqtt_mainTopic);  strcat(topicStr, "/data/sgp30_eCO2");             mqttClient.publish(topicStr, payloadStr); delay(1);
      sprintf(payloadStr, "%4dppb", sgp30.TVOC);                       strcpy(topicStr, mySettings.mqtt_mainTopic);  strcat(topicStr, "/data/sgp30_tVOC");             mqttClient.publish(topicStr, payloadStr); delay(1);
      checkCO2(float(sgp30.CO2), qualityMessage);                      strcpy(topicStr, mySettings.mqtt_mainTopic);  strcat(topicStr, "/data/sgp30_eCO2_airquality");  mqttClient.publish(topicStr, qualityMessage); delay(1);
      checkTVOC(float(sgp30.TVOC), qualityMessage);                    strcpy(topicStr, mySettings.mqtt_mainTopic);  strcat(topicStr, "/data/sgp30_tVOC_airquality");  mqttClient.publish(topicStr, qualityMessage); delay(1);
      sgp30NewData = false;
      if (dbglevel == 3) { Serial.println("SGP30 MQTT updated.");}      
      mqtt_sent = true;
    }
    if (sps30NewData)  {
      sprintf(payloadStr, "%4.1fµg/m3",valSPS30.MassPM1);              strcpy(topicStr, mySettings.mqtt_mainTopic);  strcat(topicStr, "/data/sps30_PM1");              mqttClient.publish(topicStr, payloadStr); delay(1);
      sprintf(payloadStr, "%4.1fµg/m3",valSPS30.MassPM2);              strcpy(topicStr, mySettings.mqtt_mainTopic);  strcat(topicStr, "/data/sps30_PM2");              mqttClient.publish(topicStr, payloadStr); delay(1);
      sprintf(payloadStr, "%4.1fµg/m3",valSPS30.MassPM4);              strcpy(topicStr, mySettings.mqtt_mainTopic);  strcat(topicStr, "/data/sps30_PM4");              mqttClient.publish(topicStr, payloadStr); delay(1);
      sprintf(payloadStr, "%4.1fµg/m3",valSPS30.MassPM10);             strcpy(topicStr, mySettings.mqtt_mainTopic);  strcat(topicStr, "/data/sps30_PM10");             mqttClient.publish(topicStr, payloadStr); delay(1);
      sprintf(payloadStr, "%4.1f#/cm3",valSPS30.NumPM0);               strcpy(topicStr, mySettings.mqtt_mainTopic);  strcat(topicStr, "/data/sps30_nPM0");             mqttClient.publish(topicStr, payloadStr); delay(1);
      sprintf(payloadStr, "%4.1f#/cm3",valSPS30.NumPM2);               strcpy(topicStr, mySettings.mqtt_mainTopic);  strcat(topicStr, "/data/sps30_nPM2");             mqttClient.publish(topicStr, payloadStr); delay(1);
      sprintf(payloadStr, "%4.1f#/cm3",valSPS30.NumPM4);               strcpy(topicStr, mySettings.mqtt_mainTopic);  strcat(topicStr, "/data/sps30_nPM4");             mqttClient.publish(topicStr, payloadStr); delay(1);
      sprintf(payloadStr, "%4.1f#/cm3",valSPS30.NumPM10);              strcpy(topicStr, mySettings.mqtt_mainTopic);  strcat(topicStr, "/data/sps30_nPM10");            mqttClient.publish(topicStr, payloadStr); delay(1);
      sprintf(payloadStr, "%4.1fµm",valSPS30.PartSize);                strcpy(topicStr, mySettings.mqtt_mainTopic);  strcat(topicStr, "/data/sps30_PartSize");         mqttClient.publish(topicStr, payloadStr); delay(1);
      checkPM2(valSPS30.MassPM2, qualityMessage);                      strcpy(topicStr, mySettings.mqtt_mainTopic);  strcat(topicStr, "/data/sps30_PM2_airquality");   mqttClient.publish(topicStr, qualityMessage); delay(1);
      checkPM10(valSPS30.MassPM10, qualityMessage);                    strcpy(topicStr, mySettings.mqtt_mainTopic);  strcat(topicStr, "/data/sps30_PM10_airquality");  mqttClient.publish(topicStr, qualityMessage); delay(1);
      sps30NewData = false;
      if (dbglevel == 3) { Serial.println("SPS30 MQTT updated.");}      
      mqtt_sent = true;
    }
    if (ccs811NewData) {
      sprintf(payloadStr, "%4dppm", ccs811.getCO2());                  strcpy(topicStr, mySettings.mqtt_mainTopic);  strcat(topicStr, "/data/ccs811_eCO2");            mqttClient.publish(topicStr, payloadStr); delay(1);
      sprintf(payloadStr, "%4dppb", ccs811.getTVOC());                 strcpy(topicStr, mySettings.mqtt_mainTopic);  strcat(topicStr, "/data/ccs811_tVOC");            mqttClient.publish(topicStr, payloadStr); delay(1);
      checkCO2(float(ccs811.getCO2()), qualityMessage);                strcpy(topicStr, mySettings.mqtt_mainTopic);  strcat(topicStr, "/data/ccs811_eCO2_airquality"); mqttClient.publish(topicStr, qualityMessage); delay(1);
      checkTVOC(float(ccs811.getTVOC()), qualityMessage);              strcpy(topicStr, mySettings.mqtt_mainTopic);  strcat(topicStr, "/data/ccs811_tVOC_airquality"); mqttClient.publish(topicStr, qualityMessage); delay(1);
      ccs811NewData = false;
      if (dbglevel == 3) { Serial.println("CCS811 MQTT updated.");}      
      mqtt_sent = true;
    }
    if (bme680NewData) {
      sprintf(payloadStr, "%4.1fmbar",(float(bme680.pressure)/100.0)); strcpy(topicStr, mySettings.mqtt_mainTopic);  strcat(topicStr, "/data/bme680_p");               mqttClient.publish(topicStr, payloadStr); delay(1);
      sprintf(payloadStr, "%4.1f%%",bme680.humidity);                  strcpy(topicStr, mySettings.mqtt_mainTopic);  strcat(topicStr, "/data/bme680_rH");              mqttClient.publish(topicStr, payloadStr); delay(1);
      sprintf(payloadStr, "%4.1fg",bme680_ah);                         strcpy(topicStr, mySettings.mqtt_mainTopic);  strcat(topicStr, "/data/bme680_aH");              mqttClient.publish(topicStr, payloadStr); delay(1);
      sprintf(payloadStr, "%+5.1fC",bme680.temperature);               strcpy(topicStr, mySettings.mqtt_mainTopic);  strcat(topicStr, "/data/bme680_T");               mqttClient.publish(topicStr, payloadStr); delay(1);
      sprintf(payloadStr, "%dOhm",bme680.gas_resistance);              strcpy(topicStr, mySettings.mqtt_mainTopic);  strcat(topicStr, "/data/bme680_aq");              mqttClient.publish(topicStr, payloadStr); delay(1);
      checkHumidity(bme680.humidity, qualityMessage);                  strcpy(topicStr, mySettings.mqtt_mainTopic);  strcat(topicStr, "/data/bme680_rH_airquality");   mqttClient.publish(topicStr, qualityMessage); delay(1);
      checkGasResistance(bme680.gas_resistance, qualityMessage);       strcpy(topicStr, mySettings.mqtt_mainTopic);  strcat(topicStr, "/data/bme680_aq_airquality");   mqttClient.publish(topicStr, qualityMessage); delay(1);
      bme680NewData = false;
      if (dbglevel == 3) { Serial.println("BME680 MQTT updated.");}      
      mqtt_sent = true;
    }
    if (bme280NewData) {
      sprintf(payloadStr, "%4.1fmbar",(float(bme280_pressure)/100.0)); strcpy(topicStr, mySettings.mqtt_mainTopic);  strcat(topicStr, "/data/bme280_p");               mqttClient.publish(topicStr, payloadStr); delay(1);
      sprintf(payloadStr, "%4.1f%%",bme280_hum);                       strcpy(topicStr, mySettings.mqtt_mainTopic);  strcat(topicStr, "/data/bme280_rH");              mqttClient.publish(topicStr, payloadStr); delay(1);
      sprintf(payloadStr, "%4.1fg",bme280_ah);                         strcpy(topicStr, mySettings.mqtt_mainTopic);  strcat(topicStr, "/data/bme280_aH");              mqttClient.publish(topicStr, payloadStr); delay(1);
      sprintf(payloadStr, "%+5.1fC",bme280_temp);                      strcpy(topicStr, mySettings.mqtt_mainTopic);  strcat(topicStr, "/data/bme280_T");               mqttClient.publish(topicStr, payloadStr); delay(1);
      checkHumidity(bme280_hum, qualityMessage);                       strcpy(topicStr, mySettings.mqtt_mainTopic);  strcat(topicStr, "/data/bme280_rH_airquality");   mqttClient.publish(topicStr, qualityMessage); delay(1);
      bme280NewData = false;
      if (dbglevel == 3) { Serial.println("BME280 MQTT updated.");}      
      mqtt_sent = true;
    }
    if (mlxNewData) {
      sprintf(payloadStr, "%+5.1fC",(therm.object()+mlxOffset));       strcpy(topicStr, mySettings.mqtt_mainTopic);  strcat(topicStr, "/data/mlx_To");                 mqttClient.publish(topicStr, payloadStr); delay(1);
      sprintf(payloadStr, "%+5.1fC",therm.ambient());                  strcpy(topicStr, mySettings.mqtt_mainTopic);  strcat(topicStr, "/data/mlx_Ta");                 mqttClient.publish(topicStr, payloadStr); delay(1);
      checkFever((therm.object()+mlxOffset+fhDelta), qualityMessage);  strcpy(topicStr, mySettings.mqtt_mainTopic);  strcat(topicStr, "/data/mlx_fever");              mqttClient.publish(topicStr, qualityMessage); delay(1);
      mlxNewData = false;
      if (dbglevel == 3) { Serial.println("MLX MQTT updated.");}      
      mqtt_sent = true;
    }
    if (sendMQTTonce) { // send system status
      sprintf(payloadStr, "%u", intervalMLX);                          strcpy(topicStr, mySettings.mqtt_mainTopic);  strcat(topicStr, "/status/mlx_interval");         mqttClient.publish(topicStr, payloadStr);  delay(1);
      sprintf(payloadStr, "%u", intervalLCD);                          strcpy(topicStr, mySettings.mqtt_mainTopic);  strcat(topicStr, "/status/lcd_interval");         mqttClient.publish(topicStr, payloadStr);  delay(1);
      sprintf(payloadStr, "%u", ccs811Mode);                           strcpy(topicStr, mySettings.mqtt_mainTopic);  strcat(topicStr, "/status/ccs811_mode");          mqttClient.publish(topicStr, payloadStr);  delay(1);      
      sprintf(payloadStr, "%u", intervalSGP30);                        strcpy(topicStr, mySettings.mqtt_mainTopic);  strcat(topicStr, "/status/sgp30_interval");       mqttClient.publish(topicStr, payloadStr);  delay(1);
      sprintf(payloadStr, "%u", intervalSCD30);                        strcpy(topicStr, mySettings.mqtt_mainTopic);  strcat(topicStr, "/status/scd30_interval");       mqttClient.publish(topicStr, payloadStr);  delay(1);
      sprintf(payloadStr, "%u", intervalSPS30);                        strcpy(topicStr, mySettings.mqtt_mainTopic);  strcat(topicStr, "/status/sps30_interval");       mqttClient.publish(topicStr, payloadStr);  delay(1);
      sprintf(payloadStr, "%u", intervalBME680);                       strcpy(topicStr, mySettings.mqtt_mainTopic);  strcat(topicStr, "/status/bme680_interval");      mqttClient.publish(topicStr, payloadStr);  delay(1);
      sprintf(payloadStr, "%u", intervalBME280);                       strcpy(topicStr, mySettings.mqtt_mainTopic);  strcat(topicStr, "/status/bme280_interval");      mqttClient.publish(topicStr, payloadStr);  delay(1);
      sendMQTTonce = false;
    }
    if (currentTime - lastMQTTPublish > intervalMQTTSlow) { // update system status
      sprintf(payloadStr, "%u", therm_avail);                          strcpy(topicStr, mySettings.mqtt_mainTopic);  strcat(topicStr, "/status/mlx_avail");            mqttClient.publish(topicStr, payloadStr);  delay(1);    
      sprintf(payloadStr, "%u", lcd_avail);                            strcpy(topicStr, mySettings.mqtt_mainTopic);  strcat(topicStr, "/status/lcd_avail");            mqttClient.publish(topicStr, payloadStr);  delay(1);
      sprintf(payloadStr, "%u", ccs811_avail);                         strcpy(topicStr, mySettings.mqtt_mainTopic);  strcat(topicStr, "/status/ccs811_avail");         mqttClient.publish(topicStr, payloadStr);  delay(1);
      sprintf(payloadStr, "%u", sgp30_avail);                          strcpy(topicStr, mySettings.mqtt_mainTopic);  strcat(topicStr, "/status/sgp30_avail");          mqttClient.publish(topicStr, payloadStr);  delay(1);
      sprintf(payloadStr, "%u", scd30_avail);                          strcpy(topicStr, mySettings.mqtt_mainTopic);  strcat(topicStr, "/status/scd30_avail");          mqttClient.publish(topicStr, payloadStr);  delay(1);
      sprintf(payloadStr, "%u", sps30_avail);                          strcpy(topicStr, mySettings.mqtt_mainTopic);  strcat(topicStr, "/status/sps30_avail");          mqttClient.publish(topicStr, payloadStr);  delay(1);
      sprintf(payloadStr, "%u", bme680_avail);                         strcpy(topicStr, mySettings.mqtt_mainTopic);  strcat(topicStr, "/status/bme680_avail");         mqttClient.publish(topicStr, payloadStr);  delay(1);
      sprintf(payloadStr, "%u", bme280_avail);                         strcpy(topicStr, mySettings.mqtt_mainTopic);  strcat(topicStr, "/status/bme280_avail");         mqttClient.publish(topicStr, payloadStr);  delay(1);
      lastMQTTPublish = currentTime;
      mqtt_sent = true;
    }
  } else {
  // --------------- this creates single message ---------------------------------------------------------
    if (currentTime - lastMQTTPublish > intervalMQTT) { // send all sensor data every interval time
      // creating payload String
      updateMQTTpayload(payloadStr);
      strcpy(topicStr, mySettings.mqtt_mainTopic);  
      strcat(topicStr, "/data/all");
      if (dbglevel == 3) { Serial.println(payloadStr);}      
      mqttClient.publish(topicStr, payloadStr);
      lastMQTTPublish = currentTime;
      mqtt_sent = true;
    } // end interval
  } // end send immidiate
  if (mqtt_sent == true) {
    if (dbglevel > 0) { Serial.println(F("MQTT: published data"));}
  }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////
// Support functions
//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////

bool checkI2C(byte address, TwoWire *myWire) {
  myWire->beginTransmission(address);
  if (myWire->endTransmission() == 0) { return true; } else { return false; }
}

void checkCO2(float co2, char *message) {
  // Space station CO2 is about 2000ppbm
  // Global outdoor CO2 is well published
  if (co2 < 800.)         { strcpy(message, "Normal");
  } else if (co2 < 1000.) { strcpy(message, "Threshold");
  } else if (co2 < 5000.) { strcpy(message, "Poor");
  } else                  { strcpy(message, "Excessive"); }  
}

void checkHumidity(float rH, char *message) {
  // Mayo clinic 30-50% is normal
  // Potential lower virus transmission at lower humidity
  // Humidity > 60% viruses thrive
  // Below 40% is considered dry
  // 15-25% humidity affect tear film on eye
  if ((rH >= 45.) && (rH <= 55.))       { strcpy(message, "Normal");
  } else if ((rH >= 30.) && (rH < 45.)) { strcpy(message, "Threshold Low");
  } else if ((rH >= 55.) && (rH < 60.)) { strcpy(message, "Threshold High");
  } else if ((rH >= 60.) && (rH < 80.)) { strcpy(message, "High");
  } else if ((rH >  15.) && (rH < 30.)) { strcpy(message, "Low");
  } else                                { strcpy(message, "Excessive"); }
}

void checkGasResistance(uint32_t res, char *message) {
  // The airquality index caculation of the Bosch sensor is proprietary and only available as precompiled library.
  // Direct assessment of resistance and association to airquality is not well documented.
  if (res < 5000)          { strcpy(message, "Normal");
  } else if (res < 10000)  { strcpy(message, "Threshold");
  } else if (res < 300000) { strcpy(message, "Poor");
  } else                   { strcpy(message, "Excessive"); }
}

void checkTVOC(float tVOC, char *message) {
  // usually measured in parts per billion
  if (tVOC < 220.)         { strcpy(message, "Normal");
  } else if (tVOC < 660.)  { strcpy(message, "Threshold");
  } else if (tVOC < 2200.) { strcpy(message, "Poor");
  } else                   { strcpy(message, "Excessive"); }  
}

void checkPM2(float PM2, char *message) {
  // some references would be useful here
  // I did not implement daily averages
  if (PM2 < 10.0)        { strcpy(message, "Normal");
  } else if (PM2 < 25.0) { strcpy(message, "Threshold");
  } else if (PM2 < 65.0) { strcpy(message, "Poor");
  } else                 { strcpy(message, "Excessive"); }
}

void checkPM10(float PM10, char *message) {
  // some references would be useful here
  // I did not implement daily averages
  if (PM10 < 20.0)         { strcpy(message, "Normal");
  } else if (PM10 < 50.0)  { strcpy(message, "Threshold");
  } else if (PM10 < 150.0) { strcpy(message, "Poor");
  } else                   { strcpy(message, "Excessive"); }
}

void checkFever(float T, char *message) {
  // While there are many resources on the internet to reference fever
  // its more difficult to associate forehead temperature with fever
  // That is because forhead sensors are not very reliable and
  // The conversion from temperature measured on different places of the body
  // to body core temperature is not well established.
  // It can be assumed that measurement of ear drum is likely closest to brain tempreature which is best circulated organ.  
  //
  if (T < 35.0)         { strcpy(message, "Low");
  } else if (T <= 36.4) { strcpy(message, "Threhsold Low");
  } else if (T <  37.2) { strcpy(message, "Normal");
  } else if (T <  38.3) { strcpy(message, "Threshold High");
  } else if (T <  41.5) { strcpy(message, "Fever");
  } else                { strcpy(message, "Fever High");}  
}

void mqttCallback(char* topic, byte* payload, unsigned int len) {
  if (dbglevel > 0) { 
    Serial.println("MQTT: Publish received.");
    Serial.print("MQTT: topic: ");
    Serial.println(topic);
    Serial.print("MQTT: payload: ");
    Serial.print("] ");
    for (int i = 0; i < len; i++) {
      Serial.print((char)payload[i]);
    }
    Serial.println();
  }
}

void updateMQTTpayload(char *payload) {
  /**************************************************************************************/
  // Update MQTT payload
  /**************************************************************************************/
  char tmpbuf[21];
  strcpy(payload, "{");
  if (scd30_avail && mySettings.useSCD30) { // ==CO2===========================================================
    strcat(payload,"\"scd30_CO2\":");      sprintf(tmpbuf, "%4dppm", int(scd30_ppm));              strcat(payload,tmpbuf); strcat(payload,", "); 
    strcat(payload,"\"scd30_rH\":");       sprintf(tmpbuf, "%4.1f%%", scd30_hum);                  strcat(payload,tmpbuf); strcat(payload,", ");
    strcat(payload,"\"scd30_T\":");        sprintf(tmpbuf, "%+5.1fC",scd30_temp);                  strcat(payload,tmpbuf); strcat(payload,", ");
  }  // end if avail scd30
  if (bme680_avail && mySettings.useBME680) { // ===rH,T,aQ=================================================
    strcat(payload,"\"bme680_p\":");       sprintf(tmpbuf, "%4dbar",(int)(bme680.pressure/100.0)); strcat(payload,tmpbuf); strcat(payload,", "); 
    strcat(payload,"\"bme680_rH\":");      sprintf(tmpbuf, "%4.1f%%",bme680.humidity);             strcat(payload,tmpbuf); strcat(payload,", ");
    strcat(payload,"\"bme680_aH\":");      sprintf(tmpbuf, "%4.1fg",bme680_ah);                    strcat(payload,tmpbuf); strcat(payload,", ");
    strcat(payload,"\"bme680_T\":");       sprintf(tmpbuf, "%+5.1fC",bme680.temperature);          strcat(payload,tmpbuf); strcat(payload,", ");
    strcat(payload,"\"bme680_aq\":");      sprintf(tmpbuf, "%dOhm",bme680.gas_resistance);         strcat(payload,tmpbuf); strcat(payload,", ");
  } // end if avail bme680
  if (bme280_avail && mySettings.useBME280) { // ===rH,T,aQ=================================================
    strcat(payload,"\"bme280_p\":");       sprintf(tmpbuf, "%4dbar",(int)(bme280_pressure/100.0)); strcat(payload,tmpbuf); strcat(payload,", "); 
    strcat(payload,"\"bme280_rH\":");      sprintf(tmpbuf, "%4.1f%%",bme280_hum);                  strcat(payload,tmpbuf); strcat(payload,", ");
    strcat(payload,"\"bme280_aH\":");      sprintf(tmpbuf, "%4.1fg",bme280_ah);                    strcat(payload,tmpbuf); strcat(payload,", ");
    strcat(payload,"\"bme280_T\":");       sprintf(tmpbuf, "%+5.1fC",bme280_temp);                 strcat(payload,tmpbuf); strcat(payload,", ");
  } // end if avail bme680
  if (sgp30_avail && mySettings.useSGP30) { // ===CO2,tVOC=================================================
    strcat(payload,"\"sgp30_CO2\":");      sprintf(tmpbuf, "%4dppm", sgp30.CO2);                   strcat(payload,tmpbuf); strcat(payload,", ");
    strcat(payload,"\"sgp30_tVOC\":");     sprintf(tmpbuf, "%4dppb", sgp30.TVOC);                  strcat(payload,tmpbuf); strcat(payload,", ");
  } // end if avail sgp30
  if (ccs811_avail && mySettings.useCCS811) { // ===CO2,tVOC=================================================
    strcat(payload,"\"ccs811_CO2\":");     sprintf(tmpbuf, "%4dppm", ccs811.getCO2());             strcat(payload,tmpbuf); strcat(payload,", ");
    strcat(payload,"\"ccs811_tVOC\":");    sprintf(tmpbuf, "%4dppb", ccs811.getTVOC());            strcat(payload,tmpbuf); strcat(payload,", ");
  } // end if avail ccs811
  if (sps30_avail && mySettings.useSPS30) { // ===Particle=================================================
    strcat(payload,"\"sps30_PM1\":");      sprintf(tmpbuf, "%3.0fµg/m3",valSPS30.MassPM1);         strcat(payload,tmpbuf); strcat(payload,", ");
    strcat(payload,"\"sps30_PM2\":");      sprintf(tmpbuf, "%3.0fµg/m3",valSPS30.MassPM2);         strcat(payload,tmpbuf); strcat(payload,", ");
    strcat(payload,"\"sps30_PM4\":");      sprintf(tmpbuf, "%3.0fµg/m3",valSPS30.MassPM4);         strcat(payload,tmpbuf); strcat(payload,", ");
    strcat(payload,"\"sps30_nPM10\":");    sprintf(tmpbuf, "%3.0fµg/m3",valSPS30.MassPM10);        strcat(payload,tmpbuf); strcat(payload,", ");
    strcat(payload,"\"sps30_nPM0\":");     sprintf(tmpbuf, "%3.0f#/m3",valSPS30.NumPM0);           strcat(payload,tmpbuf); strcat(payload,", ");
    strcat(payload,"\"sps30_nPM2\":");     sprintf(tmpbuf, "%3.0f#/m3",valSPS30.NumPM2);           strcat(payload,tmpbuf); strcat(payload,", ");
    strcat(payload,"\"sps30_nPM4\":");     sprintf(tmpbuf, "%3.0f#/m3",valSPS30.NumPM4);           strcat(payload,tmpbuf); strcat(payload,", ");
    strcat(payload,"\"sps30_nPM10\":");    sprintf(tmpbuf, "%3.0f#/m3",valSPS30.NumPM10);          strcat(payload,tmpbuf); strcat(payload,", ");
    strcat(payload,"\"sps30_PartSize\":"); sprintf(tmpbuf, "%3.0fµm",valSPS30.PartSize);           strcat(payload,tmpbuf); strcat(payload,", ");
  }// end if avail SPS30
  if (therm_avail && mySettings.useMLX) { // ====To,Ta================================================
    strcat(payload,"\"MLX_To\":");         sprintf(tmpbuf, "%+5.1fC",(therm.object()+mlxOffset));  strcat(payload,tmpbuf); strcat(payload,", ");
    strcat(payload,"\"MLX_Ta\":");         sprintf(tmpbuf, "%+5.1fC",therm.ambient());             strcat(payload,tmpbuf); strcat(payload,", ");
  }// end if avail  MLX
  strcat(payload, "}");
} // update MQTT


void updateLCD() {
  /**************************************************************************************/
  // Update LCD
  /**************************************************************************************/
  // This code prints one whole LCD screen line at a time.
  // It appears frequent lcd.setcursor() commands corrupt the display.
  // A line is 20 characters long and terminated at the 21st character with null
  // The first line is continuted at the 3rd line in the LCD driver and the 2nd line is continued at the 4th line.
  // Display update takes 115ms
  char lcdbuf[21];
  const char clearLine[] = "                    ";  // 20 spaces
  
  strncpy(&lcdDisplay[0][0], clearLine , 20);
  strncpy(&lcdDisplay[1][0], clearLine , 20);
  strncpy(&lcdDisplay[2][0], clearLine , 20);
  strncpy(&lcdDisplay[3][0], clearLine , 20);

  
  if (scd30_avail && mySettings.useSCD30) { // =============================================================

    sprintf(lcdbuf, "%4d", int(scd30_ppm));
    strncpy(&lcdDisplay[CO2_Y][CO2_X], lcdbuf, 4);
    
    sprintf(lcdbuf, "%4.1f%%", scd30_hum);
    strncpy(&lcdDisplay[HUM1_Y][HUM1_X], lcdbuf, 5);
    
    sprintf(lcdbuf,"%+5.1fC",scd30_temp);
    strncpy(&lcdDisplay[TEMP1_Y][TEMP1_X], lcdbuf, 6);

    if (scd30_ppm < 800)         { lcdbuf[0] = 'N';
    } else if (scd30_ppm < 1000) { lcdbuf[0] = 'T';
    } else if (scd30_ppm < 5000) { lcdbuf[0] = 'P';
    } else                       { lcdbuf[0] = '!'; }
    lcdDisplay[CO2_WARNING_Y][CO2_WARNING_X] = lcdbuf[0];

  }  // end if avail scd30
  
  if (bme680_avail && mySettings.useBME680) { // ====================================================
    
    sprintf(lcdbuf,"%4d",(int)(bme680.pressure/100.0));
    strncpy(&lcdDisplay[PRESSURE_Y][PRESSURE_X], lcdbuf, 4);
  
    sprintf(lcdbuf,"%4.1f%%",bme680.humidity);
    strncpy(&lcdDisplay[HUM2_Y][HUM2_X], lcdbuf, 5);
  
    sprintf(lcdbuf,"%4.1fg",bme680_ah);
    strncpy(&lcdDisplay[HUM3_Y][HUM3_X], lcdbuf, 5);

    if ((bme680.humidity >= 45) && (bme680.humidity <= 55))       { lcdbuf[0] = 'N';
    } else if ((bme680.humidity >= 30) && (bme680.humidity < 45)) { lcdbuf[0] = 'T';
    } else if ((bme680.humidity >= 55) && (bme680.humidity < 60)) { lcdbuf[0] = 'T';
    } else if ((bme680.humidity >= 60) && (bme680.humidity < 80)) { lcdbuf[0] = 'H';
    } else if ((bme680.humidity >  15) && (bme680.humidity < 30)) { lcdbuf[0] = 'L';
    } else                                                        { lcdbuf[0] = '!'; }
      
    sprintf(lcdbuf,"%+5.1fC",bme680.temperature);
    strncpy(&lcdDisplay[TEMP2_Y][TEMP2_X], lcdbuf, 6);
  
    sprintf(lcdbuf,"%5.1f",(float(bme680.gas_resistance)/1000.0));
    strncpy(&lcdDisplay[IAQ_Y][IAQ_X], lcdbuf, 5);
  
    if (bme680.gas_resistance < 5000)          { lcdbuf[0] = 'N';
    } else if (bme680.gas_resistance < 10000)  { lcdbuf[0] = 'T';
    } else if (bme680.gas_resistance < 300000) { lcdbuf[0] = 'P';
    } else                                     { lcdbuf[0] = '!'; }
    lcdDisplay[IAQ_WARNING_Y][IAQ_WARNING_X] = lcdbuf[0];
    
  } // end if avail bme680
  
  if (sgp30_avail && mySettings.useSGP30) { // ====================================================
    sprintf(lcdbuf, "%4d", sgp30.CO2);
    strncpy(&lcdDisplay[eCO2_Y][eCO2_X], lcdbuf, 4);

    sprintf(lcdbuf, "%4d", sgp30.TVOC);
    strncpy(&lcdDisplay[TVOC_Y][TVOC_X], lcdbuf, 4);

    if (sgp30.CO2 < 800)         { lcdbuf[0] = 'N';
    } else if (sgp30.CO2 < 1000) { lcdbuf[0] = 'T';
    } else if (sgp30.CO2 < 5000) { lcdbuf[0] = 'P';
    } else                       { lcdbuf[0] = '!'; }
    lcdDisplay[eCO2_WARNING_Y][eCO2_WARNING_X] = lcdbuf[0];

    if (sgp30.TVOC < 220)         { lcdbuf[0] = 'N';
    } else if (sgp30.TVOC < 660)  { lcdbuf[0] = 'T';
    } else if (sgp30.TVOC < 2200) { lcdbuf[0] = 'P';
    } else                        { lcdbuf[0] = '!'; }
    lcdDisplay[TVOC_WARNING_Y][TVOC_WARNING_X] = lcdbuf[0];
  } // end if avail sgp30

  if (ccs811_avail && mySettings.useCCS811) { // ====================================================

    sprintf(lcdbuf, "%4d", ccs811.getCO2());
    strncpy(&lcdDisplay[eeCO2_Y][eeCO2_X], lcdbuf, 4);

    sprintf(lcdbuf, "%4d", ccs811.getTVOC());
    strncpy(&lcdDisplay[TTVOC_Y][TTVOC_X], lcdbuf, 4);

    if (ccs811.getCO2() < 800)         { lcdbuf[0] = 'N';
    } else if (ccs811.getCO2() < 1000) { lcdbuf[0] = 'T';
    } else if (ccs811.getCO2() < 5000) { lcdbuf[0] = 'P';
    } else                             { lcdbuf[0] = '!';}
    strncpy(&lcdDisplay[eeCO2_WARNING_Y][eeCO2_WARNING_X], lcdbuf, 1);

    if (ccs811.getTVOC() < 220)         { lcdbuf[0] = 'N';
    } else if (ccs811.getTVOC() < 660)  { lcdbuf[0] = 'T';
    } else if (ccs811.getTVOC() < 2200) { lcdbuf[0] = 'P';
    } else                              { lcdbuf[0] = '!'; }
    lcdDisplay[TTVOC_WARNING_Y][TTVOC_WARNING_X] = lcdbuf[0];
  } // end if avail ccs811
  
  if (sps30_avail && mySettings.useSPS30) { // ====================================================
    sprintf(lcdbuf,"%3.0f",valSPS30.MassPM1);
    strncpy(&lcdDisplay[PM1_Y][PM1_X], lcdbuf, 3);
    
    sprintf(lcdbuf,"%3.0f",valSPS30.MassPM2);
    strncpy(&lcdDisplay[PM2_Y][PM2_X], lcdbuf, 3);
    
    sprintf(lcdbuf,"%3.0f",valSPS30.MassPM4);
    strncpy(&lcdDisplay[PM4_Y][PM4_X], lcdbuf, 3);
    
    sprintf(lcdbuf,"%3.0f",valSPS30.MassPM10);
    strncpy(&lcdDisplay[PM10_Y][PM10_X], lcdbuf, 3);

    if (valSPS30.MassPM2 < 10.0)        { lcdbuf[0] = 'N';
    } else if (valSPS30.MassPM2 < 25.0) { lcdbuf[0] = 'T';
    } else if (valSPS30.MassPM2 < 65.0) { lcdbuf[0] = 'P';
    } else                              { lcdbuf[0] = '!'; }
    lcdDisplay[PM2_WARNING_Y][PM2_WARNING_X] = lcdbuf[0];

    if (valSPS30.MassPM10 < 20.0)         { lcdbuf[0] = 'N';
    } else if (valSPS30.MassPM10 < 50.0)  { lcdbuf[0] = 'T';
    } else if (valSPS30.MassPM10 < 150.0) { lcdbuf[0] = 'P';
    } else                                { lcdbuf[0] = '!'; }
   lcdDisplay[PM10_WARNING_Y][PM10_WARNING_X] = lcdbuf[0];
  }// end if avail SPS30

  if (therm_avail && mySettings.useMLX) { // ====================================================
    if (altDisplay == true) {
      sprintf(lcdbuf,"%+5.1fC",(therm.object()+mlxOffset));
      strncpy(&lcdDisplay[TEMP1_Y][TEMP1_X], lcdbuf, 6);

      sprintf(lcdbuf,"%+5.1fC",therm.ambient());
      strncpy(&lcdDisplay[TEMP2_Y][TEMP2_X], lcdbuf, 6);

      // https://www.singlecare.com/blog/fever-temperature/
      // https://www.hopkinsmedicine.org/health/conditions-and-diseases/fever
      // https://www.ncbi.nlm.nih.gov/pmc/articles/PMC7115295/
      if (therm.object() <         (35.0 - fhDelta - mlxOffset)) { lcdbuf[0] = 'L';
      } else if (therm.object() <= (36.4 - fhDelta - mlxOffset)) { lcdbuf[0] = ' ';
      } else if (therm.object() <  (37.2 - fhDelta - mlxOffset)) { lcdbuf[0] = 'N';
      } else if (therm.object() <  (38.3 - fhDelta - mlxOffset)) { lcdbuf[0] = 'T';
      } else if (therm.object() <  (41.5 - fhDelta - mlxOffset)) { lcdbuf[0] = 'F';
      } else                                                     { lcdbuf[0] = '!'; }
      lcdDisplay[MLX_WARNING_Y][MLX_WARNING_X] = lcdbuf[0];
    }
  }// end if avail  MLX

  altDisplay = !altDisplay;
  
  lcd_port->begin(lcd_i2c[0], lcd_i2c[1]);
  lcd.clear();
  lcd.setCursor(0, 0); 
  
  // 1st line continues at 3d line
  // 2nd line continues at 4th line
  strncpy(lcdbuf, &lcdDisplay[0][0], 20); lcdbuf[20] = '\0';  lcd.print(lcdbuf);
  strncpy(lcdbuf, &lcdDisplay[2][0], 20); lcdbuf[20] = '\0';  lcd.print(lcdbuf); 
  strncpy(lcdbuf, &lcdDisplay[1][0], 20); lcdbuf[20] = '\0';  lcd.print(lcdbuf); 
  strncpy(lcdbuf, &lcdDisplay[3][0], 20); lcdbuf[20] = '\0';  lcd.print(lcdbuf); 

  if (dbglevel == 2) {                                                          // if dbg, display the lines also on serial port
    strncpy(lcdbuf, &lcdDisplay[0][0], 20); lcdbuf[20] = '\0'; Serial.println(lcdbuf);
    strncpy(lcdbuf, &lcdDisplay[1][0], 20); lcdbuf[20] = '\0'; Serial.println(lcdbuf);
    strncpy(lcdbuf, &lcdDisplay[2][0], 20); lcdbuf[20] = '\0'; Serial.println(lcdbuf);
    strncpy(lcdbuf, &lcdDisplay[3][0], 20); lcdbuf[20] = '\0'; Serial.println(lcdbuf);
  }
} // update display 

void updateConsumerLCD() {
  /**************************************************************************************/
  // Update Consumer LCD
  /**************************************************************************************/
  char lcdbuf[21];
  const char  clearLine[] = "                    ";  // 20 spaces
  
  const char   firstLine[] = "P01     rH   TEMP   ";
  const char  secondLine[] = "P25     aH          ";
  const char   thirdLine[] = "P04      P   tVOC   ";
  const char  thirdLineN[] = "P04      P          ";
  const char   forthLine[] = "P10     dP    CO2   ";
  
  if (altDisplay) { // SHOW Measurands and Assessment
    
    strncpy(&lcdDisplayAlt[0][0],  firstLine , 20);
    strncpy(&lcdDisplayAlt[1][0], secondLine , 20);
    if (sgp30_avail && mySettings.useSGP30) { strncpy(&lcdDisplayAlt[2][0],   thirdLine , 20);}
    else                                    { strncpy(&lcdDisplayAlt[2][0],  thirdLineN , 20);}
    strncpy(&lcdDisplayAlt[3][0],  forthLine , 20);

    allGood = true;

    if (scd30_avail && mySettings.useSCD30) { // =============================================================
            
      if (scd30_ppm < 800) {          lcdbuf[0] = 'N';
      } else if (scd30_ppm < 1000) {  lcdbuf[0] = 'T';
      } else if (scd30_ppm < 5000) {  lcdbuf[0] = 'P'; allGood = false;
      } else {                        lcdbuf[0] = '!'; allGood = false;}
      lcdDisplayAlt[3][18] = lcdbuf[0];
      if (scd30_temp > 25.5) {        lcdbuf[0] = 'H';
      } else if (scd30_temp < 20.0) { lcdbuf[0] = 'C';
      } else {                        lcdbuf[0] = 'N'; }
      lcdDisplayAlt[0][18] = lcdbuf[0];
      if ((scd30_hum >= 45) && (scd30_hum <= 55)) {       lcdbuf[0] = 'N';
      } else if ((scd30_hum >= 30) && (scd30_hum < 45)) { lcdbuf[0] = 'T';
      } else if ((scd30_hum >= 55) && (scd30_hum < 60)) { lcdbuf[0] = 'T';
      } else if ((scd30_hum >= 60) && (scd30_hum < 80)) { lcdbuf[0] = 'H'; allGood = false;
      } else if ((scd30_hum >  15) && (scd30_hum < 30)) { lcdbuf[0] = 'L'; allGood = false;
      } else {                                            lcdbuf[0] = '!'; allGood = false;}
      lcdDisplayAlt[0][11] = lcdbuf[0];
    }

    if (bme680_avail && mySettings.useBME680) { // ====================================================
      if ( abs(bme680_pressure24hrs - bme680.pressure)/100.0 >= 5.0 ) { lcdbuf[0] = '!'; allGood = false;
      } else                                                          { lcdbuf[0] = 'N'; }
      lcdDisplayAlt[3][11] = lcdbuf[0];
    }

    if (bme280_avail && mySettings.useBME280) { // ====================================================
      if ( abs(bme280_pressure24hrs - bme280_pressure)/100.0 >= 5.0 ) { lcdbuf[0] = '!'; allGood = false;
      } else                                                          { lcdbuf[0] = 'N'; }
      lcdDisplayAlt[3][11] = lcdbuf[0];
    }

    if (sps30_avail && mySettings.useSPS30) { // ======================================================
      if (valSPS30.MassPM2 < 10.0)        { lcdbuf[0] = 'N';
      } else if (valSPS30.MassPM2 < 25.0) { lcdbuf[0] = 'T';
      } else if (valSPS30.MassPM2 < 65.0) { lcdbuf[0] = 'P'; allGood = false;
      } else                              { lcdbuf[0] = '!'; allGood = false;}
      lcdDisplayAlt[1][4] = lcdbuf[0];
  
      if (valSPS30.MassPM10 < 20.0)         { lcdbuf[0] = 'N';
      } else if (valSPS30.MassPM10 < 50.0)  { lcdbuf[0] = 'T';
      } else if (valSPS30.MassPM10 < 150.0) { lcdbuf[0] = 'P'; allGood = false;
      } else                                { lcdbuf[0] = '!'; allGood = false;}
      lcdDisplayAlt[3][4] = lcdbuf[0];
    }

    if (sgp30_avail && mySettings.useSGP30) { // =======================================================
      if (sgp30.TVOC < 220) {         lcdbuf[0] = 'N';
      } else if (sgp30.TVOC < 660) {  lcdbuf[0] = 'T';
      } else if (sgp30.TVOC < 2200) { lcdbuf[0] = 'P'; allGood = false;
      } else {                        lcdbuf[0] = '!'; allGood = false;}
      lcdDisplayAlt[2][18] = lcdbuf[0];
    }
    
  } else { // altDisplay SHOW DATA
    
    strncpy(&lcdDisplay[0][0],     clearLine , 20);
    strncpy(&lcdDisplay[1][0],     clearLine , 20);
    strncpy(&lcdDisplay[2][0],     clearLine , 20);
    strncpy(&lcdDisplay[3][0],     clearLine , 20);

    if (scd30_avail && mySettings.useSCD30) { // =======================================================
      sprintf(lcdbuf, "%4dppm", int(scd30_ppm));
      strncpy(&lcdDisplay[3][13], lcdbuf, 7);
      sprintf(lcdbuf, "%3d%%", (int)scd30_hum);
      strncpy(&lcdDisplay[0][7], lcdbuf, 4);
      sprintf(lcdbuf,"%+5.1fC", scd30_temp);
      strncpy(&lcdDisplay[0][12], lcdbuf, 6);
      float tmp = 273.15 + scd30_temp;
      scd30_ah = scd30_hum * 13.246 / tmp * exp(19.854 - 5423.0/tmp); // [gr/m^3]
      sprintf(lcdbuf, "%4.1fg/m3", scd30_ah);
      strncpy(&lcdDisplay[1][6], lcdbuf, 8);
    }

    if (bme680_avail && mySettings.useBME680) { // ====================================================
      sprintf(lcdbuf,"%4dmb",(int)(bme680.pressure/100.0));
      strncpy(&lcdDisplay[2][6], lcdbuf, 6);
      sprintf(lcdbuf,"%2dmb",(int)(abs((bme680_pressure24hrs - bme680.pressure)/100.0)));
      strncpy(&lcdDisplay[3][8], lcdbuf, 4);
    }

    if (bme280_avail && mySettings.useBME280) { // ====================================================
      sprintf(lcdbuf,"%4dmb",(int)(bme280_pressure/100.0));
      strncpy(&lcdDisplay[2][6], lcdbuf, 6);
      sprintf(lcdbuf,"%+4.1fmb",((bme280_pressure24hrs - bme280_pressure)/100.0));
      strncpy(&lcdDisplay[3][6], lcdbuf, 6);
    }

    if (sps30_avail && mySettings.useSPS30) { // ====================================================
      sprintf(lcdbuf,"%3.0fug",valSPS30.MassPM1);
      strncpy(&lcdDisplay[0][0], lcdbuf, 5);
      sprintf(lcdbuf,"%3.0fug",valSPS30.MassPM2);
      strncpy(&lcdDisplay[1][0], lcdbuf, 5);
      sprintf(lcdbuf,"%3.0fug",valSPS30.MassPM4);
      strncpy(&lcdDisplay[2][0], lcdbuf, 5);
      sprintf(lcdbuf,"%3.0fug",valSPS30.MassPM10);
      strncpy(&lcdDisplay[3][0], lcdbuf, 5);
    }

    if (sgp30_avail && mySettings.useSGP30) { // ====================================================
      sprintf(lcdbuf, "%4dpbb", sgp30.TVOC);
      strncpy(&lcdDisplay[2][13], lcdbuf, 7);  
    }
  } // end alt display

  lcd_port->begin(lcd_i2c[0], lcd_i2c[1]);
  lcd.clear();
  lcd.setCursor(0, 0); 
  
  if (altDisplay) {
    // 1st line continues at 3d line
    // 2nd line continues at 4th line
    strncpy(lcdbuf, &lcdDisplayAlt[0][0], 20); lcdbuf[20] = '\0'; lcd.print(lcdbuf);
    strncpy(lcdbuf, &lcdDisplayAlt[2][0], 20); lcdbuf[20] = '\0'; lcd.print(lcdbuf); 
    strncpy(lcdbuf, &lcdDisplayAlt[1][0], 20); lcdbuf[20] = '\0'; lcd.print(lcdbuf); 
    strncpy(lcdbuf, &lcdDisplayAlt[3][0], 20); lcdbuf[20] = '\0'; lcd.print(lcdbuf); 
  } else {
    strncpy(lcdbuf, &lcdDisplay[0][0], 20);    lcdbuf[20] = '\0'; lcd.print(lcdbuf);
    strncpy(lcdbuf, &lcdDisplay[2][0], 20);    lcdbuf[20] = '\0'; lcd.print(lcdbuf); 
    strncpy(lcdbuf, &lcdDisplay[1][0], 20);    lcdbuf[20] = '\0'; lcd.print(lcdbuf); 
    strncpy(lcdbuf, &lcdDisplay[3][0], 20);    lcdbuf[20] = '\0'; lcd.print(lcdbuf); 
  }

  if (dbglevel == 2) { // if dbg, display the lines also on serial port
    if (altDisplay) {
      strncpy(lcdbuf, &lcdDisplayAlt[0][0], 20); lcdbuf[20] = '\0'; Serial.println(lcdbuf);
      strncpy(lcdbuf, &lcdDisplayAlt[1][0], 20); lcdbuf[20] = '\0'; Serial.println(lcdbuf);
      strncpy(lcdbuf, &lcdDisplayAlt[2][0], 20); lcdbuf[20] = '\0'; Serial.println(lcdbuf);
      strncpy(lcdbuf, &lcdDisplayAlt[3][0], 20); lcdbuf[20] = '\0'; Serial.println(lcdbuf);
      
    } else {
      strncpy(lcdbuf, &lcdDisplay[0][0], 20);    lcdbuf[20] = '\0'; Serial.println(lcdbuf);
      strncpy(lcdbuf, &lcdDisplay[1][0], 20);    lcdbuf[20] = '\0'; Serial.println(lcdbuf);
      strncpy(lcdbuf, &lcdDisplay[2][0], 20);    lcdbuf[20] = '\0'; Serial.println(lcdbuf);
      strncpy(lcdbuf, &lcdDisplay[3][0], 20);    lcdbuf[20] = '\0'; Serial.println(lcdbuf);
    }
  }
  
  altDisplay = !altDisplay; // flip display between analysis and values

} // update consumer display 

/**************************************************************************************/
// Serial Input: Support Routines
/**************************************************************************************/

void helpMenu() {
  Serial.println(F("================================================================================="));
  Serial.println(F(".........................................................................LCD 20x4"));
  Serial.println(F(".........................................................SPS30 Senserion Particle"));
  Serial.println(F("..............................................................SCD30 Senserion CO2"));
  Serial.println(F(".......................................................SGP30 Senserion tVOC, eCO2"));
  Serial.println(F("...........................BME680/280 Bosch Temperature, Humidity, Pressure, tVOC"));
  Serial.println(F("....................................................CCS811 eCO2 tVOC, Air Quality"));
  Serial.println(F("...........................................MLX90614 Melex Temperature Contactless"));
  Serial.println(F("==All Sensors===========================|========================================"));
  Serial.println(F("| z: print all sensor data              | n: This device Name, nSensi           |"));
  Serial.println(F("==SGP30=eCO2============================|==EEPROM================================"));
  Serial.println(F("| e: force eCO2, e400                   | s: save to EEPROM                     |"));
  Serial.println(F("| v: force tVOC, t3000                  | r: read from EEPROM                   |"));
  Serial.println(F("| g: get baselines                      | p: print current settings             |"));
  Serial.println(F("|                                       | d: create default settings            |"));
  Serial.println(F("==SCD30=CO2=============================|==CCS811=eCO2==========================="));
  Serial.println(F("| f: force CO2, f400.0 in ppm           | c: force basline, c400                |"));
  Serial.println(F("| t: force temperature offset, t5.0 in C| b: get baseline                       |"));
  Serial.println(F("==MLX Temp==============================|=LCD===================================="));
  Serial.println(F("| m: set temp offset, m1.4              | i: simplified display                 |"));
  Serial.println(F("==Network===============================|==MQTT=================================="));
  Serial.println(F("| 1: SSID 1, 1myssid                    | u: mqtt username, umqtt or empty      |"));
  Serial.println(F("| 2: SSID 2, 2myssid                    | w: mqtt password, ww1ldc8ts or empty  |")); 
  Serial.println(F("| 3: SSID 3, 3myssid                    | q: send mqtt immediatly, q            |")); 
  Serial.println(F("| 4: password SSID 1, 4mypas or empty   |                                       |")); 
  Serial.println(F("| 5: password SSID 2, 5mypas or empty   | 8: mqtt server, 8my,mqtt.com          |"));
  Serial.println(F("| 6: password SSID 3, 6mypas or empty   | 9: mqtt fall back server              |")); 
  Serial.println(F("==Disable===============================|==Disable==============================="));
  Serial.println(F("| x: 99 reset microcontroller           | x: 7 MAX30 on/off                     |"));
  Serial.println(F("| x: 2 LCD on/off                       | x: 8 MLX on/off                       |"));
  Serial.println(F("| x: 3 WiFi on/off                      | x: 9 BME680 on/off                    |"));
  Serial.println(F("| x: 4 SCD30 on/off                     | x: 10 BME280 on/off                   |"));
  Serial.println(F("| x: 5 SPS30 on/off                     | x: 11 CCS811 on/off                   |"));
  Serial.println(F("| x: x: 6 SGP30 on/off                  |                                       |"));
  Serial.println(F("| This does not yet intialize the sensors, you will need to power on/off or x99 |"));
  Serial.println(F("==Debug Level===========================|==Debug Level==========================="));
  Serial.println(F("| l: 0 ALL off                          |                                       |"));
  Serial.println(F("| l: 1 ALL minimal                      | l: 6 SGP30 max level                  |"));
  Serial.println(F("| l: 2 LCD max level                    | l: 7 MAX30 max level                  |"));
  Serial.println(F("| l: 3 WiFi max level                   | l: 8 MLX max level                    |"));
  Serial.println(F("| l: 4 SCD30 max level                  | l: 9 BME680/280 max level             |"));
  Serial.println(F("| l: 5 SPS30 max level                  | l: 10 CCS811 max level                |"));
  Serial.println(F("==================================Urs Utzinger==================================="));
  Serial.println(F("Dont forget to save setting to EEPROM                                            "));
  Serial.println(F("=====================================2020========================================"));
}

void inputHandle() {
  char inBuff[64];
  uint16_t tmpI;
  float tmpF;
  int bytesread;
  String value;
  String command;
  
  if (Serial.available()) {
    Serial.setTimeout(1 ); // Serial read timeout
    bytesread=Serial.readBytesUntil('\n', inBuff, 33);               // Read from serial until CR is read or timeout exceeded
    inBuff[bytesread]='\0';
    String instruction = String(inBuff);
    command = instruction.substring(0,1); 
    if (bytesread > 1) { // we have also a value
      value = instruction.substring(1,bytesread); 
      // Serial.println(value);
    }
    
    if (command == "l") {                                            // set debug level
      tmpI = value.toInt();
      if ((tmpI >= 0) && (tmpI <= 10)) {
        dbglevel = (unsigned long)tmpI;
        mySettings.debuglevel = dbglevel;
        Serial.print(F("Debug level set to:  "));
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
        if (sgp30_avail && mySettings.useSGP30) {

          sgp30_port->begin(sgp30_i2c[0], sgp30_i2c[1]);
          
          sgp30.getBaseline();
          mySettings.baselinetVOC_SGP30 = sgp30.baselineTVOC;
          sgp30.setBaseline((uint16_t)tmpI, (uint16_t)mySettings.baselinetVOC_SGP30);
          mySettings.baselineSGP30_valid = 0xF0;
          mySettings.baselineeCO2_SGP30 = (uint16_t)tmpI;
          Serial.print(F("Calibration point is:  "));
          Serial.println(tmpI);
        }
      } else { Serial.println(F("Calibration point out of valid Range")); }
    } 

    else if (command == "v") {                                       // forced tVOC set setpoint
      tmpI = value.toInt();
      if ((tmpI > 400) && (tmpI < 2000)) {
        if (sgp30_avail && mySettings.useSGP30) {

          sgp30_port->begin(sgp30_i2c[0], sgp30_i2c[1]);
          
          sgp30.getBaseline();
          mySettings.baselineeCO2_SGP30 = sgp30.baselineCO2;
          sgp30.setBaseline((uint16_t)mySettings.baselineeCO2_SGP30, (uint16_t)tmpI);
          mySettings.baselineSGP30_valid = 0xF0;
          mySettings.baselinetVOC_SGP30 = (uint16_t)tmpI;
          Serial.print(F("Calibration point is:  "));
          Serial.println(tmpI);
        }
      } else { Serial.println(F("Calibration point out of valid Range")); }
    }
    
    else if (command == "g") {                                       // forced tVOC set setpoint
      if (sgp30_avail && mySettings.useSGP30) {
        
        sgp30_port->begin(sgp30_i2c[0], sgp30_i2c[1]);
        
        sgp30.getBaseline();
        mySettings.baselineSGP30_valid = 0xF0;
        mySettings.baselinetVOC_SGP30 = sgp30.baselineTVOC;
        mySettings.baselineeCO2_SGP30 = sgp30.baselineCO2;
      }
    } 

    ///////////////////////////////////////////////////////////////////
    // SCD30
    ///////////////////////////////////////////////////////////////////
    else if (command == "f") {                                       // forced CO2 set setpoint
      tmpI = value.toInt();
      if ((tmpI >= 400) && (tmpI <= 2000)) {
        if (scd30_avail && mySettings.useSCD30) {

          scd30_port->begin(scd30_i2c[0], scd30_i2c[1]);
          
          scd30.setForcedRecalibrationFactor((uint16_t)tmpI);
          Serial.print(F("Calibration point is:  "));
          Serial.println(tmpI);
        }
      } else { Serial.println(F("Calibration point out of valid Range")); }
    } 
    
    else if (command == "t") {                                       // forced CO2 set setpoint
      tmpF = value.toFloat();
      if ((tmpF >= 0.0) && (tmpF <= 10.0)) {
        if (scd30_avail && mySettings.useSCD30) {

          scd30_port->begin(scd30_i2c[0], scd30_i2c[1]);
          
          scd30.setTemperatureOffset(tmpF);
          mySettings.tempOffset_SCD30_valid = 0xF0;
          mySettings.tempOffset_SCD30 = scd30.getTemperatureOffset();
          Serial.print(F("Temperature offset set to:  "));
          Serial.println(mySettings.tempOffset_SCD30);
        }
      } else { Serial.println(F("Offset point out of valid Range")); }
    } 

    ///////////////////////////////////////////////////////////////////
    // CCS811
    ///////////////////////////////////////////////////////////////////
    else if (command == "c") {                                       // forced baseline
      tmpI = value.toInt();
      if ((tmpI >= 0) && (tmpI <= 100000)) {
        if (ccs811_avail && mySettings.useCCS811) {

          ccs811_port->begin(ccs811_i2c[0], ccs811_i2c[1]);
          
          ccs811.setBaseline((uint16_t)tmpI);
          mySettings.baselineCCS811_valid = 0xF0;
          mySettings.baselineCCS811 = (uint16_t)tmpI;
          Serial.print(F("Calibration point is:  "));
          Serial.println(tmpI);
        }
      } else { Serial.println(F("Calibration point out of valid Range")); }
    } 

    else if (command == "b") {                                      // getbaseline
      if (ccs811_avail && mySettings.useCCS811) {
        
        ccs811_port->begin(ccs811_i2c[0], ccs811_i2c[1]);
        
        mySettings.baselineCCS811 = ccs811.getBaseline();
        Serial.print(F("CCS811: baseline is  "));
        Serial.println(mySettings.baselineCCS811);
      }
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
      value.toCharArray(mySettings.ssid1,value.length()+1);
      Serial.print(F("SSID 1 is:  "));
      Serial.println(mySettings.ssid1);
    } 

    else if (command == "2") {                                       // SSID 2
      value.toCharArray(mySettings.ssid2,value.length()+1);
      Serial.print(F("SSID 2 is:  "));
      Serial.println(mySettings.ssid2);
    } 
    
    else if (command == "3") {                                       // SSID 3
      value.toCharArray(mySettings.ssid3,value.length()+1);
      Serial.print(F("SSID 3 is:  "));
      Serial.println(mySettings.ssid3);
    } 

    else if (command == "4") {                                       // Password 1
      value.toCharArray(mySettings.pw1,value.length()+1);
      Serial.print(F("Password 1 is:  "));
      Serial.println(mySettings.pw1);
    } 

    else if (command == "5") {                                       // Password 2
      value.toCharArray(mySettings.pw2,value.length()+1);
      Serial.print(F("Password 2 is:  "));
      Serial.println(mySettings.pw2);
    } 

    else if (command == "6") {                                       // Password 3
      value.toCharArray(mySettings.pw3,value.length()+1);
      Serial.print(F("Password 3 is:  "));
      Serial.println(mySettings.pw3);
    } 

    else if (command == "8") {                                       // MQTT Server
      value.toCharArray(mySettings.mqtt_server,value.length()+1);
      Serial.print(F("MQTT server is:  "));
      Serial.println(mySettings.mqtt_server);
    } 

    else if (command == "9") {                                       // MQTT fallback Server
      value.toCharArray(mySettings.mqtt_fallback,value.length()+1);
      Serial.print(F("MQTT fallback server is:  "));
      Serial.println(mySettings.mqtt_fallback);
    } 
    
    else if (command == "u") {                                       // MQTT Server
      value.toCharArray(mySettings.mqtt_username,value.length()+1);
      Serial.print(F("MQTT username is:  "));
      Serial.println(mySettings.mqtt_username);
    } 
    
    else if (command == "w") {                                       // MQTT Server
      value.toCharArray(mySettings.mqtt_password,value.length()+1);
      Serial.print(F("MQTT password is:  "));
      Serial.println(mySettings.mqtt_password);
    } 

    else if (command == "q") {                                       // MQTT Server
      mySettings.sendMQTTimmediate = !bool(mySettings.sendMQTTimmediate);
      Serial.print(F("MQTT is send immediatly:  "));
      Serial.println(mySettings.sendMQTTimmediate);
    } 

    else if (command == "n") {                                       // MQTT main topic name (device Name)
      value.toCharArray(mySettings.mqtt_mainTopic,value.length()+1);
      Serial.print(F("MQTT mainTopic is:  "));
      Serial.println(mySettings.mqtt_mainTopic);
    } 
    
    else if (command == "x") {                                       // enable/disable equipment
      tmpI = value.toInt();
      if ((tmpI > 0) && (tmpI <= 99)) {
        switch (tmpI) {
          case 2:
            mySettings.useLCD = !bool(mySettings.useLCD);
            if (mySettings.useLCD) { Serial.println(F("LCD is used")); } else {Serial.println(F("LCD is not used"));}
            break;
          case 3:
            mySettings.useWiFi = !bool(mySettings.useWiFi);
            if (mySettings.useWiFi) { Serial.println(F("WiFi is used")); } else {Serial.println(F("WiFi is not used"));}
            break;
          case 4:
            mySettings.useSCD30 = !bool(mySettings.useSCD30);
            if (mySettings.useSCD30) { Serial.println(F("SCD30 is used")); } else {Serial.println(F("SCD30 is not used"));}
            break;
          case 5:
            mySettings.useSPS30 = !bool(mySettings.useSPS30);
            if (mySettings.useSPS30) { Serial.println(F("SPS30 is used")); } else {Serial.println(F("SPS30 is not used"));}
            break;
          case 6:
            mySettings.useSGP30 = !bool(mySettings.useSGP30);
            if (mySettings.useSGP30) { Serial.println(F("SGP30 is used")); } else {Serial.println(F("SGP30 is not used"));}
            break;
          case 7:
            mySettings.useMAX30 = !bool(mySettings.useMAX30);
            if (mySettings.useMAX30) { Serial.println(F("MAX30 is used")); } else {Serial.println(F("MAX30 is not used"));}
            break;
          case 8:
            mySettings.useMLX = !bool(mySettings.useMLX);
            if (mySettings.useMLX) { Serial.println(F("MLX is used")); } else {Serial.println(F("MLX is not used"));}
            break;
          case 9:
            mySettings.useBME680 = !bool(mySettings.useBME680);
            if (mySettings.useBME680) { Serial.println(F("BME680 is used")); } else {Serial.println(F("BME680 is not used"));}
            break;
          case 10:
            mySettings.useBME280 = !bool(mySettings.useBME280);
            if (mySettings.useCCS811) { Serial.println(F("CCS811 is used")); } else {Serial.println(F("CCS811 is not used"));}
            break;
          case 11:
            mySettings.useCCS811 = !bool(mySettings.useCCS811);
            if (mySettings.useCCS811) { Serial.println(F("CCS811 is used")); } else {Serial.println(F("CCS811 is not used"));}
            break;
          case 99:
            Serial.println(F("ESP is going to restart. Bye."));
            ESP.restart();
            break;
          default:
            break;
        }
      }
    } 

    else if (command == "i") {                                       // simpler display on LCD
      mySettings.consumerLCD = !bool(mySettings.consumerLCD);
      if (mySettings.consumerLCD) { Serial.println(F("Simpler LCD display")); } else {Serial.println(F("Engineering LCD display"));}
    } 
    
    else {                                                           // unrecognized command
      helpMenu();
    } // end if
    
  } // end serial available
} // end Input

void printSettings() {
  Serial.println(F("========================================"));
  Serial.print(F("Debug level: .................. ")); Serial.println((unsigned int)mySettings.debuglevel);
  Serial.print(F("Runtime [min]: ................ ")); Serial.println((unsigned long)mySettings.runTime/60);
  Serial.print(F("Simpler Display: .............. ")); Serial.println(mySettings.consumerLCD);
  Serial.print(F("Base SGP30 valid: ............. ")); Serial.println((int)mySettings.baselineSGP30_valid);
  Serial.print(F("Base eCO2 SGP30: [ppm]......... ")); Serial.println((int)mySettings.baselineeCO2_SGP30);
  Serial.print(F("Base tVOC SGP30: [ppb]......... ")); Serial.println((int)mySettings.baselinetVOC_SGP30);
  Serial.print(F("Base CCS811 valid: ............ ")); Serial.println((int)mySettings.baselineCCS811_valid);
  Serial.print(F("Base CCS811: [Ohm]............. ")); Serial.println((int)mySettings.baselineCCS811);
  Serial.print(F("Temp Offset SCD30 valid: ...... ")); Serial.println((int)mySettings.tempOffset_SCD30_valid);
  Serial.print(F("Temp Offset SCD30: [C]......... ")); Serial.println((float)mySettings.tempOffset_SCD30);
  //Serial.print(F("Forced Calibration SCD30 valid: ")); Serial.println((int)mySettings.forcedCalibration_SCD30_valid);
  //Serial.print(F("Forced Calibration SCD30:[ppm]. ")); Serial.println((float)mySettings.forcedCalibration_SCD30);
  Serial.print(F("MLX Temp Offset valid: ........ ")); Serial.println((int)mySettings.tempOffset_MLX_valid);
  Serial.print(F("MLX Temp Offset: .............. ")); Serial.println((float)mySettings.tempOffset_MLX);
  Serial.print(F("SSID: ......................... ")); Serial.println(mySettings.ssid1);
  Serial.print(F("PW: ........................... ")); Serial.println(mySettings.pw1);
  Serial.print(F("SSID: ......................... ")); Serial.println(mySettings.ssid2);
  Serial.print(F("PW: ........................... ")); Serial.println(mySettings.pw2);
  Serial.print(F("SSID: ......................... ")); Serial.println(mySettings.ssid3);
  Serial.print(F("PW: ........................... ")); Serial.println(mySettings.pw3);
  Serial.print(F("MQTT: ......................... ")); Serial.println(mySettings.mqtt_server);
  Serial.print(F("MQTT fallback: ................ ")); Serial.println(mySettings.mqtt_fallback);
  Serial.print(F("MQTT user: .................... ")); Serial.println(mySettings.mqtt_username);
  Serial.print(F("MQTT password: ................ ")); Serial.println(mySettings.mqtt_password);
  Serial.print(F("MQTT send immediatly: ......... ")); Serial.println((mySettings.sendMQTTimmediate)?"on":"off");
  Serial.print(F("MQTT main topic: .............. ")); Serial.println(mySettings.mqtt_mainTopic);  
  Serial.print(F("LCD: .......................... ")); Serial.println((mySettings.useLCD)?"on":"off");
  Serial.print(F("WiFi: ......................... ")); Serial.println((mySettings.useWiFi)?"on":"off");
  Serial.print(F("SCD30: ........................ ")); Serial.println((mySettings.useSCD30)?"on":"off");
  Serial.print(F("SPS30: ........................ ")); Serial.println((mySettings.useSPS30)?"on":"off");
  Serial.print(F("SGP30: ........................ ")); Serial.println((mySettings.useSGP30)?"on":"off");
  Serial.print(F("MAX30: ........................ ")); Serial.println((mySettings.useMAX30)?"on":"off");
  Serial.print(F("MLX: .......................... ")); Serial.println((mySettings.useMLX)?"on":"off");
  Serial.print(F("BME680: ....................... ")); Serial.println((mySettings.useBME680)?"on":"off");
  Serial.print(F("BME280: ....................... ")); Serial.println((mySettings.useBME280)?"on":"off");
  Serial.print(F("CCS811: ....................... ")); Serial.println((mySettings.useCCS811)?"on":"off");
  Serial.println(F("========================================"));
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
  mySettings.forcedCalibration_SCD30_valid = (byte)         0x00; // not used
  mySettings.forcedCalibration_SCD30       = (float)         0.0; // not used
  mySettings.tempOffset_MLX_valid          = (byte)         0x00;
  mySettings.tempOffset_MLX                = (float)         0.0;
  strcpy(mySettings.ssid1,                 "meddev");
  strcpy(mySettings.pw1,                   "");
  strcpy(mySettings.ssid2,                 "UAGUest");
  strcpy(mySettings.pw2,                   "");
  strcpy(mySettings.ssid3,                 "MuchoCoolioG");
  strcpy(mySettings.pw3,                   "");
  strcpy(mySettings.mqtt_server,           "my.mqqtt.server.org");
  strcpy(mySettings.mqtt_fallback,         "192.168.1.1");
  strcpy(mySettings.mqtt_username,         "mqtt");
  strcpy(mySettings.mqtt_password,         "w1ldc8ts");
  strcpy(mySettings.mqtt_mainTopic,        "Sensi");
  mySettings.sendMQTTimmediate             = true;
  mySettings.useLCD                        = true;
  mySettings.useWiFi                       = true;
  mySettings.useSCD30                      = true;
  mySettings.useSPS30                      = true;
  mySettings.useSGP30                      = true;
  mySettings.useMAX30                      = true;
  mySettings.useMLX                        = true;
  mySettings.useBME680                     = true;
  mySettings.useBME280                     = true;
  mySettings.useCCS811                     = true;
  mySettings.consumerLCD                   = true;
}

void printSensors() {
  char tmpStr[48];

  Serial.println(F("========================================"));
  Serial.print(F("Average loop delay:      ")); Serial.println(myDelayAvg);
  Serial.print(F("Loop delay:              ")); Serial.println(myDelay);
  Serial.println(F("----------------------------------------"));
  if (lcd_avail && mySettings.useLCD) {
    Serial.print(F("LCD interval:          ")); Serial.println(intervalLCD);
    Serial.print(F("LCD   Port, SDA, SCL   ")); sprintf(tmpStr, "%d SDA %d SCL %d", uint32_t(lcd_port), lcd_i2c[0], lcd_i2c[1]); Serial.println(tmpStr);
    Serial.println(F("----------------------------------------"));
  }

  if (scd30_avail && mySettings.useSCD30) {
    
    scd30_port->begin(scd30_i2c[0], scd30_i2c[1]);
    
    Serial.print(F("SCD30 CO2:             ")); Serial.print(scd30_ppm);       Serial.println(F("[ppm]"));
    Serial.print(F("SCD30 rH:              ")); Serial.print(scd30_hum);       Serial.println(F("[%]"));
    Serial.print(F("SCD30 Temperature:     ")); Serial.print(scd30_temp);      Serial.println(F("[degC]"));
    Serial.print(F("SCD30 T Offset:        ")); Serial.print(scd30.getTemperatureOffset()); Serial.println(F("[degC]"));
    Serial.print(F("SCD30 interval:        ")); Serial.println(intervalSCD30);
    Serial.print(F("SCD30 Port, SDA, SCL   ")); sprintf(tmpStr, "%d SDA %d SCL %d", uint32_t(scd30_port), scd30_i2c[0], scd30_i2c[1]); Serial.println(tmpStr);
    Serial.println(F("----------------------------------------"));
  }
  
  if (bme680_avail && mySettings.useBME680) {
    Serial.print(F("BME680 Temperature:    ")); Serial.print(bme680.temperature); Serial.println(F("[degC]"));
    Serial.print(F("BME680 Pressure:       ")); Serial.print(bme680.pressure); Serial.println(F("[Pa]"));
    Serial.print(F("BME680 Pressure Ave:   ")); Serial.print(bme680_pressure24hrs); Serial.println(F("[Pa]"));
    Serial.print(F("BME680 Humidity:       ")); Serial.print(bme680.humidity); Serial.println(F("[%]"));
    Serial.print(F("BME680 aH:             ")); Serial.print(bme680_ah);       Serial.println(F("[g/m^3]"));
    Serial.print(F("BME680 Gas Resistance: ")); Serial.print(bme680.gas_resistance); Serial.println(F("[Ohm]"));
    Serial.print(F("BME680 interval:       ")); Serial.println(intervalBME680);
    Serial.print(F("BME680 Port, SDA, SCL  ")); sprintf(tmpStr, "%d SDA %d SCL %d", uint32_t(bme680_port), bme680_i2c[0], bme680_i2c[1]); Serial.println(tmpStr); 
    Serial.println(F("----------------------------------------"));
  }
  
  if (bme280_avail && mySettings.useBME280) {
    Serial.print(F("BME280 Temperature:    ")); Serial.print(bme280_temp);     Serial.println(F("[degC]"));
    Serial.print(F("BME280 Pressure:       ")); Serial.print(bme280_pressure); Serial.println(F("[Pa]"));
    Serial.print(F("BME280 Pressure Ave:   ")); Serial.print(bme280_pressure24hrs); Serial.println(F("[Pa]"));
    if (BMEhum_avail) { 
    Serial.print(F("BME280 Humidity:       ")); Serial.print(bme280_hum);    Serial.println(F("[%]"));
    Serial.print(F("BME280 aH:             ")); Serial.print(bme280_ah);     Serial.println(F("[g/m^3]")); 
    }
    Serial.print(F("BME280 interval:       ")); Serial.println(intervalBME280);
    Serial.print(F("BME280 Port, SDA, SCL  ")); sprintf(tmpStr, "%d SDA %d SCL %d", uint32_t(bme280_port), bme280_i2c[0], bme280_i2c[1]); Serial.println(tmpStr); 
    Serial.println(F("----------------------------------------"));
  }
  
  if (sgp30_avail && mySettings.useSGP30) {
    Serial.print(F("SGP30 CO2:             ")); Serial.print(sgp30.CO2);       Serial.println(F("[ppm]"));
    Serial.print(F("SGP30 tVOC:            ")); Serial.print(sgp30.TVOC);      Serial.println(F("[ppb]"));
    Serial.print(F("SGP30 baseline CO2:    ")); Serial.println(sgp30.baselineCO2);
    Serial.print(F("SGP30 baseline tVOC:   ")); Serial.println(sgp30.baselineTVOC);
    Serial.print(F("SGP30 H2:              ")); Serial.print(sgp30.H2);        Serial.println(F("[ppm]"));
    Serial.print(F("SGP30 Ethanol:         ")); Serial.print(sgp30.ethanol);   Serial.println(F("[ppm]"));
    Serial.print(F("SGP30 interval:        ")); Serial.println(intervalSGP30);
    Serial.print(F("SGP30 Port, SDA, SCL   ")); sprintf(tmpStr, "%d SDA %d SCL %d", uint32_t(sgp30_port), sgp30_i2c[0], sgp30_i2c[1]); Serial.println(tmpStr); 
    Serial.println(F("----------------------------------------"));
  }
  
  if (ccs811_avail && mySettings.useCCS811) {
    uint16_t tmpI;
    
    ccs811_port->begin(ccs811_i2c[0], ccs811_i2c[1]);
    
    tmpI = ccs811.getCO2();
    Serial.print(F("CCS811 CO2:            ")); Serial.print(tmpI);  Serial.println(F("[ppm]"));
    tmpI = ccs811.getTVOC();
    Serial.print(F("CCS811 tVOC:           ")); Serial.print(tmpI); Serial.println(F("[ppb]"));
    tmpI = ccs811.getBaseline();
    Serial.print(F("CCS811 baseline:       ")); Serial.println(tmpI);
    // ccs811ModeFast // 4=0.25s, 3=60s, 2=10sec, 1=1sec.
    Serial.print(F("CCS811 mode:           ")); Serial.print(ccs811Mode); Serial.println(F(" [4=0.25s, 3=60s, 2=10sec, 1=1sec]"));
    Serial.print(F("CCS811 Port, SDA, SCL  ")); sprintf(tmpStr, "%d SDA %d SCL %d", uint32_t(ccs811_port), ccs811_i2c[0], ccs811_i2c[1]); Serial.println(tmpStr); 
    Serial.println(F("----------------------------------------"));
  }
  
  if (sps30_avail && mySettings.useSPS30) {
    Serial.print(F("SPS30 P1.0:            ")); Serial.print(valSPS30.MassPM1);  Serial.println(F("[μg/m3]"));
    Serial.print(F("SPS30 P2.5:            ")); Serial.print(valSPS30.MassPM2);  Serial.println(F("[μg/m3]"));
    Serial.print(F("SPS30 P4.0:            ")); Serial.print(valSPS30.MassPM4);  Serial.println(F("[μg/m3]"));
    Serial.print(F("SPS30 P10:             ")); Serial.print(valSPS30.MassPM10); Serial.println(F("[μg/m3]"));
    Serial.print(F("SPS30 P1.0:            ")); Serial.print(valSPS30.NumPM0);   Serial.println(F("[#/cm3]"));
    Serial.print(F("SPS30 P2.5:            ")); Serial.print(valSPS30.NumPM2);   Serial.println(F("[#/cm3]"));
    Serial.print(F("SPS30 P4.0:            ")); Serial.print(valSPS30.NumPM4);   Serial.println(F("[#/cm3]"));
    Serial.print(F("SPS30 P10:             ")); Serial.print(valSPS30.NumPM10);  Serial.println(F("[#/cm3]"));
    Serial.print(F("SPS30 Average:         ")); Serial.print(valSPS30.PartSize); Serial.println(F("[μm]"));
    Serial.print(F("SPS30 interval:        ")); Serial.println(intervalSPS30);
    Serial.print(F("SPS30 Port, SDA, SCL   ")); sprintf(tmpStr, "%d SDA %d SCL %d", uint32_t(sps30_port), sps30_i2c[0], sps30_i2c[1]); Serial.println(tmpStr); 
    Serial.println(F("----------------------------------------"));
  }

  if (therm_avail && mySettings.useMLX) {
    float tmpF;

    mlx_port->begin(mlx_i2c[0], mlx_i2c[1]);

    tmpF = therm.object();
    Serial.print(F("MLX Object:            ")); Serial.print(tmpF);              Serial.println(F("[degC]"));
    tmpF = therm.ambient();
    Serial.print(F("MLX Ambient:           ")); Serial.print(tmpF);              Serial.println(F("[degC]"));
    tmpF = therm.readEmissivity();
    Serial.print(F("MLX Emissivity:        ")); Serial.println(tmpF);
    Serial.print(F("MLX Offset:            ")); Serial.println(mlxOffset);  
    Serial.print(F("MLX interval:          ")); Serial.println(intervalMLX);
    Serial.print(F("MLX Port, SDA, SCL     ")); sprintf(tmpStr, "%d SDA %d SCL %d", uint32_t(mlx_port), mlx_i2c[0], mlx_i2c[1]); Serial.println(tmpStr); 
    Serial.println(F("----------------------------------------"));
  }
  Serial.println(F("========================================"));  
}

// Boot helper
// Prints message and waits until timeout or user send character on serial terminal
void serialTrigger(char* mess, int timeout) {
  unsigned long startTime = millis();
  bool clearSerial = true;
  Serial.println();
  while (!Serial.available()) {
    Serial.println(mess);
    delay(2000);
    if (millis() - startTime >= timeout) {
      clearSerial = false;
      break;
    }
  }
  if (clearSerial == true) {
    while (Serial.available()) { Serial.read(); }
  }
}
