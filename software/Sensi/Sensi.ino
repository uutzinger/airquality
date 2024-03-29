//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Air Quality Sensors                                                                                                              //
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Supported Hardware:
//  - ESP8266 micro controller.                     Using 2.x and 3.x Arduino Libary  
//                                                  https://github.com/esp8266/Arduino
//  - SPS30 Sensirion particle,                     Rearranged and expanded Sensirion arduino-sps library  
//                                                  https://github.com/uutzinger/SPS30_Arduino_Library
//  - SCD30 Senserion CO2,                          Sparkfun library, using interrupt from data ready pin, replaced byte with uint8_t, 
//                                                  https://github.com/uutzinger/SparkFun_SCD30_Arduino_Library.git
//  - SGP30 Senserion VOC, eCO2,                    Sparkfun library, replaced byte with uint8_t, 
//                                                  https://github.com/uutzinger/SparkFun_SGP30_Arduino_Library.git
//  - BME68x Bosch Temp, Humidity, Pressure, VOC,   Bosch Arduino library, 
//                                                  https://github.com/BoschSensortec/Bosch-BME68x-Library
//  - BM[E/P]280 Bosch Temp, [Humidity,] Pressure   Sparkfun library, replaced byte with uint8_t, 
//                                                  https://github.com/uutzinger/SparkFun_BME280_Arduino_Library.git
//  - CCS811 Airquality eCO2 tVOC,                  Sparkfun library, using interrupt from data ready pin
//                                                  https://github.com/sparkfun/SparkFun_CCS811_Arduino_Library.git
//  - MLX90614 Melex temp contactless,              Sparkfun library, replaced byte with uint8_t, 
//                                                  https://github.com/uutzinger/SparkFun_MLX90614_Arduino_Library.git
//  - MAX30105 Maxim pulseox, not implemented yet,  Sparkfun library, replaced byte with uint8_t, 
//                                                  https://github.com/uutzinger/SparkFun_MAX3010x_Sensor_Library.git
// Data display through:
//  - LCD 20x4,                                     LiquidCrystal_PCF8574* or Adafruit_LCD library*
//                                                  both modified for wireport selection
//                                                  https://github.com/uutzinger/Adafruit_LiquidCrystal.git
//                                                  https://github.com/uutzinger/LiquidCrystal_PCF8574.git
// Network related libraries:
//  - MQTT publication to broker                    PubSubClient library http://pubsubclient.knolleary.net
//  - ESP_Telnet                                    https://github.com/LennartHennigs/ESPTelnet
//  - ESPNTPClient                                  https://github.com/gmag11/ESPNtpClient
//  - ArduWebSockets:                               https://github.com/Links2004/arduinoWebSockets
//  - OpenWeather                                   https://openweathermap.org/
//
// Other Libraries:
//  - ArduJSON                                      https://github.com/bblanchon/ArduinoJson.git
//
// Operation:
//  Fast Mode: read often, minimal power saving, some sensors have higher sampling rate than others
//  Slow Mode: read at about 1 sample per minute, where possible enable power saving features,
//  Some sensors require 1 sec sampling interval regardless of Fast or Slow Mode.
//
//  Air quality sensors other than pressue, temperature and humidity consume more power 
//  because they have heated sensos elements and are not well suited for battery operation.
//
// Settings:
//  Settings are programmed through the serial terminal. At this time no websinterface is available.
//  Check help-screen (send ? in terminal)
//  3 wifi passwords and 2 mqtt servers.
//  NPT server
//  Settings are stored in EEPROM.
//  Settings are saved every couple of hours in EEPROM.
//
// Wifi:
//  - Scans network for known ssid, attempts connection with username password, if disconnected, attempts reconnection.
//  - MQTT server is contacted, if it can not be reached, a backup server is contacted. username password is supported.
//  - Network Time is obtained using ESPNtpClient and unix system time is converted to local time using time zone settings.
//  - Over the air (OTA) programming can be enabled. 
//  - HTTPUpdater can be enabled at http://hostname:8890/firmware username admin password. 
//  - mDNS responder and client are implemented, OTA starts them automatically.
//  - HTML webserver is implemented. Sensors can be read useing http://hostname/sensor. http://hostname/ provides self updating root page.
//  - Websockets are supported but index.html for websocket support has not yet been created.
//    Large html files can be streamed from LittleFS.
//
// Quality Assessments:
//  The LCD screen background light is blinking when a sensor is outside its recommended range.
//  Pressure:
//    A change of 5mbar within in 24hrs can cause headaches in suseptible subjects
//    The program computes the daily average pressure and the difference to the current pressure
//  CO2:
//    >1000ppm is poor
//  Temperature:
//    20-25.5C is normal range
//  Humidity:
//    30-60% is normal range
//  Particles
//    P2.5: >25ug/m3 is poor
//    P10:  >50ug/m3 is poor
//  tVOC:
//    >660ppb is poor
//
// Wire/I2C:
//  Multiple i2c pins are supported, ESP8266 Arduino supports only one active wire interface, however the pins can
//  be specified prior to each communication. The i2c clock can also be adapted prior to each communcation.
//  At startup, all pin combinations are scanned for possible scl & sda connections of the attached sensors.
//  Supported sensors are searched on all possible hardware configurations and registered. 
//  An option to provide a delay after switching the wire port is provided.
//
//  Some breakout boards do not work well together.
//  Sensors might engage in excessive clockstretching and some have pull up resistor requirements beyond the one provided 
//  by the microcontroller.
//  For example CCS911 and SCD30 require long clock stretching. They are "slow" sensors. Its best to place them on separate i2c bus.
//  The LCD display requires 5V logic and can corrupts when the i2c bus is shared. It should be placed on separate bus.
//  The MLX sensor might report negative or excessive high temperature when combined with arbitrary sensors on the same bus.
//  Some senor software drivers contain significant delay statements. Since ESP uses delay function to brancht into updates 
//  of its operating system, its difficult to predict whether sensor drivers inadventently crash the system.  
//
// Driver Improvements:
//  Some drivers were modified to allow for user specified i2c port instead of standard "Wire" port:
//  SPS30, library was modified to function with devices that have faulty version information; the content of the read operation 
//  is available regradless wheter CRC error occured. Short delay calls between sending data request command and obtaining data 
//  have been changed to use delaymicroseconds to avoid ESP oprating system to update WiFi and OS during delay function calls.
//
// yieldOS:
//  Throughout the program yieldOS functions were placed. It measures the time since the last function call and executes 
//  delay or yield function so that the system can update its OS (WiFi).
//
// Software implementation:
//  The sensor driver is divided into intializations and update section.
//  The updates are implemented as state machines and called on regular basis in the main loop.
//
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// This software is provided as is, no warranty on its proper operation is implied. Use it at your own risk.
// Urs Utzinger
// Sometime:       Website auto adjusting to available sensors
// Sometime:       MAX pulseox software (requires proximit detection and (re)writting signal processing library)
// Sometime:       Configuration of system through website
// Sometime:       Support for TFT display
// Sometime:       Add support for xxx4x sensors from Sensirion, ENS160, BME688...
//
// 2022 Novemeber: Rewrote serial input command system and menu, SGP30 fixes
// 2022 October:   Print and delete files on LittleFS, telnet fix, manually set average pressure, jsondate fix,
//                 throttle MQTT, MQTT interval setting, BME680 not start detection.
// 2022 September: Changed SPS30 library to a custom rearranged Sensirion library. 
//                 Updated initialization and updateing of WIFI dependent modules. 
//                 Modified include files and variable declarations in all files;.
//                  (all varibales defined in one ino files with extern declarations in other files, ifdef wrapping for include files)
//                 Modifed SPS30 timeout and error trigger to take 10sec autocleaning into account. 
//                  (no new data is available during autocleaning, should not enter reset during autocleaning)
//                 LCD blinking and background light control rewritten.
//                 Added openWeather driver and display.
// 2022 August:    Switch BME68x to Bosch driver, updated MQTT.
// 2022 July:      Updated character array functions to avoid buffer overflow when copying 
//                  (character arrays (strncpy, snprintf*, strlcpy). 
//                 Updated SGP30 to avoid delay in device driver.
//                 Keep track of when sensor functions create error.
//                 Changed MLX driver code when writting to EEPROM.
// 2022 June:      Added yield options in main loop to most update sections and i2c calls. 
//                 Updated index.html, updated ...JSON() functions. 
//                 Removed the use of strings in main loop.
// 2021 October:   Logfile to record system issues
// 2021 September: Testing and release
// 2021 July:      Telnet
// 2021 June:      Webserver, NTP
// 2021 Feb/March: Display update, mDNS, OTA
// 2021 January:   i2c scanning and pin switching, OTA flashing
// 2020 Winter:    Fixing the i2c issues
// 2020 Fall:      MQTT and Wifi
// 2020 Summer:    First release
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////


/************************************************************************************************************************************/
// Links to Useful Documentations
/************************************************************************************************************************************/

// Memory Optimization
// -------------------
// https://esp8266life.wordpress.com/2019/01/13/memory-memory-always-memory/
// https://arduino-esp8266.readthedocs.io/en/latest/PROGMEM.html
// These links explain the use of F() amd PROGMEM and PSTR and sprintf_p().
// Currenlty, this program takes 51% of dynamic memory and 520k of program storage.
// During runtime heapsize and maximum free memory block is around 30k.

/************************************************************************************************************************************/
// Build Configuration
/************************************************************************************************************************************/
#define VER "2.3.2"

// Debug
// -----
#undef DEBUG     // disable "crash" DEBUG output for production
//#define DEBUG  // enabling "crash" debug will pin point where in the code the program crashed as it displays 
// a DBG statement when subroutines are called. Alternative is to install exception decoder or use both.

// LCD Screen
// ----------
//#define ADALCD     // we have Adafruit LCD driver
#undef ADALCD      // we have non-Adafruit LCD driver

// Measure fast or slow
// --------------------
#define FASTMODE true
// true:  Measure with about 5 sec intervals,
// false: Measure in about 1 min intervals and enable sensor sleeping and low energy modes if available
// Slow mode needs tweaking and testing to determine which WiFi services are compatible with it.
// MQTT will need to update with single message. Likley OTA, Updater, Telnet will have issues with slow mode.
// With current software version we can not switch between slow and fast during runtime.

// Yield
// -----
// This program can experience random crashes within couple of hours or days.
// At many locations in the program, yieldOS was inserted as the functions can take more than 100ms to complete.
// yieldOS keeps track when it was called the last time and calls the yield function if interval is excceded. 
// yieldI2C was created as it appears switching the wire port and setting new speed requires a short delay.
// ESP8266: delay(1) calls yield until delay 1ms expired.
// https://arduino-esp8266.readthedocs.io/en/3.0.0/reference.html#timing-and-delays

// yieldI2C ---
//#define yieldI2C() delayMicroseconds(100);      // block system for 0.1 ms
#define yieldI2C()                                // no i2c yield, (desired)

// yieldOS ----
#define MAXUNTILYIELD 50              // time in ms until yield should be called
//#define yieldFunction() delay(1)    // options are delay(0) [=yield()], delay(1) and empty for no yield
#define yieldFunction() yield()       // just yield, would be desired
//#define yieldFunction()             // no yield

/************************************************************************************************************************************/
// You should not need to change variables and constants below this line.
// Runtime changes to this software are enabled through the terminal.
// Permanent settings for the sensor and display devices can be changed in the 'src' folder in the *.h files.
// You would need an external editor to change the .h files.
/************************************************************************************************************************************/

char tmpStr[256];  // universal buffer for formatting texT

/************************************************************************************************************************************/
// Includes for Sensi
/************************************************************************************************************************************/
#include "src/Sensi.h"

// -- Settings
#include "src/Config.h"       // Store settings in EEPROM or littleFS with JSON

// -- Network
#include "src/WiFi.h"         // 
#include "src/WebSocket.h"    //
#include "src/MQTT.h"         //
#include "src/NTP.h"          //
#include "src/Telnet.h"       //
#include "src/HTTP.h"                                      // Our HTML webpage contents with javascripts
#include "src/HTTPUpdater.h"

// --- Sensors
#include "src/SCD30.h"   // --- SCD30; Sensirion CO2 sensor, Likely  most accurate sensor for CO2
#include "src/SPS30.h"   // --- SPS30; Senserion Particle Sensor,  likely most accurate sensor for PM2. PM110 might not be accurate though.
#include "src/SGP30.h"   // --- SGP30; Senserion TVOC, eCO2, accuracy is not known
#include "src/CCS811.h"  // --- CCS811; Airquality CO2 tVOC, traditional make comunity sensor
#include "src/BME68x.h"  // --- BME68x; Bosch Temp, Humidity, Pressure, VOC, more features than 280 sensor
#include "src/BME280.h"  // --- BME280; Bosch Temp, Humidity, Pressure, there is BME and BMP version.  One is lacking hymidity sensor.
#include "src/MLX.h"     // --- MLX contact less tempreture sensor, Thermal sensor for forehead readings. Likely not accurate.
#include "src/MAX30.h"   // --- MAX30105; pulseoximeter, Not implemented yet
#include "src/LCD.h"     // --- LCD 4x20 display
#include "src/LCDlayout.h"

// --- Support functions
#include "src/Quality.h" // --- Signal assessment
#include "src/Print.h"   // --- Printing support functions
#include "src/Weather.h" // --- Open Weather dou
#include "src/LCD.h"     // --- Display 
#include "src/Print.h"   // --- Printing

/************************************************************************************************************************************/
// Sensor Configuration
/************************************************************************************************************************************/

// Sensor update mode
// ------------------
bool fastMode = FASTMODE;                                      // Frequent or in frequent sensor updates

/// Baselines of certain sensors
unsigned long lastBaseline;                                // last time baseline was onbtained
unsigned long intervalBaseline;                            // how often do we obtain the baselines?

/************************************************************************************************************************************/
// I2C & SPI
/************************************************************************************************************************************/
//
#include <Wire.h>
// #include <SPI.h>
TwoWire myWire = TwoWire(); //  inintialize the two wire system.
// We will set multiple i2c ports for each sensor and we will switch pins each time we communicate to 
// an other i2c bus. Unfortunately ESP8266 Arduino supports only one i2c instance because the wire 
// library calls twi.h for which only one instance can run.
// Each time we commounicte with a device we need to set SDA and SCL pins.

/************************************************************************************************************************************/
// Healthy Airquality
/************************************************************************************************************************************/

// Out of Healthy Range
// --------------------
bool          allGood = true;                              // a sensor is outside recommended limits
bool          blinkLCD = false;                            // blink lcd background ?
unsigned long lastBlink;                                   // last time we switched LCD intensity
unsigned long intervalBlink = 1000;                        // auto populated during execution
unsigned long lastWarning;                                 // last time we check the sensors to create a warning message

/************************************************************************************************************************************/
// LittleFS
/************************************************************************************************************************************/
bool           fsOK = false;
unsigned long  lastLogFile; // last time we checked logfile size or closes the logfile
LittleFSConfig fileSystemConfig = LittleFSConfig();
File           logFile;

/************************************************************************************************************************************/
// Time Keeping
/************************************************************************************************************************************/

unsigned long currentTime;                                 // Populated at beginning of main loop

unsigned long intervalSYS;                                 // rate at which system information (CPU usage, Heap size) is displayed
unsigned long lastcurrentTime;                             // to update system unix type time
unsigned long lastTime;                                    // last time we updated runtime
unsigned long intervalLoop;                                // desired frequency to update device states
unsigned long intervalRuntime;                             // how often do we update the uptime counter of the system
unsigned long lastSYS;                                     // determines desired frequency to update system information on serial port
unsigned long tmpTime;                                     // temporary variable to measures execution times
long          myDelay;                                     // main loop delay, automatically computed
long          myDelayMin = 1000;                           // minium delay over the last couple seconds, if less than 0, ESP8266 can not keep up with
float         myDelayAvg = 0;                              // average delay over the last couplle seconds
unsigned long myLoop = 0;                                  // main loop execution time, automatically computed
unsigned long myLoopMin;                                   // main loop recent shortest execution time, automatically computed
unsigned long myLoopMax;                                   // main loop recent maximumal execution time, automatically computed
unsigned long myLoopMaxAllTime=0;                          // main loop all time maximum execution time, automatically computed
float         myLoopAvg = 0;                               // average delay over the last couple seconds
float         myLoopMaxAvg = 0;                            // average max loop delay over the last couple seconds

int           delayRuns = 0;                               // keep track of freeing CPU time

unsigned long lastYield = 0;                               // keep record of time of last Yield event
unsigned long yieldTime = 0;
unsigned long yieldTimeMin = 1000;
unsigned long yieldTimeMax = 0;
unsigned long yieldTimeMaxAllTime=0;

unsigned long startUpdate = 0;
unsigned long deltaUpdate = 0;
unsigned long maxUpdateTime = 0;
unsigned long maxUpdateWifi = 0;
unsigned long maxUpdateOTA = 0;
unsigned long maxUpdateNTP = 0;
unsigned long maxUpdateTelnet = 0;
unsigned long maxUpdateMQTT = 0;
unsigned long maxUpdateWS = 0;
unsigned long maxUpdateHTTP = 0;
unsigned long maxUpdateHTTPUPDATER = 0;
unsigned long maxUpdatemDNS = 0;
unsigned long maxUpdateWeather = 0;
unsigned long maxUpdateSCD30 = 0;
unsigned long maxUpdateSGP30 = 0;
unsigned long maxUpdateCCS811 = 0;
unsigned long maxUpdateSPS30 = 0;
unsigned long maxUpdateBME280 = 0; 
unsigned long maxUpdateBME68x = 0; 
unsigned long maxUpdateMLX = 0; 
unsigned long maxUpdateMAX = 0;
unsigned long maxUpdateMQTTMESSAGE = 0; 
unsigned long maxUpdateWSMESSAGE = 0; 
unsigned long maxUpdateLCD = 0;
unsigned long maxUpdateINPUT = 0; 
unsigned long maxUpdateRT = 0;
unsigned long maxUpdateEEPROM = 0; 
unsigned long maxUpdateJS = 0;
unsigned long maxUpdateBASE = 0;
unsigned long maxUpdateBLINK = 0;
unsigned long maxUpdateREBOOT = 0;
unsigned long maxUpdateALLGOOD = 0;
unsigned long maxUpdateSYS = 0;

unsigned long AllmaxUpdateTime = 0;
unsigned long AllmaxUpdateWifi = 0;
unsigned long AllmaxUpdateOTA = 0;
unsigned long AllmaxUpdateNTP = 0;
unsigned long AllmaxUpdateTelnet = 0;
unsigned long AllmaxUpdateMQTT = 0;
unsigned long AllmaxUpdateWS = 0;
unsigned long AllmaxUpdateHTTP = 0;
unsigned long AllmaxUpdateHTTPUPDATER = 0;
unsigned long AllmaxUpdatemDNS = 0;
unsigned long AllmaxUpdateWeather = 0;
unsigned long AllmaxUpdateSCD30 = 0;
unsigned long AllmaxUpdateSGP30 = 0;
unsigned long AllmaxUpdateCCS811 = 0;
unsigned long AllmaxUpdateSPS30 = 0;
unsigned long AllmaxUpdateBME280 = 0; 
unsigned long AllmaxUpdateBME68x = 0; 
unsigned long AllmaxUpdateMLX = 0; 
unsigned long AllmaxUpdateMAX = 0;
unsigned long AllmaxUpdateMQTTMESSAGE = 0; 
unsigned long AllmaxUpdateWSMESSAGE = 0; 
unsigned long AllmaxUpdateLCD = 0;
unsigned long AllmaxUpdateINPUT = 0; 
unsigned long AllmaxUpdateRT = 0;
unsigned long AllmaxUpdateEEPROM = 0; 
unsigned long AllmaxUpdateJS = 0;
unsigned long AllmaxUpdateBASE = 0;
unsigned long AllmaxUpdateBLINK = 0;
unsigned long AllmaxUpdateREBOOT = 0;
unsigned long AllmaxUpdateALLGOOD = 0;
unsigned long AllmaxUpdateSYS = 0;

time_t        actualTime;                                  // https://www.cplusplus.com/reference/ctime/time_t/
tm           *localTime;                                   // https://www.cplusplus.com/reference/ctime/tm/
// Member    Type  Meaning                  Range
// tm_sec     int seconds after the minute  0-61*
// tm_min     int minutes after the hour    0-59
// tm_hour    int hours since midnight      0-23
// tm_mday    int day of the month          1-31
// tm_mon     int months since January      0-11
// tm_year    int years since 1900  
// tm_wday    int days since Sunday         0-6
// tm_yday    int days since January 1      0-365
// tm_isdst   int Daylight Saving Time flag 
extern int    lastMin;
extern int    lastYearDay;

const char   *weekDays[7] PROGMEM = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
const char   *months[12]  PROGMEM = {"January", "February", "March", "April", "May", "June", "July", "August", 
                                     "September", "October", "November", "December"};
bool          timeNewData    = false;                      // do we have new data mqtt
bool          timeNewDataWS  = false;                      // do we have new data for websocket
bool          dateNewData    = false;                      // do we have new data mqtt
bool          dateNewDataWS  = false;                      // do we have new data websocket
bool          ntpFirstTime   = true;
bool          time_avail     = false;                      // do not display time until time is established
bool          scheduleReboot = false;                      // is it time to reboot the ESP?
bool          rebootNow      = false;                      // immediate reboot was requested

/************************************************************************************************************************************/
// Serial, Telnet
/************************************************************************************************************************************/
char          serialInputBuff[64];
bool          serialReceived = false;                      // keep track of serial input
unsigned long lastSerialInput = 0;                         // keep track of serial flood
unsigned long lastTmp = 0;

const char    singleSeparator[] PROGMEM = {"--------------------------------------------------------------------------------"};
const char    doubleSeparator[] PROGMEM = {"================================================================================"};
const char    mON[] PROGMEM             = {"on"};
const char    mOFF[] PROGMEM            = {"off"};
const char    clearHome[] PROGMEM       = {"\e[2J\e[H\r\n"};
const char    waitmsg[] PROGMEM         = {"Waiting 10 seconds, skip by hitting enter"};      // Allows user to open serial terminal to observe the debug output before the loop starts

/************************************************************************************************************************************/
// Global Variables
/************************************************************************************************************************************/
// Config 
extern const unsigned int eepromAddress;                          // 
extern unsigned long      lastSaveSettings;                       // last time we updated EEPROM, should occur every couple days
extern unsigned long      lastSaveSettingsJSON;                   // last time we updated JSON, should occur every couple days
extern Settings           mySettings;                             // the settings

// LCD
extern bool          lcd_avail;
extern uint8_t       lcd_i2c[2];                                  // the pins for the i2c port, set during initialization
extern TwoWire      *lcd_port;                                    // Pointer to the i2c port

extern bool          max30_avail;
extern uint8_t       max30_i2c[2];                                 // the pins for the i2c port, set during initialization
extern TwoWire      *max30_port;                                   // pointer to the i2c port, might be useful for other microcontrollers
extern const long    intervalMAX30;
extern unsigned long lastMAX30;            

extern bool          ccs811_avail;
extern uint8_t       ccs811_i2c[2];                                // the pins for the i2c port, set during initialization
extern TwoWire      *ccs811_port;                                  // pointer to the i2c port, might be useful for other microcontrollers
extern unsigned long intervalCCS811;
extern unsigned long lastCCS811;            
extern unsigned long lastCCS811Humidity;    
extern volatile unsigned long lastCCS811Interrupt;
extern unsigned long ccs811_lastError; 
extern CCS811        ccs811;
extern unsigned long warmupCCS811;
extern uint8_t       ccs811Mode;
extern volatile SensorStates  stateCCS811;

extern bool          sgp30_avail;
extern uint8_t       sgp30_i2c[2];                                 // the pins for the i2c port, set during initialization
extern TwoWire      *sgp30_port;                                   // pointer to the i2c port, might be useful for other microcontrollers
extern unsigned long intervalSGP30;
extern unsigned long lastSGP30;
extern unsigned long lastSGP30Humidity; 
extern unsigned long lastSGP30Baseline;
extern unsigned long sgp30_lastError;
extern SGP30         sgp30;
extern unsigned long warmupSGP30;

extern bool          therm_avail;
extern uint8_t       mlx_i2c[2];                                   // the pins for the i2c port, set during initialization
extern TwoWire      *mlx_port;                                     // pointer to the i2c port, might be useful for other microcontrollers
extern unsigned long intervalMLX;
extern float         mlxOffset;        
extern unsigned long lastMLX;
extern unsigned long mlx_lastError;
extern IRTherm       therm;

