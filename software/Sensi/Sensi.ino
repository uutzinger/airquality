//////////////////////////////////////////////////////////////////////////////////////////////////////////
// Air Quality Sensors                                                                                  //
//////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Supported Hardware:
//  - ESP8266 micro controller.                     Using 2.x and 3.x Arduino Libary  
//                                                  https://github.com/esp8266/Arduino
//  - SPS30 Senserion particle,                     Paul Van Haastrecht library, modified to accept faulty version information, replaced byte with uint8_t, 
//                                                  https://github.com/uutzinger/sps30.git
//  - SCD30 Senserion CO2,                          Sparkfun library, using interrupt from data ready pin, replaced byte with uint8_t, 
//                                                  https://github.com/uutzinger/SparkFun_SCD30_Arduino_Library.git
//  - SGP30 Senserion VOC, eCO2,                    Sparkfun library, replaced byte with uint8_t, 
//                                                  https://github.com/uutzinger/SparkFun_SGP30_Arduino_Library.git
//  - BME680 Bosch Temp, Humidity, Pressure, VOC,   Adafruit library, modifed include file to allow setting wire interface
//                                                  https://github.com/adafruit/Adafruit_BME680.git
//  - BM[E/P]280 Bosch Temp, [Humidity,] Pressure   Sparkfun library, replaced byte with uint8_t, 
//                                                  https://github.com/uutzinger/SparkFun_BME280_Arduino_Library.git
//  - CCS811 Airquality eCO2 tVOC,                  Sparkfun library, using interrupt from data ready pin
//                                                  https://github.com/sparkfun/SparkFun_CCS811_Arduino_Library.git
//  - MLX90614 Melex temp contactless,              Sparkfun library, replaced byte with uint8_t, 
//                                                  https://github.com/uutzinger/SparkFun_MLX90614_Arduino_Library.git
//  - MAX30105 Maxim pulseox, not implemented yet,  Sparkfun library, replaced byte with uint8_t, 
//                                                  https://github.com/uutzinger/SparkFun_MAX3010x_Sensor_Library.git
//
// Data display through:
//  - LCD 20x4,                                     LiquidCrystal_PCF8574* or Adafruit_LCD library*
//                                                  both modified for wireport selection
//                                                  https://github.com/uutzinger/Adafruit_LiquidCrystal.git
//                                                  https://github.com/uutzinger/LiquidCrystal_PCF8574.git
//
// Network related libraries:
//  - MQTT publication to broker                    PubSubClient library http://pubsubclient.knolleary.net
//  - ESP_Telnet                                    https://github.com/LennartHennigs/ESPTelnet
//  - ESPNTPClient                                  https://github.com/gmag11/ESPNtpClient
//  - ArduWebSockets:                               https://github.com/Links2004/arduinoWebSockets
//
// Other Libraries:
//  - ArduJSON                                      https://github.com/bblanchon/ArduinoJson.git
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
//  Check help-screen (send ? in terminal)
//  3 wifi passwords and 2 mqtt servers.
//  Settings are stored in EEPROM.
//  Settings are saved every couple of hours in EEPROM.
//
// Wifi:
//  Scans network for known ssid, attempts connection with username password, if disconnected attempts reconnection.
//  MQTT server is contacted, if it can not be reached, a backup server is contected. username password is supported.
//  Network Time is obtained using ESPNtpClient and unix system time is converted to local time using time zone settings.
//  Over the air (OTA) programming can be enabled. 
//  HTTPUpdater can be enabled at http://hostname:8890/firmware username admin password ... 
//  mDNS responder and client are implemented, OTA starts them automatically
//  HTML webserver is implemented. Sensors can be read useing hostname/sensor. hostname/ provides self updating root page using websockets.
//  Large html files are streamed from LittleFS.
//
// Wire/I2C:
//  Multiple i2c pins are supported, ESP8266 Arduino supports only one active wire interface, however the pins can
//  be changed prior to each communication. The i2c clock can also be adapted prior to each communcation.
//  At startup, all pin combinations are scanned for possible scl & sda connections of the attached sensors.
//  Pins of supported sensors are registered. An option to provide a delay after switching the wire port is provided.
//
//  Some breakout boards do not work well together.
//  Sensors might engage in excessive clockstretching and some have pull up resistor requirements beyond the one provided 
//  by the microcontroller.
//  For example CCS911 and SCD30 require long clock stretching. They are "slow" sensors. Its best to place them on separate i2c bus.
//  The LCD display requires 5V logic and corrupts regularly when the i2c bus is shared, it should be placed on separate bus.
//  The MLX sensor might report negative or excessive high temperature when combined with arbitrary sensors on the same bus.
//
// Improvements:
//  Some drivers were modified to allow for user specified i2c port instead of standard "Wire" port:
//  Unclear if LiquidCrystal_PCF8574 can be updated 
//  SPS30, library was modified to function with devices that have faulty version information; content of the readoperation is available regradless wheter CRC error occured.
//
// Software implementation:
//  The sensor driver is divided into intializations and update section.
//  The updates are implemented as state machines and called on regular basis in the main loop.
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
//////////////////////////////////////////////////////////////////////////////////////////////////////////
// This software is provided as is, no warranty on its proper operation is implied. Use it at your own risk.
// Urs Utzinger
// Sometime: MAX pulseox software (requires proximit detection and rewritting signal processing library)
// Sometime: Configuration of system through website UI
// Sometime: Support for TFT display
// Sometime: Website auto adjusting to available sensors
// Sometime: Add support for xxx4x sensors from Senirion.
// 2022 June: aadded yield options in main loop to most update sections and i2c calls. Updated index.html, updated ...JSON() functions. Reduced use of String in main loop.
// 2021 October: Logfile to record system issues
// 2021 September: testing and release
// 2021 July: telnet
// 2021 June: webserver, NTP
// 2021 February, March: display update, mDNS, OTA
// 2021 January, i2c scanning and pin switching, OTA flashing
// 2020 Winter, fixing the i2c issues
// 2020 Fall, MQTT and Wifi
// 2020 Summer, first release
//////////////////////////////////////////////////////////////////////////////////////////////////////////

/******************************************************************************************************/
// Build Configuration
/******************************************************************************************************/
#define VER "2.0.2"
// Debug
// -----
#undef DEBUG // disable "crash" DEBUG output for production
//#define DEBUG  // enabling "crash" debug will pin point where in the code the program crashed as it displays 
// a DBG statement before most subroutines. Alternative is to install exception decoder or use both.

// LCD Screen
// ----------
// #define ADALCD  // we have Adafruit i2c to LCD driver
#undef ADALCD      // we have non-Adafruit i2c to LCD driver

// Measure fast or slow
// --------------------
#define FASTMODE true
// true:  Measure with about 5 sec intervals,
// false: Measure in about 1 min intervals and enable sensor sleeping and low energy modes if available
// Slow mode needs tweaking and testing to determine which WiFi services are compatible with it.
// MQTT will need to update with single message. Likley OTA, Updater, Telnet will have issues with slow mode.
// With current software version we can not switch between slow and fast during runtime.

// VS Code
// -------
// If you edit this code with Visual Studio Code, set the define in VSC.h so that it findes the include files.
// To compile the code with Arduino IDE, disable the defines in VSC.h

// Yield
// -----
// This program can experience random crashes within couple of hours or days.
// At many locations in the program, yieldOS was inserted as functions can take more than 100ms to complete.
// yieldOS keeps track when it was called the last time and calls the yield function if interval is excceded.
// yieldI2C was created as it appears switching the wire port and setting new speed requires a delay.
//
// yieldI2C() 
// ----------
//#define yieldI2C() delay(1); lastYield = millis() // yield
#define yieldI2C()                                  // no i2c yield
//
// yieldOS()
// ---------
#define MAXUNTILYIELD 100           // time in ms until yield should be called
#define yieldFunction() delay(1)    // options are delay(0) delay(1) yield() and empty for no yield)
//#define yieldFunction()           // no yield

/******************************************************************************************************/
// Includes
/******************************************************************************************************/
#include "src/Sensi.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/******************************************************************************************************/
// You should not need to change variables and constants below this line.
// Runtime changes to this software are enabled through the terminal.
// Permanent settings for the sensor and display devices can be changed in the 'src' folder in the *.h files.
// You would need an external editor to change the .h files.
/******************************************************************************************************/

// Memory Optimization:
// https://esp8266life.wordpress.com/2019/01/13/memory-memory-always-memory/
// https://arduino-esp8266.readthedocs.io/en/latest/PROGMEM.html
// These explain the use of F() amd PROGMEM and PSTR and sprintf_p().
// Currenlty program takes 51% of dynamic memory and 520k of program storage
// During runtime heapsize and maximum free memory block is around 30k.

/******************************************************************************************************/
// Blinking / Warning
/******************************************************************************************************/
//
// Blinking settings due to sensors warnings
// The implementation allows off/on intervals to be different
#define intervalBlinkOff 500                               // blink off time in ms
#define intervalBlinkOn 9500                               // blink on time in ms

/******************************************************************************************************/
// I2C
/******************************************************************************************************/
//
#include <Wire.h>
TwoWire myWire = TwoWire(); //  inintialize the two wire system.
// We will set multiple i2c ports for each sensor and we will switch pins each time we communicate to 
// an other i2c bus. Unfortunately ESP8266 Arduino supports only one i2c instance because the wire 
// library calls twi.h for which only one instance can run.
// Each time we commounicte with a device we need to set SDA and SCL pins.
#include <SPI.h>

/******************************************************************************************************/
// Program Definitions
/******************************************************************************************************/
// Uses EEPROM and LittleFS to store settings in binary as well as JSON file
// Check src/Config.h for the available settings
#include "src/Config.h"
// --- WiFi, MQTT, NTP, OTA
// Enables most web services available for ESP8266
// It is advisable to initialize only the ones you need in the configuration file.
// It is not advised to run webserver and mqtt client concurrenlty
#include "src/WiFi.h"
#include "src/WebSocket.h"
#include "src/MQTT.h"
#include "src/NTP.h"
#include "src/Telnet.h"
ESP8266WiFiMulti wifiMulti;                                // Switching between Access Points
WebSocketsServer webSocket = WebSocketsServer(81);         // The Websocket interface
WiFiClient WiFiClient;                                     // The WiFi interface
PubSubClient mqttClient(WiFiClient);                       // The MQTT interface
ESPTelnet Telnet;                                          // The Telnet interface
// --- WebServer
#include "src/HTTP.h"                                      // Our HTML webpage contents with javascripts
ESP8266WebServer httpServer(80);                           // Server on port 80
// --- Firmware update through HTTP
#include "src/HTTPUpdater.h"
ESP8266WebServer httpUpdateServer(8890);
ESP8266HTTPUpdateServer httpUpdater;                       // Code update server
// --- Sensors
#include "src/SCD30.h"  // --- SCD30; Sensirion CO2 sensor, Likely  most accurate sensor for CO2
#include "src/SPS30.h"  // --- SPS30; Senserion Particle Sensor,  likely most accurate sensor for PM2. PM110 might not be accurate though.
#include "src/SGP30.h"  // --- SGP30; Senserion TVOC, eCO2, accuracy is not known
#include "src/CCS811.h" // --- CCS811; Airquality CO2 tVOC, traditional make comunity sensor
#include "src/BME680.h" // --- BME680; Bosch Temp, Humidity, Pressure, VOC, more features than 280 sensor, dont need both 280 and 680
#include "src/BME280.h" // --- BME280; Bosch Temp, Humidity, Pressure, there is BME and BMP version.  One is lacking hymidity sensor.
#include "src/MLX.h"    // --- MLX contact less tempreture sensor, Thermal sensor for forehead readings. Likely not accurate.
#include "src/MAX.h"    // --- MAX30105; pulseoximeter, Not implemented yet
// --- LCD Display
// Classic display, uses "old" driver software. Update is slow.
// Maybe replace later with other display.
#if defined(ADALCD)
  #include "Adafruit_LiquidCrystal.h"
  Adafruit_LiquidCrystal lcd(0);                             // 0x20 is added inside driver
#else
  #include "LiquidCrystal_PCF8574.h"
  LiquidCrystal_PCF8574 lcd(0x27);                           // set the LCD address to 0x27 
#endif
#include "src/LCDlayout.h"
#include "src/LCD.h"     // --- LCD 4x20 display
// --- Support functions
#include "src/Quality.h" // --- Signal assessment
#include "src/Print.h" // --- Printing support functions