extern bool          scd30_avail;
extern uint8_t       scd30_i2c[2];                                 // the pins for the i2c port, set during initialization
extern TwoWire      *scd30_port;                                   // pointer to the i2c port, might be useful for other microcontrollers
extern uint16_t      scd30_ppm;
extern float         scd30_hum;
extern float         scd30_temp;
extern unsigned long intervalSCD30;
extern unsigned long lastSCD30;     
extern unsigned long lastSCD30Busy;
extern unsigned long lastPressureSCD30;   
extern unsigned long scd30_lastError; 
extern SCD30         scd30;
extern volatile SensorStates  stateSCD30;

extern bool          sps30_avail;
extern uint8_t       sps30_i2c[2];                                 // the pins for the i2c port, set during initialization
extern TwoWire      *sps30_port;                                   // pointer to the i2c port, might be useful for other microcontrollers
extern unsigned long intervalSPS30;
extern unsigned long lastSPS30; 
extern unsigned long sps30_lastError;
extern sps30_measurement valSPS30;
extern uint32_t      sps30AutoCleanInterval; 
extern volatile SensorStates  stateSPS30;

extern bool          bme280_avail;
extern uint8_t       bme280_i2c[2];                                // the pins for the i2c port, set during initialization
extern TwoWire      *bme280_port;                                  // pointer to the i2c port, might be useful for other microcontrollers
extern unsigned long intervalBME280;
extern unsigned long lastBME280;
extern unsigned long bme280_lastError;
extern float         bme280_temp;
extern float         bme280_pressure;;
extern float         bme280_pressure24hrs;
extern float         bme280_hum;
extern float         bme280_ah;
extern volatile SensorStates  stateBME280;

extern bool          bme68x_avail;
extern uint8_t       bme68x_i2c[2];                                // the pins for the i2c port, set during initialization
extern TwoWire      *bme68x_port;                                  // pointer to the i2c port, might be useful for other microcontrollers
extern unsigned long intervalBME68x;
extern unsigned long lastBME68x;
extern volatile SensorStates  stateBME68x;
extern bme68xData    bme68x; 
extern float         bme68x_pressure24hrs;//
extern unsigned long bme68x_lastError;
extern float         bme68x_ah;
extern bool          BMEhum_avail;

extern bool          weather_avail;
extern bool          weather_success;
extern clientData    weatherData;
extern unsigned long intervalWeather;
extern unsigned long weather_lastError;
extern volatile WiFiStates stateWeather;

extern unsigned long intervalLCD;
extern unsigned long intervalMQTT;

extern unsigned long lastLCD;              
extern unsigned long ntp_lastSync;
extern unsigned long lastLCDReset;     
extern unsigned long lastWiFi;         
extern unsigned long lastMQTT;         
extern unsigned long lastHTTP; 
extern unsigned long lastMDNS;   
extern unsigned long lastMQTTPublish;  
extern unsigned long lastMQTTStatusPublish;
extern unsigned long lastWeather;
extern unsigned long lastTelnet;         
extern unsigned long lastWebSocket;      
extern unsigned long lastTelnetInput;

extern bool          otaInProgress;
extern bool          wifi_avail;
extern bool          wifi_connected;
extern bool          ntp_avail;
extern bool          mdns_avail;
extern bool          mqtt_connected;  
extern bool          ws_connected;
extern bool          timeSynced;
extern bool          telnetReceived;

extern char          hostName[16];

extern volatile WiFiStates stateNTP;
extern volatile WiFiStates stateWiFi;
extern volatile WiFiStates stateWebSocket;
extern volatile WiFiStates stateMDNS;
extern volatile WiFiStates stateMQTT;
extern volatile WiFiStates stateOTA;
extern volatile WiFiStates stateHTTP;
extern volatile WiFiStates stateHTTPUpdater;
extern volatile WiFiStates stateTelnet;

extern char          telnetInputBuff[64];

extern unsigned long ntp_lastError;
extern bool          ntp_onfallback;

extern unsigned long mDNS_lastError;

#if defined(ADALCD)
  #include "Adafruit_LiquidCrystal.h"
  extern Adafruit_LiquidCrystal lcd;
#else
  #include "LiquidCrystal_PCF8574.h"
  extern LiquidCrystal_PCF8574 lcd;
#endif
extern bool      lastLCDInten;

extern const char mStarting[];
extern const char mMonitoring[];
extern const char mScanning[];
extern const char mConnecting[];
extern const char mWaiting[];
extern const char mNotEnabled[];

extern bool intervalNewData;

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// SETUP
////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void setup() { 
  fastMode = FASTMODE;
  // true:  Measure with about 5 sec intervals,
  // false: Measure in about 1 min intervals and enable sleeping and low energy modes if available

  // Initialize wire interface
  // These settings might be changed inside the third party sensor drivers and should be reasserted after initialization.
  myWire.begin(D1, D2);                  // SDA, SCL
  myWire.setClock(I2C_REGULAR);          // 100kHz or 400kHz speed, we need to use slowest of all sensors
  myWire.setClockStretchLimit(150000);   // we need to use largest of all sensors

  /************************************************************************************************************************************/
  // Configuration setup and read
  /************************************************************************************************************************************/
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(eepromAddress, mySettings);

  /************************************************************************************************************************************/
  // Inject hard coded values into the settings.
  // Useful when overriding eeprom settings.
  //  mySettings.runTime = 468469*60;
  //  mySettings.debuglevel  = 1;          // enable debug regardless of eeprom setting
  //  mySettings.useLCD    = true;         // enable/disable sensors
  //  mySettings.useWiFi   = true;         //
  //  mySettings.useSCD30  = true;         //
  //  mySettings.useSPS30  = true;         //
  //  mySettings.useSGP30  = true;         //
  //  mySettings.useMAX30  = true;         //
  //  mySettings.useMLX    = true;         //
  //  mySettings.useBME68x = true;         //
  //  mySettings.useBME280 = true;         //
  //  mySettings.useCCS811 = true;         //
  //  mySettings.notused   = true;         //
  //  mySettings.sendMQTTimmediate = true; //
  //  mySettings.useBacklight = true;      //
  //  mySettings.useBacklightNight = false;//
  //  mySettings.useBlinkNight = false;    //
  //  mySettings.useMQTT = false;          //
  //  mySettings.useNTP  = true;           //
  //  mySettings.usemDNS = true;           //
  //  mySettings.useHTTP = false;          //
  //  mySettings.useOTA  = false;          //
  //  mySettings.useHTTPUpdater = false;   //
  //  mySettings.useTelnet = false;        //
  //  mySettings.useSerial = true;         //
  //  mySettings.useLog = false;           //
  //  mySettings.useWeather = false;       //
  //  mySettings.nightBegin = 1200;        // minutes from midnight when to stop changing backlight because of low airquality
  //  mySettings.nightEnd = 360;           // minutes from midnight when to start changing backight because of low airquality
  //  strcpy(mySettings.ntpServer,         "us.pool.ntp.org");
  //  strcpy(mySettings.ntpFallback,       "time.nist.gov");
  //  strcpy(mySettings.timeZone,          "MST7");
  //  // strcpy(mySettings.timeZone,          "PST8PDT,M3.2.0/2:00:00,M11.1.0/2:00:00");
  //  strcpy(mySettings.weatherApiKey,     "");
  //  strcpy(mySettings.weatherCity,       "Tucson");
  //  // strcpy(mySettings.weatherCity,       "Tacoma");
  //  strcpy(mySettings.weatherCountryCode,"US");
  //  strcpy(mySettings.ssid1,             "");
  //  strcpy(mySettings.ssid2,             "");
  //  strcpy(mySettings.ssid3,             "");
  //  strcpy(mySettings.pw1,               "");
  //  strcpy(mySettings.pw2,               "");
  //  strcpy(mySettings.pw3,               "");
  //  strcpy(mySettings.mqtt_server,       "");
  //  strcpy(mySettings.mqtt_username,     "");
  //  strcpy(mySettings.mqtt_password,     "");
  //  strcpy(mySettings.mqtt_fallback,     "192.168.16.1");
  //  strcpy(mySettings.mqtt_mainTopic,    "");
  //  mySettings.intervalMQTT =1.0;        // send MQTT no faster than once per second
      
  // initialize localTime, logfile printing needs time structure
  actualTime = time(NULL);
  localTime = localtime(&actualTime);     // convert to localtime with daylight saving time

  if (mySettings.useSerial) { 
    Serial.begin(BAUDRATE);               // open serial port at specified baud rate
    Serial.setTimeout(1000);              // default is 1000 
  }
  // give user time to obeserve boot information
  if ( (mySettings.debuglevel > 0) && mySettings.useSerial ) { serialTrigger(waitmsg, 10000); }
  
  if ( mySettings.debuglevel > 0) { 
    snprintf_P(tmpStr, sizeof(tmpStr), PSTR("\r\nDebug level is: %d"), mySettings.debuglevel);  
    R_printSerialln(tmpStr); 
  }
  
  /************************************************************************************************************************************/
  // Start file system and list content
  /************************************************************************************************************************************/
  fileSystemConfig.setAutoFormat(false);
  LittleFS.setConfig(fileSystemConfig);
  fsOK=LittleFS.begin(); 
  if (!fsOK){ R_printSerialLogln(F("LittleFS mount failed!")); }
  if (mySettings.debuglevel > 0 && fsOK) {
    R_printSerialLogln(F("LittleFS started. Contents:"));
    Dir dir = LittleFS.openDir("/");
    bool filefound = false;
    unsigned int fileSize;
    while (dir.next()) {                      // List the file system contents
      String fileName = dir.fileName();
      fileSize = (unsigned int)dir.fileSize();
      snprintf_P(tmpStr, sizeof(tmpStr), PSTR("\tName: %20s, Size: %8u"), fileName.c_str(), fileSize); 
      R_printSerialLogln(tmpStr);
      filefound = true;
    }
    if (!filefound) { R_printSerialLogln(F("empty")); }
  }

  //Could also store settings on LittelFS
  //File myFile = LittleFS.open("/Sensi.config", "r");
  //myFile.read((uint8_t *)&mySettings, sizeof(mySettings));
  //myFile.close();
  
  /************************************************************************************************************************************/
  // When EEPROM is used for the first time, it has random content.
  // A boolen seems to use one byte of memory in the EEPROM and when you read from EEPROM to a boolen variable 
  // it can become uint_8. When the program sets these boolean values from true to false, it will convert 
  // from true=255 to 254 and not to 0/false.
  // At boot time we  need to ensure that any non zero value for boolean settings are forced to true.
  // Once the varibales a set with code below and written back to EEPROM, this should not be necessary any longer.
  if (mySettings.useLCD            > 0) { mySettings.useLCD            = true; } else { mySettings.useLCD              = false; }
  if (mySettings.useWiFi           > 0) { mySettings.useWiFi           = true; } else { mySettings.useWiFi             = false; }
  if (mySettings.useSCD30          > 0) { mySettings.useSCD30          = true; } else { mySettings.useSCD30            = false; }
  if (mySettings.useSPS30          > 0) { mySettings.useSPS30          = true; } else { mySettings.useSPS30            = false; }
  if (mySettings.useMAX30          > 0) { mySettings.useMAX30          = true; } else { mySettings.useMAX30            = false; }
  if (mySettings.useMLX            > 0) { mySettings.useMLX            = true; } else { mySettings.useMLX              = false; }
  if (mySettings.useBME68x         > 0) { mySettings.useBME68x         = true; } else { mySettings.useBME68x           = false; }
  if (mySettings.useBME280         > 0) { mySettings.useBME280         = true; } else { mySettings.useBME280           = false; }
  if (mySettings.useCCS811         > 0) { mySettings.useCCS811         = true; } else { mySettings.useCCS811           = false; }
  if (mySettings.sendMQTTimmediate > 0) { mySettings.sendMQTTimmediate = true; } else { mySettings.sendMQTTimmediate   = false; }
  if (mySettings.notused           > 0) { mySettings.notused           = true; } else { mySettings.notused             = false; }
  if (mySettings.useBacklight      > 0) { mySettings.useBacklight      = true; } else { mySettings.useBacklight        = false; }
  if (mySettings.useBacklightNight > 0) { mySettings.useBacklightNight = true; } else { mySettings.useBacklightNight   = false; }
  if (mySettings.useBlinkNight     > 0) { mySettings.useBlinkNight     = true; } else { mySettings.useBlinkNight       = false; }
  if (mySettings.useHTTP           > 0) { mySettings.useHTTP           = true; } else { mySettings.useHTTP             = false; }
  if (mySettings.useHTTPUpdater    > 0) { mySettings.useHTTPUpdater    = true; } else { mySettings.useHTTPUpdater      = false; }
  if (mySettings.useNTP            > 0) { mySettings.useNTP            = true; } else { mySettings.useNTP              = false; }
  if (mySettings.useMQTT           > 0) { mySettings.useMQTT           = true; } else { mySettings.useMQTT             = false; }
  if (mySettings.useOTA            > 0) { mySettings.useOTA            = true; } else { mySettings.useOTA              = false; }
  if (mySettings.usemDNS           > 0) { mySettings.usemDNS           = true; } else { mySettings.usemDNS             = false; }
  if (mySettings.useTelnet         > 0) { mySettings.useTelnet         = true; } else { mySettings.useTelnet           = false; }
  if (mySettings.useSerial         > 0) { mySettings.useSerial         = true; } else { mySettings.useSerial           = false; }
  if (mySettings.useLog            > 0) { mySettings.useLog            = true; } else { mySettings.useLog              = false; }
  if (mySettings.useWeather        > 0) { mySettings.useWeather        = true; } else { mySettings.useWeather          = false; }

  /************************************************************************************************************************************/
  // Check which devices are attached to the I2C pins, this self configures our connections to the sensors
  /************************************************************************************************************************************/
  // Wemos 
  // uint8_t portArray[] = {16, 5, 4, 0, 2, 14, 12, 13};
  // String portMap[] = {"D0", "D1", "D2", "D3", "D4", "D5", "D6", "D7"};

  // dont search on ports used for data ready or interrupt signaling, otherwise use all the ones listed above
  uint8_t portArray[] = {5, 4, 0, 2, 14, 12};
  char portMap[6][3] = {"D1", "D2", "D3", "D4", "D5", "D6"};
  uint8_t error, address;
  
  // lets turn on CCS811 so we can find it ;-)
  pinMode(CCS811_WAKE, OUTPUT);                        // CCS811 not Wake Pin
  digitalWrite(CCS811_WAKE, LOW);                      // set CCS811 to wake up

  R_printSerialLogln();
  for (uint8_t i = 0; i < sizeof(portArray); i++) {
    for (uint8_t j = 0; j < sizeof(portArray); j++) {
      if (i != j) {
        snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Scanning (SDA:SCL) - %s:%s"), portMap[i], portMap[j]); 
        R_printSerialLogln(tmpStr);
        myWire.begin(portArray[i], portArray[j]);
        myWire.setClock(I2C_SLOW);
        myWire.setClockStretchLimit(200000);
        for (address = 1; address < 127; address++ )  {
          myWire.beginTransmission(address);
          error = myWire.endTransmission();
          if (error == 0) {
            // found a device
            if      (address == 0x20) {
              lcd_avail  = true;  // LCD display Adafruit
              lcd_port   = &myWire;    lcd_i2c[0]  = portArray[i];    lcd_i2c[1] = portArray[j];
            } else if (address == 0x27) {
              lcd_avail  = true;  // LCD display PCF8574
              lcd_port   = &myWire;    lcd_i2c[0]  = portArray[i];    lcd_i2c[1] = portArray[j];
            } else if (address == 0x57) {
              max30_avail  = true;  // MAX 30105 Pulseox & Particle
              max30_port   = &myWire;  max30_i2c[0] = portArray[i];  max30_i2c[1] = portArray[j];
            } else if (address == 0x58) {
              sgp30_avail  = true;  // Senserion TVOC eCO2
              sgp30_port   = &myWire;  sgp30_i2c[0] = portArray[i];  sgp30_i2c[1] = portArray[j];
            } else if (address == 0x5A) {
              therm_avail = true;  // MLX IR sensor
              mlx_port    = &myWire;   mlx_i2c[0]  = portArray[i];   mlx_i2c[1]  = portArray[j];
            } else if (address == 0x5B) {
              ccs811_avail  = true;  // Airquality CO2 tVOC
              ccs811_port   = &myWire; ccs811_i2c[0] = portArray[i]; ccs811_i2c[1] = portArray[j];
            } else if (address == 0x61) {
              scd30_avail  = true;  // Senserion CO2
              scd30_port   = &myWire;  scd30_i2c[0] = portArray[i];  scd30_i2c[1] = portArray[j];
            } else if (address == 0x69) {
              sps30_avail  = true;  // Senserion Particle
              sps30_port   = &myWire;  sps30_i2c[0] = portArray[i];  sps30_i2c[1] = portArray[j];
            } else if (address == 0x76) {
              bme280_avail  = true;  // Bosch Temp, Humidity, Pressure
              bme280_port   = &myWire; bme280_i2c[0] = portArray[i]; bme280_i2c[1] = portArray[j];
            } else if (address == 0x77) {
              bme68x_avail  = true;  // Bosch Temp, Humidity, Pressure, VOC
              bme68x_port   = &myWire; bme68x_i2c[0] = portArray[i]; bme68x_i2c[1] = portArray[j];
            } else {
              snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Found unknonw device - %d!"),address); 
              R_printSerialLog(tmpStr);
            }
          }
        }
      }
    }
  }

  // List the devices we found
  if (lcd_avail)    { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("LCD                  available: %2d SDA %d SCL %2d"), uint32_t(lcd_port),    lcd_i2c[0],    lcd_i2c[1]);    } else { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("LCD                  not available")); }  
  R_printSerialLogln(tmpStr);
  if (max30_avail)  { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("MAX3010x             available: %2d SDA %d SCL %2d"), uint32_t(max30_port),  max30_i2c[0],  max30_i2c[1]);  } else { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("MAX3010x             not available")); }  
  printSerialLogln(tmpStr);
  if (ccs811_avail) { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("CCS811 eCO2, tVOC    available: %2d SDA %d SCL %2d"), uint32_t(ccs811_port), ccs811_i2c[0], ccs811_i2c[1]); } else { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("CCS811 eCO2, tVOC    not available")); }  
  R_printSerialLogln(tmpStr);
  if (sgp30_avail)  { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SGP30 eCO2, tVOC     available: %2d SDA %d SCL %2d"), uint32_t(sgp30_port),  sgp30_i2c[0],  sgp30_i2c[1]);  } else { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SGP30 eCO2, tVOC     not available")); }  
  R_printSerialLogln(tmpStr);
  if (therm_avail)  { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("MLX temp             available: %2d SDA %d SCL %2d"), uint32_t(mlx_port),    mlx_i2c[0],    mlx_i2c[1]);    } else { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("MLX temp             not available")); }  
  R_printSerialLogln(tmpStr);
  if (scd30_avail)  { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SCD30 CO2, rH        available: %2d SDA %d SCL %2d"), uint32_t(scd30_port),  scd30_i2c[0],  scd30_i2c[1]);  } else { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SCD30 CO2, rH        not available")); }
  R_printSerialLogln(tmpStr);
  if (sps30_avail)  { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SPS30 PM             available: %2d SDA %d SCL %2d"), uint32_t(sps30_port),  sps30_i2c[0],  sps30_i2c[1]);  } else { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SPS30 PM             not available")); }  
  R_printSerialLogln(tmpStr);
  if (bme280_avail) { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("BM[E/P]280 T, P[,rH] available: %2d SDA %d SCL %2d"), uint32_t(bme280_port), bme280_i2c[0], bme280_i2c[1]); } else { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("BM[E/P]280 T, P[,rH] not available")); }  
  R_printSerialLogln(tmpStr);
  if (bme68x_avail) { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("BME68x T, rH, P tVOC available: %2d SDA %d SCL %2d"), uint32_t(bme68x_port), bme68x_i2c[0], bme68x_i2c[1]); } else { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("BME68x T, rH, P tVOC not available")); }  
  R_printSerialLogln(tmpStr);

  /************************************************************************************************************************************/
  // Time Intervals for Loop, Display and MQTT publishing, keeping track of runtime
  /************************************************************************************************************************************/
  // Software is designed to run either with frequent updates or in slow mode with updates in the minutes

  // Updaterate of devices is defined in their respective .h files
  if (fastMode == true) {                                     // fast mode -----------------------------------
    intervalLoop     =                  100;                  // 0.1 sec, main loop runs 10 times per second
    intervalLCD      =      intervalLCDFast;                  // 
    intervalRuntime  =                60000;                  // 1 minute, uptime is updted every minute
    intervalBaseline = intervalBaselineFast;                  // 10 minutes
    intervalWeather  = intervalWeatherFast;                   // 1.5 minutes
  } else {                                                    // slow mode -----------------------------------
    intervalLoop     =                 1000;                  // 1 sec, main loop runs once a second, ESP needs main loop to run faster than 100ms otherwise yield is needed
    intervalLCD      =      intervalLCDSlow;                  // 1 minute
    intervalRuntime  =               600000;                  // 10 minutes
    intervalBaseline = intervalBaselineSlow;                  // 1 hr
    intervalWeather  =  intervalWeatherSlow;                  // 1 hr
  }
  intervalMQTT     = (unsigned long)(mySettings.intervalMQTT*1000.);          // 
  intervalNewData = true;

  /************************************************************************************************************************************/
  // Initialize all devices
  /************************************************************************************************************************************/
  if (lcd_avail && mySettings.useLCD)       { if (initializeLCD()    == false) { lcd_avail = false;    } } else { lcd_avail    = false; }  // Initialize LCD Screen
  D_printSerial(F("DBG:INI: LCD.."));
  if (scd30_avail && mySettings.useSCD30)   { if (initializeSCD30()  == false) { scd30_avail = false;  } } else { scd30_avail  = false; }  // Initialize SCD30 CO2 sensor
  D_printSerial(F("DBG:INI: SCD30.."));
  if (sgp30_avail && mySettings.useSGP30)   { if (initializeSGP30()  == false) { sgp30_avail = false;  } } else { sgp30_avail  = false; }  // SGP30 Initialize
  D_printSerial(F("DBG:INI: SGP30.."));
  if (ccs811_avail && mySettings.useCCS811) { if (initializeCCS811() == false) { ccs811_avail = false; } } else { ccs811_avail = false; }  // CCS811 Initialize
  D_printSerial(F("DBG:INI: CCS811.."));
  if (sps30_avail && mySettings.useSPS30)   { if (initializeSPS30()  == false) { sps30_avail = false;  } } else { sps30_avail  = false; }  // SPS30 Initialize Particle Sensor
  D_printSerial(F("DBG:INI: SPS30.."));
  if (bme68x_avail && mySettings.useBME68x) { if (initializeBME68x() == false) { bme68x_avail = false; } } else { bme68x_avail = false; }  // Initialize BME68x
  D_printSerial(F("DBG:INI: BME68x.."));
  if (bme280_avail && mySettings.useBME280) { if (initializeBME280() == false) { bme280_avail = false; } } else { bme280_avail = false; }  // Initialize BME280
  D_printSerial(F("DBG:INI: BME280.."));
  if (therm_avail && mySettings.useMLX)     { if (initializeMLX()    == false) { therm_avail = false;  } } else { therm_avail  = false; }  // Initialize MLX Sensor
  D_printSerial(F("DBG:INI: MLX.."));
  if (max30_avail && mySettings.useMAX30)   { if (initializeMAX30()  == false) { max30_avail = false;  } } else { max30_avail  = false; }  // Initialize MAX Pulse OX Sensor, N.A.
  D_printSerial(F("DBG:INI: MAX30.."));

  /************************************************************************************************************************************/
  // Make sure i2c bus was not changed during above's initializations.
  // These settings will work for all sensors supported.
  // Since one has only one wire instance available on EPS8266, it does not make sense to operate the
  // pins at differnt clock speeds.
  /************************************************************************************************************************************/

  myWire.setClock(I2C_REGULAR);             // in Hz
  myWire.setClockStretchLimit(150000);      // in micro seconds

  // how often do we reset the cpu usage time counters for the subroutines?
  intervalSYS                                     = intervalWiFi;
  if (intervalBME280 > intervalSYS) { intervalSYS = intervalBME280; }
  if (intervalBME68x > intervalSYS) { intervalSYS = intervalBME68x; }
  if (intervalCCS811 > intervalSYS) { intervalSYS = intervalCCS811; }
  if (intervalLCD    > intervalSYS) { intervalSYS = intervalLCD;    }
  if (intervalMAX30  > intervalSYS) { intervalSYS = intervalMAX30;  }
  if (intervalMLX    > intervalSYS) { intervalSYS = intervalMLX;    }
  if (intervalMQTT   > intervalSYS) { intervalSYS = intervalMQTT;   }
  if (intervalSCD30  > intervalSYS) { intervalSYS = intervalSCD30;  }
  if (intervalSGP30  > intervalSYS) { intervalSYS = intervalSGP30;  }
  if (intervalSPS30  > intervalSYS) { intervalSYS = intervalSPS30;  }

  /************************************************************************************************************************************/
  // Populate LCD screen, start with cleared LCD
  /************************************************************************************************************************************/
  D_printSerial(F("D:S:LCD.."));
  if (lcd_avail && mySettings.useLCD) {
    if      ( mySettings.LCDdisplayType == 1 ) { updateLCD(); }                //<<<<<<------
    else if ( mySettings.LCDdisplayType == 2 ) { updateTwoPageLCD(); }
    else if ( mySettings.LCDdisplayType == 3 ) { updateSinglePageLCD(); }
    else if ( mySettings.LCDdisplayType == 4 ) { updateSinglePageLCDwTime(); }
    else { if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("LCD display type not supported")); } } 
    if (mySettings.debuglevel > 0) { R_printSerialLogln(F("LCD updated")); }
  }

  /************************************************************************************************************************************/
  // Connect to WiFi, initialize mDNS, NTP, HTTP, MQTT, OTA, Telnet
  /************************************************************************************************************************************/
  D_printSerial(F("D:I:Wifi.."));
  initializeWiFi(); // if user requests wifi to be off, functions depending on wifi will not be enabled
  // after WiFi init we initialize other WiFi dependent modules
  if (mySettings.usemDNS)        { initializeMDNS(); }
  if (mySettings.useNTP)         { initializeNTP(); }
  if (mySettings.useHTTP)        { initializeHTTP(); }
  if (mySettings.useHTTPUpdater) { initializeHTTPUpdater(); }
  if (mySettings.useMQTT)        { initializeMQTT(); }
  if (mySettings.useOTA)         { initializeOTA(); }
  if (mySettings.useTelnet)      { initializeTelnet(); }
  if (mySettings.useWeather)     { initializeWeather(); }

  if ( (mySettings.debuglevel > 0) && mySettings.useSerial ) {  R_printSerialTelnetLogln(F("System initialized")); }
  D_printSerial(F("D:I:CMPLT.."));
  scheduleReboot = false;
  
  /************************************************************************************************************************************/
  // Initialize Timing System
  /************************************************************************************************************************************/
  currentTime           = millis();
  lastcurrentTime       = currentTime;
  lastTime              = currentTime;
  lastSCD30             = currentTime;
  lastLCD               = currentTime;
  lastPressureSCD30     = currentTime;
  lastSaveSettings      = currentTime;
  lastSaveSettingsJSON  = currentTime;
  lastMAX30             = currentTime;
  lastMLX               = currentTime;
  lastCCS811            = currentTime;
  lastCCS811Humidity    = currentTime;
  lastCCS811Interrupt   = currentTime;
  lastSCD30             = currentTime;
  lastSCD30Busy         = currentTime;
  lastSPS30             = currentTime;
  lastBME280            = currentTime;
  lastBME68x            = currentTime;
  lastSGP30             = currentTime;
  lastSGP30Humidity     = currentTime;
  lastSGP30Baseline     = currentTime;
  lastLCDReset          = currentTime;
  lastWiFi              = currentTime;
  lastMQTT              = currentTime;
  lastHTTP              = currentTime;
  lastMDNS              = currentTime;
  lastHTTP              = currentTime;
  lastMQTTPublish       = currentTime;
  lastMQTTStatusPublish = currentTime;
  lastWeather           = currentTime;
  lastBlink             = currentTime;
  lastTelnet            = currentTime;
  lastBaseline          = currentTime;
  lastWebSocket         = currentTime;
  lastWarning           = currentTime;
  lastLogFile           = currentTime;
  lastSerialInput       = currentTime;
  lastTelnetInput       = currentTime;
    
} // end setup

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Main Loop
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void loop() {
  currentTime = lastYield = millis();                   // keep track of loop time
  
  if (otaInProgress) { updateOTA(); }                   // when OTA is in progress we do not do anything else
  else {                                                // OTA is not in progress, we update the subsystems

    // Free up Processor 
    // You can not free up processor time here or at the end of the main loop as system crashes when inserting delay()
    // D_printSerialTelnet(F("D:U:LD.."));
    // if (myLoop < intervalLoop) {
    //   delayRuns = (intervalLoop - myLoop) / 25;      // how often do we want to repeat delay 
    //   if (delayRuns > 4) { delayRuns = 4; }          // we do not want to stay here for too long (100ms)
    //   for (int i=0; i<=delayRuns; i++) { delay(25); }
    // }
    
    // update current time every second -----------------------------------------------------------------
    if ( (currentTime - lastcurrentTime) >= 1000 ) {
      lastcurrentTime = currentTime;
      D_printSerialTelnet(F("D:U:D&T.."));      

      startUpdate = millis(); // how long does this code segment take?
      actualTime  = time(NULL);
      localTime   = localtime(&actualTime);             // convert to localtime with daylight saving time
      time_avail  = true;

      // every minute, broadcast time
      if (localTime->tm_min != lastMin) {
        timeNewDataWS = true;                           // broadcast date on websocket
        timeNewData   = true;                           // broadcast date on mqtt
        lastMin       = localTime->tm_min;
      }
      
      if (localTime->tm_yday != lastYearDay) {
        dateNewDataWS = true;                           // broadcast date on websocket
        dateNewData   = true;                           // broadcast date on mqtt
        lastYearDay   = localTime->tm_yday;
      }

      deltaUpdate = millis() - startUpdate;                                   // this code segment took delta ms
      if (maxUpdateTime    < deltaUpdate) { maxUpdateTime = deltaUpdate; }    // current max updatetime for this code segment
      if (AllmaxUpdateTime < deltaUpdate) { AllmaxUpdateTime = deltaUpdate; } // longest global updatetime for this code segement

      D_printSerialTelnet(F("D:S:NTP.."));
      yieldTime += yieldOS(); 
    }
    
    /**********************************************************************************************************************************/
    // Update the State Machines for all Devices and System Processes
    /**********************************************************************************************************************************/
    // Update Wireless Services 
    if (wifi_avail               && mySettings.useWiFi) { 
      startUpdate = millis();
      updateWiFi(); //<<<<<<<<<<<<<<<<<<<<--------------- WiFi update, this will reconnect if disconnected
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateWifi    < deltaUpdate) { maxUpdateWifi = deltaUpdate; }
      if (AllmaxUpdateWifi < deltaUpdate) { AllmaxUpdateWifi = deltaUpdate; }
      yieldTime += yieldOS(); 
    } 
    if (wifi_avail               && mySettings.useWiFi && mySettings.useOTA)    { 
      startUpdate = millis();
      updateOTA(); //<<<<<<<<<<<<<<<<<<<<---------------- Update OTA --------------------------------------
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateOTA    < deltaUpdate)  { maxUpdateOTA = deltaUpdate; }
      if (AllmaxUpdateOTA < deltaUpdate)  { AllmaxUpdateOTA = deltaUpdate; }
      yieldTime += yieldOS(); 
    } 
    if (wifi_avail && ntp_avail  && mySettings.useWiFi && mySettings.useNTP)    { 
      startUpdate = millis();
      updateNTP();  //<<<<<<<<<<<<<<<<<<<<--------------- Update Network Time -----------------------------
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateNTP    < deltaUpdate)  { maxUpdateNTP = deltaUpdate; }
      if (AllmaxUpdateNTP < deltaUpdate)  { AllmaxUpdateNTP = deltaUpdate; }
      yieldTime += yieldOS(); 
    } 
    if (wifi_avail                && mySettings.useWiFi && mySettings.useMQTT)   { 
      startUpdate = millis();
      updateMQTT(); //<<<<<<<<<<<<<<<<<<<<--------------- MQTT update serve connection --------------------
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateMQTT    < deltaUpdate) { maxUpdateMQTT = deltaUpdate; }
      if (AllmaxUpdateMQTT < deltaUpdate) { AllmaxUpdateMQTT = deltaUpdate; }
      yieldTime += yieldOS();
    } 
    if (wifi_avail                && mySettings.useWiFi && mySettings.useHTTP)   { 
      startUpdate = millis();
      updateWebSocket(); //<<<<<<<<<<<<<<<<<<<<---------- Websocket update server connection --------------
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateWS    < deltaUpdate)   { maxUpdateWS = deltaUpdate; }
      if (AllmaxUpdateWS < deltaUpdate)   { AllmaxUpdateWS = deltaUpdate; }
      yieldTime += yieldOS(); 
    } 
    if (wifi_avail                && mySettings.useWiFi && mySettings.useHTTP)   { 
      startUpdate = millis();
      updateHTTP(); //<<<<<<<<<<<<<<<<<<<<--------------- Update web server -------------------------------
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateHTTP    < deltaUpdate) { maxUpdateHTTP = deltaUpdate; }
      if (AllmaxUpdateHTTP < deltaUpdate) { AllmaxUpdateHTTP = deltaUpdate; }
      yieldTime += yieldOS(); 
    } 
    if (wifi_avail                && mySettings.useWiFi && mySettings.useHTTPUpdater)   { 
      startUpdate = millis();
      updateHTTPUpdater();  //<<<<<<<<<<<<<<<<<<<<------- Update firmware web server ----------------------
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateHTTPUPDATER    < deltaUpdate) { maxUpdateHTTPUPDATER = deltaUpdate; }
      if (AllmaxUpdateHTTPUPDATER < deltaUpdate) { AllmaxUpdateHTTPUPDATER = deltaUpdate; }
      yieldTime += yieldOS(); 
    } 
    if (wifi_avail && mdns_avail  && mySettings.useWiFi && mySettings.usemDNS)   { 
      startUpdate = millis();
      updateMDNS(); //<<<<<<<<<<<<<<<<<<<<--------------- Update mDNS -------------------------------------
      deltaUpdate = millis() - startUpdate;
      if (maxUpdatemDNS    < deltaUpdate) { maxUpdatemDNS = deltaUpdate; }
      if (AllmaxUpdatemDNS < deltaUpdate) { AllmaxUpdatemDNS = deltaUpdate; }
      yieldTime += yieldOS(); 
    } 
    if (wifi_avail                && mySettings.useWiFi && mySettings.useTelnet)    { 
      startUpdate = millis();
      updateTelnet();  //<<<<<<<<<<<<<<<<<<<<------------ Update Telnet -----------------------------------
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateTelnet    < deltaUpdate) { maxUpdateTelnet = deltaUpdate; }
      if (AllmaxUpdateTelnet < deltaUpdate) { AllmaxUpdateTelnet = deltaUpdate; }
      yieldTime += yieldOS(); 
    }
    if (wifi_avail                && mySettings.useWiFi && mySettings.useWeather)    { 
      startUpdate = millis();
      updateWeather();  //<<<<<<<<<<<<<<<<<<<<------------ Update Telnet -----------------------------------
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateWeather    < deltaUpdate) { maxUpdateWeather = deltaUpdate; }
      if (AllmaxUpdateWeather < deltaUpdate) { AllmaxUpdateWeather = deltaUpdate; }
      yieldTime += yieldOS(); 
    }
    
    /**********************************************************************************************************************************/
    // Update Sensor Readings
    /**********************************************************************************************************************************/
    
    if (scd30_avail    && mySettings.useSCD30) { 
      D_printSerialTelnet(F("D:U:SCD30.."));
      startUpdate = millis();
      if (updateSCD30()  == false) {scheduleReboot = true;}  //<<<<<<<<<<<<<<<<<< SCD30 Sensirion CO2 sensor
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateSCD30    < deltaUpdate)  { maxUpdateSCD30 = deltaUpdate; }
      if (AllmaxUpdateSCD30 < deltaUpdate)  { AllmaxUpdateSCD30 = deltaUpdate; }
      yieldTime += yieldOS(); 
    } 
    if (sgp30_avail    && mySettings.useSGP30) { 
      D_printSerialTelnet(F("D:U:SGP30.."));
      startUpdate = millis();
      if (updateSGP30()  == false) {scheduleReboot = true;}  //<<<<<<<<<<<<<<<<< SGP30 Sensirion eCO2 sensor
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateSGP30 < deltaUpdate)     { maxUpdateSGP30 = deltaUpdate; }
      if (AllmaxUpdateSGP30 < deltaUpdate)  { AllmaxUpdateSGP30 = deltaUpdate; }
      yieldTime += yieldOS(); 
    } 
    if (ccs811_avail   && mySettings.useCCS811) { 
      D_printSerialTelnet(F("D:U:CCS811.."));
      startUpdate = millis();
      if (updateCCS811() == false) {scheduleReboot = true;}  //<<<<<<<<<<<<<<<<<<<<------ CCS811 eCO2 sensor
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateCCS811    < deltaUpdate) { maxUpdateCCS811 = deltaUpdate; }
      if (AllmaxUpdateCCS811 < deltaUpdate) { AllmaxUpdateCCS811 = deltaUpdate; }
      yieldTime += yieldOS(); 
    }
    if (sps30_avail    && mySettings.useSPS30) { 
      D_printSerialTelnet(F("D:U:SPS30.."));
      startUpdate = millis();
      if (updateSPS30()  == false) {scheduleReboot = true;}  //<<<<<<<<<<<<< SPS30 Sensirion Particle sensor
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateSPS30    < deltaUpdate)  { maxUpdateSPS30 = deltaUpdate; }
      if (AllmaxUpdateSPS30 < deltaUpdate)  { AllmaxUpdateSPS30 = deltaUpdate; }
      yieldTime += yieldOS(); 
    } 
    if (bme280_avail   && mySettings.useBME280) { 
      D_printSerialTelnet(F("D:U:BME280.."));
      startUpdate = millis();
      if (updateBME280() == false) {scheduleReboot = true;}  //<<<<<<<<<<<<<<<<<<<< BME280, Hum, Press, Temp
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateBME280    < deltaUpdate) { maxUpdateBME280 = deltaUpdate; }
      if (AllmaxUpdateBME280 < deltaUpdate) { AllmaxUpdateBME280 = deltaUpdate; }
      yieldTime += yieldOS(); 
    } 
    if (bme68x_avail   && mySettings.useBME68x) { 
      D_printSerialTelnet(F("D:U:BME68x.."));
      startUpdate = millis();
      if (updateBME68x() == false) {scheduleReboot = true;}  //<<<<< BME68x, Hum, Press, Temp, Gasresistance
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateBME68x    < deltaUpdate) { maxUpdateBME68x = deltaUpdate; }
      if (AllmaxUpdateBME68x < deltaUpdate) { AllmaxUpdateBME68x = deltaUpdate; }      
      yieldTime += yieldOS(); 
    }
    if (therm_avail    && mySettings.useMLX) { 
      D_printSerialTelnet(F("D:U:THERM.."));
      startUpdate = millis();
      if (updateMLX()    == false) {scheduleReboot = true;}  //<<<<<<<<<<<<<< MLX Contactless Thermal Sensor
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateMLX    < deltaUpdate)    { maxUpdateMLX = deltaUpdate; }      
      if (AllmaxUpdateMLX < deltaUpdate)    { AllmaxUpdateMLX = deltaUpdate; }      
      yieldTime += yieldOS(); 
    } 
    if (max30_avail      && mySettings.useMAX30) {
      D_printSerialTelnet(F("D:U:MAX30.."));
      startUpdate = millis();
      // goes here // <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<< MAX Pulse Ox Sensor      
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateMAX    < deltaUpdate)    { maxUpdateMAX = deltaUpdate; } 
      if (AllmaxUpdateMAX < deltaUpdate)    { AllmaxUpdateMAX = deltaUpdate; }
      yieldTime += yieldOS(); 
    } 
    
    /**********************************************************************************************************************************/
    // Broadcast MQTT and WebSocket Messages
    /**********************************************************************************************************************************/
    
    if (mqtt_connected) {
      D_printSerialTelnet(F("D:U:MQTT.."));
      startUpdate = millis();
      updateMQTTMessage();  //<<<<<<<<<<<<<<<<<<<<----------------------------------- MQTT send sensor data
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateMQTTMESSAGE    < deltaUpdate) { maxUpdateMQTTMESSAGE = deltaUpdate; }
      if (AllmaxUpdateMQTTMESSAGE < deltaUpdate) { AllmaxUpdateMQTTMESSAGE = deltaUpdate; }
      // yieldTime += yieldOS(); // is inside updateMQTTMessage()
    }
    if (ws_connected) { 
      D_printSerialTelnet(F("D:U:WS.."));
      startUpdate = millis();      
      updateWebSocketMessage();  //<<<<<<<<<<<<<<<<<<<<-------------------------- WebSocket send sensor data
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateWSMESSAGE    < deltaUpdate) { maxUpdateWSMESSAGE = deltaUpdate; }
      if (AllmaxUpdateWSMESSAGE < deltaUpdate) { AllmaxUpdateWSMESSAGE = deltaUpdate; }
      // yieldTime += yieldOS(); // is inside updateWebSocketMessage 
    }

    /**********************************************************************************************************************************/
    // LCD: Display Sensor Data
    /**********************************************************************************************************************************/
    
    if (lcd_avail && mySettings.useLCD) {
      if ( ((localTime->tm_sec % (intervalLCD/1000)) == 0) && ((currentTime - lastLCD) >= intervalLCD) ) {
        lastLCD = currentTime;
        D_printSerialTelnet(F("D:U:LCD.."));
        startUpdate = millis();
        int timeMinutes = localTime->tm_hour*60+localTime->tm_min;
        
        // Update the LCD screen
        // ---
        if      ( mySettings.LCDdisplayType == 1 ) { updateLCD(); }                //<<<<<<------
        else if ( mySettings.LCDdisplayType == 2 ) { updateTwoPageLCD(); }
        else if ( mySettings.LCDdisplayType == 3 ) { updateSinglePageLCD(); }
        else if ( mySettings.LCDdisplayType == 4 ) { updateSinglePageLCDwTime(); }
        else { if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("LCD display type not supported")); } } 
          
        // Do we want to blink the LCD background ?
        // ---
        if ( mySettings.useBacklight && (allGood == false) ) {
          // Sensors indicate blinking 
          // Is our time synced?
          if ((currentTime-ntp_lastSync) < 10*NTP_INTERVAL*1000) {
            // We have network synced time.
            // Do we want to blink at night or is it day time?
            if ( (mySettings.useBlinkNight == true) ||
               ( (mySettings.useBlinkNight == false) && ( (timeMinutes < mySettings.nightBegin) && (timeMinutes > mySettings.nightEnd) ) ) 
               ) { blinkLCD = true; }
            else { blinkLCD = false;} 
          } else { blinkLCD = true; } // no network time
        } else { blinkLCD = false; } // sensors in accetable range

        // Update LCD background 
        // --=-
        // If we want to blink, dont adjust the background
        //   If background is on:
        //     If we dont want background light turn it off
        //     If background is on and we want it off during the night turn it off
        //   If background is off:
        //     If we want background
        //       If we dont care about night, turn on
        //       If we care about night, turn on if its day time
  
        if (blinkLCD == false) {
          if (lastLCDInten) {
            // LCD background is ON
            // ---
            // dont want backlight at all or dont want backlight at night
            if ( (mySettings.useBacklight == false) ||       
                 ( (mySettings.useBacklightNight == false) && 
                   ( (timeMinutes >= mySettings.nightBegin) || 
                     (timeMinutes <= mySettings.nightEnd) 
                   )
                 )  
            ) { 
              // Turn LCD light off
              switchI2C(lcd_port, lcd_i2c[0], lcd_i2c[1], lcd_i2cspeed, lcd_i2cClockStretchLimit);
              #if defined(ADALCD)
              lcd.setBacklight(LOW);
              #else
              lcd.setBacklight(0);
              #endif
              lastLCDInten = false;
            }
          } else {
            // LCD background is OFF
            // ---
            // want backlight on night and day or want backlight on during day
            if ( ( (mySettings.useBacklight == true) && (mySettings.useBacklightNight == true) ) ||  
                   ( (timeMinutes < mySettings.nightBegin) && 
                     (timeMinutes > mySettings.nightEnd) 
                   )
            ) { 
              // Turn LCD light on
              switchI2C(lcd_port, lcd_i2c[0], lcd_i2c[1], lcd_i2cspeed, lcd_i2cClockStretchLimit);
              #if defined(ADALCD)
               lcd.setBacklight(HIGH);
              #else
               lcd.setBacklight(255);
              #endif
              lastLCDInten = true;
            }           
          }
          yieldTime += yieldOS(); 
        }
  
        if ( (mySettings.debuglevel > 1) && mySettings.useSerial ) { R_printSerialTelnetLogln(F("LCD updated")); }
        deltaUpdate = millis() - startUpdate;
        if (maxUpdateLCD    < deltaUpdate) { maxUpdateLCD = deltaUpdate; }
        if (AllmaxUpdateLCD < deltaUpdate) { AllmaxUpdateLCD = deltaUpdate; }
      } // end interval LCD
    } // end LCD avail
  
    /**********************************************************************************************************************************/
    // Terminal Status Display
    /**********************************************************************************************************************************/
    
    if ((currentTime - lastSYS) >= intervalSYS) {
      lastSYS = currentTime;
      
      if (mySettings.debuglevel == 99) {   // update continously
        D_printSerialTelnet(F("D:U:SYS.."));
        startUpdate = millis();
        printProfile();
        printState();        
        printSensors();
        deltaUpdate = millis() - startUpdate;
        if (maxUpdateSYS < deltaUpdate) { maxUpdateSYS = deltaUpdate; }
      } // dbg level 99
      
      // reset max values and measure them again until next display
      // AllmaxUpdate... are the runtime maxima that are not reset between each display update
      yieldTimeMin = yieldTimeMax = 0;
      myDelayMin = intervalSYS; myLoopMin = intervalSYS; myLoopMax = 0;      
      maxUpdateTime  = maxUpdateWifi = maxUpdateOTA =  maxUpdateNTP = maxUpdateMQTT = maxUpdateHTTP
                     = maxUpdateHTTPUPDATER = maxUpdatemDNS = 0;
      maxUpdateSCD30 = maxUpdateSGP30 =  maxUpdateCCS811 = maxUpdateSPS30 = maxUpdateBME280 = maxUpdateBME68x 
                     = maxUpdateMLX = maxUpdateMAX = 0;
      maxUpdateMQTTMESSAGE = maxUpdateWSMESSAGE = maxUpdateLCD = maxUpdateINPUT = maxUpdateRT = maxUpdateEEPROM 
                     = maxUpdateBASE = maxUpdateBLINK = maxUpdateREBOOT = maxUpdateALLGOOD = 0;
    }
  
    /**********************************************************************************************************************************/
    // Serial or Telnet User Input
    /**********************************************************************************************************************************/
    
    D_printSerialTelnet(F("D:U:SER.."));
    startUpdate = millis();

    // Serial input capture
    // Telnet input is captured by telnet server (no coded here )
    if (Serial.available()) {
      int bytesread = Serial.readBytesUntil('\n', serialInputBuff, sizeof(serialInputBuff)-1);  // Read from serial until newline is read or timeout exceeded
      // while (Serial.available() > 0) { Serial.read(); } // Clear remaining data in input buffer
      serialInputBuff[bytesread] = '\0';
      serialReceived = true;
      yieldTime += yieldOS(); 
    } else {
      serialReceived = false;
    }
    
    inputHandle(); // User input handling

    deltaUpdate = millis() - startUpdate;
    if (maxUpdateINPUT < deltaUpdate) { maxUpdateINPUT = deltaUpdate; }
    if (AllmaxUpdateINPUT < deltaUpdate) { AllmaxUpdateINPUT = deltaUpdate; }

    /**********************************************************************************************************************************/
    // Other Time Managed Events such as runtime, saving baseline, rebooting, blinking LCD for warning
    /**********************************************************************************************************************************/
  
    // Update runtime every minute -------------------------------------
    if ((currentTime - lastTime) >= intervalRuntime) {
      D_printSerialTelnet(F("D:U:RUNTIME.."));
      startUpdate = millis();
      mySettings.runTime = mySettings.runTime + ((currentTime - lastTime) / 1000);
      lastTime = currentTime;
      if (mySettings.debuglevel > 1) { R_printSerialTelnetLogln(F("Runtime updated")); }
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateRT    < deltaUpdate) { maxUpdateRT = deltaUpdate; }
      if (AllmaxUpdateRT < deltaUpdate) { AllmaxUpdateRT = deltaUpdate; }
    }

    // Save Configuration infrequently ---------------------------------------
    if ((currentTime - lastSaveSettings) >= intervalSettings) {
      lastSaveSettings = currentTime;
      D_printSerialTelnet(F("D:U:EEPROM.."));
      startUpdate = millis();
      EEPROM.put(eepromAddress, mySettings);
      if (EEPROM.commit()) {
        lastSaveSettings = currentTime;
        if (mySettings.debuglevel > 1) { R_printSerialTelnetLog(F("EEPROM updated")); }
      }
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateEEPROM    < deltaUpdate) { maxUpdateEEPROM = deltaUpdate; }
      if (AllmaxUpdateEEPROM < deltaUpdate) { AllmaxUpdateEEPROM = deltaUpdate; }
      yieldTime += yieldOS(); 
    }
    
    /** JSON savinge to LittelFS takes resources
    if ((currentTime - lastSaveSettingsJSON) >= intervalSettingsJSON) {
      D_printSerialTelnet(F("D:U:JSON.."));
      startUpdate = millis();
      saveConfiguration(mySettings);
      lastSaveSettingsJSON = currentTime;
      if (mySettings.debuglevel > 1) { R_printSerialTelnetLogln(F("Sensi.json updated")); }
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateJS    < deltaUpdate) { maxUpdateJS = deltaUpdate; }
      if (AllmaxUpdateJS < deltaUpdate) { AllmaxUpdateJS = deltaUpdate; }
      yieldTime += yieldOS(); 
    }
    **/

    // regularly check logfile
    // if it is open, close it, we dont close the logfile after each write
    // if its size is larger than threshold, create a back up and start new one
    if ((currentTime - lastLogFile) >= INTERVALLOGFILE) {
      lastLogFile = currentTime;
      D_printSerialTelnet(F("D:U:Log.."));
      // Check if logfile needs to be trimmed ----------------------------------
      if (logFile) {
        if (logFile.size() > LOGFILESIZE) {
          logFile.close();
          LittleFS.remove("/Sensi.bak");
          LittleFS.rename("/Sensi.log", "/Sensi.bak");
        } else {
          logFile.close(); // just close logfile
        }
        yieldTime += yieldOS();       
      }
    }

    // Obtain baseline from sensors to create internal baseline -------------------
    if ((currentTime - lastBaseline) >= intervalBaseline) {
      D_printSerialTelnet(F("D:U:BASE.."));
      startUpdate = millis();
      lastBaseline = currentTime;
      // Copy CCS811 basline to settings when warmup is finished
      if (ccs811_avail && mySettings.useCCS811) {
        if (currentTime >= warmupCCS811) {
          switchI2C(ccs811_port, ccs811_i2c[0], ccs811_i2c[1], ccs811_i2cspeed, ccs811_i2cClockStretchLimit);
          mySettings.baselineCCS811 = ccs811.getBaseline();
          mySettings.baselineCCS811_valid = 0xF0;
          if (mySettings.debuglevel > 1) { R_printSerialTelnetLog(F("CCS811 baseline placed into settings")); }
          // warmupCCS811 = warmupCCS811 + stablebaseCCS811;          
        }
      }      
      // Copy SGP30 basline to settings when warmup is finished
      if (sgp30_avail && mySettings.useSGP30) {
        if (currentTime >=  warmupSGP30) {
          mySettings.baselinetVOC_SGP30 = sgp30.baselineTVOC; // internal variable
          mySettings.baselineeCO2_SGP30 = sgp30.baselineCO2;  // internal varianle
          mySettings.baselineSGP30_valid = 0xF0;
          if (mySettings.debuglevel > 1) { R_printSerialTelnetLog(F("SGP30 baseline placed into settings")); }
          // warmupSGP30 = warmupSGP30 + intervalSGP30Baseline;
        }
      }
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateBASE    < deltaUpdate) { maxUpdateBASE = deltaUpdate; }
      if (AllmaxUpdateBASE < deltaUpdate) { AllmaxUpdateBASE = deltaUpdate; }
      yieldTime += yieldOS();
    }

    // Update AirQuality Warning -------------------------------------------------
    if ((currentTime - lastWarning) > intervalWarning) {                         // warning interval
      D_printSerialTelnet(F("D:U:AQ?.."));
      startUpdate = millis();
      lastWarning = currentTime;
      allGood = sensorsWarning(); // <<<<<<<<<<<<<<<<<<
      deltaUpdate = millis() - startUpdate;
      if (mySettings.debuglevel > 1) { R_printSerialTelnetLogln(F("AQ Warnings updated")); }
      if (maxUpdateALLGOOD    < deltaUpdate) { maxUpdateALLGOOD = deltaUpdate; }
      if (AllmaxUpdateALLGOOD < deltaUpdate) { AllmaxUpdateALLGOOD = deltaUpdate; }
      yieldTime += yieldOS();
    }
    
    // Do we want to blink LCD?  -------------------------------------------------   
    //  blinkLCD will only be ture if night and blink requirements are met
    if ((currentTime - lastBlink) > intervalBlink) {                             // blink interval
      D_printSerialTelnet(F("D:U:BLNK.."));
      startUpdate = millis();

      // blink LCD background ----------------------------------------------------
      if (blinkLCD) {
        // if (mySettings.debuglevel > 1) { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Sensors out of normal range!")); }
        switchI2C(lcd_port, lcd_i2c[0], lcd_i2c[1], lcd_i2cspeed, lcd_i2cClockStretchLimit);
        lastBlink = currentTime;
        lastLCDInten = !lastLCDInten;
        #if defined(ADALCD)
        if (lastLCDInten) {
          lcd.setBacklight(HIGH);
          intervalBlink = intervalBlinkOn; 
        } else {
          lcd.setBacklight(LOW);
          intervalBlink = intervalBlinkOff;
        }
        #else
        if (lastLCDInten) {
          lcd.setBacklight(255);
          intervalBlink = intervalBlinkOn;
        } else {
          lcd.setBacklight(0);
          intervalBlink = intervalBlinkOff;
        }
        #endif
      } // end toggle the LCD background
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateBLINK    < deltaUpdate) { maxUpdateBLINK = deltaUpdate; }       
      if (AllmaxUpdateBLINK < deltaUpdate) { AllmaxUpdateBLINK = deltaUpdate; }             
      yieldTime += yieldOS(); 
    } // end blinking interval

    // Deal with request to reboot --------------------------------------
    // This occurs if sensors reading results in error and driver fails to recover
    // Reboot at preset time
    if (scheduleReboot == true) {
      D_printSerialTelnet(F("D:U:RBT.."));
      startUpdate = millis();
      if ( (mySettings.rebootMinute>=0) && (mySettings.rebootMinute<=1440) ) { // do not reboot if rebootMinute is -1 or bigger than 24hrs
        if (timeSynced) {
          if (localTime->tm_hour*60+localTime->tm_min == mySettings.rebootMinute) {
            if (mySettings.debuglevel > 0) { R_printSerialTelnetLog(F("Rebooting...")); }
            rebootNow = true;
          }  
        } else {
          // we might not want to reboot whenever a sensor fails
          if (mySettings.debuglevel > 1) {  R_printSerialTelnetLog(F("Rebooting when time is synced...")); }
        }
      }
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateREBOOT    < deltaUpdate) { maxUpdateREBOOT = deltaUpdate; }
      if (AllmaxUpdateREBOOT < deltaUpdate) { AllmaxUpdateREBOOT = deltaUpdate; }
    }

    // Reboot has been requested by device ERROR or by user input
    // Check if devices are in appropriate state, otherwise try again later
    if (rebootNow) {
      bool rebootok = true;
      if (bme280_avail   && mySettings.useBME280){ if  ( (stateBME280 == DATA_AVAILABLE) || (stateBME280 == IS_BUSY)        ) {rebootok = false;} }
      if (bme68x_avail   && mySettings.useBME68x){ if  ( (stateBME68x == DATA_AVAILABLE) || (stateBME68x == IS_BUSY)        ) {rebootok = false;} }
      if (ccs811_avail   && mySettings.useCCS811){ if  ( (stateCCS811 == IS_WAKINGUP)    || (stateCCS811 == DATA_AVAILABLE) ) {rebootok = false;} }
      // if (therm_avail && mySettings.useMLX)   { }
      if (scd30_avail    && mySettings.useSCD30) { if  ( (stateSCD30  == DATA_AVAILABLE) || (stateSCD30  == IS_BUSY)        ) {rebootok = false;} }
      // if (sgp30_avail && mySettings.useSGP30) { }
      if (sps30_avail    && mySettings.useSPS30) { if  ( (stateSPS30  == IS_WAKINGUP)    || (stateSPS30  == IS_BUSY)        ) {rebootok = false;} }
      if (max30_avail    && mySettings.useMAX30) { }
      if (rebootok) {
        R_printSerialTelnetLog(F("Bye ..."));
        Serial.flush();
        logFile.close();
        ESP.reset();
      }
    }
    
    /**********************************************************************************************************************************/
    // Keep track of free processor time 
    /**********************************************************************************************************************************/
    
    myLoop  = millis() - currentTime;
    myLoopAvg = 0.9 * myLoopAvg + 0.1 * float(myLoop);
    if ( myLoop > 0 ) { 
      if ( myLoop < myLoopMin ) { myLoopMin = myLoop; }
      if ( myLoop > myLoopMax ) { 
        myLoopMax = myLoop; 
        if ( myLoop > myLoopMaxAllTime)  { myLoopMaxAllTime = myLoop; }
      }
    }
    if (myLoopMax > 20) {
      myLoopMaxAvg = 0.9 * myLoopMaxAvg + 0.1 * float(myLoopMax);
    }  
    if ( yieldTime > 0 ) { 
      if ( yieldTime < yieldTimeMin ) { yieldTimeMin = yieldTime; }
      if ( yieldTime > yieldTimeMax ) {
        yieldTimeMax = yieldTime; 
        if ( yieldTime > yieldTimeMaxAllTime)  { yieldTimeMaxAllTime = yieldTime; }
      }
      yieldTime = 0;
    }
    
  } // end OTA not in progress
} // end loop

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Support functions
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