//////////////////////////////////////////////////////////////////////////////////////////////////////////
// Lets get started
//////////////////////////////////////////////////////////////////////////////////////////////////////////
void setup() {
  lastYield = millis();  
  fastMode = FASTMODE;
  // true:  Measure with about 5 sec intervals,
  // false: Measure in about 1 min intervals and enable sleeping and low energy modes if available

  // Initialize wire interface
  // These settings might get changed inside the sensor drivers and should be reasserted after initialization.
  myWire.begin(D1, D2);
  myWire.setClock(I2C_REGULAR);             // 100kHz or 400kHz speed, we need to use slowest of all sensors
  myWire.setClockStretchLimit(150000); // we need to use largest of all sensors

  /******************************************************************************************************/
  // Configuration setup and read
  /******************************************************************************************************/
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(eepromAddress, mySettings);

  /******************************************************************************************************/
  // Inject hard coded values into the settungs if needed.
  // Useful when changing eeprom settings and wanting to preserve some of the old EEPROM values.
  //mySettings.debuglevel  = 1;          // enable debug regardless of eeprom setting
  //mySettings.useLCD    = true;         // enable/disable sensors even if they are connected
  //mySettings.useWiFi   = true;         //
  //mySettings.useSCD30  = true;         //
  //mySettings.useSPS30  = true;         //
  //mySettings.useSGP30  = true;         //
  //mySettings.useMAX30  = true;         //
  //mySettings.useMLX    = true;         //
  //mySettings.useBME680 = true;         //
  //mySettings.useBME280 = false;        //
  //mySettings.useCCS811 = true;         //
  //mySettings.consumerLCD = true;       //
  //mySettings.sendMQTTimmediate = true; //
  //mySettings.useBacklight = true;      //
  //mySettings.useBacklightNight = true;      //
  //mySettings.useBlinkNight = true;      //
  //strcpy(mySettings.mqtt_mainTopic, "Sensi");
  //mySettings.useMQTT = false;
  //mySettings.useNTP  = true;
  //mySettings.usemDNS = false;
  //mySettings.useHTTP = false;
  //mySettings.useOTA  = false;
  //strcpy(mySettings.ntpServer, "us.pool.ntp.org");
  //mySettings.utcOffset = -7;
  //mySettings.enableDST = false;
  //strcpy(mySettings.timeZone, "MST7"); // https://raw.githubusercontent.com/nayarsystems/posix_tz_db/master/zones.csv
  //mySettings.useHTTPUpdater = false;
  //mySettings.useTelnet = false;
  //mySettings.useSerial = true;
  //mySettings.useLog = false;

  // initialize localTime, logfile printing needs time structure
  actualTime = time(NULL);
  localTime = localtime(&actualTime);        // convert to localtime with daylight saving time

  if (mySettings.useSerial) { 
    Serial.begin(BAUDRATE);  // open serial port at specified baud rate
    Serial.setTimeout(1000); // default is 1000 
  }
  if ( mySettings.debuglevel > 0) { 
    snprintf_P(tmpStr, sizeof(tmpStr), PSTR("\r\nDebug level is: %d"), mySettings.debuglevel);  
    R_printSerialTelnetLogln(tmpStr); 
  }

  /******************************************************************************************************/
  // Start file system and list content
  /******************************************************************************************************/
  fileSystemConfig.setAutoFormat(false);
  LittleFS.setConfig(fileSystemConfig);
  fsOK=LittleFS.begin(); 
  if (!fsOK){ R_printSerialTelnetLogln(F("LittleFS mount failed!")); }
  if (mySettings.debuglevel > 0 && fsOK) {
    R_printSerialTelnetLogln(F("LittleFS started. Contents:"));
    Dir dir = LittleFS.openDir("/");
    bool filefound = false;
    unsigned int fileSize;
    while (dir.next()) {                      // List the file system contents
      String fileName = dir.fileName();
      fileSize = (unsigned int)dir.fileSize();
      snprintf_P(tmpStr, sizeof(tmpStr), PSTR("\tName: %20s, Size: %8u"), fileName.c_str(), fileSize); 
      R_printSerialTelnetLogln(tmpStr);
      filefound = true;
    }
    if (!filefound) { R_printSerialTelnetLogln(F("empty")); }
  }

  //Could also store settings on LittelFS
  //Issue is that LittleFS and JSON library together seem to need more memory than available
  //File myFile = LittleFS.open("/Sensi.config", "r");
  //myFile.read((uint8_t *)&mySettings, sizeof(mySettings));
  //myFile.close();
  
  /******************************************************************************************************/
  // When EEPROM is used for the first time, it has random content.
  // A boolen seems to use one byte of memory in the EEPROM and when you read from EEPROM to a boolen variable 
  // it can become uint_8. When the program sets these boolean values from true to false, it will convert 
  // from 255 to 254 and not to 0.
  // At boot time we  need to ensure that any non zero value for boolean settings are forced to true.
  // Once the varibales a set with code below and written back to EEPROM, this should not be necessary any longer.
  if (mySettings.useLCD            > 0) { mySettings.useLCD            = true; } else { mySettings.useLCD              = false; }
  if (mySettings.useWiFi           > 0) { mySettings.useWiFi           = true; } else { mySettings.useWiFi             = false; }
  if (mySettings.useSCD30          > 0) { mySettings.useSCD30          = true; } else { mySettings.useSCD30            = false; }
  if (mySettings.useSPS30          > 0) { mySettings.useSPS30          = true; } else { mySettings.useSPS30            = false; }
  if (mySettings.useMAX30          > 0) { mySettings.useMAX30          = true; } else { mySettings.useMAX30            = false; }
  if (mySettings.useMLX            > 0) { mySettings.useMLX            = true; } else { mySettings.useMLX              = false; }
  if (mySettings.useBME680         > 0) { mySettings.useBME680         = true; } else { mySettings.useBME680           = false; }
  if (mySettings.useBME280         > 0) { mySettings.useBME280         = true; } else { mySettings.useBME280           = false; }
  if (mySettings.useCCS811         > 0) { mySettings.useCCS811         = true; } else { mySettings.useCCS811           = false; }
  if (mySettings.sendMQTTimmediate > 0) { mySettings.sendMQTTimmediate = true; } else { mySettings.sendMQTTimmediate   = false; }
  if (mySettings.consumerLCD       > 0) { mySettings.consumerLCD       = true; } else { mySettings.consumerLCD         = false; }
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

  /******************************************************************************************************/
  // Check which devices are attached to the I2C pins, this self configures our connections to the sensors
  /******************************************************************************************************/
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

  printSerialTelnetLogln();
  for (uint8_t i = 0; i < sizeof(portArray); i++) {
    for (uint8_t j = 0; j < sizeof(portArray); j++) {
      if (i != j) {
        snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Scanning (SDA:SCL) - %s:%s"), portMap[i], portMap[j]); 
        printSerialTelnetLogln(tmpStr);
        Wire.begin(portArray[i], portArray[j]);
        Wire.setClock(I2C_SLOW);
        for (address = 1; address < 127; address++ )  {
          Wire.beginTransmission(address);
          error = Wire.endTransmission();
          if (error == 0) {
            // found a device
            if      (address == 0x20) {
              lcd_avail  = true;  // LCD display Adafruit
              lcd_port   = &myWire;    lcd_i2c[0] = portArray[i];    lcd_i2c[1] = portArray[j];
            } else if (address == 0x27) {
              lcd_avail  = true;  // LCD display PCF8574
              lcd_port   = &myWire;    lcd_i2c[0] = portArray[i];    lcd_i2c[1] = portArray[j];
            } else if (address == 0x57) {
              max_avail  = true;  // MAX 30105 Pulseox & Particle
              max_port   = &myWire;    max_i2c[0] = portArray[i];    max_i2c[1] = portArray[j];
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
              bme680_avail  = true;  // Bosch Temp, Humidity, Pressure, VOC
              bme680_port   = &myWire; bme680_i2c[0] = portArray[i]; bme680_i2c[1] = portArray[j];
            } else {
              snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Found unknonw device - %d!"),address); 
              printSerialTelnetLog(tmpStr);
            }
          }
        }
      }
    }
  }

  // List the devices we found
  if (lcd_avail)    { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("LCD                  available: %d SDA %d SCL %d"), uint32_t(lcd_port),    lcd_i2c[0],    lcd_i2c[1]);    } else { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("LCD                  not available")); }  
  printSerialTelnetLogln(tmpStr);
  if (max_avail)    { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("MAX30105             available: %d SDA %d SCL %d"), uint32_t(max_port),    max_i2c[0],    max_i2c[1]);    } else { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("MAX30105             not available")); }  
  printSerialTelnetLogln(tmpStr);
  if (ccs811_avail) { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("CCS811 eCO2, tVOC    available: %d SDA %d SCL %d"), uint32_t(ccs811_port), ccs811_i2c[0], ccs811_i2c[1]); } else { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("CCS811 eCO2, tVOC    not available")); }  
  printSerialTelnetLogln(tmpStr);
  if (sgp30_avail)  { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SGP30 eCO2, tVOC     available: %d SDA %d SCL %d"), uint32_t(sgp30_port),  sgp30_i2c[0],  sgp30_i2c[1]);  } else { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SGP30 eCO2, tVOC     not available")); }  
  printSerialTelnetLogln(tmpStr);
  if (therm_avail)  { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("MLX temp             available: %d SDA %d SCL %d"), uint32_t(mlx_port),    mlx_i2c[0],    mlx_i2c[1]);    } else { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("MLX temp             not available")); }  
  printSerialTelnetLogln(tmpStr);
  if (scd30_avail)  { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SCD30 CO2, rH        available: %d SDA %d SCL %d"), uint32_t(scd30_port),  scd30_i2c[0],  scd30_i2c[1]);  } else { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SCD30 CO2, rH        not available")); }
  printSerialTelnetLogln(tmpStr);
  if (sps30_avail)  { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SPS30 PM             available: %d SDA %d SCL %d"), uint32_t(sps30_port),  sps30_i2c[0],  sps30_i2c[1]);  } else { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SPS30 PM             not available")); }  
  printSerialTelnetLogln(tmpStr);
  if (bme280_avail) { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("BM[E/P]280 T, P[,rH] available: %d SDA %d SCL %d"), uint32_t(bme280_port), bme280_i2c[0], bme280_i2c[1]); } else { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("BM[E/P]280 T, P[,rH] not available")); }  
  printSerialTelnetLogln(tmpStr);
  if (bme680_avail) { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("BME680 T, rH, P tVOC available: %d SDA %d SCL %d"), uint32_t(bme680_port), bme680_i2c[0], bme680_i2c[1]); } else { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("BME680 T, rH, P tVOC not available")); }  
  printSerialTelnetLogln(tmpStr);

  /******************************************************************************************************/
  // Time Intervals for Loop, Display and MQTT publishing, keeping track of runtime
  /******************************************************************************************************/
  // Software is designed to run either with frequent updates or in slow mode with updates in the minutes

  // Update rate of devices is defined in their respective .h files
  // Theser are updates created directly in the main loop
  if (fastMode == true) {                                     // fast mode -----------------------------------
    intervalLoop     =                  100;                  // 0.1 sec, main loop runs 10 times per second
    intervalLCD      =      intervalLCDFast;                  // LCD display is updated every 10 seconds
    intervalMQTT     =     intervalMQTTFast;                  // 
    intervalRuntime  =                60000;                  // 1 minute, uptime is updted every minute
    intervalBaseline = intervalBaselineFast;                  // 10 minutes
  } else {                                                    // slow mode -----------------------------------
    intervalLoop     =                 1000;                  // 1 sec, main loop runs once a second, ESP needs main loop to run faster than 100ms otherwise yield is needed
    intervalLCD      =      intervalLCDSlow;                  // 1 minute
    intervalMQTT     =     intervalMQTTSlow;                  //
    intervalRuntime  =               600000;                  // 10 minutes
    intervalBaseline = intervalBaselineSlow;                  // 1 hr
  }

  /******************************************************************************************************/
  // Initialize all devices
  /******************************************************************************************************/
  // Adafruit hack for BME680:
  if (bme680_avail) { bme680.setWire(bme680_port); }

  if (lcd_avail && mySettings.useLCD)       { if (initializeLCD()    == false) { lcd_avail = false;    } } else { lcd_avail    = false; }  // Initialize LCD Screen
  D_printSerialTelnet(F("DBG:INI: LCD.."));
  if (scd30_avail && mySettings.useSCD30)   { if (initializeSCD30()  == false) { scd30_avail = false;  } } else { scd30_avail  = false; }  // Initialize SCD30 CO2 sensor
  D_printSerialTelnet(F("DBG:INI: SCD30.."));
  if (sgp30_avail && mySettings.useSGP30)   { if (initializeSGP30()  == false) { sgp30_avail = false;  } } else { sgp30_avail  = false; }  // SGP30 Initialize
  D_printSerialTelnet(F("DBG:INI: SGP30.."));
  if (ccs811_avail && mySettings.useCCS811) { if (initializeCCS811() == false) { ccs811_avail = false; } } else { ccs811_avail = false; }  // CCS811 Initialize
  D_printSerialTelnet(F("DBG:INI: CCS811.."));
  if (sps30_avail && mySettings.useSPS30)   { if (initializeSPS30()  == false) { sps30_avail = false;  } } else { sps30_avail  = false; }  // SPS30 Initialize Particle Sensor
  D_printSerialTelnet(F("DBG:INI: SPS30.."));
  if (bme680_avail && mySettings.useBME680) { if (initializeBME680() == false) { bme680_avail = false; } } else { bme680_avail = false; }  // Initialize BME680
  D_printSerialTelnet(F("DBG:INI: BME680.."));
  if (bme280_avail && mySettings.useBME280) { if (initializeBME280() == false) { bme280_avail = false; } } else { bme280_avail = false; }  // Initialize BME280
  D_printSerialTelnet(F("DBG:INI: BME280.."));
  if (therm_avail && mySettings.useMLX)     { if (initializeMLX()    == false) { therm_avail = false;  } } else { therm_avail  = false; }  // Initialize MLX Sensor
  D_printSerialTelnet(F("DBG:INI: MLX.."));
  if (max_avail && mySettings.useMAX30)     { if (false == false)              { max_avail = false;    } } else { max_avail    = false; }  // Initialize MAX Pulse OX Sensor, N.A.
  D_printSerialTelnet(F("DBG:INI: MAX30.."));

  /******************************************************************************************************/
  // Make sure i2c bus was not changed during above's initializations.
  // These settings will work for all sensors supported.
  // Since one has only one wire instance available on EPS8266, it does not make sense to operate the
  // pins at differnt clock speeds.
  /******************************************************************************************************/
  myWire.setClock(I2C_REGULAR);             // in Hz
  myWire.setClockStretchLimit(150000);      // in micro seconds

  /******************************************************************************************************/
  // Initialize Timing System
  /******************************************************************************************************/
  currentTime         = millis();
  lastcurrentTime     = currentTime;
  lastTime            = currentTime;
  lastSCD30           = currentTime;
  lastLCD             = currentTime;
  lastPressureSCD30   = currentTime;
  lastSaveSettings    = currentTime;
  lastSaveSettingsJSON= currentTime;
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
  lastHTTP            = currentTime;
  lastMDNS            = currentTime;
  lastHTTP            = currentTime;
  lastMQTTPublish     = currentTime;
  lastBlink           = currentTime;
  lastTelnet          = currentTime;
  lastBaseline        = currentTime;
  lastWebSocket       = currentTime;
  lastWarning         = currentTime;
  lastLogFile         = currentTime;
  lastSerialInput     = currentTime;
  lastTelnetInput     = currentTime;

  // how often do we reset the cpu usage time counters for the subroutines?
  intervalSYS                                     = intervalWiFi;
  if (intervalBME280 > intervalSYS) { intervalSYS = intervalBME280; }
  if (intervalBME680 > intervalSYS) { intervalSYS = intervalBME680; }
  if (intervalCCS811 > intervalSYS) { intervalSYS = intervalCCS811; }
  if (intervalLCD    > intervalSYS) { intervalSYS = intervalLCD;    }
  if (intervalMAX30  > intervalSYS) { intervalSYS = intervalMAX30;  }
  if (intervalMLX    > intervalSYS) { intervalSYS = intervalMLX;    }
  if (intervalMQTT   > intervalSYS) { intervalSYS = intervalMQTT;   }
  if (intervalSCD30  > intervalSYS) { intervalSYS = intervalSCD30;  }
  if (intervalSGP30  > intervalSYS) { intervalSYS = intervalSGP30;  }
  if (intervalSPS30  > intervalSYS) { intervalSYS = intervalSPS30;  }

  /******************************************************************************************************/
  // Populate LCD screen, start with cleared LCD
  /******************************************************************************************************/
  D_printSerialTelnet(F("D:S:LCD.."));
  if (lcd_avail && mySettings.useLCD) {
    if (mySettings.consumerLCD) { 
      updateSinglePageLCDwTime(); 
    } else { 
      updateLCD(); 
    }
    if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("LCD updated")); }
  }
  
  /******************************************************************************************************/
  // Connect to WiFi
  /******************************************************************************************************/
  D_printSerialTelnet(F("D:I:Wifi.."));
  initializeWiFi(); // if user request wifi to be off, functions depebding on wifi will not be enabled

  /******************************************************************************************************/
  // System is initialized
  /******************************************************************************************************/
  if ( (mySettings.debuglevel > 0) && mySettings.useSerial ) {  R_printSerialTelnetLogln(F("System initialized")); }
  D_printSerialTelnet(F("D:I:CMPLT.."));

  scheduleReboot = false;

  // give user time to obeserve boot information
  if ( (mySettings.debuglevel > 0) && mySettings.useSerial ) { serialTrigger(waitmsg, 5000); }

} // end setup

void loop() {
  //////////////////////////////////////////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////////////////////////////////
  currentTime = lastYield = millis(); // keep track of local time
  
  if (otaInProgress) { updateOTA(); } // when OTA is in progress we do not do anything else
  else { // OTA is not in progress, we update the subsystems

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
      actualTime = time(NULL);
      localTime = localtime(&actualTime);        // convert to localtime with daylight saving time
      time_avail = true;
      
      // every minute, broadcast time, window to occur is 0..29 secs
      // we need to give window because some service routines of ESP might block execution for several seconds
      // and we can not guarantee that we reach this code at the exactly specified second
      if (localTime->tm_sec < 30) {
        if (updateTime == true) {
          timeNewDataWS = true; //brodcast time on websocket 
          timeNewData = true;   //brodcast time on mqtt
          updateTime = false;
        } 
      } else {
        if (updateTime == false) { updateTime = true; }
      }

      // at midnight update date, window to occur is one minute
      if (localTime->tm_hour == 0) {
        if (localTime->tm_min == 0) {
          if (updateDate == true) {
            dateNewDataWS = true; // broadcast date on websocket
            dateNewData = true;   // broadcast date on mqtt
            updateDate =  false;
          }
        } else if ( (localTime->tm_min >= 1) && (updateDate==false) ) { updateDate = true; }
      }       

      deltaUpdate = millis() - startUpdate; // this code segment took delta ms
      if (maxUpdateTime    < deltaUpdate) { maxUpdateTime = deltaUpdate; } // current max updatetime for this code segment
      if (AllmaxUpdateTime < deltaUpdate) { AllmaxUpdateTime = deltaUpdate; } // longest global updatetime for this code segement

      D_printSerialTelnet(F("D:S:NTP.."));
      yieldTime += yieldOS(); 

    }
    
    /******************************************************************************************************/
    // Update the State Machines for all Devices and System Processes
    /******************************************************************************************************/
    // Update Wireless Services 
    if (wifi_avail && mySettings.useWiFi) { 
      startUpdate = millis();      
      updateWiFi(); //<<<<<<<<<<<<<<<<<<<<--------------- WiFi update, this will reconnect if disconnected
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateWifi    < deltaUpdate) { maxUpdateWifi = deltaUpdate; }
      if (AllmaxUpdateWifi < deltaUpdate) { AllmaxUpdateWifi = deltaUpdate; }
      yieldTime += yieldOS(); 
    } 
    if (wifi_avail     && mySettings.useWiFi && mySettings.useOTA)    { 
      startUpdate = millis();
      updateOTA(); //<<<<<<<<<<<<<<<<<<<<---------------- Update OTA --------------------------------------
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateOTA    < deltaUpdate) { maxUpdateOTA = deltaUpdate; }
      if (AllmaxUpdateOTA < deltaUpdate) { AllmaxUpdateOTA = deltaUpdate; }
      yieldTime += yieldOS(); 
    } 
    if (wifi_avail     && mySettings.useWiFi && mySettings.useNTP)    { 
      startUpdate = millis();
      updateNTP();  //<<<<<<<<<<<<<<<<<<<<--------------- Update Network Time -----------------------------
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateNTP    < deltaUpdate) { maxUpdateNTP = deltaUpdate; }
      if (AllmaxUpdateNTP < deltaUpdate) { AllmaxUpdateNTP = deltaUpdate; }
      yieldTime += yieldOS(); 
    } 
    if (wifi_avail     && mySettings.useWiFi && mySettings.useMQTT)   { 
      startUpdate = millis();
      updateMQTT(); //<<<<<<<<<<<<<<<<<<<<--------------- MQTT update serve connection --------------------
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateMQTT    < deltaUpdate) { maxUpdateMQTT = deltaUpdate; }
      if (AllmaxUpdateMQTT < deltaUpdate) { AllmaxUpdateMQTT = deltaUpdate; }
      // yield inside updateMQTT
      // yieldTime += yieldOS();
    } 
    if (wifi_avail     && mySettings.useWiFi && mySettings.useHTTP)   { 
      startUpdate = millis();
      updateWebSocket(); //<<<<<<<<<<<<<<<<<<<<---------- Websocket update server connection --------------
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateWS    < deltaUpdate) { maxUpdateWS = deltaUpdate; }
      if (AllmaxUpdateWS < deltaUpdate) { AllmaxUpdateWS = deltaUpdate; }
      // yields inside updatewebsocket
      // yieldTime += yieldOS(); 
    } 
    if (wifi_avail     && mySettings.useWiFi && mySettings.useHTTP)   { 
      startUpdate = millis();
      updateHTTP(); //<<<<<<<<<<<<<<<<<<<<--------------- Update web server -------------------------------
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateHTTP    < deltaUpdate)  { maxUpdateHTTP = deltaUpdate; }
      if (AllmaxUpdateHTTP < deltaUpdate) { AllmaxUpdateHTTP = deltaUpdate; }
      // might not be needed, http is event driven in the system routines
      yieldTime += yieldOS(); 
    } 
    if (wifi_avail     && mySettings.useWiFi && mySettings.useHTTPUpdater)   { 
      startUpdate = millis();
      updateHTTPUpdater();  //<<<<<<<<<<<<<<<<<<<<------- Update firmware web server ----------------------
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateHTTPUPDATER    < deltaUpdate) { maxUpdateHTTPUPDATER = deltaUpdate; }
      if (AllmaxUpdateHTTPUPDATER < deltaUpdate) { AllmaxUpdateHTTPUPDATER = deltaUpdate; }
      yieldTime += yieldOS(); 
    } 
    if (wifi_avail     && mySettings.useWiFi && mySettings.usemDNS)   { 
      startUpdate = millis();
      updateMDNS(); //<<<<<<<<<<<<<<<<<<<<--------------- Update mDNS -------------------------------------
      deltaUpdate = millis() - startUpdate;
      if (maxUpdatemDNS    < deltaUpdate) { maxUpdatemDNS = deltaUpdate; }
      if (AllmaxUpdatemDNS < deltaUpdate) { AllmaxUpdatemDNS = deltaUpdate; }
      yieldTime += yieldOS(); 
    } 
    if (wifi_avail     && mySettings.useWiFi && mySettings.useTelnet)    { 
      startUpdate = millis();
      updateTelnet();  //<<<<<<<<<<<<<<<<<<<<------------ Update Telnet -----------------------------------
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateTelnet    < deltaUpdate) { maxUpdateTelnet = deltaUpdate; }
      if (AllmaxUpdateTelnet < deltaUpdate) { AllmaxUpdateTelnet = deltaUpdate; }
      yieldTime += yieldOS(); 
    }
    
    /******************************************************************************************************/
    // Update Sensor Readings
    /******************************************************************************************************/
    
    if (scd30_avail    && mySettings.useSCD30)                        { 
      D_printSerialTelnet(F("D:U:SCD30.."));
      startUpdate = millis();
      if (updateSCD30()  == false) {scheduleReboot = true;}  //<<<<<<<<<<<<<<<<<< SCD30 Sensirion CO2 sensor
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateSCD30    < deltaUpdate) { maxUpdateSCD30 = deltaUpdate; }
      if (AllmaxUpdateSCD30 < deltaUpdate) { AllmaxUpdateSCD30 = deltaUpdate; }
      yieldTime += yieldOS(); 
    } 
    if (sgp30_avail    && mySettings.useSGP30)                        { 
      D_printSerialTelnet(F("D:U:SGP30.."));
      startUpdate = millis();
      if (updateSGP30()  == false) {scheduleReboot = true;}  //<<<<<<<<<<<<<<<<< SGP30 Sensirion eCO2 sensor
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateSGP30 < deltaUpdate)    { maxUpdateSGP30 = deltaUpdate; }
      if (AllmaxUpdateSGP30 < deltaUpdate) { AllmaxUpdateSGP30 = deltaUpdate; }
      yieldTime += yieldOS(); 
    } 
    if (ccs811_avail   && mySettings.useCCS811)                       { 
      D_printSerialTelnet(F("D:U:CCS811.."));
      startUpdate = millis();
      if (updateCCS811() == false) {scheduleReboot = true;}  //<<<<<<<<<<<<<<<<<<<<------ CCS811 eCO2 sensor
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateCCS811    < deltaUpdate)    { maxUpdateCCS811 = deltaUpdate; }
      if (AllmaxUpdateCCS811 < deltaUpdate) { AllmaxUpdateCCS811 = deltaUpdate; }
      yieldTime += yieldOS(); 
    }
    if (sps30_avail    && mySettings.useSPS30)                        { 
      D_printSerialTelnet(F("D:U:SPS30.."));
      startUpdate = millis();
      if (updateSPS30()  == false) {scheduleReboot = true;}  //<<<<<<<<<<<<< SPS30 Sensirion Particle sensor
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateSPS30    < deltaUpdate) { maxUpdateSPS30 = deltaUpdate; }
      if (AllmaxUpdateSPS30 < deltaUpdate) { AllmaxUpdateSPS30 = deltaUpdate; }
      yieldTime += yieldOS(); 
    } // -------------------------------------------------------------- 
    if (bme280_avail   && mySettings.useBME280)                       { 
      D_printSerialTelnet(F("D:U:BME280.."));
      startUpdate = millis();
      if (updateBME280() == false) {scheduleReboot = true;}  //<<<<<<<<<<<<<<<<<<<< BME280, Hum, Press, Temp
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateBME280    < deltaUpdate) { maxUpdateBME280 = deltaUpdate; }
      if (AllmaxUpdateBME280 < deltaUpdate) { AllmaxUpdateBME280 = deltaUpdate; }
      yieldTime += yieldOS(); 
    } 
    if (bme680_avail   && mySettings.useBME680)                       { 
      D_printSerialTelnet(F("D:U:BME680.."));
      startUpdate = millis();
      if (updateBME680() == false) {scheduleReboot = true;}  //<<<<< BME680, Hum, Press, Temp, Gasresistance
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateBME680    < deltaUpdate) { maxUpdateBME680 = deltaUpdate; }
      if (AllmaxUpdateBME680 < deltaUpdate) { AllmaxUpdateBME680 = deltaUpdate; }      
      yieldTime += yieldOS(); 
    }
    if (therm_avail    && mySettings.useMLX)                          { 
      D_printSerialTelnet(F("D:U:THERM.."));
      startUpdate = millis();
      if (updateMLX()    == false) {scheduleReboot = true;}  //<<<<<<<<<<<<<< MLX Contactless Thermal Sensor
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateMLX    < deltaUpdate) { maxUpdateMLX = deltaUpdate; }      
      if (AllmaxUpdateMLX < deltaUpdate) { AllmaxUpdateMLX = deltaUpdate; }      
      yieldTime += yieldOS(); 
    } 
    if (max_avail      && mySettings.useMAX30)                        {
      D_printSerialTelnet(F("D:U:MAX30.."));
      startUpdate = millis();
      // goes here // <<<<<<<<<<<<------------------------------------------------------ MAX Pulse Ox Sensor      
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateMAX    < deltaUpdate) { maxUpdateMAX = deltaUpdate; } 
      if (AllmaxUpdateMAX < deltaUpdate) { AllmaxUpdateMAX = deltaUpdate; }
      yieldTime += yieldOS(); 
    } 
    
    /******************************************************************************************************/
    // Broadcast MQTT and WebSocket Messages
    /******************************************************************************************************/
    
    if (mqtt_connected)                                               {
      D_printSerialTelnet(F("D:U:MQTT.."));
      startUpdate = millis();
      updateMQTTMessage();  //<<<<<<<<<<<<<<<<<<<<----------------------------------- MQTT send sensor data
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateMQTTMESSAGE    < deltaUpdate) { maxUpdateMQTTMESSAGE = deltaUpdate; }
      if (AllmaxUpdateMQTTMESSAGE < deltaUpdate) { AllmaxUpdateMQTTMESSAGE = deltaUpdate; }
      // yieldTime += yieldOS(); // is inside updateMQTTMessage()
    }
    if (ws_connected)                                                 { 
      D_printSerialTelnet(F("D:U:WS.."));
      startUpdate = millis();      
      updateWebSocketMessage();  //<<<<<<<<<<<<<<<<<<<<-------------------------- WebSocket send sensor data
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateWSMESSAGE    < deltaUpdate) { maxUpdateWSMESSAGE = deltaUpdate; }
      if (AllmaxUpdateWSMESSAGE < deltaUpdate) { AllmaxUpdateWSMESSAGE = deltaUpdate; }
      // yieldTime += yieldOS(); // is inside updateWebSocketMessage 
    }
    
    /******************************************************************************************************/
    // LCD: Display Sensor Data
    /******************************************************************************************************/
    
    if (lcd_avail && mySettings.useLCD) {
      if ((currentTime - lastLCD) >= intervalLCD) {
        lastLCD = currentTime;
        D_printSerialTelnet(F("D:U:LCD.."));
        startUpdate = millis();
        if (  mySettings.consumerLCD ) { updateSinglePageLCDwTime(); } else { updateLCD(); } //<<<<<<------
        // yieldTime += yieldOS(); // is inside the update routine
        // adjust LCD light if needed
        if (lastLCDInten) { // LCD light is ON --------------------------
          if ( (mySettings.useBacklight == false) || // dont want backlight at all
               // or dont want backlight at night
               ( (mySettings.useBacklightNight == false) && ( (localTime->tm_hour*60+localTime->tm_min > mySettings.nightBegin) && (localTime->tm_hour*60+localTime->tm_min < mySettings.nightEnd) ) )  
             ) { // TURN OFF LCD light
               switchI2C(lcd_port, lcd_i2c[0], lcd_i2c[1], lcd_i2cspeed, lcd_i2cClockStretchLimit);
               #if defined(ADALCD)
                 lcd.setBacklight(LOW);
               #else
                 lcd.setBacklight(0);
               #endif
               lastLCDInten = false;
               yieldTime += yieldOS(); 
          }
        } else {            // LCD light is OFF -------------------------
          if (blinkLCD == false) { // dont change light when it could be off/on due to blinking
            if (mySettings.useBacklight == true) { // backlight is enabled
              if ( (mySettings.useBacklightNight == true) || // backlight on also at night
                   // or backlight on during the day
                   ( (mySettings.useBacklightNight == false) && ( (localTime->tm_hour*60+localTime->tm_min < mySettings.nightBegin) && (localTime->tm_hour*60+localTime->tm_min > mySettings.nightEnd) ) ) 
                 ) { // TURN ON LCD light
                   switchI2C(lcd_port, lcd_i2c[0], lcd_i2c[1], lcd_i2cspeed, lcd_i2cClockStretchLimit);
                   #if defined(ADALCD)
                     lcd.setBacklight(HIGH);
                   #else
                     lcd.setBacklight(255);
                   #endif
                   lastLCDInten = true;
                   yieldTime += yieldOS(); 
              } // end turn it on  
            } // end backlight is enabled
          } // end its not blinking
        } // end backlight is off
        
        if ( (mySettings.debuglevel > 1) && mySettings.useSerial ) { R_printSerialTelnetLogln(F("LCD updated")); }
        deltaUpdate = millis() - startUpdate;
        if (maxUpdateLCD    < deltaUpdate) { maxUpdateLCD = deltaUpdate; }
        if (AllmaxUpdateLCD < deltaUpdate) { AllmaxUpdateLCD = deltaUpdate; }
      }
    }
  
    /******************************************************************************************************/
    // Terminal Status Display
    /******************************************************************************************************/
    
    if ((currentTime - lastSYS) >= intervalSYS) {
      lastSYS = currentTime;
      
      if (mySettings.debuglevel == 99) { // update continously
        D_printSerialTelnet(F("D:U:SYS.."));
        startUpdate = millis();
        printProfile();
        // yieldTime += yieldOS(); // is inside the printProfile routine
        printState();        
        // yieldTime += yieldOS(); // is inside the printState routine
        printSensors();
        // yieldTime += yieldOS(); // is inside the printSensors routine
        deltaUpdate = millis() - startUpdate;
        if (maxUpdateSYS < deltaUpdate) { maxUpdateSYS = deltaUpdate; }
      } // dbg level 99
      
      // reset max values and measure them again until next display
      // AllmaxUpdate... are the runtime maxima that are not reset between each display update
      yieldTimeMin = yieldTimeMax = 0;
      myDelayMin = intervalSYS; myLoopMin = intervalSYS; myLoopMax = 0;      
      maxUpdateTime = maxUpdateWifi = maxUpdateOTA =  maxUpdateNTP = maxUpdateMQTT = maxUpdateHTTP = maxUpdateHTTPUPDATER = maxUpdatemDNS = 0;
      maxUpdateSCD30 = maxUpdateSGP30 =  maxUpdateCCS811 = maxUpdateSPS30 = maxUpdateBME280 = maxUpdateBME680 = maxUpdateMLX = maxUpdateMAX = 0;
      maxUpdateMQTTMESSAGE = maxUpdateWSMESSAGE = maxUpdateLCD = maxUpdateINPUT = maxUpdateRT = maxUpdateEEPROM = maxUpdateBASE = maxUpdateBLINK = maxUpdateREBOOT = maxUpdateALLGOOD = 0;
      //maxUpdateJS
    }
  
    /******************************************************************************************************/
    // Serial or Telnet User Input
    /******************************************************************************************************/
    
    D_printSerialTelnet(F("D:U:SER.."));
    startUpdate = millis();

    // Serial input capture
    // Telnet input is captured by telnet server (no coded here )
    if (Serial.available()) {
      int bytesread = Serial.readBytesUntil('\n', serialInputBuff, 63);  // Read from serial until newline is read or timeout exceeded
      while (Serial.available() > 0) { Serial.read(); }                  // Clear remaining data in input buffer
      serialInputBuff[bytesread] = '\0';
      serialReceived = true;
      yieldTime += yieldOS(); 
    } else {
      serialReceived = false;
    }
    
    inputHandle(); // Serial input handling
    // yieldTime += yieldOS(); // is inside the input Handle routine

    deltaUpdate = millis() - startUpdate;
    if (maxUpdateINPUT < deltaUpdate) { maxUpdateINPUT = deltaUpdate; }
    if (AllmaxUpdateINPUT < deltaUpdate) { AllmaxUpdateINPUT = deltaUpdate; }

    /******************************************************************************************************/
    // Other Time Managed Events such as runtime, saving baseline, rebooting, blinking LCD for warning
    /******************************************************************************************************/
  
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
      EEPROM.put(0, mySettings);
      if (EEPROM.commit()) {
        lastSaveSettings = currentTime;
        if (mySettings.debuglevel > 1) { R_printSerialTelnetLog(F("EEPROM updated")); }
      }
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateEEPROM    < deltaUpdate) { maxUpdateEEPROM = deltaUpdate; }
      if (AllmaxUpdateEEPROM < deltaUpdate) { AllmaxUpdateEEPROM = deltaUpdate; }
      yieldTime += yieldOS(); 
    }
    
    /** JSON savinge to LittelFS crashes ESP8266
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
    // if open close it, we dont close the logfile after each write
    // if size larger than threshold create back up
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
    if ((currentTime - lastBlink) > intervalBlink) {                             // blink interval
      D_printSerialTelnet(F("D:U:BLNK.."));
      startUpdate = millis();
      // do we want to blink the LCD background ?
      if ( mySettings.useBacklight && (allGood == false) ) {
        if (timeSynced) { // is it night and we dont want to blink?
          if ( (mySettings.useBlinkNight == true) || // blink also at night
               // or is it day time?
               ( (mySettings.useBlinkNight == false) && ( (localTime->tm_hour*60+localTime->tm_min < mySettings.nightBegin) && (localTime->tm_hour*60+localTime->tm_min > mySettings.nightEnd) ) ) 
             )
          { blinkLCD = true; } 
        } else { // no network time
            blinkLCD = true;
        } 
      } else {
            blinkLCD = false;
      }
      
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
    } // blinking interval

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
    // Check if devices in appropriate state, otherwise try again later
    if (rebootNow) {
      bool rebootok = true;
      if (bme280_avail   && mySettings.useBME280){ if  ( (stateBME280 == DATA_AVAILABLE) || (stateBME280 == IS_BUSY)        ) {rebootok = false;} }
      if (bme680_avail   && mySettings.useBME680){ if  ( (stateBME680 == DATA_AVAILABLE) || (stateBME680 == IS_BUSY)        ) {rebootok = false;} }
      if (ccs811_avail   && mySettings.useCCS811){ if  ( (stateCCS811 == IS_WAKINGUP)    || (stateCCS811 == DATA_AVAILABLE) ) {rebootok = false;} }
      // if (therm_avail && mySettings.useMLX)   { }
      if (scd30_avail    && mySettings.useSCD30) { if  ( (stateSCD30  == DATA_AVAILABLE) || (stateSCD30  == IS_BUSY)        ) {rebootok = false;} }
      // if (sgp30_avail && mySettings.useSGP30) { }
      if (sps30_avail    && mySettings.useSPS30) { if  ( (stateSPS30  == IS_WAKINGUP)    || (stateSPS30  == IS_BUSY)        ) {rebootok = false;} }
      if (max_avail      && mySettings.useMAX30) { }
      if (rebootok) {
        R_printSerialTelnetLog(F("Bye ..."));
        Serial.flush();
        logFile.close();
        ESP.reset();
      }
    }
    
    /******************************************************************************************************/
    // Keep track of free processor time 
    /******************************************************************************************************/
    
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

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////
// Support functions
//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////