// yield when necessary, there should be a yield/delay every 100ms
unsigned long yieldOS() {
  unsigned long beginYieldOS = millis(); // measure how long it takes to complete yield
  if ( (beginYieldOS - lastYield) > MAXUNTILYIELD ) { // only yield if sufficient time expired since previous yield
    yieldFunction(); // replaced with define statement at beginning of the this document
    lastYield = millis();
    return (lastYield - beginYieldOS);
  }
  return (millis()-beginYieldOS);
}

// Scan the I2C bus whether device is attached at address
bool checkI2C(uint8_t address, TwoWire *myWire) {
  myWire->beginTransmission(address);
  if (myWire->endTransmission() == 0) { return true; } else { return false; }
}

// Switches I2C port for ESP8266
// # if defined(ESP8266)
void switchI2C(TwoWire *myPort, int sdaPin, int sclPin, uint32_t i2cSpeed, uint32_t i2cStretch) {
  myPort->begin(sdaPin, sclPin);
  myPort->setClock(i2cSpeed);
  myPort->setClockStretchLimit(i2cStretch);
  yieldI2C(); // replaced with function defined at beginning of this document
}

// Boot helper
// Prints message and waits until timeout or user send character on serial terminal
void serialTrigger(const char* mess, int timeout) {
  unsigned long startTime = millis();
  bool clearSerial = true;
  Serial.println(); Serial.println(mess);
  while ( !Serial.available() && (millis() - startTime < timeout) ) {
    delay(1000);
  }
  while (Serial.available()) { Serial.read(); }
}

/**************************************************************************************/
// JSON: Support Routines, JSON for the senosors is in their respective code segments
/**************************************************************************************/
// Member	  Type	Meaning	Range
// tm_sec	  int	  seconds after the minute	0-61*
// tm_min	  int	  minutes after the hour	  0-59
// tm_hour	int	  hours since midnight	    0-23
// tm_mday	int	  day of the month	        1-31
// tm_mon	  int	  months since January	    0-11
// tm_year	int	  years since 1900	
// tm_wday	int	  days since Sunday	        0-6
// tm_yday	int	  days since January 1	    0-365
// tm_isdst	int	  Daylight Saving Time flag	

void timeJSON(char *payLoad, size_t len) {
  const char * str = "{ \"time\": ";
  size_t l = strlen(str);
  strlcpy(payLoad, str, l+1);
  timeJSONMQTT(payLoad+l, len-l-1);
  strlcat(payLoad, "}", len);
}

void timeJSONMQTT(char *payLoad, size_t len) {
  timeval tv;
  gettimeofday (&tv, NULL);
  snprintf_P(payLoad, len, PSTR("{ \"hour\": %d, \"minute\": %d, \"second\": %d, \"microsecond\": %06ld }"),
  localTime->tm_hour,
  localTime->tm_min,
  localTime->tm_sec,
  tv.tv_usec); 
}

void dateJSON(char *payLoad, size_t len){
  const char * str = "{ \"date\": ";
  size_t l = strlen(str);
  strlcpy(payLoad, str, l+1);
  dateJSONMQTT(payLoad+l, len-l-1);
  strlcat(payLoad, "}", len);
}

void dateJSONMQTT(char *payLoad, size_t len) {
  snprintf_P(payLoad, len, PSTR("{ \"day\": %d, \"month\": %d, \"year\": %d }"),
  localTime->tm_mday,
  localTime->tm_mon+1,
  localTime->tm_year+1900); 
}

void systemJSON(char *payLoad, size_t len){
  const char * str = "{ \"system\": ";
  size_t l = strlen(str);
  strlcpy(payLoad, str, l+1);
  systemJSONMQTT(payLoad+l, len-l-1);
  strlcat(payLoad, "}", len);
}

void systemJSONMQTT(char *payLoad, size_t len) {
  snprintf_P(payLoad, len, PSTR("{\"freeheap\": %d, \"heapfragmentation\": %d, \"maxfreeblock\": %d, \"maxlooptimeavg\": %d, \"maxlooptime\": %d }"),
  ESP.getFreeHeap(),
  ESP.getHeapFragmentation(),      
  ESP.getMaxFreeBlockSize(),
  (int)myLoopMaxAvg,
  (int)myLoopMax);
}

/**************************************************************************************/
// Terminal Input: Support Routines
/**************************************************************************************/

bool inputHandle() {
  unsigned long tmpuI;                  // unsinged int
  long          tmpI;                   // int
  float         tmpF;                   // float
  bool          processSerial = false;  // 
  bool          processTelnet = false;  //
  char          command[2];             // one character command
  char          value[64];              // text following command
  char          text[64];               // text variable
  int           textlen;                // length of text variable    
  char         *pEnd;                   // strip line termination
  
  if (serialReceived) {
    // lastTmp = millis();
    // Commands are either sent with newline or with cariage return/newline combination as termination.
    // If we receive cariage return '\r' we need to strip it from the input buffer
    pEnd = strchr (serialInputBuff, '\r');  // search for carriage return
    if (pEnd) { *pEnd = '\0'; }  // if found truncate
    // Disabled serial input rate so that we can upload into a file
    // if ((lastTmp - lastSerialInput) > SERIALMAXRATE) { // limit command input rate to protect from accidentally pasted text
    //   lastSerialInput = lastTmp;
    if ( strlen(serialInputBuff) > 0) {processSerial = true;} else {processSerial = false;}
    // } else { processSerial = false; }
    //R_printSerialTelnetLog(F("Serial: ")); R_printSerialTelnetLogln(serialInputBuff);
  } 
  if (telnetReceived) {
    // lastTmp = millis();
    pEnd = strchr (telnetInputBuff, '\r');  // search for carriage return
    if (pEnd) { *pEnd = '\0'; }   // if found truncate
    // Disabled telnet input rate so that we can upload into file
    // if ((lastTmp - lastTelnetInput) > SERIALMAXRATE) { // limit command input rate to protect from accidentally pasted text
    //  lastTelnetInput = lastTmp;
    if ( strlen(telnetInputBuff) > 0) {processTelnet = true;} else {processTelnet = false;}
    //} else { processTelnet = false; }
    telnetReceived = false; // somewhere we need to turn telnet received notification off
    //R_printSerialTelnetLog(F("Telnet: ")); R_printSerialTelnetLogln(telnetInputBuff);
  }

  // extract command and value
  if (processSerial || processTelnet) { 
    if (processTelnet) {
      if (strlen(telnetInputBuff) > 1) { 
        if (telnetInputBuff[0] == 'Z' && telnetInputBuff[1] == 'Z') {
          if (strlen(telnetInputBuff) > 2) { strncpy(command, telnetInputBuff+2, 1); command[1]='\0'; }  // no null char appended when using strncpy
          if (strlen(telnetInputBuff) > 3) { 
            strlcpy(text, telnetInputBuff+3, sizeof(text)); 
            textlen = strlen(text);
          } else {
            text[0] = '\0';
            textlen = 0;
          } 
        } else {
          helpMenu();
          return false;
        }
      } else {
        helpMenu();
        return false;
      }
    }
    if (processSerial) {
      if (strlen(serialInputBuff) > 1) { 
        if (serialInputBuff[0] == 'Z' && serialInputBuff[1] == 'Z') {
          if (strlen(serialInputBuff) > 2) { strncpy(command, serialInputBuff+2, 1); command[1]='\0'; } // no null char appended when using strncpy
          if (strlen(serialInputBuff) > 3) { 
            strlcpy(text, serialInputBuff+3, sizeof(text)); 
            textlen = strlen(text);
          } else {
            text[0] = '\0';
            textlen = 0;
          }
        } else {
          R_printSerialTelnetLogln(F("Commands need to be preceeded by ZZ"));
          helpMenu();
          return false; }
      } else {
        helpMenu();
        return false;
      }
    }

    // If issues with processing commands show the received text
    // R_printSerialTelnetLog(F("Command: ")); R_printSerialTelnetLogln(command);
    // R_printSerialTelnetLog(F("Text: "));   R_printSerialTelnetLogln(text);
    // snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Text length : %u"),strlen(text)); 
    // R_printSerialTelnetLogln(tmpStr);

    ///////////////////////////////////////////////////////////////////
    // General Commands
    ///////////////////////////////////////////////////////////////////

    if (command[0] == 'z') {                                                 // dump all sensor data
      printSensors();
    }

    else if (command[0] == '.') {                                            // print a clear execution times of subroutines
      tmpTime = millis();
      printProfile();
      deltaUpdate = millis() - tmpTime;
      if (maxUpdateSYS < deltaUpdate) { maxUpdateSYS = deltaUpdate; }
    }

    else if (command[0] =='?' || command[0] =='h') {                         // help requested
      helpMenu(); // no yield needed
    }

    else if (command[0] == 'n') {                                            // MQTT main topic name (device Name)
      if (textlen > 0) { // we have a value
        strlcpy(mySettings.mqtt_mainTopic, text, sizeof(mySettings.mqtt_mainTopic));
      }
      snprintf_P(tmpStr, sizeof(tmpStr), PSTR("MQTT mainTopic is: %s"),mySettings.mqtt_mainTopic); 
      R_printSerialTelnetLogln(tmpStr);
      yieldTime += yieldOS(); 
    }

    else if (command[0] == 'j') {                                            // test json output
      char payLoad[256]; // this will cover all possible payload sizes
      timeJSON(payLoad, sizeof(payLoad));   R_printSerialTelnetLog(payLoad); 
      snprintf_P(tmpStr, sizeof(tmpStr), PSTR(" len: %u"), strlen(payLoad));   printSerialTelnetLogln(tmpStr);
      yieldTime += yieldOS(); 
      dateJSON(payLoad, sizeof(payLoad));     printSerialTelnetLog(payLoad); 
      snprintf_P(tmpStr, sizeof(tmpStr), PSTR(" len: %u"), strlen(payLoad));   printSerialTelnetLogln(tmpStr);
      yieldTime += yieldOS(); 
      ipJSON(payLoad, sizeof(payLoad));       printSerialTelnetLog(payLoad); 
      snprintf_P(tmpStr, sizeof(tmpStr), PSTR(" len: %u"), strlen(payLoad));   printSerialTelnetLogln(tmpStr);
      yieldTime += yieldOS(); 
      hostnameJSON(payLoad, sizeof(payLoad)); printSerialTelnetLog(payLoad); 
      snprintf_P(tmpStr, sizeof(tmpStr), PSTR(" len: %u"), strlen(payLoad));   printSerialTelnetLogln(tmpStr);
      yieldTime += yieldOS(); 
      systemJSON(payLoad, sizeof(payLoad));   printSerialTelnetLog(payLoad); 
      snprintf_P(tmpStr, sizeof(tmpStr), PSTR(" len: %u"), strlen(payLoad));   printSerialTelnetLogln(tmpStr);
      yieldTime += yieldOS(); 
      wifiJSON(payLoad, sizeof(payLoad));   printSerialTelnetLog(payLoad); 
      snprintf_P(tmpStr, sizeof(tmpStr), PSTR(" len: %u"), strlen(payLoad));   printSerialTelnetLogln(tmpStr);
      yieldTime += yieldOS(); 
      bme280JSON(payLoad, sizeof(payLoad));   printSerialTelnetLog(payLoad); 
      snprintf_P(tmpStr, sizeof(tmpStr), PSTR(" len: %u"), strlen(payLoad));   printSerialTelnetLogln(tmpStr);
      yieldTime += yieldOS(); 
      bme68xJSON(payLoad, sizeof(payLoad));   printSerialTelnetLog(payLoad); 
      snprintf_P(tmpStr, sizeof(tmpStr), PSTR(" len: %u"), strlen(payLoad));   printSerialTelnetLogln(tmpStr);
      yieldTime += yieldOS(); 
      ccs811JSON(payLoad, sizeof(payLoad));   printSerialTelnetLog(payLoad); 
      snprintf_P(tmpStr, sizeof(tmpStr), PSTR(" len: %u"), strlen(payLoad));   printSerialTelnetLogln(tmpStr);
      yieldTime += yieldOS(); 
      mlxJSON(payLoad, sizeof(payLoad));      printSerialTelnetLog(payLoad); 
      snprintf_P(tmpStr, sizeof(tmpStr), PSTR(" len: %u"), strlen(payLoad));   printSerialTelnetLogln(tmpStr);
      yieldTime += yieldOS();
      scd30JSON(payLoad, sizeof(payLoad));    printSerialTelnetLog(payLoad);
      snprintf_P(tmpStr, sizeof(tmpStr), PSTR(" len: %u"), strlen(payLoad));   printSerialTelnetLogln(tmpStr);
      yieldTime += yieldOS(); 
      sgp30JSON(payLoad, sizeof(payLoad));    printSerialTelnetLog(payLoad); 
      snprintf_P(tmpStr, sizeof(tmpStr), PSTR(" len: %u"), strlen(payLoad));   printSerialTelnetLogln(tmpStr);
      yieldTime += yieldOS(); 
      sps30JSON(payLoad, sizeof(payLoad));    printSerialTelnetLog(payLoad); 
      snprintf_P(tmpStr, sizeof(tmpStr), PSTR(" len: %u"), strlen(payLoad));   printSerialTelnetLogln(tmpStr);
      yieldTime += yieldOS(); 
      max30JSON(payLoad, sizeof(payLoad));    printSerialTelnetLog(payLoad); 
      snprintf_P(tmpStr, sizeof(tmpStr), PSTR(" len: %u"), strlen(payLoad));   printSerialTelnetLogln(tmpStr);
      yieldTime += yieldOS(); 
      weatherJSON(payLoad, sizeof(payLoad));    printSerialTelnetLog(payLoad); 
      snprintf_P(tmpStr, sizeof(tmpStr), PSTR(" len: %u"), strlen(payLoad));   printSerialTelnetLogln(tmpStr);
      yieldTime += yieldOS(); 
    }

    // Should produce something like:
    //{"time":{"hour":0,"minute":0,"second":15}} len: 42
    //{"date":{"day":1,"month":1,"year":1970}} len: 40
    //{"bme280":{"avail":false,"p": 0.0,"pavg": 0.0,"rH": 0.0,"aH": 0.0,"T":  0.0,"dp_airquality":"Normal","rH_airquality":"Excessive","T_airquality":"Cold"}} len: 152
    //{"bme68x":{"avail":true,"p":927.4,"pavg":927.4,"rH":32.3,"aH": 8.3,"T": 26.6,"resistance":0,"dp_airquality":"Normal","rH_airquality":"Threshold Low","resistance_airquality":"?","T_airquality":"Hot"}} len: 199
    //{"ccs811":{"avail":true,"eCO2":0,"tVOC":0,"eCO2_airquality":"Normal","tVOC_airquality":"Normal"}} len: 97
    //{"mlx":{"avail":true,"To": 26.4,"Ta": 27.0,"fever":"Low ","T_airquality":"Hot"}} len: 80
    //{"scd30":{"avail":true,"CO2":0,"rH":-1.0,"aH":-1.0,"T":-999.0,"CO2_airquality":"Normal","rH_airquality":"?","T_airquality":"?"}} len: 128
    //{"sgp30":{"avail":true,"eCO2":400,"tVOC":0,"eCO2_airquality":"Normal","tVOC_airquality":"Normal"}} len: 98
    //{"sps30":{"avail":false,"PM1": 0.0,"PM2": 0.0,"PM4": 0.0,"PM10": 0.0,"nPM0": 0.0,"nPM1": 0.0,"nPM2": 0.0,"nPM4": 0.0,"nPM10": 0.0,"PartSize": 0.0,"PM2_airquality":"Normal","PM10_airquality":"Normal"}} len: 200
    // max30, weather ...

    ///////////////////////////////////////////////////////////////////
    // Network Commands
    ///////////////////////////////////////////////////////////////////

    else if (command[0] == 'W') {                                          // SSIDs, passwords, servers      
      if (textlen > 0) { // we have extended command, otherwise we want to know WiFi status and intervals
        if (textlen >= 2) { // SSID or password was given
          if (textlen > 2) { strlcpy(value,text+2,sizeof(value)); } else { value[0] = '\0'; }// ssid or password was provided

          if (text[0] == 's') {                                           // set SSID
            if (text[1] == '1' ) {
              if (textlen == 2) { mySettings.ssid1[0] = '\0'; } // empty ssid
              else { strlcpy(mySettings.ssid1, value, sizeof(mySettings.ssid1)); }
              snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SSID 1 is: %s"), mySettings.ssid1); 
            }
            else if (text[1] == '2' ) {
              if (textlen == 2) { mySettings.ssid2[0] = '\0'; }
              else { strlcpy(mySettings.ssid2, value, sizeof(mySettings.ssid2)); }
              snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SSID 2 is: %s"), mySettings.ssid2); 
            }
            else if (text[1] == '3' ) {
              if (textlen == 0) { mySettings.ssid3[0] = '\0'; }
              else { strlcpy(mySettings.ssid3, text, sizeof(mySettings.ssid3)); }
              snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SSID 3 is: %s"), mySettings.ssid3); 
            }
            else { strcpy_P(tmpStr, PSTR("SSID # must be 1..3")); }
            R_printSerialTelnetLogln(tmpStr);
            yieldTime += yieldOS(); 
          
          } else if (text[0] == 'p') {                                    // set password
            if      (text[1] == '1' ) {
              if (textlen == 2) { mySettings.pw1[0] = '\0'; } // empty ssid
              else { strlcpy(mySettings.pw1, value, sizeof(mySettings.pw1)); }
              snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Password 1 is: %s"), mySettings.pw1); 
            }
            else if (text[1] == '2' ) {
              if (textlen == 2) { mySettings.pw2[0] = '\0'; } // empty ssid
              else { strlcpy(mySettings.pw2, value, sizeof(mySettings.pw2)); }
              snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Password 2 is: %s"), mySettings.pw2); 
            }
            else if (text[1] == '3' ) {
              if (textlen == 2) { mySettings.pw3[0] = '\0'; } // empty ssid
              else { strlcpy(mySettings.pw3, value, sizeof(mySettings.pw3)); }
              snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Password 3 is: %s"), mySettings.pw3); 
            }
            else{ strcpy_P(tmpStr, PSTR("Password # must be 1..3")); }
            R_printSerialTelnetLogln(tmpStr);
            yieldTime += yieldOS(); 
          }
        } else { helpMenu(); }
      } else { printState(); }
    }

    ///////////////////////////////////////////////////////////////////
    // Network MQTT Commands
    ///////////////////////////////////////////////////////////////////

    else if (command[0] == 'M') {
      if (textlen > 0) { // subcommand was given
        if (textlen > 1) { strlcpy(value,text+1,sizeof(value)); }  else { value[0] = '\0'; } // username, password, servers, interval was provided
        
        if        (text[0] == 'u') {                                      // username
          if (textlen == 1) { mySettings.mqtt_username[0] = '\0'; } // empty mqtt username
          else { strlcpy(mySettings.mqtt_username, value, sizeof(mySettings.mqtt_username)); }
          snprintf_P(tmpStr, sizeof(tmpStr), PSTR("MQTT username is: %s"), mySettings.mqtt_username); 
        
        } else if (text[0] == 'p') {                                      // password
          if (textlen == 1) { mySettings.mqtt_password[0] = '\0'; } // empty mqtt password
          else { strlcpy(mySettings.mqtt_password, value, sizeof(mySettings.mqtt_password)); }
          snprintf_P(tmpStr, sizeof(tmpStr), PSTR("MQTT password is: %s"), mySettings.mqtt_password); 
        
        } else if (text[0] == 's') {                                      // mqtt server
          if (textlen == 1) { mySettings.mqtt_server[0] = '\0'; } // empty server
          else { strlcpy(mySettings.mqtt_server, value, sizeof(mySettings.mqtt_server)); }
          snprintf_P(tmpStr, sizeof(tmpStr), PSTR("MQTT server is: %s"), mySettings.mqtt_server); 

        } else if (text[0] == 'f') {                                      // mqtt server failover
          if (textlen == 1) { mySettings.mqtt_fallback[0] = '\0'; } // empty fallback
          else { strlcpy(mySettings.mqtt_fallback, value, sizeof(mySettings.mqtt_fallback)); }
          snprintf_P(tmpStr, sizeof(tmpStr), PSTR("MQTT fallback server is: %s"), mySettings.mqtt_fallback); 
        
        } else if (text[0] == 'i') {                                      // mqtt interval
          if (strlen(value) > 0) { tmpF = strtof(value, NULL);  } // no interval given
          else { tmpF= 1.0; }
          if ((tmpF >= 0.0) && (tmpF <= 3600.0)) {
            mySettings.intervalMQTT = (float) tmpF;
            intervalMQTT = mySettings.intervalMQTT * 1000;                   // time is provided in seconds, timer runs in milliSeconds
            intervalNewData = true;
            snprintf_P(tmpStr, sizeof(tmpStr), PSTR("MQTT interval set to: %d ms"),intervalMQTT);
          } else { strcpy_P(tmpStr, PSTR("MQTT out of valid range")); }
          
        } else if (text[0] == 'm') {                                      // mqtt send mode
          mySettings.sendMQTTimmediate = !bool(mySettings.sendMQTTimmediate);
          snprintf_P(tmpStr, sizeof(tmpStr), PSTR("MQTT is sent immediatly: %s"), mySettings.sendMQTTimmediate?FPSTR(mOFF):FPSTR(mON)); 

        } else { strcpy_P(tmpStr, PSTR("No valid command provided")); }
        R_printSerialTelnetLogln(tmpStr);
        yieldTime += yieldOS(); 
      } else { helpMenu(); }
    }

    ///////////////////////////////////////////////////////////////////
    // Network NTP Commands
    ///////////////////////////////////////////////////////////////////
  
    else if (command[0] == 'N') {
      if (textlen > 0) { // subcommand was given
        if (textlen > 1) { strlcpy(value,text+1,sizeof(value)); }  else { value[0] = '\0'; } // username, password, servers, interval was provided
        
        if        (text[0] == 's') {                                      // NTP server
          if (textlen <= 1) { mySettings.ntpServer[0] = '\0'; } // 
          else { strlcpy(mySettings.ntpServer, value, sizeof(mySettings.ntpServer)); }
          snprintf_P(tmpStr, sizeof(tmpStr), PSTR("NTP server is: %s"), mySettings.ntpServer); 
          stateNTP = START_UP;

        } else if (text[0] == 'f') {                                      // NTP fallback
          if (textlen == 1) { mySettings.ntpFallback[0] = '\0'; } // 
          else { strlcpy(mySettings.ntpFallback, value, sizeof(mySettings.ntpFallback)); }
          snprintf_P(tmpStr, sizeof(tmpStr), PSTR("NTP fallback server is: %s"), mySettings.ntpFallback); 
          stateNTP = START_UP;
        
        } else if (text[0] == 't') {                                      // time zone
          if (textlen <= 1) { strcpy_P(mySettings.timeZone, PSTR("GMT0")); } // 
          else              { strlcpy(mySettings.timeZone,  value, sizeof(mySettings.timeZone)); }
          NTP.setTimeZone(mySettings.timeZone);
          snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Time Zone is: %s"), mySettings.timeZone); 
        
        } else if (text[0] == 'n') {                                      // night start
          if (strlen(value) > 0) { tmpuI = strtoul(value, NULL, 10); } 
          else { tmpuI = 22*60;  }
          if ((tmpuI >= 0) && (tmpuI <= 1440) && (tmpuI >= mySettings.nightEnd)) {
            mySettings.nightBegin = (uint16_t)tmpuI;
            snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Night begin set to: %u after midnight"), mySettings.nightBegin); 
          } else { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Night start out of valid range %u - 1440 minutes after midnight"), mySettings.nightEnd ); }
        
        } else if (text[0] == 'e') {                                      // night end
          if (strlen(value) > 0) { tmpuI = strtoul(value, NULL, 10); } 
          else { tmpuI = 6*60; }
          if ((tmpuI >= 0) && (tmpuI <= 1440) && (tmpuI <= mySettings.nightBegin)) {
            mySettings.nightEnd = (uint16_t)tmpuI;
            snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Night end set to: %u minutes after midnight"), mySettings.nightEnd); 
          } else { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Night end out of valid range 0 - %u minutes after midnight"), mySettings.nightBegin); }
          
        } else if (text[0] == 'r') {                                      // reboot time
          if (strlen(value) > 0) { tmpI = strtoul(value, NULL, 10); } 
          else { tmpI = 2*60; }
          if ((tmpI >= -1) && (tmpI <= 1440)) {
            mySettings.rebootMinute = (int16_t)tmpI;
            snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Reboot minute set to: %d minutes after midnight"), mySettings.rebootMinute); 
          } else { strcpy_P(tmpStr, PSTR("Reboot time out of valid range -1(off) - 1440 minutes after midnight")); }

        } else { strcpy_P(tmpStr, PSTR("No valid command provided")); }
        R_printSerialTelnetLogln(tmpStr);
        yieldTime += yieldOS(); 
      } else { helpMenu(); }
    }

    ///////////////////////////////////////////////////////////////////
    // Network Weather Commands
    ///////////////////////////////////////////////////////////////////

    else if (command[0] == 'R') {
      if (textlen > 0) { // subcommand was given
        if (textlen > 1) { strlcpy(value,text+1,sizeof(value)); } else { value[0] = '\0'; }

        if        (text[0] == 'k') {                                      // api key
          if (textlen == 1) { mySettings.weatherApiKey[0]= '\0'; } 
          else { strlcpy(mySettings.weatherApiKey, value, sizeof(mySettings.weatherApiKey)); }
          snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Api Key is: %s"), mySettings.weatherApiKey); 

        } else if (text[0] == 'c') {                                      // city
          if (textlen == 1) { mySettings.weatherCity[0] = '\0'; }
          else { strlcpy(mySettings.weatherCity, value, sizeof(mySettings.weatherCity)); }
          snprintf_P(tmpStr, sizeof(tmpStr), PSTR("City is: %s"), mySettings.weatherCity); 

        } else if (text[0] == 'l') {                                      // country
          if (textlen == 1) { mySettings.weatherCountryCode[0] = '\0'; }
          else { strlcpy(mySettings.weatherCountryCode, value, sizeof(mySettings.weatherCountryCode)); }
          snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Country is: %s"), mySettings.weatherCountryCode); 

        } else if (text[0] == 'a') {                                      // altitude
          if (strlen(value) > 0) { 
            tmpF = strtof(value,NULL);
            if ((tmpF >= 0.) && (tmpF <= 9000.) ) {
              mySettings.altitude = (float)tmpF;
              snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Altitude set to: %f [m]"), mySettings.altitude); 
            } else { strcpy_P(tmpStr, PSTR("Altitude out of valid range [m]")); }
          } else { strcpy_P(tmpStr, PSTR("No altitude provided")); }
        
        } else if (text[0] == 'p') {                                      // average pressure
          if (strlen(value) > 0) { 
            tmpF = strtof(value, NULL); 
            if ((tmpF >= 0.) && (tmpF <= 2000.0)) {
              mySettings.avgP      = tmpF*100.;
              bme68x_pressure24hrs = tmpF*100.;
              bme280_pressure24hrs = tmpF*100.;
              snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Average pressure set to: %f [mbar]"), mySettings.avgP/100.); 
            } else { strcpy_P(tmpStr, PSTR("Average pressure out of valid range [mbar]")); }
          } else { strcpy_P(tmpStr, PSTR("No average pressure provided")); }
          
        } else { strcpy_P(tmpStr, PSTR("No valid command provided")); }
        R_printSerialTelnetLogln(tmpStr);
        yieldTime += yieldOS(); 
      } else { helpMenu(); }
    }

    ///////////////////////////////////////////////////////////////////
    // SGP30
    ///////////////////////////////////////////////////////////////////

    else if (command[0] == 'P') {
      if (textlen > 0) { // subcommand was given
        if (textlen > 1) { strlcpy(value,text+1,sizeof(value)); }  else { value[0] = '\0'; } // value was given

        if        (text[0] == 'c') {                                      // force CO2
          if (strlen(value) > 0) {
            tmpuI = strtoul(value, NULL, 10);
            if ((tmpuI >= 0) && (tmpuI <= 65535)) {
              if (sgp30_avail && mySettings.useSGP30) {
                switchI2C(sgp30_port, sgp30_i2c[0], sgp30_i2c[1], sgp30_i2cspeed, sgp30_i2cClockStretchLimit);
                sgp30.getBaseline();                                                         // read both eCO2 and tVOC baselines
                mySettings.baselinetVOC_SGP30 = sgp30.baselineTVOC;                          // dont change tVOC
                sgp30.setBaseline((uint16_t)tmpuI, (uint16_t)mySettings.baselinetVOC_SGP30); // set baseline
                mySettings.baselineSGP30_valid = 0xF0;
                mySettings.baselineeCO2_SGP30 = (uint16_t)tmpuI;
                snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SGP30 calibration point is: %u"), tmpuI); 
              } else { strcpy_P(tmpStr, PSTR("SGP30 not available")); }
            } else { strcpy_P(tmpStr, PSTR("Provided baseline for eCO2 is out of range")); }
          } else { strcpy_P(tmpStr, PSTR("SGP30 no baseline provided")); }
          
        } else if (text[0] == 'v') {                                      // force tVOC
          if (strlen(value) > 0) {
            tmpuI = strtoul(value, NULL, 10);
            if ((tmpuI >= 0) && (tmpuI <= 65535)) {
              if (sgp30_avail && mySettings.useSGP30) {
                switchI2C(sgp30_port, sgp30_i2c[0], sgp30_i2c[1], sgp30_i2cspeed, sgp30_i2cClockStretchLimit);
                sgp30.getBaseline();                                                         // read both eCO2 and tVOC baselines
                mySettings.baselineeCO2_SGP30 = sgp30.baselineCO2;                           // dont change eCO2
                sgp30.setBaseline((uint16_t)mySettings.baselineeCO2_SGP30, (uint16_t)tmpuI); // set TVOC baseline
                mySettings.baselineSGP30_valid = 0xF0;
                mySettings.baselinetVOC_SGP30 = (uint16_t)tmpuI;
                snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SGP30 tVOC calibration point is: %u"), tmpuI); 
              } else { strcpy_P(tmpStr, PSTR("SGP30 not available")); }
            } else { strcpy_P(tmpStr, PSTR("SGP30 provided baseline for tVOC is out of range")); }
          } else { strcpy_P(tmpStr, PSTR("SGP30 no baseline provided")); }

        } else if (text[0] == 'z') {                                      // zero baseline
          if (sgp30_avail && mySettings.useSGP30) {
            switchI2C(sgp30_port, sgp30_i2c[0], sgp30_i2c[1], sgp30_i2cspeed, sgp30_i2cClockStretchLimit);
            sgp30.setBaseline((uint16_t)0, (uint16_t)0); // set TVOC baseline
            mySettings.baselineSGP30_valid = 0xF0;
            mySettings.baselinetVOC_SGP30 = (uint16_t)0;
            mySettings.baselineeCO2_SGP30 = (uint16_t)0;
            strcpy_P(tmpStr, PSTR("SGP30 baseline reset")); 
          } else { strcpy_P(tmpStr, PSTR("SGP30 not available")); }

        } else if (text[0] == 'g') {                                  // get baseline
          if (sgp30_avail && mySettings.useSGP30) {
            switchI2C(sgp30_port, sgp30_i2c[0], sgp30_i2c[1], sgp30_i2cspeed, sgp30_i2cClockStretchLimit);
            sgp30.getBaseline();
            mySettings.baselineSGP30_valid = 0xF0;
            mySettings.baselinetVOC_SGP30 = sgp30.baselineTVOC;
            mySettings.baselineeCO2_SGP30 = sgp30.baselineCO2;
            strcpy_P(tmpStr, PSTR("SGP baselines read"));
          } else { strcpy_P(tmpStr, PSTR("SGP30 not available")); }
        
        } else { strcpy_P(tmpStr, PSTR("SPG30 no valid command provided")); }
         
        R_printSerialTelnetLogln(tmpStr);
        yieldTime += yieldOS();
      } else { helpMenu(); }
    }

    ///////////////////////////////////////////////////////////////////
    // SCD30
    ///////////////////////////////////////////////////////////////////

    else if (command[0] == 'D') {
      if (textlen > 0) { // subcommand was given
        if (textlen > 1) { strlcpy(value,text+1,sizeof(value)); }  else { value[0] = '\0'; } // value was given

        if      (text[0] == 'c') {                                    // force CO2
          if (strlen(value) > 0) {
            tmpuI = strtoul(value, NULL, 10);
            if ((tmpuI >= 400) && (tmpuI <= 2000)) {
              if (scd30_avail && mySettings.useSCD30) {
                switchI2C(scd30_port, scd30_i2c[0], scd30_i2c[1], scd30_i2cspeed, scd30_i2cClockStretchLimit);
                scd30.setForcedRecalibrationFactor((uint16_t)tmpuI);
                snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Calibration point is: %u"), tmpuI); 
              }
            } else {
              strcpy_P(tmpStr, PSTR("SDC30 calibration point out of valid Range")); }
          } else { strcpy_P(tmpStr, PSTR("SCD30 no calibration data provided")); }

        } else if (text[0] == 'p') {                                  // set pressure compensation
          if (strlen(value) > 0) {
            tmpuI = strtoul(value,NULL,10);
            if ((tmpuI >= 700) && (tmpuI <= 1200)) {
              if (scd30_avail && mySettings.useSCD30) {
                switchI2C(scd30_port, scd30_i2c[0], scd30_i2c[1], scd30_i2cspeed, scd30_i2cClockStretchLimit);
                scd30.setAmbientPressure((uint16_t)tmpuI);
                snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SCD30 ambinet pressure set to: %u"), tmpuI); 
              } else { strcpy_P(tmpStr, PSTR("SCD30 not available")); }
            } else { strcpy_P(tmpStr, PSTR("SCD30 ambient pressure out of valid range")); }
          } else { strcpy_P(tmpStr, PSTR("SCD30 no abient pressure provided")); }

        } else if (text[0] == 't') {                                  // set temperature offset
          if (strlen(value) > 0) {
            tmpF = strtof(value,NULL);
            if ((tmpF >= 0.0) && (tmpF <= 10.0)) {
              if (scd30_avail && mySettings.useSCD30) {
                switchI2C(scd30_port, scd30_i2c[0], scd30_i2c[1], scd30_i2cspeed, scd30_i2cClockStretchLimit);
                scd30.setTemperatureOffset(tmpF);
                mySettings.tempOffset_SCD30_valid = 0xF0;
                mySettings.tempOffset_SCD30 = scd30.getTemperatureOffset();
                snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SCD30 temperature offset set to: %f"), mySettings.tempOffset_SCD30); 
              } else { strcpy_P(tmpStr, PSTR("SCD30 not available")); }
            } else { strcpy_P(tmpStr, PSTR("SCD30 temp offset point out of valid range")); }
          } else { strcpy_P(tmpStr, PSTR("SCD30 no temp offset provided")); }

        } else if (text[0] == 'T') {                                  // get temperature offset
          if (scd30_avail && mySettings.useSCD30) {
            switchI2C(scd30_port, scd30_i2c[0], scd30_i2c[1], scd30_i2cspeed, scd30_i2cClockStretchLimit);
            mySettings.tempOffset_SCD30 = scd30.getTemperatureOffset();
            mySettings.tempOffset_SCD30_valid = 0xF0;
            snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SCD30 temperature offset is: %f"), mySettings.tempOffset_SCD30); 
          } else { strcpy_P(tmpStr, PSTR("SCD30 not available")); }

        } else { strcpy_P(tmpStr, PSTR("No valid command provided")); }

        R_printSerialTelnetLogln(tmpStr);
        yieldTime += yieldOS(); 
      } else { helpMenu(); }
    }

    ///////////////////////////////////////////////////////////////////
    // CCS811
    ///////////////////////////////////////////////////////////////////
    else if (command[0] == 'C') {
      if (textlen > 0) { // subcommand was given
        if (textlen > 1) { strlcpy(value,text+1,sizeof(value)); }  else { value[0] = '\0'; } // value was given

        if      (text[0] == 'c') {                                    // force baseline
          if (strlen(value) > 0) {
            tmpuI = strtoul(value, NULL, 10);
            if ((tmpuI >= 0) && (tmpuI <= 65535)) {
              if (ccs811_avail && mySettings.useCCS811) {
                switchI2C(ccs811_port, ccs811_i2c[0], ccs811_i2c[1], ccs811_i2cspeed, ccs811_i2cClockStretchLimit);
                ccs811.setBaseline((uint16_t)tmpuI);
                mySettings.baselineCCS811_valid = 0xF0;
                mySettings.baselineCCS811 = (uint16_t)tmpuI;
                snprintf_P(tmpStr, sizeof(tmpStr), PSTR("CCS811 baseline point is: %d"),tmpuI); 
              } else { strcpy_P(tmpStr, PSTR("CCS811 not available")); }
            } else { strcpy_P(tmpStr, PSTR("CCS811 baseline out of valid range")); }
          } else { strcpy_P(tmpStr, PSTR("CCS811 no baseline provided")); }

        } else if (text[0] == 'b') {                                  // get temperature offset
          if (ccs811_avail && mySettings.useCCS811) {
            switchI2C(ccs811_port, ccs811_i2c[0], ccs811_i2c[1], ccs811_i2cspeed, ccs811_i2cClockStretchLimit);
            mySettings.baselineCCS811 = ccs811.getBaseline();
            snprintf_P(tmpStr, sizeof(tmpStr), PSTR("CCS811: baseline is %d"), mySettings.baselineCCS811);
          } else { strcpy_P(tmpStr, PSTR("CCS811 not available")); }
          
        } else { strcpy_P(tmpStr, PSTR("No valid command provided")); }
        R_printSerialTelnetLogln(tmpStr);
        yieldTime += yieldOS(); 
      } else { helpMenu(); }
    }

    ///////////////////////////////////////////////////////////////////
    // MLX settings
    ///////////////////////////////////////////////////////////////////
    else if (command[0] == 'X') {
      if (textlen > 0) { // subcommand was given
        if (textlen > 1) { strlcpy(value,text+1,sizeof(value)); }  else { value[0] = '\0'; } // value was given

        if        (text[0] == 't') {                                  // set temprature offset
          if (strlen(value) > 0) {
            tmpF = strtof(value, NULL);
            if ((tmpF >= -20.0) && (tmpF <= 20.0)) {
              mlxOffset = tmpF;
              mySettings.tempOffset_MLX_valid = 0xF0;
              mySettings.tempOffset_MLX = (float)tmpF;
              snprintf_P(tmpStr, sizeof(tmpStr), PSTR("MLX temperature offset set to: %f"),mySettings.tempOffset_MLX);
            } else { strcpy_P(tmpStr, PSTR("MLX temperature offset out of valid range")); }
          } else { strcpy_P(tmpStr, PSTR("MLX no temperature offset provided")); }

        } else if (text[0] == 'e') {                                  // set emissivity
          if (strlen(value) > 0) {
            tmpF = strtof(value, NULL);
            if ((tmpF >= 0) && (tmpF <= 1.0)) {
              if (therm_avail && mySettings.useMLX) {
                switchI2C(mlx_port, mlx_i2c[0], mlx_i2c[1], mlx_i2cspeed, mlx_i2cClockStretchLimit);
                therm.setEmissivity((float)tmpF);
                mySettings.emissivity = (float) therm.readEmissivity(); 
              } else { mySettings.emissivity = (float) tmpF; }
              snprintf_P(tmpStr, sizeof(tmpStr), PSTR("MLX emissivity set to: %f"),mySettings.emissivity);
            } else { strcpy_P(tmpStr, PSTR("MLX emissivity out of valid range")); }
          } else { strcpy_P(tmpStr, PSTR("MLX no emissivity provided")); }

        } else if (text[0] == 'E') {                                  // get emissivity
          if (therm_avail && mySettings.useMLX) {
            switchI2C(mlx_port, mlx_i2c[0], mlx_i2c[1], mlx_i2cspeed, mlx_i2cClockStretchLimit);
            mySettings.emissivity = (float) therm.readEmissivity(); 
            snprintf_P(tmpStr, sizeof(tmpStr), PSTR("MLX emissivity read: %f"),mySettings.emissivity);
          } else { strcpy_P(tmpStr, PSTR("MLX not available")); }
          
        } else {strcpy_P(tmpStr, PSTR("No valid command provided")); }
        R_printSerialTelnetLogln(tmpStr);
        yieldTime += yieldOS(); 
      } else { helpMenu(); }
    }

    ///////////////////////////////////////////////////////////////////
    // LCD
    ///////////////////////////////////////////////////////////////////
    else if (command[0] == 'L') {
      if (textlen > 0) { // subcommand was given
        if (textlen > 1) { strlcpy(value,text+1,sizeof(value)); }  else { value[0] = '\0'; } // value was given

        if (text[0] == 'i') {                                         // set LCD display layout 1..4
          if (strlen(value) > 0) { tmpuI = strtoul(value, NULL, 10);  }
          else { tmpuI = 4; }
          if ((tmpuI >= 1) && (tmpuI <= 4)) {
            mySettings.LCDdisplayType = (uint8_t) tmpuI;
            snprintf_P(tmpStr, sizeof(tmpStr), PSTR("LCD display layout interval set to: %u"), mySettings.LCDdisplayType);
          } else { strcpy_P(tmpStr, PSTR("LCD display layout out of valid range")); }
           
        } else { strcpy_P(tmpStr, PSTR("No valid command provided")); }    

        R_printSerialTelnetLogln(tmpStr);
        yieldTime += yieldOS(); 
      } else { helpMenu(); }
    }

    ///////////////////////////////////////////////////////////////////
    // Enable / Disable Sensors
    ///////////////////////////////////////////////////////////////////

    else if (command[0] == 'x') {                                          // enable/disable equipment
      if (textlen > 0) { // options was given
        tmpuI = strtoul(text, NULL, 10);
        if ((tmpuI > 0) && (tmpuI < 100)) {
          switch (tmpuI) {
            case 2:
              mySettings.useLCD = !bool(mySettings.useLCD);
              if (mySettings.useLCD)   {            R_printSerialTelnetLogln(F("LCD is used"));    }      else { R_printSerialTelnetLogln(F("LCD is not used")); }
              break;
            case 3:
              mySettings.useWiFi = !bool(mySettings.useWiFi);
              if (mySettings.useWiFi)  {            R_printSerialTelnetLogln(F("WiFi is used"));   }      else { R_printSerialTelnetLogln(F("WiFi is not used")); }
              break;
            case 4:
              mySettings.useSCD30 = !bool(mySettings.useSCD30);
              if (mySettings.useSCD30) {            R_printSerialTelnetLogln(F("SCD30 is used"));  }      else { R_printSerialTelnetLogln(F("SCD30 is not used")); }
              break;
            case 5:
              mySettings.useSPS30 = !bool(mySettings.useSPS30);
              if (mySettings.useSPS30) {            R_printSerialTelnetLogln(F("SPS30 is used"));  }      else { R_printSerialTelnetLogln(F("SPS30 is not used")); }
              break;
            case 6:
              mySettings.useSGP30 = !bool(mySettings.useSGP30);
              if (mySettings.useSGP30) {            R_printSerialTelnetLogln(F("SGP30 is used"));  }      else { R_printSerialTelnetLogln(F("SGP30 is not used")); }
              break;
            case 7:
              mySettings.useMQTT = !bool(mySettings.useMQTT); 
              if (mySettings.useMQTT==true) {       R_printSerialTelnetLogln(F("MQTT is used"));   }      else { R_printSerialTelnetLogln(F("MQTT is not used")); }
              break;
            case 8:
              mySettings.useNTP = !bool(mySettings.useNTP);
              if (mySettings.useNTP==true) {        R_printSerialTelnetLogln(F("NTP is used"));    }      else { R_printSerialTelnetLogln(F("NTP is not used")); }
              break;
            case 9:
              mySettings.usemDNS = !bool(mySettings.usemDNS);
              if (mySettings.usemDNS==true) {       R_printSerialTelnetLogln(F("mDNS is used"));    }      else { R_printSerialTelnetLogln(F("mDNS is not used")); }
              break;
            case 10:
              mySettings.useMAX30 = !bool(mySettings.useMAX30);
              if (mySettings.useMAX30==true) {      R_printSerialTelnetLogln(F("MAX30 is used"));   }      else { R_printSerialTelnetLogln(F("MAX30 is not used")); }
              break;
            case 11:
              mySettings.useMLX = !bool(mySettings.useMLX);
              if (mySettings.useMLX==true)   {      R_printSerialTelnetLogln(F("MLX is used"));     }      else { R_printSerialTelnetLogln(F("MLX is not used")); }
              break;
            case 12:
              mySettings.useBME68x = !bool(mySettings.useBME68x);
              if (mySettings.useBME68x==true) {     R_printSerialTelnetLogln(F("BME68x is used"));  }      else { R_printSerialTelnetLogln(F("BME68x is not used")); }
              break;
            case 13:
              mySettings.useBME280 = !bool(mySettings.useBME280);
              if (mySettings.useBME280==true) {     R_printSerialTelnetLogln(F("BME280 is used"));  }      else { R_printSerialTelnetLogln(F("BME280 is not used")); }
              break;
            case 14:
              mySettings.useCCS811 = !bool(mySettings.useCCS811);
              if (mySettings.useCCS811==true) {     R_printSerialTelnetLogln(F("CCS811 is used"));  }      else { R_printSerialTelnetLogln(F("CCS811 is not used")); }
              break;
            case 15:
              mySettings.useBacklight = !bool(mySettings.useBacklight);
              if (mySettings.useBacklight==true) {  R_printSerialTelnetLogln(F("Backlight is on"));}       else { R_printSerialTelnetLogln(F("Backlight is off")); }
              break;
            case 16:
              mySettings.useBacklightNight = !bool(mySettings.useBacklightNight);
              if (mySettings.useBacklightNight==true) { R_printSerialTelnetLogln(F("Backlight is on at night"));} else { R_printSerialTelnetLogln(F("Backlight is off at night")); }
              break;
            case 17:
              mySettings.useBlinkNight = !bool(mySettings.useBlinkNight);
              if (mySettings.useBlinkNight==true) { R_printSerialTelnetLogln(F("Blink Backlight is on at night"));} else { R_printSerialTelnetLogln(F("Blink Backlight is off at night")); }
              break;
            case 18:
              mySettings.useHTTP = !bool(mySettings.useHTTP);
              if (mySettings.useHTTP==true) {       R_printSerialTelnetLogln(F("HTTP webserver is on"));}  else { R_printSerialTelnetLogln(F("HTTP webserver is off")); }
              break;
            case 19:
              mySettings.useOTA = !bool(mySettings.useOTA);
              if (mySettings.useOTA==true) {        R_printSerialTelnetLogln(F("OTA server is on")); }     else { R_printSerialTelnetLogln(F("OTA server is off")); }
              break;
            case 20:
              mySettings.useSerial = !bool(mySettings.useSerial);
              if (mySettings.useSerial==true) {     R_printSerialTelnetLogln(F("Serial is on")); }         else { R_printSerialTelnetLogln(F("Serial is off")); }
              break;
            case 21:
              mySettings.useTelnet = !bool(mySettings.useTelnet);
              if (mySettings.useTelnet==true) {     R_printSerialTelnetLogln(F("Telnet is on")); }         else { R_printSerialTelnetLogln(F("Telnet is off")); }
              break;
            case 22:
              mySettings.useHTTPUpdater = !bool(mySettings.useHTTPUpdater);
              if (mySettings.useHTTPUpdater==true) {R_printSerialTelnetLogln(F("HTTPUpdater is on")); }    else { R_printSerialTelnetLogln(F("HTTPUpdater is off")); }
              break;
            case 23:
              mySettings.useLog = !bool(mySettings.useLog);
              if (mySettings.useLog==true) {        R_printSerialTelnetLogln(F("Log file is on")); }       else { R_printSerialTelnetLogln(F("Log file is off")); }
              break;
            case 24:
              mySettings.useWeather = !bool(mySettings.useWeather);
              if (mySettings.useWeather==true) {    R_printSerialTelnetLogln(F("Weather is on")); }         else { R_printSerialTelnetLogln(F("Weather is off")); }
              break;
            case 99:
              R_printSerialTelnetLogln(F("ESP is going to restart soon..."));
              rebootNow = true;
              break;
            default:
              R_printSerialTelnetLog(F("Invalid option"));
              break;
          } // end switch
        } // valid option
      } else { R_printSerialTelnetLog(F("No valid option provided")); }
      yieldTime += yieldOS(); 
    } // end turning on/off sensors

    ///////////////////////////////////////////////////////////////////
    // Set Debug Levels
    ///////////////////////////////////////////////////////////////////

    else if (command[0] == 'l') {                                            // set debug level
      if (textlen > 0) {
        tmpuI = strtoul(text, NULL, 10);
        if ((tmpuI >= 0) && (tmpuI <= 99)) {
          mySettings.debuglevel = (unsigned int)tmpuI;
          snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Debug level set to: %u"), mySettings.debuglevel);  
        } else { strcpy_P(tmpStr, PSTR("Debug level out of valid Range")); }
        
      } else { strcpy_P(tmpStr, PSTR("No debug level provided")); }
      R_printSerialTelnetLogln(tmpStr);
      yieldTime += yieldOS(); 
    }

    ///////////////////////////////////////////////////////////////////
    // Settings and Filesystem
    ///////////////////////////////////////////////////////////////////

    else if (command[0] == 's') {                                            // print Settings
      printSettings();
    }

    else if (command[0] == 'd') {                                            // default Settings
      if (textlen > 0) {
        if (text[0] == 'd') {
          defaultSettings();
          yieldTime += yieldOS(); 
          printSettings(); 
        }
      }
    }

    else if (command[0] == 'F') {
      if (textlen > 0) { 

        if (text[0] == 's') {                                                // save
          tmpTime = millis();
          if        (text[1] == 'E') {                                       // save EEPROM
            D_printSerialTelnet(F("D:S:EPRM.."));
            EEPROM.put(0, mySettings);
            if (EEPROM.commit()) { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Settings saved to EEPROM in: %dms"), millis() - tmpTime); } // takes 400ms
            else {snprintf_P(tmpStr, sizeof(tmpStr), PSTR("EEPROM failed to commit"));} 
          } else if (text[1] == 'J') {                                       // save JSON
            D_printSerialTelnet(F("D:S:JSON.."));
            saveConfiguration(mySettings);
            snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Settings saved to JSON file in: %dms"), millis() - tmpTime); 
          } else { strcpy_P(tmpStr, PSTR("Need to provide E or J for saving settings")); }
          R_printSerialTelnetLogln(tmpStr);
          yieldTime += yieldOS(); 

        } else if (text[0] == 'r') {                                         // read
          tmpTime = millis();
          if        (text[1] == 'E') {                                       // read EEPROM
            D_printSerialTelnet(F("D:R:EPRM.."));
            EEPROM.begin(EEPROM_SIZE);
            EEPROM.get(eepromAddress, mySettings);
            snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Settings read from eeprom in: %dms"), millis() - tmpTime); 
          } else if (text[1] == 'J') {                                       // read JSON
            D_printSerialTelnet(F("D:R:JSON.."));
            tmpTime = millis();
            loadConfiguration(mySettings);
            snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Settings loaded from JSON file in: %dms"), millis() - tmpTime); 
          } else { strcpy_P(tmpStr, PSTR("Need to provide E or J for reading settings")); }
          R_printSerialTelnetLogln(tmpStr);
          yieldTime += yieldOS(); 
          printSettings();

        } else if (text[0] == 'f') {                                         // format file system
          if (text[1] == 'f') {
            if ( LittleFS.format() ) { R_printSerialTelnetLogln(F("LittleFS formated")); }
          } else { R_printSerialTelnetLogln(F("LittleFS not formated, need to provide ff")); }

        } else if (text[0] == 'p') {                                         // print file
          if (textlen > 1) { 
            strlcpy(value,text+1,sizeof(value));
            if (fsOK) {
              File file = LittleFS.open(value, "r");
              if (file) { 
                while(file.available()){
                  size_t bytesRead = file.readBytes(tmpStr, sizeof(tmpStr)-1);
                  tmpStr[bytesRead] = '\0';
                  R_printSerialTelnetLog(tmpStr);
                  yieldTime += yieldOS(); 
                }
                file.close();
              } else { 
                printSerialTelnetLog(F("Failed to open file: ")); 
                printSerialTelnetLogln(value); 
              }
            } else { R_printSerialTelnetLogln(F("LittleFS not mounted")); }
            yieldTime += yieldOS();
          } else { R_printSerialTelnetLogln(F("Need to provide a file name")); } 

        } else if (text[0] == 'x') {                                         // delete file
          if (textlen > 1) { 
            strlcpy(value,text+1,sizeof(value)); 
            if (fsOK) {
              if (LittleFS.exists(value)) {
                LittleFS.remove(value);
                printSerialTelnetLog(value); 
                printSerialTelnetLogln(F(" deleted")); 
              } else { 
                printSerialTelnetLog(value); 
                printSerialTelnetLog(F(" does not exist")); 
              }
            } else { R_printSerialTelnetLogln(F("LittleFS not mounted")); }
            yieldTime += yieldOS(); 
          } else { R_printSerialTelnetLogln(F("Need to provide a file name")); }

        } else if (text[0] == 'w') {                                         // upload file
          if (processSerial) {
            if (textlen > 1) { 
              strlcpy(value,text+1,sizeof(value)); 
              if (fsOK) {
                File file = LittleFS.open(value, "w");
                if (file) {  // file opened successfully
                  printSerialTelnetLog(F("Write into file: ")); 
                  printSerialTelnetLogln(value); 
                  while (Serial.available()) {
                    char c = Serial.read();
                    if (c == 0x1B) { break; } // ESC, Ctrl-Z
                    file.write(c);
                  }
                  file.close();
                  printSerialTelnetLogln(F("Finished writting"));
                } else { printSerialTelnetLog(F("Failed to open file: ")); printSerialTelnetLogln(value); }
                file.close();
                yieldTime += yieldOS(); 
              } else { R_printSerialTelnetLogln(F("LittleFS not mounted")); }
            } else { R_printSerialTelnetLogln(F("Need to provide a file name")); }
          } else { R_printSerialTelnetLogln(F("Only works over serial/USB")); }

        } else { R_printSerialTelnetLogln(F("Sub command not implemented")); }

      } else {                                                               // list file system
        R_printSerialTelnetLogln(F(" Contents:"));
        if (fsOK) {
          Dir mydir = LittleFS.openDir("/");
          unsigned int fileSize;
          while (mydir.next()) {                                             // List the file system contents
            String fileName = mydir.fileName();
            fileSize = (unsigned int)mydir.fileSize();
            snprintf_P(tmpStr, sizeof(tmpStr), PSTR("\tName: %24s, Size: %8u"), fileName.c_str(), fileSize); 
            R_printSerialTelnetLogln(tmpStr);
          }
        } else { R_printSerialTelnetLogln(F("LittleFS not mounted")); }
        yieldTime += yieldOS(); 
      }
    } // end Settings and Filesystem

    else {                                                                   // unrecognized command
      R_printSerialTelnetLog(F("Command not available: "));
      printSerialTelnetLogln(command);
      printSerialTelnetLogln(F("Use: ZZ? for help"));
      yieldTime += yieldOS(); 
    } // end if
    
    processSerial  = false;
    processTelnet  = false;
    return true;
  } // end process of input occured
  
  else { 
    processSerial  = false;
    processTelnet  = false;
    return false; 
  } // end no process of input occured
  
} // end input handle

void helpMenu() {  
    printSerialTelnetLogln();
    R_printSerialTelnetLogln(FPSTR(doubleSeparator)); yieldTime += yieldOS(); 
    snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Sensi, 2020-2022, Urs Utzinger                                             %s"),  FPSTR(VER)); 
    printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
    printSerialTelnetLogln(FPSTR(doubleSeparator));                                                                 yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("Supports........................................................................"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("........................................................................LCD 20x4"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("........................................................SPS30 Senserion Particle"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F(".............................................................SCD30 Senserion CO2"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("......................................................SGP30 Senserion tVOC, eCO2"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("..........................BME68x/280 Bosch Temperature, Humidity, Pressure, tVOC"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("...................................................CCS811 eCO2 tVOC, Air Quality"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("..........................................MLX90614 Melex Temperature Contactless"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("==General======================================================================="));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| All commands preceeded by ZZ                                                 |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(FPSTR(singleSeparator));                                                                 yieldTime += yieldOS(); 
    //                       "================================================================================"
    printSerialTelnetLogln(F("| ?: help screen                        | n: set this device name, nSensi      |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| z: print sensor data                  |                                      |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| j: print sensor data in JSON          | .: execution times                   |"));  yieldTime += yieldOS(); 

    printSerialTelnetLogln(F("==WiFi==================================|======================================="));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| W: WiFi states                        |                                      |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| Ws1: set SSID 1 Ws1UAWiFi             | Wp1: set password                    |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| Ws2: set SSID 2                       | Wp2: set password                    |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| Ws3: set SSID 3                       | Wp3: set password                    |"));  yieldTime += yieldOS(); 

    printSerialTelnetLogln(F("==MQTT==================================|======================================="));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| Mu: set username                      | Mi: set time interval Mi1.0 [s]      |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| Mp: set password                      | Ms: set server                       |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| Mm: individual/single msg             | Mf: set fallback server              |"));  yieldTime += yieldOS(); 

    printSerialTelnetLogln(F("==NTP===================================|======================================="));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| Ns: set server                        | Nn: set night start min after midni  |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| Nf: set fallback server               | Ne: set night end min after midnight |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| Nt: set time zone                     | Nr: set reboot time -1=off           |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| https://raw.githubusercontent.com/nayarsystems/posix_tz_db/master/zones.csv  |"));  yieldTime += yieldOS(); 

    printSerialTelnetLogln(F("==Weather===============================|======================================="));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| Rk: set api key Wae05a02...           | Ra: set altitude in meters           |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| Rc: set city id WcTucson              | Rp: set average pressure in [mbar]   |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| Rl: set country WcUS                  |                                      |"));  yieldTime += yieldOS(); 

    printSerialTelnetLogln(F("==SGP30=================================|==SCD30================================"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| Pc: force eCO2 Gc400 ppm              | Dc: force eCO2 Cc400 ppm             |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| Pv: force tVOC Gv100 ppb              | Dt: set temp offset Ct5.0 [C]        |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| Pz: set baseline to 0                 | DT: get temp offset CT               |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| Pg: get baseline                      | Dp: set ambinet pressure Dp5 [mbar]  |"));  yieldTime += yieldOS(); 

    printSerialTelnetLogln(F("==CCS811================================|==MLX=================================="));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| Cc: force baseline [uint16]           | Xt: set temp offset Xt5.0 [C]        |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| Cb: get baseline [uint16]             | Xe: set emissivity Xe0.98            |"));  yieldTime += yieldOS(); 

    printSerialTelnetLogln(F("==LCD===================================|======================================="));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| Li: display layout 1..4 Li4           | 1:simple 2:engr 3:1p 4:1p w time & W |"));  yieldTime += yieldOS(); 

    printSerialTelnetLogln(F("==Disable===============================|==Disable=============================="));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| x: 2 LCD on/off                       | x: 14 CCS811 on/off                  |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| x: 3 WiFi on/off                      | x: 15 LCD backlight on/off           |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| x: 4 SCD30 on/off                     | x: 16 LCD backlight at night on/off  |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| x: 5 SPS30 on/off                     | x: 17 Blink backlgiht at night on/off|"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| x: 6 SGP30 on/off                     | x: 18 HTML server on/off             |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| x: 7 MQTT on/off                      | x: 19 OTA on/off                     |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| x: 8 NTP on/off                       | x: 20 Serial on/off                  |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| x: 9 mDNS on/off                      | x: 21 Telnet on/off                  |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| x: 10 MAX30 on/off                    | x: 22 HTTP Updater on/off            |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| x: 11 MLX on/off                      | x: 23 Logfile on/off                 |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| x: 12 BME68x on/off                   | x: 24 Weather on/off                 |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| x: 13 BME280 on/off                   | x: 99 Reset/Reboot                   |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("|---------------------------------------|--------------------------------------|"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| You will need to reboot with x99 to initialize the sensor                    |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("==Debug Level===========================|==Debug Level=========================="));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| l: 0 ALL off                          | l: 6 SGP30 max level                 |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| l: 1 Errors only                      | l: 7 MAX30 max level                 |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| l: 2 Minimal                          | l: 8 MLX max level                   |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| l: 3 WiFi max level                   | l: 9 BME68x/280 max level            |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| l: 4 SCD30 max level                  | l: 10 CCS811 max level               |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| l: 5 SPS30 max level                  | l: 11 LCD max level                  |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| l: 99 continous                       |                                      |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("==Settigns==============================|==Filesystem==========================="));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| s:   print current settings           | Fff: format filesystem               |"));  yieldTime += yieldOS();
    printSerialTelnetLogln(F("| FsE: save settings to EEPROM          | F:  list content of filesystem       |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| FsJ: save settings to Sensio.json     | Fp: print file Fp/Sensi.json         |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| FrE / FrS: read settings              | Fx: delete file Fx/Sensi.bak         |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| dd: default settings                  | Fw: write into file Fw/index.html ^Z |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(FPSTR(doubleSeparator));                                                                 yieldTime += yieldOS(); 
}

void printSettings() {
  printSerialTelnetLogln();
  R_printSerialTelnetLogln(FPSTR(doubleSeparator)); yieldTime += yieldOS(); 
  printSerialTelnetLogln(F("-System-----------------------------")); yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Debug level: .................. %u"),   mySettings.debuglevel ); 
  printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Reboot Minute: ................ %i"),   mySettings.rebootMinute); 
  printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Runtime [min]: ................ %lu"),  mySettings.runTime / 60); 
  printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Display type: ................. %u"),   mySettings.LCDdisplayType);  
  printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Serial terminal: .............. %s"),  (mySettings.useSerial) ? FPSTR(mON) : FPSTR(mOFF));  
  printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Telnet terminal: .............. %s"),  (mySettings.useTelnet) ? FPSTR(mON) : FPSTR(mOFF));  
  printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Log file:....... .............. %s"),  (mySettings.useLog) ? FPSTR(mON) : FPSTR(mOFF));  
  printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
  printSerialTelnetLogln(FPSTR(doubleSeparator)); yieldTime += yieldOS();   
  printSerialTelnetLogln(F("-Network----------------------------")); yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("HTTP: ......................... %s"),  (mySettings.useHTTP) ? FPSTR(mON) : FPSTR(mOFF)); 
  printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("OTA: .......................... %s"),  (mySettings.useOTA)  ? FPSTR(mON) : FPSTR(mOFF)); 
  printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("mDNS: ......................... %s"),  (mySettings.usemDNS) ? FPSTR(mON) : FPSTR(mOFF));
  printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("HTTPUpdater: .................. %s"),  (mySettings.useHTTPUpdater) ? FPSTR(mON) : FPSTR(mOFF)); 
  printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Telnet: ....................... %s"),  (mySettings.useTelnet) ? FPSTR(mON) : FPSTR(mOFF)); 
  printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Serial: ....................... %s"),  (mySettings.useSerial) ? FPSTR(mON) : FPSTR(mOFF)); 
  printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
  printSerialTelnetLogln(F("-Credentials------------------------")); yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("WiFi: ......................... %s"),  (mySettings.useWiFi) ? FPSTR(mON) : FPSTR(mOFF)); 
  printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SSID: ......................... %s"),   mySettings.ssid1);  
  printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("PW: ........................... %s"),   mySettings.pw1);  
  printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SSID: ......................... %s"),   mySettings.ssid2);  
  printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("PW: ........................... %s"),   mySettings.pw2);  
  printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SSID: ......................... %s"),   mySettings.ssid3);  
  printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("PW: ........................... %s"),   mySettings.pw3);  
  printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
  printSerialTelnetLogln(F("-Time-------------------------------")); yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("NTP: .......................... %s"),  (mySettings.useNTP) ? FPSTR(mON) : FPSTR(mOFF)); 
  printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("NTP Server: ................... %s"),   mySettings.ntpServer);  
  printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("NTP Fallback Server: .......... %s"),   mySettings.ntpFallback);  
  printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Time Zone: .................... %s"),   mySettings.timeZone);  
  printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
  printSerialTelnetLogln(F("-MQTT-------------------------------")); yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("MQTT: ......................... %s"),  (mySettings.useMQTT) ? FPSTR(mON) : FPSTR(mOFF)); 
  printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("MQTT: ......................... %s"),   mySettings.mqtt_server);  
  printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("MQTT fallback: ................ %s"),   mySettings.mqtt_fallback);  
  printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("MQTT user: .................... %s"),   mySettings.mqtt_username);  
  printSerialTelnetLogln(tmpStr); yieldTime += yieldOS();
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("MQTT password: ................ %s"),   mySettings.mqtt_password);  
  printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("MQTT send immediatly: ......... %s"),  (mySettings.sendMQTTimmediate) ? FPSTR(mON) : FPSTR(mOFF)); 
  printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("MQTT main topic: .............. %s"),   mySettings.mqtt_mainTopic);  
  printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("MQTT interval: . .............. %f"),   mySettings.intervalMQTT);  
  printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
  printSerialTelnetLogln(FPSTR(doubleSeparator)); yieldTime += yieldOS(); 
  printSerialTelnetLogln(F("-Sensors----------------------------"));  yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SCD30: ........................ %s"),  (mySettings.useSCD30)   ? FPSTR(mON) : FPSTR(mOFF)); 
  printSerialTelnetLogln(tmpStr);  yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SPS30: ........................ %s"),  (mySettings.useSPS30)   ? FPSTR(mON) : FPSTR(mOFF)); 
  printSerialTelnetLogln(tmpStr);  yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SGP30: ........................ %s"),  (mySettings.useSGP30)   ? FPSTR(mON) : FPSTR(mOFF)); 
  printSerialTelnetLogln(tmpStr);  yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("MAX30: ........................ %s"),  (mySettings.useMAX30)   ? FPSTR(mON) : FPSTR(mOFF)); 
  printSerialTelnetLogln(tmpStr);  yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("MLX: .......................... %s"),  (mySettings.useMLX)     ? FPSTR(mON) : FPSTR(mOFF)); 
  printSerialTelnetLogln(tmpStr);  yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("BME68x: ....................... %s"),  (mySettings.useBME68x)  ? FPSTR(mON) : FPSTR(mOFF)); 
  printSerialTelnetLogln(tmpStr);  yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("BME280: ....................... %s"),  (mySettings.useBME280)  ? FPSTR(mON) : FPSTR(mOFF)); 
  printSerialTelnetLogln(tmpStr);  yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("CCS811: ....................... %s"),  (mySettings.useCCS811)  ? FPSTR(mON) : FPSTR(mOFF)); 
  printSerialTelnetLogln(tmpStr);  yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Weather: ...................... %s"),  (mySettings.useWeather) ? FPSTR(mON) : FPSTR(mOFF)); 
  printSerialTelnetLogln(tmpStr);  yieldTime += yieldOS(); 
  printSerialTelnetLogln(F("-Offsets&Baselines------------------")); yieldTime += yieldOS();   
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Base SGP30 valid: ............. %X"),   mySettings.baselineSGP30_valid); 
  printSerialTelnetLogln(tmpStr);  yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Base eCO2 SGP30: .............. %u"),   mySettings.baselineeCO2_SGP30); 
  printSerialTelnetLogln(tmpStr);  yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Base tVOC SGP30: .............. %u"),   mySettings.baselinetVOC_SGP30); 
  printSerialTelnetLogln(tmpStr);  yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Base CCS811 valid: ............ %X"),   mySettings.baselineCCS811_valid); 
  printSerialTelnetLogln(tmpStr);  yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Base CCS811: .................. %u"),   mySettings.baselineCCS811); 
  printSerialTelnetLogln(tmpStr);  yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Temp Offset SCD30 valid: ...... %X"),   mySettings.tempOffset_SCD30_valid); 
  printSerialTelnetLogln(tmpStr);  yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Temp Offset SCD30: [C]......... %f"),   mySettings.tempOffset_SCD30); 
  printSerialTelnetLogln(tmpStr);  yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Forced Calibration SCD30 valid: %X"),   mySettings.forcedCalibration_SCD30_valid); 
  printSerialTelnetLogln(tmpStr);  yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Forced Calibration SCD30: [ppm].%f"),   mySettings.forcedCalibration_SCD30); 
  printSerialTelnetLogln(tmpStr);  yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("MLX Temp Offset valid: ........ %X"),   mySettings.tempOffset_MLX_valid); 
  printSerialTelnetLogln(tmpStr);  yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("MLX Temp Offset: [C]........... %f"),   mySettings.tempOffset_MLX); 
  printSerialTelnetLogln(tmpStr);  yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("MLX emissivity ................ %f"),   mySettings.emissivity); 
  printSerialTelnetLogln(tmpStr);  yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Average Pressure: [mbar]....... %f"),   mySettings.avgP/100.); 
  printSerialTelnetLogln(tmpStr);  yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Altitude: [m] ................. %f"),   mySettings.altitude); 
  printSerialTelnetLogln(tmpStr);  yieldTime += yieldOS(); 
  printSerialTelnetLogln(FPSTR(doubleSeparator)); yieldTime += yieldOS(); 
  printSerialTelnetLogln(F("-Weather----------------------------")); yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("API Key: ...................... %s"),   mySettings.weatherApiKey); 
  printSerialTelnetLogln(tmpStr);  yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("City: ......................... %s"),   mySettings.weatherCity); 
  printSerialTelnetLogln(tmpStr);  yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Country: ...................... %s"),   mySettings.weatherCountryCode); 
  printSerialTelnetLogln(tmpStr);  yieldTime += yieldOS(); 
  printSerialTelnetLogln(F("-LCD--------------------------------")); yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("LCD: .......................... %s"),  (mySettings.useLCD) ? FPSTR(mON) : FPSTR(mOFF)); 
  printSerialTelnetLogln(tmpStr);  yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("LCD Backlight: ................ %s"),  (mySettings.useBacklight) ? FPSTR(mON) : FPSTR(mOFF)); 
  printSerialTelnetLogln(tmpStr);  yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("LCD Backlight at Night: ....... %s"),  (mySettings.useBacklightNight) ? FPSTR(mON) : FPSTR(mOFF)); 
  printSerialTelnetLogln(tmpStr);  yieldTime += yieldOS();   
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("LCD Blink at Night: ........... %s"),  (mySettings.useBlinkNight) ? FPSTR(mON) : FPSTR(mOFF)); 
  printSerialTelnetLogln(tmpStr);  yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Night Start: [min][hrs]........ %u, %u"),   mySettings.nightBegin, mySettings.nightBegin/60); 
  printSerialTelnetLogln(tmpStr);  yieldTime += yieldOS();  
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Night End: [min][hrs].......... %u, %u"),   mySettings.nightEnd, mySettings.nightEnd/60); 
  printSerialTelnetLogln(tmpStr);  yieldTime += yieldOS(); 
  printSerialTelnetLogln(FPSTR(doubleSeparator)); yieldTime += yieldOS(); 
}