// yield when necessary, there should be a yield/delay every 100ms
unsigned long yieldOS() {
  unsigned long beginYieldOS = millis();
  if ( (beginYieldOS - lastYield) > MAXUNTILYIELD ) {
    yieldFunction(); // replaced with define statement at beginning of the this document
    lastYield = millis();
    return lastYield-beginYieldOS;
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
  yieldI2C(); // repalced with function defined at beginning of this document
}  

// Boot helper
// Prints message and waits until timeout or user send character on serial terminal
void serialTrigger(const char* mess, int timeout) {
  unsigned long startTime = millis();
  bool clearSerial = true;
  Serial.println();
  Serial.println(mess);
  while (!Serial.available()) {
    delay(1000);
    if (millis() - startTime >= timeout) {
      clearSerial = false;
      break;
    }
  }
  if (clearSerial == true) {
    while (Serial.available()) {Serial.read();}
  }
}

/**************************************************************************************/
// JSON: Support Routines, JSON for the senosors is in their code segment
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
  timeval tv;
  gettimeofday (&tv, NULL);
  snprintf_P(payLoad, len, PSTR("{ \"time\": { \"hour\": %d, \"minute\": %d, \"second\": %d, \"microsecond\": %06ld}}"),
  localTime->tm_hour,
  localTime->tm_min,
  localTime->tm_sec,
  tv.tv_usec); 
}