void printSensors() { 
  printSerialTelnetLogln();
  R_printSerialTelnetLogln(FPSTR(doubleSeparator)); yieldTime += yieldOS(); 
  
  if (lcd_avail && mySettings.useLCD) {
    snprintf_P(tmpStr, sizeof(tmpStr),PSTR("LCD interval: %d Port: %d SDA %d SCL %d Speed %d CLKStretch %d"), intervalLCD, uint32_t(lcd_port), lcd_i2c[0], lcd_i2c[1], lcd_i2cspeed, lcd_i2cClockStretchLimit); 
    printSerialTelnetLogln(tmpStr);
  } else {
    printSerialTelnetLogln(F("LCD: not available"));
  }
  yieldTime += yieldOS(); 
  printSerialTelnetLogln(FPSTR(singleSeparator)); yieldTime += yieldOS(); 

  if (scd30_avail && mySettings.useSCD30) {
    char qualityMessage[16];
    switchI2C(scd30_port, scd30_i2c[0], scd30_i2c[1], scd30_i2cspeed, scd30_i2cClockStretchLimit);
    snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SCD30 CO2:%hu[ppm], rH:%4.1f[%%], T:%4.1f[C] T_offset:%4.1f[C]"), scd30_ppm, scd30_hum, scd30_temp, scd30.getTemperatureOffset() ); 
    printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
    checkCO2(float(scd30_ppm),qualityMessage,15);
    snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SCD30 CO2 is: %s"), qualityMessage); 
    printSerialTelnetLogln(tmpStr); yieldTime += yieldOS();     
    snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SCD30 interval: %d Port: %d SDA %d SCL %d Speed %d CLKStretch %d"), intervalSCD30, uint32_t(scd30_port), scd30_i2c[0], scd30_i2c[1], scd30_i2cspeed,scd30_i2cClockStretchLimit); 
    printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
    snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SCD30 last error: %dmin"), (currentTime - scd30_lastError)/60000); 
    printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
  } else {
    printSerialTelnetLogln(F("SCD30: not available")); yieldTime += yieldOS(); 
  }
  printSerialTelnetLogln(FPSTR(singleSeparator)); yieldTime += yieldOS(); 

  if (bme68x_avail && mySettings.useBME68x) {
    char qualityMessage[16];
    snprintf_P(tmpStr, sizeof(tmpStr),PSTR("BME68x T:%+5.1f[C] P:%5.1f[mbar] P_ave:%5.1f[mbar] rH:%4.1f[%%] aH:%4.1f[g/m^3] \r\nGas resistance:%6.0f[Ohm]"), bme68x.temperature, bme68x.pressure/100., bme68x_pressure24hrs/100., bme68x.humidity, bme68x_ah, bme68x.gas_resistance);  
    printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
    checkAmbientTemperature(bme68x.temperature,qualityMessage, 15);
    snprintf_P(tmpStr, sizeof(tmpStr),PSTR("Temperature is %s, "),qualityMessage); 
    printSerialTelnetLog(tmpStr); yieldTime += yieldOS(); 
    checkdP((bme68x.pressure - bme68x_pressure24hrs)/100.0,qualityMessage,15),
    snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Change in pressure is %s, "),qualityMessage); 
    printSerialTelnetLog(tmpStr); yieldTime += yieldOS(); 
    checkHumidity(bme68x.humidity,qualityMessage,15);
    snprintf_P(tmpStr, sizeof(tmpStr),PSTR("Humidity is %s"),qualityMessage); 
    printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
    checkGasResistance(bme68x.gas_resistance,qualityMessage,15);
    snprintf_P(tmpStr, sizeof(tmpStr),PSTR("Gas resistance is %s"),qualityMessage); 
    printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
    snprintf_P(tmpStr, sizeof(tmpStr), PSTR("BME68x interval: %d Port: %d SDA %d SCL %d Speed %d CLKStretch %d"), intervalBME68x, uint32_t(bme68x_port), bme68x_i2c[0], bme68x_i2c[1], bme68x_i2cspeed, bme68x_i2cClockStretchLimit); 
    printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
    snprintf_P(tmpStr, sizeof(tmpStr), PSTR("BME68x last error: %dmin"), (currentTime - bme68x_lastError)/60000); 
    printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
  } else {
    printSerialTelnetLogln(F("BME68x: not available")); yieldTime += yieldOS(); 
  }
  printSerialTelnetLogln(FPSTR(singleSeparator)); yieldTime += yieldOS(); 

  if (bme280_avail && mySettings.useBME280) {
    char qualityMessage[16];
    snprintf_P(tmpStr, sizeof(tmpStr),PSTR("BME280 T:%+5.1f[C] P:%5.1f[mbar] P_ave:%5.1f[mbar]"), bme280_temp, bme280_pressure/100., bme280_pressure24hrs/100.);  
    printSerialTelnetLog(tmpStr); yieldTime += yieldOS(); 
    if (BMEhum_avail) { 
      snprintf_P(tmpStr, sizeof(tmpStr), PSTR(" rH:%4.1f[%%] aH:%4.1f[g/m^3]"), bme280_hum, bme280_ah); 
      printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
    }
    checkAmbientTemperature(bme280_temp,qualityMessage,15);
    snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Temperature is %s, "),qualityMessage); 
    printSerialTelnetLog(tmpStr); yieldTime += yieldOS(); 
    checkdP((bme280_pressure - bme280_pressure24hrs)/100.0,qualityMessage,15);
    snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Change in pressure is %s"),qualityMessage); 
    printSerialTelnetLog(tmpStr); yieldTime += yieldOS();   
    if (BMEhum_avail) {
      checkHumidity(bme280_hum,qualityMessage,15);
      snprintf_P(tmpStr, sizeof(tmpStr),PSTR(" Humidity is %s"),qualityMessage); 
      printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
    }
    snprintf_P(tmpStr, sizeof(tmpStr), PSTR("BME280 interval: %d Port: %d SDA %d SCL %d Speed %d CLKStretch %d"), intervalBME280, uint32_t(bme280_port), bme280_i2c[0], bme280_i2c[1], bme280_i2cspeed, bme280_i2cClockStretchLimit ); 
    printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
    snprintf_P(tmpStr, sizeof(tmpStr), PSTR("BME280 last error: %dmin"), (currentTime - bme280_lastError)/60000); 
    printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
  } else {
    printSerialTelnetLogln(F("BME280: not available"));  yieldTime += yieldOS(); 
  }
  printSerialTelnetLogln(FPSTR(singleSeparator)); yieldTime += yieldOS(); 

  if (sgp30_avail && mySettings.useSGP30) {
    char qualityMessage[16];
    snprintf_P(tmpStr, sizeof(tmpStr),PSTR("SGP30 CO2:%hu[ppm] tVOC:%hu[ppb] baseline_CO2:%hu baseline_tVOC:%hu \r\nH2:%hu[ppb] Ethanol:%hu[ppm]"), sgp30.CO2, sgp30.TVOC, sgp30.baselineCO2, sgp30.baselineTVOC,sgp30.H2, sgp30.ethanol);  
    printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
    checkCO2(float(sgp30.CO2),qualityMessage,15);
    snprintf_P(tmpStr, sizeof(tmpStr),PSTR("CO2 conentration is %s, "),qualityMessage); 
    printSerialTelnetLog(tmpStr); yieldTime += yieldOS(); 
    checkTVOC(sgp30.TVOC,qualityMessage,15);  
    snprintf_P(tmpStr, sizeof(tmpStr), PSTR("tVOC conentration is %s"),qualityMessage);  
    printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
    snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SGP30 interval: %d Port: %d SDA %d SCL %d Speed %d CLKStretch %d"), intervalSGP30, uint32_t(sgp30_port), sgp30_i2c[0], sgp30_i2c[1], sgp30_i2cspeed, sgp30_i2cClockStretchLimit);  
    printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
    snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SGP30 last error: %dmin"), (currentTime - sgp30_lastError)/60000); 
    printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
  } else {
    printSerialTelnetLogln(F("SGP30: not available")); yieldTime += yieldOS(); 
  }
  printSerialTelnetLogln(FPSTR(singleSeparator)); yieldTime += yieldOS(); 

  if (ccs811_avail && mySettings.useCCS811) {
    char qualityMessage[16]; 
    uint16_t CO2uI = ccs811.getCO2();
    uint16_t TVOCuI = ccs811.getTVOC();
    switchI2C(ccs811_port, ccs811_i2c[0], ccs811_i2c[1], ccs811_i2cspeed, ccs811_i2cClockStretchLimit);
    uint16_t BaselineuI = ccs811.getBaseline();
    snprintf_P(tmpStr, sizeof(tmpStr),PSTR("CCS811 CO2:%hu tVOC:%hu CCS811_baseline:%hu"), CO2uI, TVOCuI, BaselineuI);
    printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
    checkCO2(float(CO2uI),qualityMessage,15);                                          
    snprintf_P(tmpStr, sizeof(tmpStr),PSTR("CO2 concentration is %s, "), qualityMessage); 
    printSerialTelnetLog(tmpStr); yieldTime += yieldOS(); 
    checkTVOC(TVOCuI,qualityMessage,15);
    snprintf_P(tmpStr, sizeof(tmpStr),PSTR("tVOC concentration is %s"), qualityMessage); 
    printSerialTelnetLogln(tmpStr); yieldTime += yieldOS();     
    snprintf_P(tmpStr, sizeof(tmpStr),PSTR("CCS811 mode: %d Port: %d SDA %d SCL %d Speed %d CLKStretch %d"), ccs811Mode, uint32_t(ccs811_port), ccs811_i2c[0], ccs811_i2c[1], ccs811_i2cspeed, ccs811_i2cClockStretchLimit); 
    printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("[4=0.25s, 3=60s, 2=10sec, 1=1sec]")); yieldTime += yieldOS(); 
    snprintf_P(tmpStr, sizeof(tmpStr), PSTR("CCS811 last error: %dmin"), (currentTime - ccs811_lastError)/60000); 
    printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
  } else {
    printSerialTelnetLogln(F("CCS811: not available")); yieldTime += yieldOS(); 
  }
  printSerialTelnetLogln(FPSTR(singleSeparator)); yieldTime += yieldOS(); 

  if (sps30_avail && mySettings.useSPS30) {
    char qualityMessage[16];
    snprintf_P(tmpStr, sizeof(tmpStr),PSTR("SPS30 P1.0:%4.1f[μg/m3] P2.5:%4.1f[μg/m3] P4.0:%4.1f[μg/m3] P10:%4.1f[μg/m3]"), valSPS30.mc_1p0, valSPS30.mc_2p5, valSPS30.mc_4p0, valSPS30.mc_10p0); 
    printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
    snprintf_P(tmpStr, sizeof(tmpStr),PSTR("SPS30 P.5:%4.1f[#/cm3] P1.0:%4.1f[#/cm3] P2.5:%4.1f[#/cm3] P4.0:%4.1f[#/cm3] P10:%4.1f[#/cm3]"), valSPS30.nc_0p5, valSPS30.nc_1p0, valSPS30.nc_2p5, valSPS30.nc_4p0, valSPS30.nc_10p0); 
    printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
    snprintf_P(tmpStr, sizeof(tmpStr),PSTR("SPS30 Average %4.1f[μm]"), valSPS30.typical_particle_size);
    printSerialTelnetLogln(tmpStr); yieldTime += yieldOS();     
    checkPM2(valSPS30.mc_2p5,qualityMessage,15);   
    snprintf_P(tmpStr, sizeof(tmpStr),PSTR("PM2.5 is %s, "), qualityMessage);  
    printSerialTelnetLog(tmpStr); yieldTime += yieldOS(); 
    checkPM10(valSPS30.mc_10p0,qualityMessage,15);
    snprintf_P(tmpStr, sizeof(tmpStr),PSTR("PM10 is %s"),  qualityMessage); 
    printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
    snprintf_P(tmpStr, sizeof(tmpStr),PSTR("Autoclean interval: %lu"),sps30AutoCleanInterval); 
    printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
    snprintf_P(tmpStr, sizeof(tmpStr),PSTR("SPS30 interval: %d Port: %d SDA %d SCL %d Speed %d CLKStretch %d"), intervalSPS30, uint32_t(sps30_port), sps30_i2c[0], sps30_i2c[1], sps30_i2cspeed, sps30_i2cClockStretchLimit); 
    printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
    snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SPS30 last error: %dmin"), (currentTime - sps30_lastError)/60000); 
    printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
  } else {
    printSerialTelnetLogln(F("SPS30: not available")); yieldTime += yieldOS(); 
  }
  printSerialTelnetLogln(FPSTR(singleSeparator)); yieldTime += yieldOS(); 

  if (therm_avail && mySettings.useMLX) {
    char qualityMessage[16];
    switchI2C(mlx_port, mlx_i2c[0], mlx_i2c[1], mlx_i2cspeed, mlx_i2cClockStretchLimit);
    float tmpOF = therm.object()+mlxOffset;
    float tmpAF = therm.ambient();
    snprintf_P(tmpStr, sizeof(tmpStr),PSTR("MLX Object:%+5.1f[C] MLX Ambient:%+5.1f[C]"), tmpOF, tmpAF); 
    printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
    float tmpEF = therm.readEmissivity();
    snprintf_P(tmpStr, sizeof(tmpStr),PSTR("MLX Emissivity:%4.2f MLX Offset:%4.1f"), tmpEF, mlxOffset); 
    printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
    checkFever(tmpOF,qualityMessage, 15);
    snprintf_P(tmpStr, sizeof(tmpStr),PSTR("Object temperature is %s, "), qualityMessage);  
    printSerialTelnetLog(tmpStr); yieldTime += yieldOS(); 
    checkAmbientTemperature(tmpOF,qualityMessage,15);
    snprintf_P(tmpStr, sizeof(tmpStr),PSTR("Ambient temperature is %s"), qualityMessage); 
    printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
    snprintf_P(tmpStr, sizeof(tmpStr),PSTR("MLX interval: %d Port: %d SDA %d SCL %d Speed %d CLKStretch %d"), intervalMLX, uint32_t(mlx_port), mlx_i2c[0], mlx_i2c[1], mlx_i2cspeed, mlx_i2cClockStretchLimit); 
    printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
    snprintf_P(tmpStr, sizeof(tmpStr), PSTR("MLX last error: %dmin"), (currentTime - mlx_lastError)/60000); 
    printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
  } else {
    printSerialTelnetLogln(F("MLX: not available")); yieldTime += yieldOS(); 
  }  
  printSerialTelnetLogln(FPSTR(singleSeparator)); yieldTime += yieldOS(); 

  if (max30_avail && mySettings.useMAX30) {
    char qualityMessage[16];
    //switchI2C(max30_port, max30_i2c[0], max30_i2c[1], I2C_FAST, I2C_LONGSTRETCH);
    snprintf_P(tmpStr, sizeof(tmpStr),PSTR("MAX interval: %d Port: %d SDA %d SCL %d Speed %d CLKStretch %d"), intervalMAX, uint32_t(max30_port), max30_i2c[0], max30_i2c[1], max30_i2cspeed, max30_i2cClockStretchLimit); 
    printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
  } else {
    printSerialTelnetLogln(F("MAX: not available")); yieldTime += yieldOS(); 
  }
  printSerialTelnetLogln(FPSTR(singleSeparator)); yieldTime += yieldOS(); 

  if (weather_avail && mySettings.useWeather && weather_success) {
    snprintf_P(tmpStr, sizeof(tmpStr),PSTR("Weather: %s"), weatherData.description);
    printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
    snprintf_P(tmpStr, sizeof(tmpStr),PSTR("Temperature:%+5.1f[C] Temperature Max:%+5.1f[C] Temperature Min:%+5.1f[C]"), weatherData.temp, weatherData.tempMax, weatherData.tempMin); 
    printSerialTelnetLogln(tmpStr); yieldTime += yieldOS();     
    snprintf_P(tmpStr, sizeof(tmpStr),PSTR("Pressure:%d Humidity:%d%%"), weatherData.pressure, weatherData.humidity); 
    printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
    snprintf_P(tmpStr, sizeof(tmpStr),PSTR("Wind Speed:%+5.1f[m/s] Direction:%d[deg]"), weatherData.windSpeed, weatherData.windDirection); 
    printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
    snprintf_P(tmpStr, sizeof(tmpStr),PSTR("Visibility :%d[m]"), weatherData.visibility); 
    printSerialTelnetLogln(tmpStr); yieldTime += yieldOS();     
    snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Weather last error: %dmin"), (currentTime - weather_lastError)/60000); 
    printSerialTelnetLogln(tmpStr); yieldTime += yieldOS();     
  } else {
    printSerialTelnetLogln(F("Weather: not available or not synced")); yieldTime += yieldOS(); 
  }
  printSerialTelnetLogln(FPSTR(singleSeparator)); yieldTime += yieldOS(); 

  snprintf_P(tmpStr, sizeof(tmpStr),PSTR("NTP: %s, time is %s, daylight saving time is %s, \r\nfallback is %s"), 
          ntp_avail ? "available" : "not available", 
          timeSynced ? "synchronized" : "not synchronized", 
          (localTime->tm_isdst>0) ? "observed" : "not observed", 
          ntp_onfallback ? "used" : "not used");
  printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 

  if (localTime->tm_wday>=0 && localTime->tm_wday<7 && localTime->tm_mon>=0 && localTime->tm_mon<12) {
    snprintf_P(tmpStr, sizeof(tmpStr),PSTR("%s %s %d %u, "), 
            weekDays[localTime->tm_wday], 
            months[localTime->tm_mon], 
            localTime->tm_mday, 
            (localTime->tm_year+1900));  
    printSerialTelnetLog(tmpStr); yieldTime += yieldOS(); 
  }  
  
  snprintf_P(tmpStr, sizeof(tmpStr),PSTR("%d%d:%d%d:%d%d"), 
         localTime->tm_hour/10 ? localTime->tm_hour/10 : 0 , localTime->tm_hour%10,
         localTime->tm_min/10,    localTime->tm_min%10,
         localTime->tm_sec/10,    localTime->tm_sec%10);
  printSerialTelnetLogln(tmpStr);
  yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("NTP last error: %dmin"), (currentTime - ntp_lastError)/60000); 
  printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 

  R_printSerialTelnetLogln(FPSTR(doubleSeparator)); yieldTime += yieldOS(); 
}