void dateJSON(char *payLoad, size_t len) {
  snprintf_P(payLoad, len, PSTR("{ \"date\": { \"day\": %d, \"month\": %d, \"year\": %d}}"),
  localTime->tm_mday,
  localTime->tm_mon+1,
  localTime->tm_year+1900); 
}

void systemJSON(char *payLoad, size_t len) {
  snprintf_P(payLoad, len, PSTR("{ \"system\": {\"freeheap\": %d, \"heapfragmentation\": %d, \"maxfreeblock\": %d, \"maxlooptime\": %d}}"),
  ESP.getFreeHeap(),
  ESP.getHeapFragmentation(),      
  ESP.getMaxFreeBlockSize(),
  (int)myLoopMaxAvg);
}

void ipJSON(char *payLoad, size_t len) {
  IPAddress lip = WiFi.localIP();
  snprintf_P(payLoad, len, PSTR("{ \"ip\": \"%d.%d.%d.%d\"}"), lip[0], lip[1], lip[2], lip[3]);

}

void hostnameJSON(char *payLoad, size_t len) {
   snprintf_P(payLoad, len, PSTR("{ \"hostname\": \"%s\"}"), hostName);
}

void max30JSON(char *payLoad, size_t len) {
  // Not Implemented Yet
  snprintf_P(payLoad, len, PSTR("{ \"max30\": {\"avail\": %s, \"HR\": %5.1f, \"O2Sat\": %5.1f, \"MAX_quality\": \"%s\"}}"), 
                       max_avail ? "true" : "false", 
                       -1.0,
                       -1.0,
                       "n.a.");  
}

/**************************************************************************************/
// Terminal Input: Support Routines
/**************************************************************************************/

bool inputHandle() {
  unsigned long tmpuI;
  long          tmpI;
  float         tmpF;
  bool          processSerial = false;
  bool          processTelnet = false;
  char          command[2];
  char          value[64];
  char          text[64];
  char          rtext[64];
  char         *pEnd;
  
  if (serialReceived) {
    lastTmp = millis();
    if ((lastTmp - lastSerialInput) > SERIALMAXRATE) { // limit command input rate to protect from pasted text
      lastSerialInput = lastTmp;
      if ( strlen(serialInputBuff) > 0) {processSerial = true;} else {processSerial = false;}
    } else {
      processSerial = false;
    }
    //R_printSerialTelnetLog(F("Serial: ")); R_printSerialTelnetLogln(serialInputBuff);
  } 
  if (telnetReceived) {
    lastTmp = millis();
    if ((lastTmp - lastTelnetInput) > SERIALMAXRATE) { // limit command input rate to protect from pasted text
      lastTelnetInput = lastTmp;
      if ( strlen(telnetInputBuff) > 0) {processTelnet = true;} else {processTelnet = false;}
    } else {
      processTelnet = false;
    }
    telnetReceived = false; // somewhere we need to turn telnet received notification off
    //R_printSerialTelnetLog(F("Telnet: ")); R_printSerialTelnetLogln(telnetInputBuff);
  }

  if (processSerial || processTelnet) { 
    if (processTelnet) {
      if (strlen(telnetInputBuff) > 0) { strncpy(command, telnetInputBuff, 1); command[1]='\0'; }
      if (strlen(telnetInputBuff) > 1) { strncpy(value, telnetInputBuff+1, sizeof(value)); } else {value[0] = '\0';}
    }
    if (processSerial) {
      if (strlen(serialInputBuff) > 0) { strncpy(command, serialInputBuff, 1); command[1]='\0'; }
      if (strlen(serialInputBuff) > 1) { strncpy(value, serialInputBuff+1, sizeof(value)); } else {value[0] = '\0';}
    }
    //R_printSerialTelnetLog(F("Command: ")); R_printSerialTelnetLogln(command);
    //R_printSerialTelnetLog(F("Value: "));   R_printSerialTelnetLogln(value);

    if (command[0] == 'a') {                                            // set start of night in minutes after midnight
      if (strlen(value)>0) {
        tmpuI = strtoul(value, NULL, 10);
        if ((tmpuI >= 0) && (tmpuI <= 1440) && (tmpuI >= mySettings.nightEnd)) {
          mySettings.nightBegin = (uint16_t)tmpuI;
          snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Night begin set to: %u after midnight"), mySettings.nightBegin); 
          R_printSerialTelnetLogln(tmpStr);
        } else {
          snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Night start out of valid range %u - 1440 minutes after midnight"), mySettings.nightEnd ); 
          R_printSerialTelnetLogln(tmpStr);
        }
      } else {
        snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Night start no value provided")); 
        R_printSerialTelnetLogln(tmpStr);
      }
      yieldTime += yieldOS(); 
    }

    else if (command[0] == 'A') {                                            // set end of night in minutes after midnight
      if (strlen(value)>0) {
        tmpuI = strtoul(value, NULL, 10);
        if ((tmpuI >= 0) && (tmpuI <= 1440) && (tmpuI <= mySettings.nightBegin)) {
          mySettings.nightEnd = (uint16_t)tmpuI;
          snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Night end set to: %u minutes after midnight"), mySettings.nightEnd); 
          R_printSerialTelnetLogln(tmpStr);
        } else {
          snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Night end out of valid range 0 - %u minutes after midnight"), mySettings.nightBegin); 
          R_printSerialTelnetLogln(tmpStr);
        }
      } else {
        snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Night end no value provided")); 
        R_printSerialTelnetLogln(tmpStr);
      }    
      yieldTime += yieldOS(); 
    }

    else if (command[0] == 'R') {                                            // set end of night in minutes after midnight
      if (strlen(value)>0) {
        tmpI = strtol(value, NULL, 10);
        if ((tmpI >= -1) && (tmpI <= 1440)) {
          mySettings.rebootMinute = (int16_t)tmpI;
          snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Reboot minute set to: %d minutes after midnight"), mySettings.rebootMinute); 
          R_printSerialTelnetLogln(tmpStr);
        } else {
          R_printSerialTelnetLogln("Reboot time out of valid range -1(off) - 1440 minutes after midnight");
        }
      } else {
          R_printSerialTelnetLogln("Reboot time no value provided");
      }
      yieldTime += yieldOS(); 
    }

    else if (command[0] == 'l') {                                            // set debug level
      if (strlen(value)>0) {
        tmpuI = strtoul(value, NULL, 10);
        if ((tmpuI >= 0) && (tmpuI <= 99)) {
          mySettings.debuglevel = (unsigned int)tmpuI;
          snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Debug level set to: %u"), mySettings.debuglevel);  
          R_printSerialTelnetLogln(tmpStr);
        } else {
          R_printSerialTelnetLogln(F("Debug level out of valid Range"));
        }
      } else {
          R_printSerialTelnetLogln(F("No debug level provided"));
      }
      yieldTime += yieldOS(); 
    }

    else if (command[0] == 'z') {                                            // dump all sensor data
      printSensors();
      //yieldTime += yieldOS(); 
    }

    else if (command[0] == '.') {                                            // print a clear execution times of subroutines
      tmpTime = millis();
      printProfile();
      //yieldTime += yieldOS(); 
      deltaUpdate = millis() - tmpTime;
      if (maxUpdateSYS < deltaUpdate) { maxUpdateSYS = deltaUpdate; }
    }

    else if (command[0] == 'W') {                                            // print a clear execution times of subroutines
      //tmpTime = millis();
      printState();
      //yieldTime += yieldOS(); // yield is inside printState
      //deltaUpdate = millis() - tmpTime;
      //
    }

    else if (command[0] == 'L') {                                            // list content of Little File System
      R_printSerialTelnetLogln(F(" Contents:"));
      if (fsOK) {
        Dir mydir = LittleFS.openDir("/");
        unsigned int fileSize;
        while (mydir.next()) {                      // List the file system contents
          String fileName = mydir.fileName();
          fileSize = (unsigned int)mydir.fileSize();
          snprintf_P(tmpStr, sizeof(tmpStr), PSTR("\tName: %24s, Size: %8u"), fileName.c_str(), fileSize); 
          R_printSerialTelnetLogln(tmpStr);
        }
      } else { R_printSerialTelnetLogln(F("LittleFS not mounted")); }
      yieldTime += yieldOS(); 
    }
    
    ///////////////////////////////////////////////////////////////////
    // SGP30
    ///////////////////////////////////////////////////////////////////
    else if (command[0] == 'e') {                                            // forced eCO2 set setpoint
      if (strlen(value)>0) {
        tmpuI = strtoul(value, NULL, 10);
        if ((tmpuI >= 400) && (tmpuI <= 2000)) {
          if (sgp30_avail && mySettings.useSGP30) {
            switchI2C(sgp30_port, sgp30_i2c[0], sgp30_i2c[1], sgp30_i2cspeed, sgp30_i2cClockStretchLimit);
            sgp30.getBaseline();
            mySettings.baselinetVOC_SGP30 = sgp30.baselineTVOC;             // dong change tVOC
            sgp30.setBaseline((uint16_t)tmpuI, (uint16_t)mySettings.baselinetVOC_SGP30); // set CO2 and TVOC baseline
            mySettings.baselineSGP30_valid = 0xF0;
            mySettings.baselineeCO2_SGP30 = (uint16_t)tmpuI;
            snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Calibration point is: %u"), tmpuI); 
            R_printSerialTelnetLogln(tmpStr);
          }
        } else {
          R_printSerialTelnetLogln(F("Calibration point out of valid Range"));
        }
      } else {
        R_printSerialTelnetLogln(F("No calibration value provided"));
      }
      yieldTime += yieldOS(); 
    }

    else if (command[0] == 'v') {                                            // forced tVOC set setpoint
      if (strlen(value)>0) {
        tmpuI = strtoul(value, NULL, 10);
        if ((tmpuI > 400) && (tmpuI < 2000)) {
          if (sgp30_avail && mySettings.useSGP30) {
            switchI2C(sgp30_port, sgp30_i2c[0], sgp30_i2c[1], sgp30_i2cspeed, sgp30_i2cClockStretchLimit);
            sgp30.getBaseline();
            mySettings.baselineeCO2_SGP30 = sgp30.baselineCO2;  // dont change eCO2
            sgp30.setBaseline((uint16_t)mySettings.baselineeCO2_SGP30, (uint16_t)tmpuI);
            mySettings.baselineSGP30_valid = 0xF0;
            mySettings.baselinetVOC_SGP30 = (uint16_t)tmpuI;
            snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Calibration point is: %u"), tmpuI); 
            R_printSerialTelnetLogln(tmpStr);
          }
        } else { R_printSerialTelnetLogln(F("Calibration point out of valid Range")); }
      } else {
        R_printSerialTelnetLogln(F("No calibration value provided"));
      }
      yieldTime += yieldOS(); 
    }

    else if (command[0] == 'g') {                                            // read eCO2 and tVOC set setpoint
      if (sgp30_avail && mySettings.useSGP30) {
        switchI2C(sgp30_port, sgp30_i2c[0], sgp30_i2c[1], sgp30_i2cspeed, sgp30_i2cClockStretchLimit);
        sgp30.getBaseline();
        mySettings.baselineSGP30_valid = 0xF0;
        mySettings.baselinetVOC_SGP30 = sgp30.baselineTVOC;
        mySettings.baselineeCO2_SGP30 = sgp30.baselineCO2;
        R_printSerialTelnetLogln(F("SGP baselines read"));
      }
      yieldTime += yieldOS(); 
    }

    ///////////////////////////////////////////////////////////////////
    // SCD30
    ///////////////////////////////////////////////////////////////////
    else if (command[0] == 'f') {                                            // forced CO2 set setpoint
      if (strlen(value)>0) {
        tmpuI = strtoul(value, NULL, 10);
        if ((tmpuI >= 400) && (tmpuI <= 2000)) {
          if (scd30_avail && mySettings.useSCD30) {
            switchI2C(scd30_port, scd30_i2c[0], scd30_i2c[1], scd30_i2cspeed, scd30_i2cClockStretchLimit);
            scd30.setForcedRecalibrationFactor((uint16_t)tmpuI);
            snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Calibration point is: %u"), tmpuI); 
            R_printSerialTelnetLogln(tmpStr);
          }
        } else {
          R_printSerialTelnetLogln(F("Calibration point out of valid Range"));
        }
      } else {
          R_printSerialTelnetLogln(F("No calibration data provided"));
      }
      yieldTime += yieldOS(); 
    }

    else if (command[0] == 't') {                                            // forced CO2 set setpoint
      if (strlen(value)>0) {
        tmpF = strtof(value,NULL);
        if ((tmpF >= 0.0) && (tmpF <= 10.0)) {
          if (scd30_avail && mySettings.useSCD30) {
            switchI2C(scd30_port, scd30_i2c[0], scd30_i2c[1], scd30_i2cspeed, scd30_i2cClockStretchLimit);
            scd30.setTemperatureOffset(tmpF);
            mySettings.tempOffset_SCD30_valid = 0xF0;
            mySettings.tempOffset_SCD30 = scd30.getTemperatureOffset();
            snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Temperature offset set to: %f"), mySettings.tempOffset_SCD30); 
            R_printSerialTelnetLogln(tmpStr);
          }
        } else {
          R_printSerialTelnetLogln(F("Offset point out of valid Range"));
        }
      } else {
          R_printSerialTelnetLogln(F("No offset data provided"));
      }
      yieldTime += yieldOS(); 
    }

    ///////////////////////////////////////////////////////////////////
    // CCS811
    ///////////////////////////////////////////////////////////////////
    else if (command[0] == 'c') {                                            // forced baseline
      if (strlen(value)>0) {
        tmpuI = strtoul(value, NULL, 10);
        if ((tmpuI >= 0) && (tmpuI <= 65535)) {
          if (ccs811_avail && mySettings.useCCS811) {
            switchI2C(ccs811_port, ccs811_i2c[0], ccs811_i2c[1], ccs811_i2cspeed, ccs811_i2cClockStretchLimit);
            ccs811.setBaseline((uint16_t)tmpuI);
            mySettings.baselineCCS811_valid = 0xF0;
            mySettings.baselineCCS811 = (uint16_t)tmpuI;
            snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Calibration point is: %d"),tmpuI); 
            R_printSerialTelnetLogln(tmpStr);
          }
        } else {
          R_printSerialTelnetLogln(F("Calibration point out of valid Range"));
        }
      } else {
        R_printSerialTelnetLogln(F("No calibration data provided"));
      }
      yieldTime += yieldOS(); 
    }

    else if (command[0] == 'b') {                                            // getbaseline
      if (ccs811_avail && mySettings.useCCS811) {
        switchI2C(ccs811_port, ccs811_i2c[0], ccs811_i2c[1], ccs811_i2cspeed, ccs811_i2cClockStretchLimit);
        mySettings.baselineCCS811 = ccs811.getBaseline();
        snprintf_P(tmpStr, sizeof(tmpStr), PSTR("CCS811: baseline is %d"), mySettings.baselineCCS811); 
        R_printSerialTelnetLogln(tmpStr);
        yieldTime += yieldOS(); 
      }
    }

    ///////////////////////////////////////////////////////////////////
    // MLX settings
    ///////////////////////////////////////////////////////////////////
    else if (command[0] == 'm') {                                            // forced MLX temp offset
      if (strlen(value)>0) {
        tmpF = strtof(value, NULL);
        if ((tmpF >= -20.0) && (tmpF <= 20.0)) {
          mlxOffset = tmpF;
          mySettings.tempOffset_MLX_valid = 0xF0;
          mySettings.tempOffset_MLX = (float)tmpF;
          snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Temperature offset set to: %f"),mySettings.tempOffset_MLX); 
          R_printSerialTelnetLogln(tmpStr);
        } else {
          R_printSerialTelnetLogln(F("Offset point out of valid Range"));
        }
      } else {
        R_printSerialTelnetLogln(F("No offset data provided"));
      }
      yieldTime += yieldOS(); 
    }

    ///////////////////////////////////////////////////////////////////
    // EEPROM settings
    ///////////////////////////////////////////////////////////////////
    else if (command[0] == 's') {                                            // save EEPROM
      tmpTime = millis();
      if (strlen(value) > 0) { // we have also a value
        if (value[0] == 'j') {
          D_printSerialTelnet(F("D:S:JSON.."));
          saveConfiguration(mySettings);
          snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Settings saved to JSON file in: %dms"), millis() - tmpTime); 
          R_printSerialTelnetLogln(tmpStr);
        }
      } else {
        D_printSerialTelnet(F("D:S:EPRM.."));
        EEPROM.put(0, mySettings);
        if (EEPROM.commit()) { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("EEPROM saved in: %dms"), millis() - tmpTime); } // takes 400ms
        else {snprintf_P(tmpStr, sizeof(tmpStr), PSTR("EEPROM failed to commit"));} 
        R_printSerialTelnetLogln(tmpStr);
      }
      yieldTime += yieldOS(); 
    }

    else if (command[0] == 'r') {                                          // read EEPROM
      tmpTime = millis();
      if (strlen(value) > 0) { // we have also a value
        if (value[0] == 'j') {
          D_printSerialTelnet(F("D:R:JSON.."));
          tmpTime = millis();
          loadConfiguration(mySettings);
          snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Settings loaded from JSON file in: %dms"), millis() - tmpTime); 
          R_printSerialTelnetLogln(tmpStr);
          yieldTime += yieldOS(); 
        }
      } else {
        // loadConfiguration(mySettings);
        D_printSerialTelnet(F("D:R:EPRM.."));
        EEPROM.begin(EEPROM_SIZE);
        EEPROM.get(eepromAddress, mySettings);
        yieldTime += yieldOS(); 
        snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Settings read from JSON/EEPROM file in: %dms"), millis() - tmpTime); 
        R_printSerialTelnetLogln(tmpStr);
        printSettings(); 
       //yieldTime += yieldOS();  // yield is inside printSettings 
      }
    }

    else if (command[0] == 'p') {                                          // print display settings
      printSettings();
      //yieldTime += yieldOS(); 
    }

    else if (command[0] == 'd') {                                          // load default settings
      defaultSettings();
      yieldTime += yieldOS(); 
      printSettings();
      // yieldTime += yieldOS(); 
    }

    else if (command[0] == 'P') {                                          // SSIDs, passwords, servers      
      if (strlen(value) > 0) { // we have a value
        tmpI = strtol(value,&pEnd,10); // which SSID or PW or Server do we want to change
        strncpy(text,pEnd,sizeof(text));
        if (strlen(text) >= 0) {
          switch (tmpI) {
            case 1: // SSID 1
              if (strlen(text) == 0) { mySettings.ssid1[0] = '\0'; }
              else { strncpy(mySettings.ssid1, text, sizeof(mySettings.ssid1)); }
              snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SSID 1 is: %s"), mySettings.ssid1); 
              R_printSerialTelnetLogln(tmpStr);
              break;
            case 2: // SSID 2
              if (strlen(text) == 0) { mySettings.ssid2[0] = '\0'; }
              else { strncpy(mySettings.ssid2, text, sizeof(mySettings.ssid2)); }
              snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SSID 2 is: %s"), mySettings.ssid2); 
              R_printSerialTelnetLogln(tmpStr);
              break;
            case 3: // SSID 3
              if (strlen(text) == 0) { mySettings.ssid3[0] = '\0'; }
              else { strncpy(mySettings.ssid3, text, sizeof(mySettings.ssid3)); }
              snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SSID 3 is: %s"), mySettings.ssid3); 
              R_printSerialTelnetLogln(tmpStr);
              break;
            case 4: // Passsword 1
              if (strlen(text) == 0) { mySettings.pw1[0] = '\0'; }
              else { strncpy(mySettings.pw1, text, sizeof(mySettings.pw1)); }
              snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Password 1 is: %s"), mySettings.pw1); 
              R_printSerialTelnetLogln(tmpStr);
              break;
            case 5: // Password 2
              if (strlen(text) == 0) { mySettings.pw2[0] = '\0'; }
              else { strncpy(mySettings.pw2, text, sizeof(mySettings.pw2)); }
              snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Password 2 is: %s"), mySettings.pw2); 
              R_printSerialTelnetLogln(tmpStr);
              break;
            case 6: // Password 3
              if (strlen(text) == 0) { mySettings.pw3[0] = '\0'; }
              else { strncpy(mySettings.pw3, text, sizeof(mySettings.pw3)); }
              snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Password 3 is: %s"), mySettings.pw3); 
              R_printSerialTelnetLogln(tmpStr);
              break;
            case 8: // MQTT Server
              if (strlen(text) == 0) { mySettings.mqtt_server[0] = '\0'; }
              else { strncpy(mySettings.mqtt_server, text, sizeof(mySettings.mqtt_server)); }
              snprintf_P(tmpStr, sizeof(tmpStr), PSTR("MQTT server is: %s"), mySettings.mqtt_server); 
              R_printSerialTelnetLogln(tmpStr);
              break;
            case 9: // MQTT Fallback Server
              if (strlen(text) == 0) { mySettings.mqtt_fallback[0] = '\0'; }
              else { strncpy(mySettings.mqtt_fallback, text, sizeof(mySettings.mqtt_fallback)); }
              snprintf_P(tmpStr, sizeof(tmpStr), PSTR("MQTT fallback server is: %s"), mySettings.mqtt_fallback); 
              R_printSerialTelnetLogln(tmpStr);
              break;
            default:
              R_printSerialTelnetLogln(F("No option provided"));
          }
        } else {
          R_printSerialTelnetLogln(F("No valid text provided to option"));
        } // end text provided
        yieldTime += yieldOS(); 
      } // end valid value
    }

    else if (command[0] == 'u') {                                          // MQTT Server username
      if (strlen(value) > 0) { // we have a value
        strncpy(mySettings.mqtt_username, value, sizeof(mySettings.mqtt_username));
      } else {
        mySettings.mqtt_username[0] = '\0'; // empty user name
      }
      snprintf_P(tmpStr, sizeof(tmpStr), PSTR("MQTT username is: %s"), mySettings.mqtt_username); 
      R_printSerialTelnetLogln(tmpStr);
      yieldTime += yieldOS(); 
    }

    else if (command[0] == 'w') {                                          // MQTT Server password
      if (strlen(value) > 0) { // we have a value
        strncpy(mySettings.mqtt_password, value, sizeof(mySettings.mqtt_password));
      }  else {
        mySettings.mqtt_password[0] = '\0'; // empty password
      }
      snprintf_P(tmpStr, sizeof(tmpStr), PSTR("MQTT password is: %s"), mySettings.mqtt_password); 
      R_printSerialTelnetLogln(tmpStr);
      yieldTime += yieldOS(); 
    }

    else if (command[0] == 'q') {                                          // MQTT Server update as soon as new data is available
      mySettings.sendMQTTimmediate = !bool(mySettings.sendMQTTimmediate);
      snprintf_P(tmpStr, sizeof(tmpStr), PSTR("MQTT is sent immediatly: %s"), mySettings.sendMQTTimmediate?FPSTR(mOFF):FPSTR(mON)); 
      R_printSerialTelnetLogln(tmpStr);
      yieldTime += yieldOS(); 
    }

    else if (command[0] == 'n') {                                          // MQTT main topic name (device Name)
      if (strlen(value) > 0) { // we have a value
        strncpy(mySettings.mqtt_mainTopic, value, sizeof(mySettings.mqtt_mainTopic));
      } else {
        mySettings.mqtt_mainTopic[0] = '\0';
      }
      snprintf_P(tmpStr, sizeof(tmpStr), PSTR("MQTT mainTopic is: %s"),mySettings.mqtt_mainTopic); 
      R_printSerialTelnetLogln(tmpStr);
      yieldTime += yieldOS(); 
    }

    else if (command[0] == 'N') {                                          // NTP server, network time
      if (strlen(value) > 0) {
        strncpy(mySettings.ntpServer, value, sizeof(mySettings.ntpServer));
      } else {
        mySettings.ntpServer[0] = '\0';
      }
      stateNTP = START_UP;
      snprintf_P(tmpStr, sizeof(tmpStr), PSTR("NTP server is: %s"), mySettings.ntpServer); 
      R_printSerialTelnetLogln(tmpStr);
      yieldTime += yieldOS(); 
    }

    else if (command[0] == 'F') {                                          // NTP server, network time
      if (strlen(value) > 0) {
        strncpy(mySettings.ntpFallback, value, sizeof(mySettings.ntpFallback));
      } else {
        mySettings.ntpFallback[0] = '\0';
      }
      snprintf_P(tmpStr, sizeof(tmpStr), PSTR("NTP fallback server is: %s"), mySettings.ntpFallback); 
      R_printSerialTelnetLogln(tmpStr);
      yieldTime += yieldOS(); 
    }

    else if (command[0] == 'o') {                                          // Time Zone Offset, use to table listed in help page
      if (strlen(value) > 0) {
        strncpy(mySettings.timeZone, value, sizeof(mySettings.timeZone));
      } else {
        mySettings.timeZone[0] = '\0';
      }
      NTP.setTimeZone(mySettings.timeZone);
      snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Time Zone is: %s"), mySettings.timeZone); 
      R_printSerialTelnetLogln(tmpStr);
      yieldTime += yieldOS(); 
    }

    else if (command[0] == 'x') {                                          // enable/disable equipment
      if (strlen(value) > 0) {
        tmpuI = strtoul(value, NULL, 10);
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
              mySettings.useBME680 = !bool(mySettings.useBME680);
              if (mySettings.useBME680==true) {     R_printSerialTelnetLogln(F("BME680 is used"));  }      else { R_printSerialTelnetLogln(F("BME680 is not used")); }
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
            case 99:
              R_printSerialTelnetLogln(F("ESP is going to restart soon..."));
              rebootNow = true;
              break;
            default:
              R_printSerialTelnetLog(F("Invalid option"));
              break;
          } // end switch
        } // valid option
      } else {
        R_printSerialTelnetLog(F("No valid option provided"));
      }
      yieldTime += yieldOS(); 
    }

    else if (command[0] == 'j') {                                          // test json output
      D_printSerialTelnet(F("D:D:JSON.."));
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
      bme280JSON(payLoad, sizeof(payLoad));   printSerialTelnetLog(payLoad); 
      snprintf_P(tmpStr, sizeof(tmpStr), PSTR(" len: %u"), strlen(payLoad));   printSerialTelnetLogln(tmpStr);
      yieldTime += yieldOS(); 
      bme680JSON(payLoad, sizeof(payLoad));   printSerialTelnetLog(payLoad); 
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
    }

    // Should produce something like:
    //{"time":{"hour":0,"minute":0,"second":15}} len: 42
    //{"date":{"day":1,"month":1,"year":1970}} len: 40
    //{"bme280":{"avail":false,"p": 0.0,"pavg": 0.0,"rH": 0.0,"aH": 0.0,"T":  0.0,"dp_airquality":"Normal","rH_airquality":"Excessive","T_airquality":"Cold"}} len: 152
    //{"bme680":{"avail":true,"p":927.4,"pavg":927.4,"rH":32.3,"aH": 8.3,"T": 26.6,"resistance":0,"dp_airquality":"Normal","rH_airquality":"Threshold Low","resistance_airquality":"?","T_airquality":"Hot"}} len: 199
    //{"ccs811":{"avail":true,"eCO2":0,"tVOC":0,"eCO2_airquality":"Normal","tVOC_airquality":"Normal"}} len: 97
    //{"mlx":{"avail":true,"To": 26.4,"Ta": 27.0,"fever":"Low ","T_airquality":"Hot"}} len: 80
    //{"scd30":{"avail":true,"CO2":0,"rH":-1.0,"aH":-1.0,"T":-999.0,"CO2_airquality":"Normal","rH_airquality":"?","T_airquality":"?"}} len: 128
    //{"sgp30":{"avail":true,"eCO2":400,"tVOC":0,"eCO2_airquality":"Normal","tVOC_airquality":"Normal"}} len: 98
    //{"sps30":{"avail":false,"PM1": 0.0,"PM2": 0.0,"PM4": 0.0,"PM10": 0.0,"nPM0": 0.0,"nPM1": 0.0,"nPM2": 0.0,"nPM4": 0.0,"nPM10": 0.0,"PartSize": 0.0,"PM2_airquality":"Normal","PM10_airquality":"Normal"}} len: 200

    else if (command[0] == 'i') {                                                         // simpler display on LCD
      mySettings.consumerLCD = !bool(mySettings.consumerLCD);
      if (mySettings.consumerLCD) {
        R_printSerialTelnetLog(F("Simpler LCD display"));
      } else {
        R_printSerialTelnetLog(F("Engineering LCD display"));
      }
      yieldTime += yieldOS(); 
    }

    else if (command[0] =='?' || command[0] =='h') {                                                          // help requested
      helpMenu();
      //yieldTime += yieldOS(); // is inside helpMenu()
    }

    else {                                                                             // unrecognized command
      R_printSerialTelnetLog(F("Command not available: "));
      printSerialTelnetLogln(command);
      yieldTime += yieldOS(); 
    } // end if
    
    processSerial  = false;
    processTelnet  = false;
    return true;
  } // end process occured
  
  else { 
    processSerial  = false;
    processTelnet  = false;
    return false; 
  } // end no process occured
  
} // end Input