void printState() {
  printSerialTelnetLogln();
  R_printSerialTelnetLogln(FPSTR(doubleSeparator)); yieldTime += yieldOS(); 

  if (wifi_avail && mySettings.useWiFi) {
    snprintf_P(tmpStr, sizeof(tmpStr), PSTR("WiFi status: %s "), WiFi.status() ? FPSTR(mON) : FPSTR(mOFF));  
    printSerialTelnetLog(tmpStr); yieldTime += yieldOS(); 
    snprintf_P(tmpStr, sizeof(tmpStr), PSTR("WiFi host: %s"), hostName); 
    printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
    uint8_t mac[6]; 
    WiFi.macAddress(mac);
    snprintf_P(tmpStr, sizeof(tmpStr), PSTR("WiFi MAC: %02X:%02X:%02X:%02X:%02X:%02X "), mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]); 
    printSerialTelnetLog(tmpStr); yieldTime += yieldOS(); 
    if (wifi_connected) {
      snprintf_P(tmpStr, sizeof(tmpStr), PSTR("is connected to: %s "), WiFi.SSID().c_str()); 
      printSerialTelnetLog(tmpStr); yieldTime += yieldOS(); 
      snprintf_P(tmpStr, sizeof(tmpStr), PSTR("with IP: %s"), WiFi.localIP().toString().c_str()); 
      printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
    } else {
      printSerialTelnetLogln(F("is not connected")); yieldTime += yieldOS(); 
    } 

    printSerialTelnetLogln(FPSTR(doubleSeparator)); yieldTime += yieldOS(); 
    printSerialTelnetLog(F("WiFi:         ")); yieldTime += yieldOS(); 
    switch (stateWiFi){
      case START_UP:         printSerialTelnetLog(FPSTR(mStarting));    break;
      case CHECK_CONNECTION: printSerialTelnetLog(FPSTR(mMonitoring));  break;
      case IS_SCANNING:      printSerialTelnetLog(FPSTR(mScanning));    break;
      case IS_CONNECTING:    printSerialTelnetLog(FPSTR(mConnecting));  break;
      case IS_WAITING:       printSerialTelnetLog(FPSTR(mWaiting));     break;
      default:               printSerialTelnetLog(F("Invalid State"));  break;
    }
    yieldTime += yieldOS(); 

    printSerialTelnetLog(F("mDNS:         "));
    if (mySettings.usemDNS){
      switch (stateMDNS){
        case START_UP:         printSerialTelnetLogln(FPSTR(mStarting));     break;
        case CHECK_CONNECTION: printSerialTelnetLogln(FPSTR(mMonitoring));   break;
        case IS_SCANNING:      printSerialTelnetLogln(FPSTR(mScanning));     break;
        case IS_CONNECTING:    printSerialTelnetLogln(FPSTR(mConnecting));   break;
        case IS_WAITING:       printSerialTelnetLogln(FPSTR(mWaiting));      break;
        default:               printSerialTelnetLog(F("Invalid State"));     break;
      }
    } else {                   printSerialTelnetLogln(FPSTR(mNotEnabled)); }
    yieldTime += yieldOS(); 

    printSerialTelnetLog(F("MQTT:         "));
    if (mySettings.useMQTT){
      switch (stateMQTT){
        case START_UP:         printSerialTelnetLog(FPSTR(mStarting));     break;
        case CHECK_CONNECTION: printSerialTelnetLog(FPSTR(mMonitoring));   break;
        case IS_SCANNING:      printSerialTelnetLog(FPSTR(mScanning));     break;
        case IS_CONNECTING:    printSerialTelnetLog(FPSTR(mConnecting));   break;
        case IS_WAITING:       printSerialTelnetLog(FPSTR(mWaiting));      break;
        default:               printSerialTelnetLog(F("Invalid State"));   break;
      }
    } else {                   printSerialTelnetLog(FPSTR(mNotEnabled)); }
    yieldTime += yieldOS(); 

    printSerialTelnetLog(F("WebSocket:    "));
    if (mySettings.useHTTP){
      switch (stateWebSocket){
        case START_UP:         printSerialTelnetLogln(FPSTR(mStarting));       break;
        case CHECK_CONNECTION: printSerialTelnetLogln(FPSTR(mMonitoring));     break;
        case IS_SCANNING:      printSerialTelnetLogln(FPSTR(mScanning));       break;
        case IS_CONNECTING:    printSerialTelnetLogln(FPSTR(mConnecting));     break;
        case IS_WAITING:       printSerialTelnetLogln(FPSTR(mWaiting));        break;
        default:               printSerialTelnetLog(F("Invalid State"));       break;
      }
    } else {                   printSerialTelnetLogln(FPSTR(mNotEnabled)); }
    yieldTime += yieldOS(); 

    printSerialTelnetLog(F("OTA:          "));
    if (mySettings.useOTA){
      switch (stateOTA){
        case START_UP:         printSerialTelnetLog(FPSTR(mStarting));     break;
        case CHECK_CONNECTION: printSerialTelnetLog(FPSTR(mMonitoring));   break;
        case IS_SCANNING:      printSerialTelnetLog(FPSTR(mScanning));     break;
        case IS_CONNECTING:    printSerialTelnetLog(FPSTR(mConnecting));   break;
        case IS_WAITING:       printSerialTelnetLog(FPSTR(mWaiting));      break;
        default:               printSerialTelnetLog(F("Invalid State"));   break;
      }
    } else {                   printSerialTelnetLog(FPSTR(mNotEnabled)); }
    yieldTime += yieldOS(); 

    printSerialTelnetLog(F("HTTP:         "));
    if (mySettings.useHTTP){
      switch (stateHTTP){
        case START_UP:         printSerialTelnetLogln(FPSTR(mStarting));     break;
        case CHECK_CONNECTION: printSerialTelnetLogln(FPSTR(mMonitoring));   break;
        case IS_SCANNING:      printSerialTelnetLogln(FPSTR(mScanning));     break;
        case IS_CONNECTING:    printSerialTelnetLogln(FPSTR(mConnecting));   break;
        case IS_WAITING:       printSerialTelnetLogln(FPSTR(mWaiting));      break;
        default:               printSerialTelnetLog(F("Invalid State"));     break;
      }
    } else {                   printSerialTelnetLogln(FPSTR(mNotEnabled)); }
    yieldTime += yieldOS(); 

    printSerialTelnetLog(F("HTTP Updater: "));
    if (mySettings.useHTTPUpdater){
      switch (stateHTTPUpdater){
        case START_UP:         printSerialTelnetLog(FPSTR(mStarting));   break;
        case CHECK_CONNECTION: printSerialTelnetLog(FPSTR(mMonitoring)); break;
        case IS_SCANNING:      printSerialTelnetLog(FPSTR(mScanning));   break;
        case IS_CONNECTING:    printSerialTelnetLog(FPSTR(mConnecting)); break;
        case IS_WAITING:       printSerialTelnetLog(FPSTR(mWaiting));    break;
        default:               printSerialTelnetLog(F("Invalid State")); break;
      }
    } else {                   printSerialTelnetLog(FPSTR(mNotEnabled)); }
    yieldTime += yieldOS(); 

    printSerialTelnetLog(F("NTP:          "));
    if (mySettings.useNTP){
      switch (stateNTP){
        case START_UP:         printSerialTelnetLogln(FPSTR(mStarting));    break;
        case CHECK_CONNECTION: printSerialTelnetLogln(FPSTR(mMonitoring));  break;
        case IS_SCANNING:      printSerialTelnetLogln(FPSTR(mScanning));    break;
        case IS_CONNECTING:    printSerialTelnetLogln(FPSTR(mConnecting));  break;
        case IS_WAITING:       printSerialTelnetLogln(FPSTR(mWaiting));     break;
        default:               printSerialTelnetLog(F("Invalid State"));    break;
      }
    } else {                   printSerialTelnetLogln(FPSTR(mNotEnabled)); }
    yieldTime += yieldOS(); 

    printSerialTelnetLog(F("Telnet:       "));
    if (mySettings.useTelnet){
      switch (stateTelnet){
        case START_UP:         printSerialTelnetLog(FPSTR(mStarting));    break;
        case CHECK_CONNECTION: printSerialTelnetLog(FPSTR(mMonitoring));  break;
        case IS_SCANNING:      printSerialTelnetLog(FPSTR(mScanning));    break;
        case IS_CONNECTING:    printSerialTelnetLog(FPSTR(mConnecting));  break;
        case IS_WAITING:       printSerialTelnetLog(FPSTR(mWaiting));     break;
        default:               printSerialTelnetLog(F("Invalid State"));  break;
      }
    } else {                   printSerialTelnetLog(FPSTR(mNotEnabled)); }
    yieldTime += yieldOS(); 

    printSerialTelnetLog(F("Weather:      "));
    if (mySettings.useWeather){
      switch (stateWeather){
        case START_UP:         printSerialTelnetLogln(FPSTR(mStarting));    break;
        case CHECK_CONNECTION: printSerialTelnetLogln(FPSTR(mMonitoring));  break;
        case IS_SCANNING:      printSerialTelnetLogln(FPSTR(mScanning));    break;
        case IS_CONNECTING:    printSerialTelnetLogln(FPSTR(mConnecting));  break;
        case IS_WAITING:       printSerialTelnetLogln(FPSTR(mWaiting));     break;
        default:               printSerialTelnetLogln(F("Invalid State"));  break;
      }
    } else {                   printSerialTelnetLogln(FPSTR(mNotEnabled)); }
    yieldTime += yieldOS(); 

  } // wifi avail

  R_printSerialTelnetLogln(FPSTR(singleSeparator)); //----------------------------------------------------------------//
  yieldTime += yieldOS(); 
  // BME280   12345 
  // BME280 n.a.    
  printSerialTelnetLogln(F("Intervals:"));
  if (mySettings.useBME280 && bme280_avail) { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("BME280   %8lus "),    intervalBME280/1000);  } else { strcpy(tmpStr, ("BME280        n.a. ")); } 
  printSerialTelnetLog(tmpStr);  yieldTime += yieldOS(); 
  if (mySettings.useBME68x && bme68x_avail) { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("BME68x   %5lus "),    intervalBME68x/1000);  } else { strcpy(tmpStr, ("BME68x     n.a. ")); }    
  printSerialTelnetLog(tmpStr);  yieldTime += yieldOS(); 
  if (mySettings.useCCS811 && ccs811_avail) { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("CCS811   %5lus"),     intervalCCS811/1000);  } else { strcpy(tmpStr, ("CCS811     n.a. "));  } 
  printSerialTelnetLogln(tmpStr);  yieldTime += yieldOS(); 
  if (mySettings.useMLX    && therm_avail)  { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("MLX      %8lus "),    intervalMLX/1000);     } else { strcpy(tmpStr, ("MLX           n.a. ")); } 
  printSerialTelnetLog(tmpStr);  yieldTime += yieldOS(); 
  if (mySettings.useSCD30  && scd30_avail)  { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SCD30    %5lus "),    intervalSCD30/1000);   } else { strcpy(tmpStr, ("SCD30      n.a. ")); }    
  printSerialTelnetLog(tmpStr);  yieldTime += yieldOS(); 
  if (mySettings.useSGP30  && sgp30_avail)  { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SGP30    %5lus"),     intervalSGP30/1000);   } else { strcpy(tmpStr, ("SGP30      n.a. "));  } 
  printSerialTelnetLogln(tmpStr);  yieldTime += yieldOS(); 
  if (mySettings.useSPS30  && sps30_avail)  { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SPS30    %8lus "),    intervalSPS30/1000);   } else { strcpy(tmpStr, ("SPS30         n.a. ")); } 
  printSerialTelnetLog(tmpStr);  yieldTime += yieldOS(); 
  if (mySettings.useMAX30  && max30_avail)  { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("MAX30    %5lus "),    intervalMAX30/1000);   } else { strcpy(tmpStr, ("MAX30      n.a. ")); }    
  printSerialTelnetLog(tmpStr);  yieldTime += yieldOS();   
  if (mySettings.useLCD    && lcd_avail)    { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("LCD      %5lus"),     intervalLCD/1000);     } else { strcpy(tmpStr, ("LCD        n.a. "));  } 
  printSerialTelnetLogln(tmpStr);  yieldTime += yieldOS(); 
  if (mySettings.useMQTT)                   { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("MQTT     %8lus "),    intervalMQTT/1000);    } else { strcpy(tmpStr, ("MQTT          n.a. ")); } 
  printSerialTelnetLog(tmpStr);  yieldTime += yieldOS();
  if (mySettings.useWiFi   && wifi_avail)   { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("WiFi     %5lus "),    intervalWiFi/1000);    } else { strcpy(tmpStr, ("WiFi       n.a. "));  } 
  printSerialTelnetLog(tmpStr);  yieldTime += yieldOS(); 
  if (mySettings.useWeather && weather_avail) { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Weather  %5lus"),   intervalWeather/1000); } else { strcpy(tmpStr, ("Weather    n.a. "));  } 
  printSerialTelnetLogln(tmpStr);  yieldTime += yieldOS();  

                                            { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Baseline  %5lumin "), intervalBaseline/60000);                                     }    
                                            printSerialTelnetLog(tmpStr); yieldTime += yieldOS();  
                                            { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("System   %5lus "),    intervalSYS/1000);                                                   }
                                            printSerialTelnetLog(tmpStr); yieldTime += yieldOS(); 
                                            { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Warnings %5lumin"), intervalWarning/60000);                                                   }    
                                            printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
  if (mySettings.useLog)                    { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("LogFile  %6lumin "), INTERVALLOGFILE/60000); } else { strcpy(tmpStr, ("Logfile       n.a. "));   } 
                                            printSerialTelnetLog(tmpStr); yieldTime += yieldOS();   
                                            { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Settings %3luhrs"), intervalSettings/3600000);                                                  }    
                                            printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
                                            
  R_printSerialTelnetLogln(FPSTR(doubleSeparator)); yieldTime += yieldOS();  
} // end print state

void printProfile() {
  printSerialTelnetLogln();
  R_printSerialTelnetLogln(FPSTR(doubleSeparator)); yieldTime += yieldOS();
  
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Average loop %d[us] Avg max loop %d[ms] \r\nCurrent loop Min/Max/Alltime: %d/%d/%d[ms] Current: %d[ms]"), int(myLoopAvg*1000), int(myLoopMaxAvg), myLoopMin, myLoopMax, myLoopMaxAllTime, myLoop); 
  printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Free Heap Size: %d[B] Heap Fragmentation: %d%% Max Block Size: %d[B]"), ESP.getFreeHeap(), ESP.getHeapFragmentation(), ESP.getMaxFreeBlockSize()); printSerialTelnetLogln(tmpStr);
  printSerialTelnetLogln(FPSTR(doubleSeparator)); yieldTime += yieldOS(); 

  // This displays the amount of time it took complete the update routines.
  // The currentMax/alltimeMax is displayed. Alltime max is reset when network services are started up in their update routines.
  printSerialTelnetLogln(F("All values in [ms]"));
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Time&Date:    %4u %4u mDNS:   %4u %4u HTTPupd: %4u %4u HTTP:    %4u %4u\r\nMQTT:         %4u %4u WS:     %4u %4u NTP:     %4u %4u OTA:     %4u %4u\r\nWifi:         %4u %4u Weather:%4u %4u"), 
                    maxUpdateTime, AllmaxUpdateTime, maxUpdatemDNS, AllmaxUpdatemDNS, maxUpdateHTTPUPDATER, AllmaxUpdateHTTPUPDATER, maxUpdateHTTP, AllmaxUpdateHTTP, maxUpdateMQTT, AllmaxUpdateMQTT, 
                    maxUpdateWS, AllmaxUpdateWS, maxUpdateNTP, AllmaxUpdateNTP, maxUpdateOTA, AllmaxUpdateOTA, maxUpdateWifi, AllmaxUpdateWifi,maxUpdateWeather, AllmaxUpdateWeather);         
  printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("mDNS last error: %dmin"), (currentTime - mDNS_lastError)/60000); 
  printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 

  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SCD30:        %4u %4u SGP30:  %4u %4u CCS811:  %4u %4u SPS30:   %4u %4u\r\nBME280:       %4u %4u BME68x: %4u %4u MLX:     %4u %4u MAX:     %4u %4u"), maxUpdateSCD30, AllmaxUpdateSCD30, 
                    maxUpdateSGP30, AllmaxUpdateSGP30, maxUpdateCCS811, AllmaxUpdateCCS811, maxUpdateSPS30, AllmaxUpdateSPS30, maxUpdateBME280, AllmaxUpdateBME280, maxUpdateBME68x, AllmaxUpdateBME68x, maxUpdateMLX, AllmaxUpdateMLX, maxUpdateMAX, AllmaxUpdateMAX);
  printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 

  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("MQTTmsg:      %4u %4u WSmsg:  %4u %4u LCD:     %4u %4u Input:   %4u %4u\r\nRunTime:      %4u %4u EEPROM: %4u %4u"), maxUpdateMQTTMESSAGE, AllmaxUpdateMQTTMESSAGE, maxUpdateWSMESSAGE, AllmaxUpdateWSMESSAGE, 
                    maxUpdateLCD, AllmaxUpdateLCD, maxUpdateINPUT, AllmaxUpdateINPUT, maxUpdateRT, AllmaxUpdateRT, maxUpdateEEPROM, AllmaxUpdateEEPROM);
                    //maxUpdateJS, AllmaxUpdateJS
  printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 

  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Baseline:     %4u %4u Blink:  %4u %4u Reboot:  %4u %4u AllGood: %4u %4u"), 
                    maxUpdateBASE, AllmaxUpdateBASE, maxUpdateBLINK, AllmaxUpdateBLINK, maxUpdateREBOOT, AllmaxUpdateREBOOT, maxUpdateALLGOOD, AllmaxUpdateALLGOOD);
  printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
  
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("This display: %4u"), 
                    maxUpdateSYS);
  printSerialTelnetLogln(tmpStr);  yieldTime += yieldOS(); 

  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Yield Time:   %4u/%u/%u"), 
                    yieldTimeMin, yieldTimeMax, yieldTimeMaxAllTime); 
  printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 

  printSerialTelnetLogln(FPSTR(doubleSeparator)); yieldTime += yieldOS(); 
}