void helpMenu() {  
    printSerialTelnetLogln();
  R_printSerialTelnetLogln(FPSTR(doubleSeparator));                                                                 yieldTime += yieldOS(); 
    snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Sensi, 2020-2022, Urs Utzinger                                             %s"),  FPSTR(VER)); 
    printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
    // printSerialTelnetLogln(F("| Sensi, 2020-2022, Urs Utzinger                                               |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(FPSTR(doubleSeparator));                                                                 yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("Supports........................................................................"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("........................................................................LCD 20x4"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("........................................................SPS30 Senserion Particle"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F(".............................................................SCD30 Senserion CO2"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("......................................................SGP30 Senserion tVOC, eCO2"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("..........................BME680/280 Bosch Temperature, Humidity, Pressure, tVOC"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("...................................................CCS811 eCO2 tVOC, Air Quality"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("..........................................MLX90614 Melex Temperature Contactless"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("==General===============================|======================================="));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| z: print all sensor data              | n: this device Name, nSensi          |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| p: print current settings             | s: save settings (EEPROM) sj: JSON   |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| d: create default seetings            | r: read settings (EEPROM) rj: JSON   |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| .: execution times                    | L: list content of filesystem        |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| j: print sensor data in JSON          | W: print WiFi states and Intervals   |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("==Network===============================|==MQTT================================="));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| P1: SSID 1, P1myssid                  | u: mqtt username, umqtt or empty     |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| P2: SSID 2, P2myssid                  | w: mqtt password, ww1ldc8ts or empty |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| P3: SSID 3, P3myssid                  | q: toggle send mqtt immediatly, q    |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| P4: password SSID 1, P4mypas or empty |                                      |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| P5: password SSID 2, P5mypas or empty | P8: mqtt server, P8my.mqtt.com       |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| P6: password SSID 3, P6mypas or empty | P9: mqtt fall back server            |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("|-Network Time--------------------------|--------------------------------------|"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| o: set time zone oMST7                | N: ntp server, Ntime.nist.gov        |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("|                                       | F: ntp fallback server               |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| https://raw.githubusercontent.com/nayarsystems/posix_tz_db/master/zones.csv  |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| a: night start in min after midnight  | A: night end                         |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| R: reboot time in min after midnight: -1=off                                 |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("==Sensors&LCD==================================================================="));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("|-SGP30-eCO2----------------------------|-MAX----------------------------------|"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| e: force eCO2, e400                   |                                      |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| v: force tVOC, t3000                  |                                      |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| g: get baselines                      |                                      |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("|-SCD30=CO2-----------------------------|--CCS811-eCO2-------------------------|"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| f: force CO2, f400.0 in ppm           | c: force basline, c400               |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| t: force temperature offset, t5.0 in C| b: get baseline                      |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("|-MLX Temp------------------------------|-LCD----------------------------------|"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| m: set temp offset, m1.4              | i: simplified display                |"));  yieldTime += yieldOS(); 
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
    printSerialTelnetLogln(F("| x: 12 BME680 on/off                   |                                      |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| x: 13 BME280 on/off                   | x: 99 Reset/Reboot                   |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("|---------------------------------------|--------------------------------------|"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| You will need to x99 to initialize the sensor                                |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("==Debug Level===========================|==Debug Level=========================="));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| l: 0 ALL off                          | l: 6 SGP30 max level                 |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| l: 1 Errors only                      | l: 7 MAX30 max level                 |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| l: 2 Minimal                          | l: 8 MLX max level                   |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| l: 3 WiFi max level                   | l: 9 BME680/280 max level            |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| l: 4 SCD30 max level                  | l: 10 CCS811 max level               |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| l: 5 SPS30 max level                  | l: 11 LCD max level                  |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("| l: 99 continous                       |                                      |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(FPSTR(doubleSeparator));                                                                 yieldTime += yieldOS(); 
    printSerialTelnetLogln(F("|  Dont forget to save settings with s                                         |"));  yieldTime += yieldOS(); 
    printSerialTelnetLogln(FPSTR(doubleSeparator));                                                                 yieldTime += yieldOS(); 
}

void printSettings() {
  R_printSerialTelnetLogln(FPSTR(doubleSeparator)); yieldTime += yieldOS(); 
  printSerialTelnetLogln(F("-System-----------------------------")); yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Debug level: .................. %u"),   mySettings.debuglevel ); 
    printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Reboot Minute: ................ %i"),   mySettings.rebootMinute); 
  printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Runtime [min]: ................ %lu"),  mySettings.runTime / 60); 
  printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Simpler Display: .............. %s"),  (mySettings.consumerLCD) ? FPSTR(mON) : FPSTR(mOFF));  
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
  printSerialTelnetLogln(FPSTR(doubleSeparator)); yieldTime += yieldOS(); 
  printSerialTelnetLogln(F("-Sensors----------------------------"));  yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SCD30: ........................ %s"),  (mySettings.useSCD30)  ? FPSTR(mON) : FPSTR(mOFF)); 
  printSerialTelnetLogln(tmpStr);  yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SPS30: ........................ %s"),  (mySettings.useSPS30)  ? FPSTR(mON) : FPSTR(mOFF)); 
  printSerialTelnetLogln(tmpStr);  yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SGP30: ........................ %s"),  (mySettings.useSGP30)  ? FPSTR(mON) : FPSTR(mOFF)); 
  printSerialTelnetLogln(tmpStr);  yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("MAX30: ........................ %s"),  (mySettings.useMAX30)  ? FPSTR(mON) : FPSTR(mOFF)); 
  printSerialTelnetLogln(tmpStr);  yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("MLX: .......................... %s"),  (mySettings.useMLX)    ? FPSTR(mON) : FPSTR(mOFF)); 
  printSerialTelnetLogln(tmpStr);  yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("BME680: ....................... %s"),  (mySettings.useBME680) ? FPSTR(mON) : FPSTR(mOFF)); 
  printSerialTelnetLogln(tmpStr);  yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("BME280: ....................... %s"),  (mySettings.useBME280) ? FPSTR(mON) : FPSTR(mOFF)); 
  printSerialTelnetLogln(tmpStr);  yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("CCS811: ....................... %s"),  (mySettings.useCCS811) ? FPSTR(mON) : FPSTR(mOFF)); 
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
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Average Pressure: [mbar]....... %f"),   mySettings.avgP/100.); 
  printSerialTelnetLogln(tmpStr);  yieldTime += yieldOS(); 
  printSerialTelnetLogln(FPSTR(doubleSeparator)); yieldTime += yieldOS(); 
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
  R_printSerialTelnetLogln(FPSTR(singleSeparator)); yieldTime += yieldOS(); 
  
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
    snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SCD30 CO2:%hu[ppm], rH:%4.1f[%%], T:%4.1f[C] T_offset:%4.1f[C}"), scd30_ppm, scd30_hum, scd30_temp, scd30.getTemperatureOffset() ); 
    printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
    checkCO2(float(scd30_ppm),qualityMessage,15);
    snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SCD30 CO2 is: %s"), qualityMessage); 
    printSerialTelnetLogln(tmpStr); yieldTime += yieldOS();     
    snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SCD30 interval: %d Port: %d SDA %d SCL %d Speed %d CLKStretch %d"), intervalSCD30, uint32_t(scd30_port), scd30_i2c[0], scd30_i2c[1], scd30_i2cspeed,scd30_i2cClockStretchLimit); 
    printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
  } else {
    printSerialTelnetLogln(F("SCD30: not available")); yieldTime += yieldOS(); 
  }
  printSerialTelnetLogln(FPSTR(singleSeparator)); yieldTime += yieldOS(); 

  if (bme680_avail && mySettings.useBME680) {
    char qualityMessage[16];
    snprintf_P(tmpStr, sizeof(tmpStr),PSTR("BME680 T:%+5.1f[C] P:%5.1f[mbar] P_ave:%5.1f[mbar] rH:%4.1f[%%] aH:%4.1f[g/m^3] \r\nGas resistance:%d[Ohm]"), bme680.temperature, bme680.pressure/100., bme680_pressure24hrs/100., bme680.humidity, bme680_ah, bme680.gas_resistance);  
    printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
    checkAmbientTemperature(bme680.temperature,qualityMessage, 15);
    snprintf_P(tmpStr, sizeof(tmpStr),PSTR("Temperature is %s, "),qualityMessage); 
    printSerialTelnetLog(tmpStr); yieldTime += yieldOS(); 
    checkdP((bme680.pressure - bme680_pressure24hrs)/100.0,qualityMessage,15),
    snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Change in pressure is %s, "),qualityMessage); 
    printSerialTelnetLog(tmpStr); yieldTime += yieldOS(); 
    checkHumidity(bme680.humidity,qualityMessage,15);
    snprintf_P(tmpStr, sizeof(tmpStr),PSTR("Humidity is %s"),qualityMessage); 
    printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
    checkGasResistance(bme680.gas_resistance,qualityMessage,15);
    snprintf_P(tmpStr, sizeof(tmpStr),PSTR("Gas resistance is %s"),qualityMessage); 
    printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
    snprintf_P(tmpStr, sizeof(tmpStr), PSTR("BME680 interval: %d Port: %d SDA %d SCL %d Speed %d CLKStretch %d"), intervalBME680, uint32_t(bme680_port), bme680_i2c[0], bme680_i2c[1], bme680_i2cspeed, bme680_i2cClockStretchLimit); 
    printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
  } else {
    printSerialTelnetLogln(F("BME680: not available")); yieldTime += yieldOS(); 
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
  } else {
    printSerialTelnetLogln(F("CCS811: not available")); yieldTime += yieldOS(); 
  }
  printSerialTelnetLogln(FPSTR(singleSeparator)); yieldTime += yieldOS(); 

  if (sps30_avail && mySettings.useSPS30) {
    char qualityMessage[16];
    snprintf_P(tmpStr, sizeof(tmpStr),PSTR("SPS30 P1.0:%4.1f[μg/m3] P2.5:%4.1f[μg/m3] P4.0:%4.1f[μg/m3] P10:%4.1f[μg/m3]"), valSPS30.MassPM1, valSPS30.MassPM2, valSPS30.MassPM4, valSPS30.MassPM10); 
    printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
    snprintf_P(tmpStr, sizeof(tmpStr),PSTR("SPS30 P1.0:%4.1f[#/cm3] P2.5:%4.1f[#/cm3] P4.0:%4.1f[#/cm3] P10:%4.1f[#/cm3]"), valSPS30.NumPM0, valSPS30.NumPM2, valSPS30.NumPM4, valSPS30.NumPM10); 
    printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
    snprintf_P(tmpStr, sizeof(tmpStr),PSTR("SPS30 Average %4.1f[μm]"), valSPS30.PartSize);
    printSerialTelnetLogln(tmpStr); yieldTime += yieldOS();     
    checkPM2(valSPS30.MassPM2,qualityMessage,15);   
    snprintf_P(tmpStr, sizeof(tmpStr),PSTR("PM2.5 is %s, "), qualityMessage);  
    printSerialTelnetLog(tmpStr); yieldTime += yieldOS(); 
    checkPM10(valSPS30.MassPM10,qualityMessage,15);
    snprintf_P(tmpStr, sizeof(tmpStr),PSTR("PM10 is %s"),  qualityMessage); 
    printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
    snprintf_P(tmpStr, sizeof(tmpStr),PSTR("Autoclean interval: %lu"),autoCleanIntervalSPS30); 
    printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
    snprintf_P(tmpStr, sizeof(tmpStr),PSTR("SPS30 interval: %d Port: %d SDA %d SCL %d Speed %d CLKStretch %d"), intervalSPS30, uint32_t(sps30_port), sps30_i2c[0], sps30_i2c[1], sps30_i2cspeed, sps30_i2cClockStretchLimit); 
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
    snprintf_P(tmpStr, sizeof(tmpStr),PSTR("MLX Emissivity:%4.1f MLX Offset:%4.1f"), tmpEF, mlxOffset); 
    printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
    checkFever(tmpOF,qualityMessage, 15);
    snprintf_P(tmpStr, sizeof(tmpStr),PSTR("Object temperature is %s, "), qualityMessage);  
    printSerialTelnetLog(tmpStr); yieldTime += yieldOS(); 
    checkAmbientTemperature(tmpOF,qualityMessage,15);
    snprintf_P(tmpStr, sizeof(tmpStr),PSTR("Ambient temperature is %s"), qualityMessage); 
    printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
    snprintf_P(tmpStr, sizeof(tmpStr),PSTR("MLX interval: %d Port: %d SDA %d SCL %d Speed %d CLKStretch %d"), intervalMLX, uint32_t(mlx_port), mlx_i2c[0], mlx_i2c[1], mlx_i2cspeed, mlx_i2cClockStretchLimit); 
    printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
  } else {
    printSerialTelnetLogln(F("MLX: not available")); yieldTime += yieldOS(); 
  }  
  printSerialTelnetLogln(FPSTR(singleSeparator)); yieldTime += yieldOS(); 

  if (max_avail && mySettings.useMAX30) {
    char qualityMessage[16];
    //switchI2C(max_port, max_i2c[0], max_i2c[1], I2C_FAST, I2C_LONGSTRETCH);
    snprintf_P(tmpStr, sizeof(tmpStr),PSTR("MAX interval: %d Port: %d SDA %d SCL %d Speed %d CLKStretch %d"), intervalMAX, uint32_t(max_port), max_i2c[0], max_i2c[1], max_i2cspeed, max_i2cClockStretchLimit); 
    printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
  } else {
    printSerialTelnetLogln(F("MAX: not available")); yieldTime += yieldOS(); 
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
}