void defaultSettings() {
  mySettings.runTime                       = (unsigned long)   0;
  mySettings.debuglevel                    = (unsigned int)    1;
  mySettings.baselineSGP30_valid           = (uint8_t)         0x00;
  mySettings.baselineeCO2_SGP30            = (uint16_t)        0;
  mySettings.baselinetVOC_SGP30            = (uint16_t)        0;
  mySettings.baselineCCS811_valid          = (uint8_t)         0x00;
  mySettings.baselineCCS811                = (uint16_t)        0;
  mySettings.tempOffset_SCD30_valid        = (uint8_t)         0x00;
  mySettings.tempOffset_SCD30              = (float)           0.01;
  mySettings.forcedCalibration_SCD30_valid = (uint8_t)         0x00; // not used
  mySettings.forcedCalibration_SCD30       = (float)           0.0; // not used
  mySettings.tempOffset_MLX_valid          = (uint8_t)         0x00;
  mySettings.tempOffset_MLX                = (float)           0.0;
  strcpy_P(mySettings.ssid1,                 PSTR("meddev"));
  strcpy_P(mySettings.pw1,                   PSTR(""));
  strcpy_P(mySettings.ssid2,                 PSTR("UAGUest"));
  strcpy_P(mySettings.pw2,                   PSTR(""));
  strcpy_P(mySettings.ssid3,                 PSTR("Jarvis"));
  strcpy_P(mySettings.pw3,                   PSTR(""));
  strcpy_P(mySettings.mqtt_server,           PSTR("my.mqqtt.server.org"));
  strcpy_P(mySettings.mqtt_fallback,         PSTR("192.168.1.1"));
  strcpy_P(mySettings.mqtt_username,         PSTR("mqtt"));
  strcpy_P(mySettings.mqtt_password,         PSTR(""));
  strcpy_P(mySettings.mqtt_mainTopic,        PSTR("Airquality"));
  mySettings.sendMQTTimmediate             = true;
  mySettings.useLCD                        = true;
  mySettings.useWiFi                       = true;
  mySettings.useSCD30                      = true;
  mySettings.useSPS30                      = true;
  mySettings.useSGP30                      = true;
  mySettings.useMAX30                      = true;
  mySettings.useMLX                        = true;
  mySettings.useBME68x                     = true;
  mySettings.useBME280                     = true;
  mySettings.useCCS811                     = true;
  mySettings.notused                       = true;
  mySettings.avgP                          = (float) 100000.0;
  mySettings.useBacklight                  = true;
  mySettings.useBacklightNight             = false;
  mySettings.useBlinkNight                 = false;
  mySettings.nightBegin                    = (uint16_t) 1320;
  mySettings.nightEnd                      = (uint16_t)  420;
  mySettings.rebootMinute                  = (int16_t)    -1;
  mySettings.useHTTP                       = false; 
  mySettings.useNTP                        = false; 
  mySettings.useMQTT                       = false; 
  mySettings.useOTA                        = false; 
  mySettings.usemDNS                       = false; 
  mySettings.useHTTPUpdater                = false; 
  mySettings.useTelnet                     = false; 
  mySettings.useSerial                     = true; 
  mySettings.useLog                        = false; 
  strcpy_P(mySettings.ntpServer,           PSTR("us.pool.ntp.org")); 
  strcpy_P(mySettings.ntpFallback,         PSTR("us.pool.ntp.org")); 
  strcpy_P(mySettings.timeZone,            PSTR("MST7")); // https://raw.githubusercontent.com/nayarsystems/posix_tz_db/master/zones.csv
  mySettings.useWeather                    = false;
  strcpy_P(mySettings.weatherApiKey,       PSTR("")); // 
  strcpy_P(mySettings.weatherCity,         PSTR("Tucson")); // 
  strcpy_P(mySettings.weatherCountryCode,  PSTR("US")); // 
  mySettings.intervalMQTT                  = (float) 1.0;
  mySettings.altitude                      = (float) 0.0;
  mySettings.emissivity                    = (float) 0.98;
  mySettings.LCDdisplayType                = 4;
}