void printState() {

  R_printSerialTelnetLogln(FPSTR(singleSeparator)); yieldTime += yieldOS(); 

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

    printSerialTelnetLogln(FPSTR(singleSeparator)); yieldTime += yieldOS(); 
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
        case START_UP:         printSerialTelnetLogln(FPSTR(mStarting));    break;
        case CHECK_CONNECTION: printSerialTelnetLogln(FPSTR(mMonitoring));  break;
        case IS_SCANNING:      printSerialTelnetLogln(FPSTR(mScanning));    break;
        case IS_CONNECTING:    printSerialTelnetLogln(FPSTR(mConnecting));  break;
        case IS_WAITING:       printSerialTelnetLogln(FPSTR(mWaiting));     break;
        default:               printSerialTelnetLog(F("Invalid State"));    break;
      }
    } else {                   printSerialTelnetLogln(FPSTR(mNotEnabled)); }
    yieldTime += yieldOS(); 

  } // wifi avail


  R_printSerialTelnetLogln(FPSTR(singleSeparator)); //----------------------------------------------------------------//
  yieldTime += yieldOS(); 
  // BME280   12345 
  // BME280 n.a.    
  printSerialTelnetLogln(F("Intervals:"));
  if (mySettings.useBME280 && bme280_avail) { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("BME280   %8lu "),    intervalBME280);  } else { strncpy(tmpStr, ("BME280       n.a. "), sizeof(tmpStr)); } 
  printSerialTelnetLog(tmpStr);  yieldTime += yieldOS(); 
  if (mySettings.useBME680 && bme680_avail) { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("BME680   %5lu "),    intervalBME680);  } else { strncpy(tmpStr, ("BME680    n.a. "), sizeof(tmpStr)); }    
  printSerialTelnetLog(tmpStr);  yieldTime += yieldOS(); 
  if (mySettings.useCCS811 && ccs811_avail) { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("CCS811   %5lu"),     intervalCCS811);  } else { strncpy(tmpStr, ("CCS811    n.a."), sizeof(tmpStr)); } 
  printSerialTelnetLogln(tmpStr);  yieldTime += yieldOS(); 
  if (mySettings.useMLX    && therm_avail)  { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("MLX      %8lu "),    intervalMLX);     } else { strncpy(tmpStr, ("MLX          n.a. "), sizeof(tmpStr)); } 
  printSerialTelnetLog(tmpStr);  yieldTime += yieldOS(); 
  if (mySettings.useSCD30  && scd30_avail)  { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SCD30    %5lu "),    intervalSCD30);   } else { strncpy(tmpStr, ("SCD30     n.a. "), sizeof(tmpStr)); }    
  printSerialTelnetLog(tmpStr);  yieldTime += yieldOS(); 
  if (mySettings.useSGP30  && sgp30_avail)  { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SGP30    %5lu"),     intervalSGP30);   } else { strncpy(tmpStr, ("SGP30     n.a."), sizeof(tmpStr)); } 
  printSerialTelnetLogln(tmpStr);  yieldTime += yieldOS(); 
  if (mySettings.useSPS30  && sps30_avail)  { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SPS30    %8lu "),    intervalSPS30);   } else { strncpy(tmpStr, ("SPS30        n.a. "), sizeof(tmpStr)); } 
  printSerialTelnetLog(tmpStr);  yieldTime += yieldOS(); 
  if (mySettings.useMAX30  && max_avail)    { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("MAX30    %5lu "),    intervalMAX30);   } else { strncpy(tmpStr, ("MAX30     n.a. "), sizeof(tmpStr)); }    
  printSerialTelnetLog(tmpStr);  yieldTime += yieldOS();   
  if (mySettings.useLCD    && lcd_avail)    { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("LCD      %5lu"), intervalLCD);     } else { strncpy(tmpStr, ("LCD       n.a."), sizeof(tmpStr)); } 
  printSerialTelnetLogln(tmpStr);  yieldTime += yieldOS(); 
  if (mySettings.useMQTT)                   { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("MQTT     %8lu "),    intervalMQTT);    } else { strncpy(tmpStr, ("MQTT         n.a. "), sizeof(tmpStr)); } 
  printSerialTelnetLog(tmpStr);  yieldTime += yieldOS();   
  if (mySettings.useWiFi   && wifi_avail)   { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("WiFi     %5lu"), intervalWiFi);    } else { strncpy(tmpStr, ("WiFi      n.a."), sizeof(tmpStr)); } 
  printSerialTelnetLogln(tmpStr);  yieldTime += yieldOS(); 
                                            { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Baseline %8lu "),    intervalBaseline);                                              }    
                                            printSerialTelnetLog(tmpStr); yieldTime += yieldOS();  
                                            { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("System   %5lu "),    intervalSYS);                                                   }
                                            printSerialTelnetLog(tmpStr); yieldTime += yieldOS(); 
                                            { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Warnings %5lu"), intervalWarning);                                               }    
                                            printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
  if (mySettings.useLog)                    { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("LogFile  %8lu"), INTERVALLOGFILE); } else { strncpy(tmpStr, ("Logfile      n.a."), sizeof(tmpStr)); } 
                                            printSerialTelnetLogln(tmpStr); yieldTime += yieldOS();   
                                            { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Settings %8lu"), intervalSettings);                                              }    
                                            printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
  R_printSerialTelnetLogln(FPSTR(singleSeparator)); yieldTime += yieldOS();  
}

void printProfile() {
  R_printSerialTelnetLogln(FPSTR(doubleSeparator)); yieldTime += yieldOS();         
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Average loop %d[us] Avg max loop %d[ms] \r\nCurrent loop Min/Max/Alltime: %d/%d/%d[ms] Current: %d[ms]"), int(myLoopAvg*1000), int(myLoopMaxAvg), myLoopMin, myLoopMax, myLoopMaxAllTime, myLoop); 
  printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Free Heap Size: %d[B] Heap Fragmentation: %d%% Max Block Size: %d[B]"), ESP.getFreeHeap(), ESP.getHeapFragmentation(), ESP.getMaxFreeBlockSize()); printSerialTelnetLogln(tmpStr);
  printSerialTelnetLogln(FPSTR(doubleSeparator)); yieldTime += yieldOS(); 

  // This displays the amount of time it took complete the update routines.
  // The currentMax/alltimeMax is displayed. Alltime max is reset when network services are started up in their update routines.
  printSerialTelnetLogln(F("All values in [ms]"));
  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Time&Date:    %4u %4u mDNS:   %4u %4u HTTPupd: %4u %4u HTTP:    %4u %4u\r\nMQTT:         %4u %4u WS:     %4u %4u NTP:     %4u %4u OTA:     %4u %4u\r\nWifi:         %4u %4u"), 
                    maxUpdateTime, AllmaxUpdateTime, maxUpdatemDNS, AllmaxUpdatemDNS, maxUpdateHTTPUPDATER, AllmaxUpdateHTTPUPDATER, maxUpdateHTTP, AllmaxUpdateHTTP, maxUpdateMQTT, AllmaxUpdateMQTT, 
                    maxUpdateWS, AllmaxUpdateWS, maxUpdateNTP, AllmaxUpdateNTP, maxUpdateOTA, AllmaxUpdateOTA, maxUpdateWifi, AllmaxUpdateWifi);         
  printSerialTelnetLogln(tmpStr); yieldTime += yieldOS(); 

  snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SCD30:        %4u %4u SGP30:  %4u %4u CCS811:  %4u %4u SPS30:   %4u %4u\r\nBME280:       %4u %4u BME680: %4u %4u MLX:     %4u %4u MAX:     %4u %4u"), maxUpdateSCD30, AllmaxUpdateSCD30, 
                    maxUpdateSGP30, AllmaxUpdateSGP30, maxUpdateCCS811, AllmaxUpdateCCS811, maxUpdateSPS30, AllmaxUpdateSPS30, maxUpdateBME280, AllmaxUpdateBME280, maxUpdateBME680, AllmaxUpdateBME680, maxUpdateMLX, AllmaxUpdateMLX, maxUpdateMAX, AllmaxUpdateMAX);
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

  printSerialTelnetLogln(FPSTR(singleSeparator)); yieldTime += yieldOS(); 
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
  mySettings.tempOffset_SCD30              = (float)        0.01;
  mySettings.forcedCalibration_SCD30_valid = (uint8_t)         0x00; // not used
  mySettings.forcedCalibration_SCD30       = (float)         0.0; // not used
  mySettings.tempOffset_MLX_valid          = (uint8_t)         0x00;
  mySettings.tempOffset_MLX                = (float)         0.0;
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
  mySettings.useBME680                     = true;
  mySettings.useBME280                     = true;
  mySettings.useCCS811                     = true;
  mySettings.consumerLCD                   = true;
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
}
