//////////////////////////////////////////////////////////////////////////////////////////////////////////
// Air Quality Sensor
//////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Supported Hardware:
//  - ESP8266 micro controller.                     Using 2.x Arduino Libary  
//                                                  3.x is not compatible withthird party drivers
//  - SPS30 Senserion particle,                     Paul Vah library
//  - SCD30 Senserion CO2,                          Sparkfun library, using interrupt from data ready pin
//  - SGP30 Senserion VOC, eCO2,                    Sparkfun library
//  - BME680 Bosch Temp, Humidity, Pressure, VOC,   Adafruit library
//  - BM[E/P]280 Bosch Temp, [Humidity,] Pressure   Sparkfun library
//  - CCS811 Airquality eCO2 tVOC,                  Sparkfun library, using interrupt from data ready pin
//  - MLX90614 Melex temp contactless,              Sparkfun library
//  - MAX30105 Maxim pulseox, not implemented yet,  Sparkfun library
//
// Data display through:
//  - LCD 20x4,                                     LiquidCrystal_PCF8574 or Adafruit_LCD library
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
//  Over the Air programming can be enabled. 
//  HTTPUpdater can be enabled at http://host:8890/firmware username admin password .... 
//  mDNS responder and client are implemented, OTA starts them automatically
//  HTML webserver is implemented. Sensors can be read useing hostname/sensor. hostname/ provides self updating root page using websockets.
//  Large html files are streamed from LittleFS.
//
// Wire/I2C:
//  Multiple i2c pins are supported, ESP8266 Arduino supports only one active wire interface, however the pins can
//  be changed prior to each communication.
//  At startup, all pin combinations are scanned for possible scl & sda connections of the attached sensors.
//  Pins of supported sensors are registered.
//
//  Some breakout boards do not work well together with others.
//  Sensors might engage in excessive clockstretching and some have pull up resistor requirements beyond the one provided 
//  by the microcontroller.
//  For example CCS911 and SCD30 require long clock stretching. They are "slow" sensors. Its best to place them on separate i2c bus.
//  The LCD display requires 5V logic and corrupts regulalry when the i2c bus is shared, it should be placed on separate bus.
//  The MLX sensor might report negative or excessive high temperature when combined with arbitrary sensors.
//
// Improvements:
//  Some drivers were modified to allow for user specified i2c port instead of standard "Wire" port.
//  SDS30 library was modified to function with devices that have faulty version information
//
// Software implementation:
//  The sensor driver is divided into intializations and update section.
//  The updates are implemented as state machines and called on regular basis in the main loop.

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
// This software is provided as is, no warranty to its proper operation is implied. Use it at your own risk.
// Urs Utzinger
// Sometime: MAX pulseox software (requires proximit detection and rewritting signal processing library)
// Sometime: Configuration of system through website UI
// Sometime: Logfile to record system issues
// Sometime: Support for TFT display
// 2021 September: testing and release
// 2021 July: telnet
// 2021 June: webserver, NTP
// 2021 February, March: display update, mDNS, OTA
// 2021 January, i2c scanning and pin switching, OTA flashing
// 2020 Winter, fixing the i2c issues
// 2020 Fall, MQTT and Wifi
// 2020 Summer, first release
//////////////////////////////////////////////////////////////////////////////////////////////////////////

#undef DEBUG                                             
//#define DEBUG
// enabling this will pin point where program crashed as it displays a DBG statement that can be searched in the program

#undef PROFILE
//#define PROFILE
// enabling this will measure execution times of code segments

// #define ADALCD                                          // we have Adafruit i2c to LCD driver
#undef ADALCD                                              // we have non-Adafruit i2c to LCD driver

#define FASTMODE true
// true:  Measure with about 5 sec intervals,
// false: Measure in about 1 min intervals and enable sensor sleeping and low energy modes if available
// Slow mode needs tweaking and testing to determine which WiFi services are compatible with it.
// MQTT will need to update with single message. Likley OTA, Updater, Telnet will have issues with slow mode.
// With current software version we can not switch between slow and fast during runtime

// If you edit this code with Visual Studio Code: Set the define in VSC.h so that it findes the include files.
// To compile the code, disable the defines in VSC.h

/******************************************************************************************************/
// You should not need to change variables and constants below this line.
// Runtime changes to this software are enabled through the terminal.
// Permanent settings for the sensor and display devices can be changed in the src folder in the *.h files
/******************************************************************************************************/

// Memory Optimization:
// https://esp8266life.wordpress.com/2019/01/13/memory-memory-always-memory/
// https://arduino-esp8266.readthedocs.io/en/latest/PROGMEM.html
// These explain the use of F() amd PROGMEM and PSTR and sprintf_p().
// Currenlty program takes 50% of dynamic memory and 520k of program storage
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
#include "src/Sensi.h"
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
// --- SCD30; Sensirion CO2 sensor, Likely  most accurate sensor for CO2
#include "src/SCD30.h"
// --- SPS30; Senserion Particle Sensor,  likely most accurate sensor for PM2. PM110 might not be accurate though.
#include "src/SPS30.h"
// --- SGP30; Senserion TVOC, eCO2, accuracy is not known
#include "src/SGP30.h"
// --- CCS811; Airquality CO2 tVOC, traditional make comunity sensor
#include "src/CCS811.h"
// --- BME680; Bosch Temp, Humidity, Pressure, VOC, more features than 280 sensor, dont need both 280 and 680
#include "src/BME680.h"
// --- BME280; Bosch Temp, Humidity, Pressure, there is BME and BMP version.  One is lacking hymidity sensor.
#include "src/BME280.h"
// --- MLX contact less tempreture sensor, Thermal sensor for forehead readings. Likely not accurate.
#include "src/MLX.h"
// --- MAX30105; pulseoximeter, Not implemented yet
#include "src/MAX.h"
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
#include "src/LCD.h"
// --- Signal Interpretation
// See inside source code for the assessments and thresholds.
#include "src/Quality.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////
// Lets get started
//////////////////////////////////////////////////////////////////////////////////////////////////////////
void setup() {

  fastMode = FASTMODE;
  // true:  Measure with about 5 sec intervals,
  // false: Measure in about 1 min intervals and enable sleeping and low energy modes if available

  // Initialize wire interface
  // These settings might get changed inside the sensor drivers and should be reasserted after initialization.
  myWire.begin(D1, D2);
  myWire.setClock(100000); // 100kHz or 400kHz speed
  myWire.setClockStretchLimit(200000);

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
  //strcpy(mySettings.mqtt_mainTopic, "Sensi");
  //mySettings.useMQTT = false;
  //mySettings.useNTP  = true;
  //mySettings.usemDNS = false;
  //mySettings.useHTTP = false;
  //mySettings.useOTA  = false;
  //strcpy(mySettings.ntpServer, "time.nist.gov");
  //mySettings.utcOffset = -7;
  //mySettings.enableDST = false;
  //strcpy(mySettings.timeZone, "MST7"); // https://raw.githubusercontent.com/nayarsystems/posix_tz_db/master/zones.csv
  //mySettings.useBacklight = false;
  //mySettings.useHTTPUpdater = false;
  //mySettings.useTelnet = false;
  //mySettings.useSerial = true;

  if (mySettings.useSerial) {
    Serial.begin(BAUDRATE);
  }
  if ( mySettings.debuglevel > 0) { sprintf_P(tmpStr, PSTR("\r\nDebug level is: %d\r\n"), mySettings.debuglevel);  printSerialTelnet(tmpStr); }

  /******************************************************************************************************/
  // Start file system and list content
  /******************************************************************************************************/
  fileSystemConfig.setAutoFormat(false);
  LittleFS.setConfig(fileSystemConfig);
  fsOK=LittleFS.begin(); 
  if(!fsOK){ printSerialTelnet(F("LittleFS mount failed!\r\n")); }
  if ( mySettings.debuglevel > 0 && fsOK) {
    printSerialTelnet(F("LittleFS started. Contents:\r\n"));
    Dir dir = LittleFS.openDir("/");
    bool filefound = false;
    while (dir.next()) {                      // List the file system contents
      String fileName = dir.fileName();
      size_t fileSize = dir.fileSize();
      sprintf_P(tmpStr, PSTR("\tName: %s, Size: %s\r\n"), fileName.c_str(), formatBytes(fileSize).c_str()); printSerialTelnet(tmpStr);
      filefound = true;
    }
    if (!filefound) { printSerialTelnet(F("empty\r\n")); }
  }  

  //Could also store settings on LittelFS
  //Issue is that LittleFS and JSON library together seem to need more memory than available
  //File myFile = LittleFS.open("/Sensi.config", "r");
  //myFile.read((byte *)&mySettings, sizeof(mySettings));
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
  if (mySettings.useHTTP           > 0) { mySettings.useHTTP           = true; } else { mySettings.useHTTP             = false; }
  if (mySettings.useHTTPUpdater    > 0) { mySettings.useHTTPUpdater    = true; } else { mySettings.useHTTPUpdater      = false; }
  if (mySettings.useNTP            > 0) { mySettings.useNTP            = true; } else { mySettings.useNTP              = false; }
  if (mySettings.useMQTT           > 0) { mySettings.useMQTT           = true; } else { mySettings.useMQTT             = false; }
  if (mySettings.useOTA            > 0) { mySettings.useOTA            = true; } else { mySettings.useOTA              = false; }
  if (mySettings.usemDNS           > 0) { mySettings.usemDNS           = true; } else { mySettings.usemDNS             = false; }
  if (mySettings.useTelnet         > 0) { mySettings.useTelnet         = true; } else { mySettings.useTelnet           = false; }
  if (mySettings.useSerial         > 0) { mySettings.useSerial         = true; } else { mySettings.useSerial           = false; }

  /******************************************************************************************************/
  // Check which devices are attached to the I2C pins, this self configures our connections to the sensors
  /******************************************************************************************************/
  // Wemos 
  // uint8_t portArray[] = {16, 5, 4, 0, 2, 14, 12, 13};
  // String portMap[] = {"D0", "D1", "D2", "D3", "D4", "D5", "D6", "D7"};

  // dont search on ports used for data ready or interrupt signaling, otherwise use all the ones listed above
  uint8_t portArray[] = {5, 4, 0, 2, 14, 12};
  char portMap[6][3] = {"D1", "D2", "D3", "D4", "D5", "D6"};
  byte error, address;
  
  // lets turn on CCS811 so we can find it ;-)
  pinMode(CCS811_WAKE, OUTPUT);                        // CCS811 not Wake Pin
  digitalWrite(CCS811_WAKE, LOW);                      // set CCS811 to wake up

  for (uint8_t i = 0; i < sizeof(portArray); i++) {
    for (uint8_t j = 0; j < sizeof(portArray); j++) {
      if (i != j) {
        sprintf_P(tmpStr, PSTR("Scanning (SDA:SCL) - %s:%s\r\n"), portMap[i], portMap[j]); printSerialTelnet(tmpStr);
        Wire.begin(portArray[i], portArray[j]);
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
              sprintf_P(tmpStr, PSTR("Found unknonw device - %d!"),address); printSerialTelnet(tmpStr);
            }
          }
        }
      }
    }
  }

  // List the devices we found
  if (lcd_avail)    { sprintf_P(tmpStr, PSTR("LCD                  available: %d SDA %d SCL %d\r\n"), uint32_t(lcd_port),    lcd_i2c[0],    lcd_i2c[1]);    } else { sprintf_P(tmpStr, PSTR("LCD                  not available\r\n")); }  printSerialTelnet(tmpStr);
  if (max_avail)    { sprintf_P(tmpStr, PSTR("MAX30105             available: %d SDA %d SCL %d\r\n"), uint32_t(max_port),    max_i2c[0],    max_i2c[1]);    } else { sprintf_P(tmpStr, PSTR("MAX30105             not available\r\n")); }  printSerialTelnet(tmpStr);
  if (ccs811_avail) { sprintf_P(tmpStr, PSTR("CCS811 eCO2, tVOC    available: %d SDA %d SCL %d\r\n"), uint32_t(ccs811_port), ccs811_i2c[0], ccs811_i2c[1]); } else { sprintf_P(tmpStr, PSTR("CCS811 eCO2, tVOC    not available\r\n")); }  printSerialTelnet(tmpStr);
  if (sgp30_avail)  { sprintf_P(tmpStr, PSTR("SGP30 eCO2, tVOC     available: %d SDA %d SCL %d\r\n"), uint32_t(sgp30_port),  sgp30_i2c[0],  sgp30_i2c[1]);  } else { sprintf_P(tmpStr, PSTR("SGP30 eCO2, tVOC     not available\r\n")); }  printSerialTelnet(tmpStr);
  if (therm_avail)  { sprintf_P(tmpStr, PSTR("MLX temp             available: %d SDA %d SCL %d\r\n"), uint32_t(mlx_port),    mlx_i2c[0],    mlx_i2c[1]);    } else { sprintf_P(tmpStr, PSTR("MLX temp             not available\r\n")); }  printSerialTelnet(tmpStr);
  if (scd30_avail)  { sprintf_P(tmpStr, PSTR("SCD30 CO2, rH        available: %d SDA %d SCL %d\r\n"), uint32_t(scd30_port),  scd30_i2c[0],  scd30_i2c[1]);  } else { sprintf_P(tmpStr, PSTR("SCD30 CO2, rH        not available\r\n")); }  printSerialTelnet(tmpStr);
  if (sps30_avail)  { sprintf_P(tmpStr, PSTR("SPS30 PM             available: %d SDA %d SCL %d\r\n"), uint32_t(sps30_port),  sps30_i2c[0],  sps30_i2c[1]);  } else { sprintf_P(tmpStr, PSTR("SPS30 PM             not available\r\n")); }  printSerialTelnet(tmpStr);
  if (bme280_avail) { sprintf_P(tmpStr, PSTR("BM[E/P]280 T, P[,rH] available: %d SDA %d SCL %d\r\n"), uint32_t(bme280_port), bme280_i2c[0], bme280_i2c[1]); } else { sprintf_P(tmpStr, PSTR("BM[E/P]280 T, P[,rH] not available\r\n")); }  printSerialTelnet(tmpStr);
  if (bme680_avail) { sprintf_P(tmpStr, PSTR("BME680 T, rH, P tVOC available: %d SDA %d SCL %d\r\n"), uint32_t(bme680_port), bme680_i2c[0], bme680_i2c[1]); } else { sprintf_P(tmpStr, PSTR("BME680 T, rH, P tVOC not available\r\n")); }  printSerialTelnet(tmpStr);

  /******************************************************************************************************/
  // Time Intervals for Loop, Display and MQTT publishing, keeping track of runtime
  /******************************************************************************************************/
  // Software is designed to run either with frequent updates or in slow mode with updates in the minutes
  
  if (fastMode == true) {                                     // fast mode -----------------------------------
    intervalLoop     =                  100;                  // 0.1 sec, main loop runs 10 times per second
    intervalLCD      =      intervalLCDFast;                  // LCD display is updated every 10 seconds
    intervalMQTT     =     intervalMQTTFast;                  //
    intervalRuntime  =                60000;                  // 1 minute, uptime is updted every minute
    intervalBaseline = intervalBaselineFast;                  // 10 minutes
  } else {                                                    // slow mode -----------------------------------
    intervalLoop     =                 1000;                  // 1 sec, main loop runs once a second
    intervalLCD      =      intervalLCDSlow;                  // 1 minute
    intervalMQTT     =     intervalMQTTSlow;                  //
    intervalRuntime  =               600000;                  // 10 minutes
    intervalBaseline = intervalBaselineSlow;                  // 1 hr
  }

  /******************************************************************************************************/
  // Initialize all devices
  /******************************************************************************************************/

  if (lcd_avail && mySettings.useLCD)       { if (initializeLCD()    == false) { lcd_avail    = false; } } else { lcd_avail    = false; }  // Initialize LCD Screen
  #if defined(DEBUG)
  printSerialTelnet(F("DBG:INI: LCD\r\n"));
  #endif
  if (scd30_avail && mySettings.useSCD30)   { if (initializeSCD30()  == false) { scd30_avail  = false; } } else { scd30_avail  = false; }  // Initialize SCD30 CO2 sensor
  #if defined(DEBUG)
  printSerialTelnet(F("DBG:INI: SCD30\r\n"));
  #endif
  if (sgp30_avail && mySettings.useSGP30)   { if (initializeSGP30()  == false) { sgp30_avail  = false; } } else { sgp30_avail  = false; }  // SGP30 Initialize
  #if defined(DEBUG)
  printSerialTelnet(F("DBG:INI: SGP30\r\n"));
  #endif
  if (ccs811_avail && mySettings.useCCS811) { if (initializeCCS811() == false) { ccs811_avail = false; } } else { ccs811_avail = false; }  // CCS811 Initialize
  #if defined(DEBUG)
  printSerialTelnet(F("DBG:INI: CCS811\r\n"));
  #endif
  if (sps30_avail && mySettings.useSPS30)   { if (initializeSPS30()  == false) { sps30_avail  = false; } } else { sps30_avail  = false; }  // SPS30 Initialize Particle Sensor
  #if defined(DEBUG) 
  printSerialTelnet(F("DBG:INI: SPS30\r\n"));
  #endif
  if (bme680_avail && mySettings.useBME680) { if (initializeBME680() == false) { bme680_avail = false; } } else { bme680_avail = false; }  // Initialize BME680
  #if defined(DEBUG)
  printSerialTelnet(F("DBG:INI: BME680\r\n"));
  #endif
  if (bme280_avail && mySettings.useBME280) { if (initializeBME280() == false) { bme280_avail = false; } } else { bme280_avail = false; }  // Initialize BME280
  #if defined(DEBUG)
  printSerialTelnet(F("DBG:INI: BME280\r\n"));
  #endif
  if (therm_avail && mySettings.useMLX)     { if (initializeMLX()    == false) { therm_avail  = false; } } else { therm_avail  = false; }  // Initialize MLX Sensor
  #if defined(DEBUG)
  printSerialTelnet(F("DBG:INI: MLX\r\n"));
  #endif
  if (max_avail && mySettings.useMAX30)     {                                                            } else { max_avail    = false; }  // Initialize MAX Pulse OX Sensor, N.A.
  #if defined(DEBUG)
  printSerialTelnet(F("DBG:INI: MAX30\r\n"));
  #endif

  /******************************************************************************************************/
  // Make sure i2c bus was not changed during above's initializations.
  // These settings will work for all sensors supported.
  // Since one has only one wire instance available on EPS8266, it does not make sense to operate the
  // pins at differnt clock speeds.
  /******************************************************************************************************/
  myWire.setClock(100000);
  myWire.setClockStretchLimit(200000);

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
  yieldTimeBefore     = currentTime;

  /******************************************************************************************************/
  // Populate LCD screen, start with cleared LCD
  /******************************************************************************************************/
  #if defined(DEBUG)
    printSerialTelnet(F("DBG:SET: LCD\r\n"));
  #endif
  if (lcd_avail && mySettings.useLCD) {
    if (mySettings.consumerLCD) { updateSinglePageLCDwTime(); } else { updateLCD(); }
    if (mySettings.debuglevel > 0) { printSerialTelnet(F("LCD updated.")); }
  }
  
  /******************************************************************************************************/
  // Connect to WiFi
  /******************************************************************************************************/
  #if defined(DEBUG)
    printSerialTelnet(F("DBG:INI: Wifi\r\n"));
  #endif
  initializeWiFi(); // if user request wifi to be off, functions depebding on wifi will not be enabled

  /******************************************************************************************************/
  // System is initialized
  /******************************************************************************************************/
  if ( (mySettings.debuglevel > 0) && mySettings.useSerial ) {  printSerialTelnet(F("System initialized\r\n")); }
  #if defined(DEBUG)
  printSerialTelnet(F("DBG:INI: COMPLETED\r\n"));
  #endif

  scheduleReboot = false;

  // give user time to obeserve boot information
  if ( (mySettings.debuglevel > 0) && mySettings.useSerial ) { serialTrigger(waitmsg, 10000); }

} // end setup

//////////////////////////////////////////////////////////////////////////////////////////////////////////
// The never ending story
//////////////////////////////////////////////////////////////////////////////////////////////////////////
void loop() {
  //////////////////////////////////////////////////////////////////////////////////////////////////////////
  //////////////////////////////////////////////////////////////////////////////////////////////////////////
  currentTime = millis(); // keep track of local time

  if (otaInProgress) { updateOTA(); } // when OTA is in progress we do not do anything else
  else { // OTA is not in progress, we update the subsystems

    // update current time every second
    if ( (currentTime - lastcurrentTime) >= 1000 ) {
      lastcurrentTime = currentTime;

      #if defined(DEBUG)
      printSerialTelnet(F("DBG:UPDATE: DATE&TIME\r\n"));
      #endif

      #if defined(PROFILE)
      startUpdate = millis(); // how long does this code segment take?
      #endif
      
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

      #if defined(DEBUG)
      sprintf_P(tmpStr, PSTR("DBG:NTP:TIME ntp: %u local: %u, internal: %u\r\n"), NTP.millis()/1000, (unsigned long) actualTime, currentTime/1000 );
      printSerialTelnet(tmpStr);
      sprintf_P(tmpStr, PSTR("DBG:NTP:DATE %d:%d:%d %d,%d,%d\r\n"),localTime->tm_hour, localTime->tm_min, localTime->tm_sec, localTime->tm_mon, localTime->tm_mday, localTime->tm_year+1900 );
      printSerialTelnet(tmpStr);
      #endif

      #if defined(PROFILE)
      deltaUpdate = millis() - startUpdate; // this code segment took delta ms
      if (maxUpdateTime < deltaUpdate) { maxUpdateTime = deltaUpdate; } // current max updatetime for this code segment
      if (AllmaxUpdateTime < deltaUpdate) { AllmaxUpdateTime = deltaUpdate; } // longest global updatetime for this code segement
      #endif
      
      #if defined(PROFILE)
      yieldTimeBefore = millis(); 
      #endif
      yield(); 
      #if defined(PROFILE)
      yieldTime += (millis()-yieldTimeBefore); 
      #endif
      
    }
    
    /******************************************************************************************************/
    // Update the State Machines for all Devices and System Processes
    /******************************************************************************************************/
    // Update Wireless Services
    if (wifi_avail && mySettings.useWiFi)                         { 
      #if defined(DEBUG) 
      printSerialTelnet(F("DBG:UPDATE: Wifi\r\n"));
      #endif
      
      #if defined(PROFILE)
      startUpdate = millis();
      #endif
      
      updateWiFi(); //<<<<<<<<<<<<<<<<<<<<
      
      #if defined(PROFILE)
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateWifi < deltaUpdate) { maxUpdateWifi = deltaUpdate; }
      if (AllmaxUpdateWifi < deltaUpdate) { AllmaxUpdateWifi = deltaUpdate; }
      #endif
    } // -------------------------------------------------------------- WiFi update, this will reconnect if disconnected
    if (wifi_avail     && mySettings.useWiFi && mySettings.useOTA)    { 
      #if defined(DEBUG)
      printSerialTelnet(F("DBG:UPDATE: OTA\r\n"));
      #endif
      
      #if defined(PROFILE)
      startUpdate = millis();
      #endif
      
      updateOTA(); //<<<<<<<<<<<<<<<<<<<<
      
      #if defined(PROFILE)
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateOTA < deltaUpdate) { maxUpdateOTA = deltaUpdate; }         
      if (AllmaxUpdateOTA < deltaUpdate) { AllmaxUpdateOTA = deltaUpdate; }         
      #endif
    } // -------------------------------------------------------------- Update OTA
    if (wifi_avail     && mySettings.useWiFi && mySettings.useNTP)    { 
      #if defined(DEBUG) 
      printSerialTelnet(F("DBG:UPDATE: NTP\r\n"));
      #endif  
      
      #if defined(PROFILE)
      startUpdate = millis();
      #endif
      
      updateNTP();  //<<<<<<<<<<<<<<<<<<<<
      
      #if defined(PROFILE)
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateNTP < deltaUpdate) { maxUpdateNTP = deltaUpdate; }
      if (AllmaxUpdateNTP < deltaUpdate) { AllmaxUpdateNTP = deltaUpdate; }
      #endif
    } // -------------------------------------------------------------- Update network time
    if (wifi_avail     && mySettings.useWiFi && mySettings.useMQTT)   { 
      #if defined(DEBUG)
      printSerialTelnet(F("DBG:UPDATE: MQTT\r\n"));
      #endif

      #if defined(PROFILE)
      startUpdate = millis();
      #endif

      updateMQTT(); //<<<<<<<<<<<<<<<<<<<<
      
      #if defined(PROFILE)
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateMQTT < deltaUpdate) { maxUpdateMQTT = deltaUpdate; }
      if (AllmaxUpdateMQTT < deltaUpdate) { AllmaxUpdateMQTT = deltaUpdate; }
      #endif
    } // -------------------------------------------------------------- MQTT update server connection
    if (wifi_avail     && mySettings.useWiFi && mySettings.useHTTP)   { 
      #if defined(DEBUG) 
      printSerialTelnet(F("DBG:UPDATE: WS\r\n"));
      #endif
      
      #if defined(PROFILE)
      startUpdate = millis();
      #endif
      
      updateWebSocket(); //<<<<<<<<<<<<<<<<<<<<
      
      #if defined(PROFILE)
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateWS < deltaUpdate) { maxUpdateWS = deltaUpdate; }
      if (AllmaxUpdateWS < deltaUpdate) { AllmaxUpdateWS = deltaUpdate; }
      #endif
    } // -------------------------------------------------------------- Websocket update server connection
    if (wifi_avail     && mySettings.useWiFi && mySettings.useHTTP)   { 
      #if defined(DEBUG) 
      printSerialTelnet(F("DBG:UPDATE: HTTP\r\n"));
      #endif
      
      #if defined(PROFILE)
      startUpdate = millis();
      #endif
      
      updateHTTP(); //<<<<<<<<<<<<<<<<<<<<
      
      #if defined(PROFILE)
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateHTTP < deltaUpdate) { maxUpdateHTTP = deltaUpdate; }
      if (AllmaxUpdateHTTP < deltaUpdate) { AllmaxUpdateHTTP = deltaUpdate; }
      #endif
    } // -------------------------------------------------------------- Update web server
    if (wifi_avail     && mySettings.useWiFi && mySettings.useHTTPUpdater)   { 
      #if defined(DEBUG)
      printSerialTelnet(F("DBG:UPDATE: HTTPUPDATER\r\n"));
      #endif
      
      #if defined(PROFILE)
      startUpdate = millis();
      #endif
      
      updateHTTPUpdater();  //<<<<<<<<<<<<<<<<<<<<
      
      #if defined(PROFILE)
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateHTTPUPDATER < deltaUpdate) { maxUpdateHTTPUPDATER = deltaUpdate; }
      if (AllmaxUpdateHTTPUPDATER < deltaUpdate) { AllmaxUpdateHTTPUPDATER = deltaUpdate; }
      #endif
    } // -------------------------------------------------------------- Update firmware web server
    if (wifi_avail     && mySettings.useWiFi && mySettings.usemDNS)   { 
      #if defined(DEBUG) 
      printSerialTelnet(F("DBG:UPDATE: mDNS\r\n"));
      #endif  
      
      #if defined(PROFILE)
      startUpdate = millis();
      #endif
      
      updateMDNS(); //<<<<<<<<<<<<<<<<<<<<
      
      #if defined(PROFILE)
      deltaUpdate = millis() - startUpdate;
      if (maxUpdatemDNS < deltaUpdate) { maxUpdatemDNS = deltaUpdate; }
      if (AllmaxUpdatemDNS < deltaUpdate) { AllmaxUpdatemDNS = deltaUpdate; }
      #endif
    } // -------------------------------------------------------------- Update mDNS


    if (wifi_avail     && mySettings.useWiFi && mySettings.useTelnet)    { 
      #if defined(DEBUG) 
      printSerialTelnet(F("DBG:UPDATE: Telnet\r\n"));
      #endif  
      
      #if defined(PROFILE)
      startUpdate = millis();
      #endif
      
      updateTelnet();  //<<<<<<<<<<<<<<<<<<<<
      
      #if defined(PROFILE)
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateTelnet < deltaUpdate) { maxUpdateTelnet = deltaUpdate; }
      if (AllmaxUpdateTelnet < deltaUpdate) { AllmaxUpdateTelnet = deltaUpdate; }
      #endif
    }

    #if defined(PROFILE)
    yieldTimeBefore = millis(); 
    #endif
    yield(); 
    #if defined(PROFILE)
    yieldTime += (millis()-yieldTimeBefore); 
    #endif
    
    // Update Sensor Readings
    if (scd30_avail    && mySettings.useSCD30)                        { 
      #if defined(DEBUG)
      printSerialTelnet(F("DBG:UPDATE: SCD30\r\n"));
      #endif
      
      #if defined(PROFILE)
      startUpdate = millis();
      #endif
      
      if (updateSCD30()  == false) {scheduleReboot = true;}  //<<<<<<<<<<<<<<<<<<<<
      
      #if defined(PROFILE)
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateSCD30 < deltaUpdate) { maxUpdateSCD30 = deltaUpdate; }
      if (AllmaxUpdateSCD30 < deltaUpdate) { AllmaxUpdateSCD30 = deltaUpdate; }
      #endif
    } // -------------------------------------------------------------- SCD30 Sensirion CO2 sensor
    if (sgp30_avail    && mySettings.useSGP30)                        { 
      #if defined(DEBUG)
      printSerialTelnet(F("DBG:UPDATE: SGP30\r\n"));
      #endif  
      
      #if defined(PROFILE)
      startUpdate = millis();
      #endif
      
      if (updateSGP30()  == false) {scheduleReboot = true;}  //<<<<<<<<<<<<<<<<<<<<
      
      #if defined(PROFILE)
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateSGP30 < deltaUpdate) { maxUpdateSGP30 = deltaUpdate; }
      if (AllmaxUpdateSGP30 < deltaUpdate) { AllmaxUpdateSGP30 = deltaUpdate; }
      #endif
    } // -------------------------------------------------------------- SGP30 Sensirion eCO2 sensor
    if (ccs811_avail   && mySettings.useCCS811)                       { 
      #if defined(DEBUG)
      printSerialTelnet(F("DBG:UPDATE: CCS811\r\n"));
      #endif

      #if defined(PROFILE)
      startUpdate = millis();
      #endif
      
      if (updateCCS811() == false) {scheduleReboot = true;}  //<<<<<<<<<<<<<<<<<<<<
      
      #if defined(PROFILE)
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateCCS811 < deltaUpdate) { maxUpdateCCS811 = deltaUpdate; }
      if (AllmaxUpdateCCS811 < deltaUpdate) { AllmaxUpdateCCS811 = deltaUpdate; }
      #endif
    } // CCS811
    if (sps30_avail    && mySettings.useSPS30)                        { 
      #if defined(DEBUG)
      printSerialTelnet(F("DBG:UPDATE: SPS30\r\n"));
      #endif
      
      #if defined(PROFILE)
      startUpdate = millis();
      #endif
      
      if (updateSPS30()  == false) {scheduleReboot = true;}  //<<<<<<<<<<<<<<<<<<<<
      
      #if defined(PROFILE)
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateSPS30 < deltaUpdate) { maxUpdateSPS30 = deltaUpdate; }
      if (AllmaxUpdateSPS30 < deltaUpdate) { AllmaxUpdateSPS30 = deltaUpdate; }
      #endif
    } // -------------------------------------------------------------- SPS30 Sensirion Particle Sensor State Machine
    if (bme280_avail   && mySettings.useBME280)                       { 
      #if defined(DEBUG) 
      printSerialTelnet(F("DBG:UPDATE: BME280\r\n"));
      #endif  
      
      #if defined(PROFILE)
      startUpdate = millis();
      #endif
      
      if (updateBME280() == false) {scheduleReboot = true;}  //<<<<<<<<<<<<<<<<<<<<
      
      #if defined(PROFILE)
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateBME280 < deltaUpdate) { maxUpdateBME280 = deltaUpdate; }
      if (AllmaxUpdateBME280 < deltaUpdate) { AllmaxUpdateBME280 = deltaUpdate; }
      #endif
    } // -------------------------------------------------------------- BME280, Hum, Press, Temp
    if (bme680_avail   && mySettings.useBME680)                       { 
      #if defined(DEBUG)
      printSerialTelnet(F("DBG:UPDATE: BME680\r\n"));
      #endif  
      
      #if defined(PROFILE)
      startUpdate = millis();
      #endif
      
      if (updateBME680() == false) {scheduleReboot = true;}  //<<<<<<<<<<<<<<<<<<<<
      
      #if defined(PROFILE)
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateBME680 < deltaUpdate) { maxUpdateBME680 = deltaUpdate; }
      if (AllmaxUpdateBME680 < deltaUpdate) { AllmaxUpdateBME680 = deltaUpdate; }
      #endif
    } // -------------------------------------------------------------- BME680, Hum, Press, Temp, Gasresistance
    if (therm_avail    && mySettings.useMLX)                          { 
      #if defined(DEBUG)
      printSerialTelnet(F("DBG:UPDATE: MLX\r\n"));
      #endif
      
      #if defined(PROFILE)
      startUpdate = millis();
      #endif
      
      if (updateMLX()    == false) {scheduleReboot = true;}  //<<<<<<<<<<<<<<<<<<<<
      
      #if defined(PROFILE)
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateMLX < deltaUpdate) { maxUpdateMLX = deltaUpdate; }      
      if (AllmaxUpdateMLX < deltaUpdate) { AllmaxUpdateMLX = deltaUpdate; }      
      #endif
    } // -------------------------------------------------------------- MLX Contactless Thermal Sensor
    if (max_avail      && mySettings.useMAX30)                        {
      #if defined(DEBUG) 
      printSerialTelnet(F("DBG:UPDATE: MAX30\r\n"));
      #endif
      
      #if defined(PROFILE)
      startUpdate = millis();
      #endif
      
      #if defined(PROFILE)
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateMAX < deltaUpdate) { maxUpdateMAX = deltaUpdate; } //<<<<<<<<<<<<<<<<<<<<
      if (AllmaxUpdateMAX < deltaUpdate) { AllmaxUpdateMAX = deltaUpdate; }
      #endif
    } // -------------------------------------------------------------- MAX Pulse Ox Sensor

    #if defined(PROFILE)
    yieldTimeBefore = millis(); 
    #endif
    yield(); 
    #if defined(PROFILE)
    yieldTime += (millis()-yieldTimeBefore); 
    #endif
    
    /******************************************************************************************************/
    // Broadcast MQTT and WebSocket Messages
    /******************************************************************************************************/
    if (mqtt_connected)                                               {
      #if defined(DEBUG)
      printSerialTelnet(F("DBG:UPDATE: MQTTMESSAGE\r\n"));
      #endif
      
      #if defined(PROFILE)
      startUpdate = millis();
      #endif
      
      updateMQTTMessage();  //<<<<<<<<<<<<<<<<<<<<
      
      #if defined(PROFILE)
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateMQTTMESSAGE < deltaUpdate) { maxUpdateMQTTMESSAGE = deltaUpdate; }
      if (AllmaxUpdateMQTTMESSAGE < deltaUpdate) { AllmaxUpdateMQTTMESSAGE = deltaUpdate; }
      #endif
    } // -------------------------------------------------------------- MQTT send sensor data

    #if defined(PROFILE)
    yieldTimeBefore = millis();
    #endif
    yield(); 
    #if defined(PROFILE)
    yieldTime += (millis()-yieldTimeBefore); 
    #endif

    if (ws_connected)                                                 { 
      #if defined(DEBUG)
      printSerialTelnet(F("DBG:UPDATE: WSMESSAGE\r\n"));
      #endif

      #if defined(PROFILE)
      startUpdate = millis();
      #endif
      
      updateWebSocketMessage();  //<<<<<<<<<<<<<<<<<<<<
      
      #if defined(PROFILE)
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateWSMESSAGE < deltaUpdate) { maxUpdateWSMESSAGE = deltaUpdate; }
      if (AllmaxUpdateWSMESSAGE < deltaUpdate) { AllmaxUpdateWSMESSAGE = deltaUpdate; }
      #endif
    } // -------------------------------------------------------------- WebSocket send sensor data

    #if defined(PROFILE)
    yieldTimeBefore = millis(); 
    #endif
    yield(); 
    #if defined(PROFILE)
    yieldTime += (millis()-yieldTimeBefore); 
    #endif
    
    /******************************************************************************************************/
    // LCD: Display Sensor Data
    /******************************************************************************************************/
    if (lcd_avail && mySettings.useLCD) {
      if ((currentTime - lastLCD) >= intervalLCD) {
        lastLCD = currentTime;
        #if defined(DEBUG)
        printSerialTelnet(F("DBG:UPDATE: LCD\r\n"));
        #endif
        
        #if defined(PROFILE)
        tmpTime = millis();
        #endif
        
        if (  mySettings.consumerLCD ) { updateSinglePageLCDwTime(); } else { updateLCD(); } //<<<<<<<<<<<<<<<<<<<<
        if ( (mySettings.debuglevel > 1) && mySettings.useSerial ) { printSerialTelnet(F("LCD updated\r\n")); }
        
        #if defined(PROFILE)
        deltaUpdate = millis() - tmpTime;
        if (maxUpdateLCD < deltaUpdate) { maxUpdateLCD = deltaUpdate; }
        if (AllmaxUpdateLCD < deltaUpdate) { AllmaxUpdateLCD = deltaUpdate; }
        #endif

        #if defined(PROFILE)
        yieldTimeBefore = millis(); 
        #endif
        yield(); 
        #if defined(PROFILE)
        yieldTime += (millis()-yieldTimeBefore); 
        #endif
        
      }
    }
  
    /******************************************************************************************************/
    // Terminal Status Display
    /******************************************************************************************************/
    if ((currentTime - lastSYS) >= intervalSYS) {
      lastSYS = currentTime;
      
      if (mySettings.debuglevel == 99) {

        #if defined(DEBUG)
        printSerialTelnet(F("DBG:UPDATE: SYS\r\n"));
        #endif

        #if defined(PROFILE)
        startUpdate = millis();
        #endif

        // Updating these values over serial display takes longer than the average loop time, therefore the minimum loop delay is always negative (meaning there is not enough time to complete a loop every 1/10 second).
        // However average loop delay is calculated only from positive values.

        printSerialTelnet(FPSTR(clearHome)); // clear and home characters in terminal
        printSerialTelnet(FPSTR(doubleSeparator)); //---------------------------------------------------------------------------------------------------//        
        sprintf_P(tmpStr, PSTR("Average loop time %f[ms]  Min/Max/Alltime: %d/%d/%d[ms] Current: %d[ms]\r\n"), myLoopAvg, myLoopMin, myLoopMax, myLoopMaxAllTime, myLoop); printSerialTelnet(tmpStr);
        sprintf_P(tmpStr, PSTR("Free Heap Size: %dbytes Heap Fragmentation: %d%% Max Block Size: %dbytes\r\n"), ESP.getFreeHeap(), ESP.getHeapFragmentation(), ESP.getMaxFreeBlockSize()); printSerialTelnet(tmpStr);
        printSerialTelnet(FPSTR(doubleSeparator)); //---------------------------------------------------------------------------------------------------//

        #if defined(PROFILE)
        // This displays the amount of time it took complete the update routines.
        // The currentMax/alltimeMax is displayed. Alltime max is reset when network services are started up in their update routines.
        sprintf_P(tmpStr, PSTR("Update Time&Date: %4u %4u mDNS:   %4u %4u HTTPupd: %4u %4u HTTP:    %4u %4u\r\nUpdate MQTT:      %4u %4u WS:     %4u %4u NTP:     %4u %4u OTA:     %4u %4u\r\nUpdate Wifi:      %4u %4u \r\n"), 
                         maxUpdateTime, AllmaxUpdateTime, maxUpdatemDNS, AllmaxUpdatemDNS, maxUpdateHTTPUPDATER, AllmaxUpdateHTTPUPDATER, maxUpdateHTTP, AllmaxUpdateHTTP, maxUpdateMQTT, AllmaxUpdateMQTT, 
                         maxUpdateWS, AllmaxUpdateWS, maxUpdateNTP, AllmaxUpdateNTP, maxUpdateOTA, AllmaxUpdateOTA, maxUpdateWifi, AllmaxUpdateWifi);         
        printSerialTelnet(tmpStr);
        sprintf_P(tmpStr, PSTR("Update SCD30:     %4u %4u SGP30:  %4u %4u CCS811:  %4u %4u SPS30:   %4u %4u\r\nUpdate BME280:    %4u %4u BME680: %4u %4u MLX:     %4u %4u MAX:     %4u %4u\r\n"), maxUpdateSCD30, AllmaxUpdateSCD30, 
                         maxUpdateSGP30, AllmaxUpdateSGP30, maxUpdateCCS811, AllmaxUpdateCCS811, maxUpdateSPS30, AllmaxUpdateSPS30, maxUpdateBME280, AllmaxUpdateBME280, maxUpdateBME680, AllmaxUpdateBME680, maxUpdateMLX, AllmaxUpdateMLX, maxUpdateMAX, AllmaxUpdateMAX);
        printSerialTelnet(tmpStr);
        sprintf_P(tmpStr, PSTR("Update MQTTmsg:   %4u %4u WSmsg:  %4u %4u LCD:     %4u %4u Input:   %4u %4u\r\nUpdate RunTime:   %4u %4u EEPROM: %4u %4u\r\n"), maxUpdateMQTTMESSAGE, AllmaxUpdateMQTTMESSAGE, maxUpdateWSMESSAGE, AllmaxUpdateWSMESSAGE, 
                         maxUpdateLCD, AllmaxUpdateLCD, maxUpdateINPUT, AllmaxUpdateINPUT, maxUpdateRT, AllmaxUpdateRT, maxUpdateEEPROM, AllmaxUpdateEEPROM);
                         //maxUpdateJS, AllmaxUpdateJS
        printSerialTelnet(tmpStr);
        sprintf_P(tmpStr, PSTR("Update Baseline:  %4u %4u Blink:  %4u %4u Reboot:  %4u %4u AllGood: %4u %4u\r\n"), 
                         maxUpdateBASE, AllmaxUpdateBASE, AllmaxUpdateBLINK, maxUpdateBLINK, AllmaxUpdateREBOOT, maxUpdateREBOOT, AllmaxUpdateALLGOOD, maxUpdateALLGOOD);
        printSerialTelnet(tmpStr);
        sprintf_P(tmpStr, PSTR("Update this display: %u\r\n"), 
                         maxUpdateSYS);
        printSerialTelnet(tmpStr);
        sprintf_P(tmpStr, PSTR("Yield Time:          %u %u %u          ", 
                         yieldTimeMin, yieldTimeMax, yieldTimeMaxAllTime); 
        printSerialTelnet(tmpStr);
        yieldTimeMin = yieldTimeMax = 0;
        printSerialTelnet(F("All values in [ms]\r\n"));
        printSerialTelnet(FPSTR(singleSeparator)); //---------------------------------------------------------------------------------------------------//
        #endif

        printState();
        
        printSensors();
        
        #if defined(PROFILE)
        deltaUpdate = millis() - startUpdate;
        if (maxUpdateSYS < deltaUpdate) { maxUpdateSYS = deltaUpdate; }
        #endif

        #if defined(PROFILE)
        yieldTimeBefore = millis(); 
        #endif
        yield(); 
        #if defined(PROFILE)
        yieldTime += (millis()-yieldTimeBefore); 
        #endif

      } // dbg level 99
      // reset max values and measure them again until next display
      // AllmaxUpdate... are the runtime maxima that are not reset between each display update
      myDelayMin = intervalSYS; myLoopMin = intervalSYS; myLoopMax = 0;
      
      #if defined(PROFILE)
      maxUpdateTime = maxUpdateWifi = maxUpdateOTA =  maxUpdateNTP = maxUpdateMQTT = maxUpdateHTTP = maxUpdateHTTPUPDATER = maxUpdatemDNS = 0;
      maxUpdateSCD30 = maxUpdateSGP30 =  maxUpdateCCS811 = maxUpdateSPS30 = maxUpdateBME280 = maxUpdateBME680 = maxUpdateMLX = maxUpdateMAX = 0;
      maxUpdateMQTTMESSAGE = maxUpdateWSMESSAGE = maxUpdateLCD = maxUpdateINPUT = maxUpdateRT = maxUpdateEEPROM = maxUpdateBASE = maxUpdateBLINK = maxUpdateREBOOT = maxUpdateALLGOOD = 0;
      //maxUpdateJS
      #endif
    }
  
    /******************************************************************************************************/
    // Serial or Telnet User Input
    /******************************************************************************************************/
    #if defined(DEBUG)
    printSerialTelnet(F("DBG:UPDATE: SERIALINPUT\r\n"));
    #endif
    #if defined(PROFILE)
    startUpdate = millis();
    #endif

    // Serial input capture
    if (Serial.available()) {
      Serial.setTimeout(1000);                                           // Serial read timeout in ms
      int bytesread = Serial.readBytesUntil('\n', serialInputBuff, 63);  // Read from serial until newline is read or timeout exceeded
      serialInputBuff[bytesread] = '\0';
      serialReceived = true;
    }
    
    // telnet input is captured by telnet server

    inputHandle();                                                     // Serial input handling
    
    #if defined(PROFILE)
    deltaUpdate = millis() - startUpdate;
    if (maxUpdateINPUT < deltaUpdate) { maxUpdateINPUT = deltaUpdate; }
    if (AllmaxUpdateINPUT < deltaUpdate) { AllmaxUpdateINPUT = deltaUpdate; }
    #endif

    #if defined(PROFILE)
    yieldTimeBefore = millis(); 
    #endif
    yield(); 
    #if defined(PROFILE)
    yieldTime += (millis()-yieldTimeBefore); 
    #endif

    /******************************************************************************************************/
    // Other Time Managed Events such as runtime, saving baseline, rebooting, blinking LCD for warning
    /******************************************************************************************************/
  
    // Update runtime every minute -------------------------------------
    if ((currentTime - lastTime) >= intervalRuntime) {
      #if defined(DEBUG) 
      printSerialTelnet(F("DBG:UPDATE: RUNTIME\r\n"));
      #endif
      #if defined(PROFILE)
      startUpdate = millis();
      #endif
      mySettings.runTime = mySettings.runTime + ((currentTime - lastTime) / 1000);
      lastTime = currentTime;
      if (mySettings.debuglevel > 1) { printSerialTelnet(F("Runtime updated\r\n")); }
      #if defined(PROFILE)
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateRT < deltaUpdate) { maxUpdateRT = deltaUpdate; }
      if (AllmaxUpdateRT < deltaUpdate) { AllmaxUpdateRT = deltaUpdate; }
      #endif
    }
  
    // Safe Configuration infrequently ---------------------------------------
    if ((currentTime - lastSaveSettings) >= intervalSettings) {
      lastSaveSettings = currentTime;
      #if defined(DEBUG)
      printSerialTelnet(F("DBG:UPDATE: EEPROM\r\n"));
      #endif
      #if defined(PROFILE)
      startUpdate = millis();
      #endif
      EEPROM.put(0, mySettings);
      if (EEPROM.commit()) {
        lastSaveSettings = currentTime;
        if (mySettings.debuglevel >0) { printSerialTelnet(F("EEPROM updated")); }
      }
      #if defined(PROFILE)
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateEEPROM < deltaUpdate) { maxUpdateEEPROM = deltaUpdate; }
      if (AllmaxUpdateEEPROM < deltaUpdate) { AllmaxUpdateEEPROM = deltaUpdate; }
      #endif

      #if defined(PROFILE)
      yieldTimeBefore = millis(); 
      #endif
      yield(); 
      #if defined(PROFILE)
      yieldTime += (millis()-yieldTimeBefore); 
      #endif
    }
    
    /** JSON savinge to LittelFS crashes ESP8266
    if ((currentTime - lastSaveSettingsJSON) >= intervalSettingsJSON) {
      #if defined(DEBUG)
      printSerialTelnet(F("DBG:UPDATE: JSON config file\r\n"));
      #endif
      #if defined(PROFILE)
      startUpdate = millis();
      #endif
      saveConfiguration(mySettings);
      lastSaveSettingsJSON = currentTime;
      if (mySettings.debuglevel >0) { printSerialTelnet(F("Sensi.json updated\r\n")); }
      #if defined(PROFILE)
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateJS < deltaUpdate) { maxUpdateJS = deltaUpdate; }
      if (AllmaxUpdateJS < deltaUpdate) { AllmaxUpdateJS = deltaUpdate; }
      #endif

      #if defined(PROFILE)
      yieldTimeBefore = millis(); 
      #endif
      yield(); 
      #if defined(PROFILE)
      yieldTime += (millis()-yieldTimeBefore); 
      #endif
    }
    **/
    // Obtain basline from sensors to create internal baseline --------
    if ((currentTime - lastBaseline) >= intervalBaseline) {
      #if defined(DEBUG) 
      printSerialTelnet(F("DBG:UPDATE: BASELINE\r\n"));
      #endif
      #if defined(PROFILE)
      startUpdate = millis();
      #endif
      lastBaseline = currentTime;
      // Copy CCS811 basline to settings when warmup is finished
      if (ccs811_avail && mySettings.useCCS811) {
        if (currentTime >= warmupCCS811) {
          ccs811_port->begin(ccs811_i2c[0], ccs811_i2c[1]);
          mySettings.baselineCCS811 = ccs811.getBaseline();
          mySettings.baselineCCS811_valid = 0xF0;
          if (mySettings.debuglevel == 10) { printSerialTelnet(F("CCS811 baseline placed into settings")); }
          // warmupCCS811 = warmupCCS811 + stablebaseCCS811;
        }
      }
      // Copy SGP30 basline to settings when warmup is finished
      if (sgp30_avail && mySettings.useSGP30) {
        if (currentTime >=  warmupSGP30) {
          mySettings.baselinetVOC_SGP30 = sgp30.baselineTVOC;
          mySettings.baselineeCO2_SGP30 = sgp30.baselineCO2;
          mySettings.baselineSGP30_valid = 0xF0;
          if (mySettings.debuglevel == 6) { printSerialTelnet(F("SGP30 baseline setting updated")); }
          // warmupSGP30 = warmupSGP30 + intervalSGP30Baseline;
        }
      }
      #if defined(PROFILE)
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateBASE < deltaUpdate) { maxUpdateBASE = deltaUpdate; }
      if (AllmaxUpdateBASE < deltaUpdate) { AllmaxUpdateBASE = deltaUpdate; }
      #endif
    }

    // Update AirQuality Warning --------------------------------------------------
    if ((currentTime - lastWarning) > intervalWarning) {                         // warning interval
      #if defined(DEBUG) 
      printSerialTelnet(F("DBG:UPDATE: AQ ALL GOOD\r\n"));
      #endif
      #if defined(PROFILE)
      startUpdate = millis();
      #endif
      lastWarning = currentTime;
      allGood = sensorsWarning(); // <<<<<<<<<<<<<<<<<<
      #if defined(PROFILE)
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateALLGOOD < deltaUpdate) { maxUpdateALLGOOD = deltaUpdate; }
      if (AllmaxUpdateALLGOOD < deltaUpdate) { AllmaxUpdateALLGOOD = deltaUpdate; }
      #endif
    }
    // Warning ----------------------------------------------------------    
    if ((currentTime - lastBlink) > intervalBlink) {                             // blink interval
      #if defined(DEBUG) 
      printSerialTelnet(F("DBG:UPDATE: BLINK\r\n"));
      #endif
      #if defined(PROFILE)
      startUpdate = millis();
      #endif
      bool blinkLCD = false;
      // do we want to blink the LCD background ?
      if (mySettings.useBacklight && (allGood == false)) {
        if (timeSynced) { // is it night and we dont want to blink?
          if ( ( localTime->tm_hour*60+localTime->tm_min < mySettings.nightBegin ) && ( localTime->tm_hour*60+localTime->tm_min > mySettings.nightEnd ) ) {
            blinkLCD = true;
          }
        } else { // no network time
          blinkLCD = true;
        } 
      } else { // just make sure the back light is on when we dont blink
        if (lastBlinkInten == false) { // make sure backlight is on
          lcd_port->begin(lcd_i2c[0], lcd_i2c[1]);
          #if defined(ADALCD)
          if (mySettings.useBacklight) { lcd.setBacklight(HIGH); } else { lcd.setBacklight(LOW); }
          #else
          if (mySettings.useBacklight) { lcd.setBacklight(255);  } else { lcd.setBacklight(0); }
          #endif
          lastBlinkInten = true;
          intervalBlink = intervalBlinkOn;
        }    
      }
      
      // blink LCD background
      if (blinkLCD) {
        // if (mySettings.debuglevel > 0) { sprintf_P(tmpStr, PSTR("Sensors out of normal range!")); }
        lcd_port->begin(lcd_i2c[0], lcd_i2c[1]);
        lastBlink = currentTime;
        lastBlinkInten = !lastBlinkInten;
        #if defined(ADALCD)
        if (lastBlinkInten) {
          lcd.setBacklight(HIGH);
          intervalBlink = intervalBlinkOn;
        } else {
          lcd.setBacklight(LOW);
          intervalBlink = intervalBlinkOff;
        }
        #else
        if (lastBlinkInten) {
          lcd.setBacklight(255);
          intervalBlink = intervalBlinkOn;
        } else {
          lcd.setBacklight(0);
          intervalBlink = intervalBlinkOff;
        }
        #endif
      } // toggel the LCD background
      #if defined(PROFILE)
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateBLINK < deltaUpdate) { maxUpdateBLINK = deltaUpdate; }       
      if (AllmaxUpdateBLINK < deltaUpdate) { AllmaxUpdateBLINK = deltaUpdate; }       
      #endif
    } // blinking interval

    // Deal with request to reboot --------------------------------------
    // This occurs if sensors reading results in error and driver fails to recover
    // Reboot at preset time
    if (scheduleReboot == true) {
      #if defined(DEBUG)
      printSerialTelnet(F("DBG:UPDATE: REBOOT CHECK\r\n"));
      #endif
      #if defined(PROFILE)
      startUpdate = millis();
      #endif
      if ( (mySettings.rebootMinute>=0) && (mySettings.rebootMinute<=1440) ) { // do not reboot if rebootMinute is -1 or bigger than 24hrs
        if (timeSynced) {
          if (localTime->tm_hour*60+localTime->tm_min == mySettings.rebootMinute) {
            if (mySettings.debuglevel > 0) { printSerialTelnet(F("Rebooting...")); }
            Serial.flush() ;
            ESP.reset();
          }  
        } else {
          if (mySettings.debuglevel > 0) {  printSerialTelnet(F("Rebooting...")); }
          Serial.flush() ;
          ESP.reset();
        }
      }
      #if defined(PROFILE)
      deltaUpdate = millis() - startUpdate;
      if (maxUpdateREBOOT < deltaUpdate) { maxUpdateREBOOT = deltaUpdate; }
      if (AllmaxUpdateREBOOT < deltaUpdate) { AllmaxUpdateREBOOT = deltaUpdate; }
      #endif
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
  
    #if defined(PROFILE)
    yieldTimeBefore = millis(); 
    #endif
    yield(); 
    #if defined(PROFILE)
    yieldTime += (millis()-yieldTimeBefore); 
    if ( yieldTime > 0 ) { 
      if ( yieldTime < yieldTimeMin ) { yieldTimeMin = yieldTime; }
      if ( yieldTime > yieldTimeMax ) { 
        yieldTimeMax = yieldTime; 
        if ( yieldTime > yieldTimeMaxAllTime)  { yieldTimeMaxAllTime = yieldTime; }
      }
    }
    yieldTime = 0;
    #endif
    
    // Free up Processor
    // This crashes ESP, with that many susbsystems enabled, we run without delays
    /*
    myDelay = long(currentTime + intervalLoop) - long(millis()); // how much time is left until system loop expires
    if (myDelay < myDelayMin) { myDelayMin = myDelay; }
    if (myDelay > 0) { // there is time left, so initiate a delay, could also go to sleep here
      #if defined(DEBUG)
      printSerialTelnet(F("DBG:UPDATE: LOOP DELAY");
      #endif
      myDelayAvg = 0.9 * myDelayAvg + 0.1 * float(myDelay);
      delay(myDelay); 
    }
    */
    
  } // OTA not in progress
} // loop

//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////
// Support functions
//////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////

// Scan the I2C bus whether device is attached at address
bool checkI2C(byte address, TwoWire *myWire) {
  myWire->beginTransmission(address);
  if (myWire->endTransmission() == 0) { return true; } else { return false; }
}

String formatBytes(size_t bytes) {
  if (bytes < 1024) {
    return String(bytes) + "B";
  } else if (bytes < (1024 * 1024)) {
    return String(bytes / 1024.0) + "KB";
  } else if (bytes < (1024 * 1024 * 1024)) {
    return String(bytes / 1024.0 / 1024.0) + "MB";
  } else {
    return String(bytes / 1024.0 / 1024.0 / 1024.0) + "GB";
  }
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
    while (Serial.available()) {
      Serial.read();
    }
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
void timeJSON(char *payLoad) {
      timeval tv;
      gettimeofday (&tv, NULL);
      sprintf_P(payLoad, "{\"time\":{\"hour\":%d,\"minute\":%d,\"second\":%d,\"microsecond\":%06ld}}",
      localTime->tm_hour,
      localTime->tm_min,
      localTime->tm_sec,
      tv.tv_usec); 
}
    
void dateJSON(char *payLoad) {
      sprintf_P(payLoad, "{\"date\":{\"day\":%d,\"month\":%d,\"year\":%d}}",
      localTime->tm_mday,
      localTime->tm_mon,
      localTime->tm_year+1900); 
}

/**************************************************************************************/
// Terminal Input: Support Routines
/**************************************************************************************/

void helpMenu() {
  printSerialTelnet(FPSTR(doubleSeparator)); //================================================================//
  printSerialTelnet(F("| Sensi, 2020, 2021, Urs Utzinger                                               |\r\n"));
  printSerialTelnet(FPSTR(doubleSeparator)); //================================================================//
  printSerialTelnet(F("Supports.........................................................................\r\n"));
  printSerialTelnet(F(".........................................................................LCD 20x4\r\n"));
  printSerialTelnet(F(".........................................................SPS30 Senserion Particle\r\n"));
  printSerialTelnet(F("..............................................................SCD30 Senserion CO2\r\n"));
  printSerialTelnet(F(".......................................................SGP30 Senserion tVOC, eCO2\r\n"));
  printSerialTelnet(F("...........................BME680/280 Bosch Temperature, Humidity, Pressure, tVOC\r\n"));
  printSerialTelnet(F("....................................................CCS811 eCO2 tVOC, Air Quality\r\n"));
  printSerialTelnet(F("...........................................MLX90614 Melex Temperature Contactless\r\n"));
  printSerialTelnet(F("==All Sensors===========================|========================================\r\n"));
  printSerialTelnet(F("| z: print all sensor data              | n: this device Name, nSensi           |\r\n"));
  printSerialTelnet(F("| p: print current settings             | s: save settings (JSON & binary)      |\r\n"));
  printSerialTelnet(F("| d: create default seetings            | r: read settings (JSON)               |\r\n"));
  printSerialTelnet(F("|                                       | L: list content of filesystem         |\r\n"));
  printSerialTelnet(F("==Network===============================|==MQTT==================================\r\n"));
  printSerialTelnet(F("| P1: SSID 1, P1myssid                  | u: mqtt username, umqtt or empty      |\r\n"));
  printSerialTelnet(F("| P2: SSID 2, P2myssid                  | w: mqtt password, ww1ldc8ts or empty  |\r\n"));
  printSerialTelnet(F("| P3: SSID 3, P3myssid                  | q: toggle send mqtt immediatly, q     |\r\n"));
  printSerialTelnet(F("| P4: password SSID 1, P4mypas or empty |                                       |\r\n"));
  printSerialTelnet(F("| P5: password SSID 2, P5mypas or empty | P8: mqtt server, P8my.mqtt.com        |\r\n"));
  printSerialTelnet(F("| P6: password SSID 3, P6mypas or empty | P9: mqtt fall back server             |\r\n"));
  printSerialTelnet(F("|-Network Time--------------------------|---------------------------------------|\r\n"));
  printSerialTelnet(F("| o: set time zone oMST7                | N: ntp server, Ntime.nist.gov         |\r\n"));
  printSerialTelnet(F("| https://raw.githubusercontent.com/nayarsystems/posix_tz_db/master/zones.csv   |\r\n"));
  printSerialTelnet(F("| a: night start in min after midnight  | A: night end                          |\r\n"));
  printSerialTelnet(F("| R: reboot time in min after midnight: -1=off                                  |\r\n"));
  printSerialTelnet(F("==Sensors========================================================================\r\n"));
  printSerialTelnet(F("|-SGP30-eCO2----------------------------|-MAX-----------------------------------|\r\n"));
  printSerialTelnet(F("| e: force eCO2, e400                   |                                       |\r\n"));
  printSerialTelnet(F("| v: force tVOC, t3000                  |                                       |\r\n"));
  printSerialTelnet(F("| g: get baselines                      |                                       |\r\n"));
  printSerialTelnet(F("|-SCD30=CO2-----------------------------|--CCS811-eCO2--------------------------|\r\n"));
  printSerialTelnet(F("| f: force CO2, f400.0 in ppm           | c: force basline, c400                |\r\n"));
  printSerialTelnet(F("| t: force temperature offset, t5.0 in C| b: get baseline                       |\r\n"));
  printSerialTelnet(F("|-MLX Temp------------------------------|-LCD-----------------------------------|\r\n"));
  printSerialTelnet(F("| m: set temp offset, m1.4              | i: simplified display                 |\r\n"));
  printSerialTelnet(F("==Disable===============================|==Disable===============================\r\n"));
  printSerialTelnet(F("| x: 2 LCD on/off                       | x: 11 MLX on/off                      |\r\n"));
  printSerialTelnet(F("| x: 3 WiFi on/off                      | x: 12 BME680 on/off                   |\r\n"));
  printSerialTelnet(F("| x: 4 SCD30 on/off                     | x: 13 BME280 on/off                   |\r\n"));
  printSerialTelnet(F("| x: 5 SPS30 on/off                     | x: 14 CCS811 on/off                   |\r\n"));
  printSerialTelnet(F("| x: 6 SGP30 on/off                     | x: 15 LCD backlight on/off            |\r\n"));
  printSerialTelnet(F("| x: 7 MQTT on/off                      | x: 16 HTML server on/off              |\r\n"));
  printSerialTelnet(F("| x: 8 NTP on/off                       | x: 17 OTA on/off                      |\r\n"));
  printSerialTelnet(F("| x: 9 mDNS on/off                      | x: 18 Serial on/off                   |\r\n"));
  printSerialTelnet(F("| x: 10 MAX30 on/off                    | x: 19 Telnet on/off                   |\r\n"));
  printSerialTelnet(F("| x: 99 reset microcontroller           | x: 20 HTTP Updater on/off             |\r\n"));
  printSerialTelnet(F("|---------------------------------------|---------------------------------------|\r\n"));
  printSerialTelnet(F("| You will need to x99 to initialize the sensor                                 |\r\n"));
  printSerialTelnet(F("==Debug Level===========================|==Debug Level===========================\r\n"));
  printSerialTelnet(F("| l: 0 ALL off                          | l: 99 continous                       |\r\n"));
  printSerialTelnet(F("| l: 1 minimal                          | l: 6 SGP30 max level                  |\r\n"));
  printSerialTelnet(F("| l: 2 LCD max level                    | l: 7 MAX30 max level                  |\r\n"));
  printSerialTelnet(F("| l: 3 WiFi max level                   | l: 8 MLX max level                    |\r\n"));
  printSerialTelnet(F("| l: 4 SCD30 max level                  | l: 9 BME680/280 max level             |\r\n"));
  printSerialTelnet(F("| l: 5 SPS30 max level                  | l: 10 CCS811 max level                |\r\n"));
  printSerialTelnet(FPSTR(doubleSeparator)); //================================================================//
  printSerialTelnet(F("|  Dont forget to save settings                                                 |\r\n"));
  printSerialTelnet(FPSTR(doubleSeparator)); //================================================================//
}

/******************************************************************************************************/
// This deals with the user input from terminal console and allows configuration the system during runtime
/******************************************************************************************************/
void inputHandle() {
  uint16_t tmpuI;
  int16_t  tmpI;
  float    tmpF;
  String   value;
  String   command;
  String   instruction;
  bool     process = false;

  if (serialReceived) {
    instruction = String(serialInputBuff);
    serialReceived = false;
    process = true;
  } 

  if (telnetReceived) {
    telnetReceived = false;
    instruction = telnetInputBuff;
    process = true;
  }

  if (process) { 

    command = instruction.substring(0, 1);
    if (instruction.length() > 1) { // we have also a value
      value = instruction.substring(1, instruction.length());
      //Serial.println(value);
    }

    if (command == "a") {                                            // set start of night in minutes after midnight
      tmpuI = value.toInt();
      if ((tmpuI >= 0) && (tmpuI <= 1440) && (tmpuI >= mySettings.nightEnd)) {
        mySettings.nightBegin = (uint16_t)tmpuI;
        sprintf_P(tmpStr, PSTR("Night begin set to: %u\r\n"), mySettings.nightBegin);  printSerialTelnet(tmpStr);
      } else {
        sprintf_P(tmpStr, PSTR("Night start out of valid range %u - 1440\r\n"), mySettings.nightEnd ); printSerialTelnet(tmpStr);
     }
    }

    else if (command == "A") {                                            // set end of night in minutes after midnight
      tmpuI = value.toInt();
      if ((tmpuI >= 0) && (tmpuI <= 1440) && (tmpuI <= mySettings.nightBegin)) {
        mySettings.nightEnd = (uint16_t)tmpuI;
        sprintf_P(tmpStr, PSTR("Night end set to: %u\r\n"), mySettings.nightEnd); printSerialTelnet(tmpStr);
      } else {
        sprintf_P(tmpStr, PSTR("Night end out of valid range 0 - %u\r\n"), mySettings.nightBegin); printSerialTelnet(tmpStr);
      }
    }

    else if (command == "R") {                                            // set end of night in minutes after midnight
      tmpI = value.toInt();
      if ((tmpI >= -1) && (tmpI <= 1440)) {
        mySettings.rebootMinute = (int16_t)tmpI;
        sprintf_P(tmpStr, PSTR("Reboot minute set to: %d\r\n"), mySettings.rebootMinute); printSerialTelnet(tmpStr);
      } else {
        printSerialTelnet(("Reboot time out of valid range -1(off) - 1440\r\n"));
      }
    }

    else if (command == "l") {                                            // set debug level
      tmpuI = value.toInt();
      if ((tmpuI >= 0) && (tmpuI <= 99)) {
        mySettings.debuglevel = (unsigned int)tmpuI;
        mySettings.debuglevel = mySettings.debuglevel;
        sprintf_P(tmpStr, PSTR("Debug level set to: %u\r\n"), mySettings.debuglevel);  printSerialTelnet(tmpStr);
      } else {
        printSerialTelnet(F("Debug level out of valid Range\r\n"));
      }
    }

    else if (command == "z") {                                            // dump all sensor data
      printSensors();
      printState();
    }

    else if (command == "L") {                                            // list content of Little File System
      printSerialTelnet(F("LittleFS Contents:\r\n"));
      if (fsOK) {
        Dir mydir = LittleFS.openDir("/");
        while (mydir.next()) {                      // List the file system contents
          String fileName = mydir.fileName();
          size_t fileSize = mydir.fileSize();
          sprintf_P(tmpStr, PSTR("\tName: %s, Size: %s\r\n"), fileName.c_str(), formatBytes(fileSize).c_str()); printSerialTelnet(tmpStr);
        }
      } else {printSerialTelnet(F("LittleFS not mounted"));}
    }

    ///////////////////////////////////////////////////////////////////
    // SGP30
    ///////////////////////////////////////////////////////////////////
    else if (command == "e") {                                            // forced CO2 set setpoint
      tmpuI = value.toInt();
      if ((tmpuI >= 400) && (tmpuI <= 2000)) {
        if (sgp30_avail && mySettings.useSGP30) {
          sgp30_port->begin(sgp30_i2c[0], sgp30_i2c[1]);
          sgp30.getBaseline();
          mySettings.baselinetVOC_SGP30 = sgp30.baselineTVOC;
          sgp30.setBaseline((uint16_t)tmpuI, (uint16_t)mySettings.baselinetVOC_SGP30); // set CO2 and TVOC baseline
          mySettings.baselineSGP30_valid = 0xF0;
          mySettings.baselineeCO2_SGP30 = (uint16_t)tmpuI;
          sprintf_P(tmpStr, PSTR("Calibration point is: %u\r\n"), tmpuI); printSerialTelnet(tmpStr);
        }
      } else {
        printSerialTelnet(F("Calibration point out of valid Range\r\n"));
      }
    }

    else if (command == "v") {                                            // forced tVOC set setpoint
      tmpuI = value.toInt();
      if ((tmpuI > 400) && (tmpuI < 2000)) {
        if (sgp30_avail && mySettings.useSGP30) {
          sgp30_port->begin(sgp30_i2c[0], sgp30_i2c[1]);
          sgp30.getBaseline();
          mySettings.baselineeCO2_SGP30 = sgp30.baselineCO2;
          sgp30.setBaseline((uint16_t)mySettings.baselineeCO2_SGP30, (uint16_t)tmpuI);
          mySettings.baselineSGP30_valid = 0xF0;
          mySettings.baselinetVOC_SGP30 = (uint16_t)tmpuI;
          sprintf_P(tmpStr, PSTR("Calibration point is: %u\r\n"), tmpuI); printSerialTelnet(tmpStr);
        }
      } else { printSerialTelnet(F("Calibration point out of valid Range\r\n")); }
    }

    else if (command == "g") {                                            // forced tVOC set setpoint
      if (sgp30_avail && mySettings.useSGP30) {
        sgp30_port->begin(sgp30_i2c[0], sgp30_i2c[1]);
        sgp30.getBaseline();
        mySettings.baselineSGP30_valid = 0xF0;
        mySettings.baselinetVOC_SGP30 = sgp30.baselineTVOC;
        mySettings.baselineeCO2_SGP30 = sgp30.baselineCO2;
        printSerialTelnet(F("SGP baselines read\r\n"));
      }
    }

    ///////////////////////////////////////////////////////////////////
    // SCD30
    ///////////////////////////////////////////////////////////////////
    else if (command == "f") {                                            // forced CO2 set setpoint
      tmpuI = value.toInt();
      if ((tmpuI >= 400) && (tmpuI <= 2000)) {
        if (scd30_avail && mySettings.useSCD30) {
          scd30_port->begin(scd30_i2c[0], scd30_i2c[1]);
          scd30.setForcedRecalibrationFactor((uint16_t)tmpuI);
          sprintf_P(tmpStr, PSTR("Calibration point is: %u\r\n"), tmpuI); printSerialTelnet(tmpStr);
        }
      } else {
        printSerialTelnet(F("Calibration point out of valid Range\r\n"));
      }
    }

    else if (command == "t") {                                            // forced CO2 set setpoint
      tmpF = value.toFloat();
      if ((tmpF >= 0.0) && (tmpF <= 10.0)) {
        if (scd30_avail && mySettings.useSCD30) {
          scd30_port->begin(scd30_i2c[0], scd30_i2c[1]);
          scd30.setTemperatureOffset(tmpF);
          mySettings.tempOffset_SCD30_valid = 0xF0;
          mySettings.tempOffset_SCD30 = scd30.getTemperatureOffset();
          sprintf_P(tmpStr, PSTR("Temperature offset set to: %f\r\n"), mySettings.tempOffset_SCD30); printSerialTelnet(tmpStr);
        }
      } else {
        printSerialTelnet(F("Offset point out of valid Range\r\n"));
      }
    }

    ///////////////////////////////////////////////////////////////////
    // CCS811
    ///////////////////////////////////////////////////////////////////
    else if (command == "c") {                                            // forced baseline
      tmpuI = value.toInt();
      if ((tmpuI >= 0) && (tmpuI <= 100000)) {
        if (ccs811_avail && mySettings.useCCS811) {
          ccs811_port->begin(ccs811_i2c[0], ccs811_i2c[1]);
          ccs811.setBaseline((uint16_t)tmpuI);
          mySettings.baselineCCS811_valid = 0xF0;
          mySettings.baselineCCS811 = (uint16_t)tmpuI;
          sprintf_P(tmpStr, PSTR("Calibration point is: %d\r\n"),tmpuI); printSerialTelnet(tmpStr);
        }
      } else {
        printSerialTelnet(F("Calibration point out of valid Range\r\n"));
      }
    }

    else if (command == "b") {                                            // getbaseline
      if (ccs811_avail && mySettings.useCCS811) {
        ccs811_port->begin(ccs811_i2c[0], ccs811_i2c[1]);
        mySettings.baselineCCS811 = ccs811.getBaseline();
        sprintf_P(tmpStr, PSTR("CCS811: baseline is %d\r\n"), mySettings.baselineCCS811); printSerialTelnet(tmpStr);
      }
    }

    ///////////////////////////////////////////////////////////////////
    // MLX settings
    ///////////////////////////////////////////////////////////////////
    else if (command == "m") {                                            // forced MLX temp offset
      tmpF = value.toFloat();
      if ((tmpF >= -20.0) && (tmpF <= 20.0)) {
        mlxOffset = tmpF;
        mySettings.tempOffset_MLX_valid = 0xF0;
        mySettings.tempOffset_MLX = tmpF;
        sprintf_P(tmpStr, PSTR("Temperature offset set to: %f\r\n"),mySettings.tempOffset_MLX); printSerialTelnet(tmpStr);
      } else {
        printSerialTelnet(F("Offset point out of valid Range\r\n"));
      }
    }

    ///////////////////////////////////////////////////////////////////
    // EEPROM settings
    ///////////////////////////////////////////////////////////////////
    else if (command == "s") {                                            // save EEPROM
      tmpTime = millis();
      #if defined(DEBUG)
      printSerialTelnet(F("DBG:SAVE: EEPROM\r\n"));
      #endif  
      EEPROM.put(0, mySettings);
      if (EEPROM.commit()) { sprintf_P(tmpStr, PSTR("EEPROM saved in: %dms\r\n"), millis() - tmpTime); } 
      else {sprintf_P(tmpStr, PSTR("EEPROM failed to commit\r\n"));} 
      printSerialTelnet(tmpStr);
      
      /**
      #if defined(DEBUG)
      printSerialTelnet(F("DBG:SAVE: JSON/LittleFS\r\n"));
      #endif
      tmpTime = millis();
      saveConfiguration(mySettings);
      sprintf_P(tmpStr, PSTR("Settings saved to JSON file in: %dms\r\n"), millis() - tmpTime); printSerialTelnet(tmpStr);
      **/
    }

    else if (command == "r") {                                          // read EEPROM
      tmpTime = millis();
      // loadConfiguration(mySettings);
      printSerialTelnet(F("Loading config from JSON is disabled\r\n"));
      EEPROM.begin(EEPROM_SIZE);
      EEPROM.get(eepromAddress, mySettings);
      sprintf_P(tmpStr, PSTR("Settings read from JSON/EEPROM file in: %dms\r\n"), millis() - tmpTime); printSerialTelnet(tmpStr);
      printSettings();
    }

    else if (command == "p") {                                          // print display settings
      printSettings();
    }

    else if (command == "d") {                                          // load default settings
      defaultSettings();
      printSettings();
    }

    else if (command == "P") {                                          // SSIDs, passwords, servers      
      if (instruction.length() > 1) { // we have a value
        tmpI = instruction.substring( 1, 2).toInt(); // which SSID or PW or Server do we want to change
        if (instruction.length() > 2) { value = instruction.substring(2, instruction.length()); }
        else                          { value = ""; }
        //Serial.printf("%i: ", tmpI);
        //Serial.println(value);
      }

      switch (tmpI) {
        case 1: // SSID 1
          value.toCharArray(mySettings.ssid1, value.length() + 1);
          sprintf_P(tmpStr, PSTR("SSID 1 is: %s\r\n"), mySettings.ssid1); printSerialTelnet(tmpStr);
          break;
        case 2: // SSID 2
          value.toCharArray(mySettings.ssid2, value.length() + 1);
          sprintf_P(tmpStr, PSTR("SSID 2 is: %s\r\n"), mySettings.ssid2); printSerialTelnet(tmpStr);
          break;
        case 3: // SSID 3
          value.toCharArray(mySettings.ssid3, value.length() + 1);
          sprintf_P(tmpStr, PSTR("SSID 3 is: %s\r\n"), mySettings.ssid3); printSerialTelnet(tmpStr);
          break;
        case 4: // Passsword 1
          value.toCharArray(mySettings.pw1, value.length() + 1);
          sprintf_P(tmpStr, PSTR("Password 1 is: %s\r\n"), mySettings.pw1); printSerialTelnet(tmpStr);
          break;
        case 5: // Password 2
          value.toCharArray(mySettings.pw2, value.length() + 1);
          sprintf_P(tmpStr, PSTR("Password 2 is: %s\r\n"), mySettings.pw2); printSerialTelnet(tmpStr);
          break;
        case 6: // Password 3
          value.toCharArray(mySettings.pw3, value.length() + 1);
          sprintf_P(tmpStr, PSTR("Password 3 is: %s\r\n"), mySettings.pw3); printSerialTelnet(tmpStr);
          break;
        case 8: // MQTT Server
          value.toCharArray(mySettings.mqtt_server, value.length() + 1);
          sprintf_P(tmpStr, PSTR("MQTT server is: %s\r\n"), mySettings.mqtt_server); printSerialTelnet(tmpStr);
          break;
        case 9: // MQTT Fallback Server
          value.toCharArray(mySettings.mqtt_fallback, value.length() + 1);
          sprintf_P(tmpStr, PSTR("MQTT fallback server is: %s\r\n"), mySettings.mqtt_fallback); printSerialTelnet(tmpStr);
          break;
        default:
          break;
      }
    }

    else if (command == "u") {                                          // MQTT Server username
      value.toCharArray(mySettings.mqtt_username, value.length() + 1);
      sprintf_P(tmpStr, PSTR("MQTT username is: %s\r\n"), mySettings.mqtt_username); printSerialTelnet(tmpStr);
    }

    else if (command == "w") {                                          // MQTT Server password
      value.toCharArray(mySettings.mqtt_password, value.length() + 1);
      sprintf_P(tmpStr, PSTR("MQTT password is: %s\r\n"), mySettings.mqtt_password); printSerialTelnet(tmpStr);
    }

    else if (command == "q") {                                          // MQTT Server update as soon as new data is available
      mySettings.sendMQTTimmediate = !bool(mySettings.sendMQTTimmediate);
      sprintf_P(tmpStr, PSTR("MQTT is sent immediatly: %s\r\n"), mySettings.sendMQTTimmediate?FPSTR(mOFF):FPSTR(mON)); printSerialTelnet(tmpStr);
    }

    else if (command == "n") {                                          // MQTT main topic name (device Name)
      value.toCharArray(mySettings.mqtt_mainTopic, value.length() + 1);
      sprintf_P(tmpStr, PSTR("MQTT mainTopic is: %s\r\n"),mySettings.mqtt_mainTopic); printSerialTelnet(tmpStr);
    }

    else if (command == "N") {                                          // NTP server, network time
      value.toCharArray(mySettings.ntpServer, value.length() + 1);
      sprintf_P(tmpStr, PSTR("NTP server is: %s\r\n"), mySettings.ntpServer); printSerialTelnet(tmpStr);
      stateNTP = START_UP;
    }

    else if (command == "o") {                                          // Time Zone Offset, use to table listed in help page
      value.toCharArray(mySettings.timeZone, value.length() + 1);
      NTP.setTimeZone(mySettings.timeZone);
      sprintf_P(tmpStr, PSTR("Time Zone is: %s\r\n"), mySettings.timeZone); printSerialTelnet(tmpStr);
    }

    else if (command == "x") {                                          // enable/disable equipment
      tmpuI = value.toInt();
      if ((tmpuI > 0) && (tmpuI <= 99)) {
        switch (tmpuI) {
          case 2:
            mySettings.useLCD = !bool(mySettings.useLCD);
            if (mySettings.useLCD)   {     printSerialTelnet(F("LCD is used\r\n"));    } else { printSerialTelnet(F("LCD is not used\r\n")); }
            break;
          case 3:
            mySettings.useWiFi = !bool(mySettings.useWiFi);
            if (mySettings.useWiFi)  {     printSerialTelnet(F("WiFi is used\r\n"));   } else { printSerialTelnet(F("WiFi is not used\r\n")); }
            break;
          case 4:
            mySettings.useSCD30 = !bool(mySettings.useSCD30);
            if (mySettings.useSCD30) {     printSerialTelnet(F("SCD30 is used\r\n"));  } else { printSerialTelnet(F("SCD30 is not used\r\n")); }
            break;
          case 5:
            mySettings.useSPS30 = !bool(mySettings.useSPS30);
            if (mySettings.useSPS30) {     printSerialTelnet(F("SPS30 is used\r\n"));  } else { printSerialTelnet(F("SPS30 is not used\r\n")); }
            break;
          case 6:
            mySettings.useSGP30 = !bool(mySettings.useSGP30);
            if (mySettings.useSGP30) {     printSerialTelnet(F("SGP30 is used\r\n"));  } else { printSerialTelnet(F("SGP30 is not used\r\n")); }
            break;
          case 7:
            mySettings.useMQTT = !bool(mySettings.useMQTT); 
            if (mySettings.useMQTT==true) {      printSerialTelnet(F("MQTT is used\r\n"));   } else { printSerialTelnet(F("MQTT is not used\r\n")); }
            break;
          case 8:
            mySettings.useNTP = !bool(mySettings.useNTP);
            if (mySettings.useNTP==true) {       printSerialTelnet(F("NTP is used\r\n"));    } else { printSerialTelnet(F("NTP is not used\r\n")); }
            break;
          case 9:
            mySettings.usemDNS = !bool(mySettings.usemDNS);
            if (mySettings.usemDNS==true) {     printSerialTelnet(F("mDNS is used\r\n"));    } else { printSerialTelnet(F("mDNS is not used\r\n")); }
            break;
          case 10:
            mySettings.useMAX30 = !bool(mySettings.useMAX30);
            if (mySettings.useMAX30==true) {    printSerialTelnet(F("MAX30 is used\r\n"));   } else { printSerialTelnet(F("MAX30 is not used\r\n")); }
            break;
          case 11:
            mySettings.useMLX = !bool(mySettings.useMLX);
            if (mySettings.useMLX==true)   {    printSerialTelnet(F("MLX is used\r\n"));     } else { printSerialTelnet(F("MLX is not used\r\n")); }
            break;
          case 12:
            mySettings.useBME680 = !bool(mySettings.useBME680);
            if (mySettings.useBME680==true) {   printSerialTelnet(F("BME680 is used\r\n"));  } else { printSerialTelnet(F("BME680 is not used\r\n")); }
            break;
          case 13:
            mySettings.useBME280 = !bool(mySettings.useBME280);
            if (mySettings.useBME280==true) {   printSerialTelnet(F("BME280 is used\r\n"));  } else { printSerialTelnet(F("BME280 is not used\r\n")); }
            break;
          case 14:
            mySettings.useCCS811 = !bool(mySettings.useCCS811);
            if (mySettings.useCCS811==true) {   printSerialTelnet(F("CCS811 is used\r\n"));  } else { printSerialTelnet(F("CCS811 is not used\r\n")); }
            break;
          case 15:
            mySettings.useBacklight = !bool(mySettings.useBacklight);
            if (mySettings.useBacklight==true) { printSerialTelnet(F("Backlight is on\r\n"));} else { printSerialTelnet(F("Backlight is off\r\n")); }
            break;
          case 16:
            mySettings.useHTTP = !bool(mySettings.useHTTP);
            if (mySettings.useHTTP==true) { printSerialTelnet(F("HTTP webserver is on\r\n"));} else { printSerialTelnet(F("HTTP webserver is off\r\n")); }
            break;
          case 17:
            mySettings.useOTA = !bool(mySettings.useOTA);
            if (mySettings.useOTA==true) {     printSerialTelnet(F("OTA server is on\r\n")); } else { printSerialTelnet(F("OTA server is off\r\n")); }
            break;
          case 18:
            mySettings.useSerial = !bool(mySettings.useSerial);
            if (mySettings.useSerial==true) {  printSerialTelnet(F("Serial is on\r\n")); }     else { printSerialTelnet(F("Serial is off\r\n")); }
            break;
          case 19:
            mySettings.useTelnet = !bool(mySettings.useTelnet);
            if (mySettings.useTelnet==true) {  printSerialTelnet(F("Telnet is on\r\n")); }     else { printSerialTelnet(F("Telnet is off\r\n")); }
            break;
          case 20:
            mySettings.useHTTPUpdater = !bool(mySettings.useHTTPUpdater);
            if (mySettings.useHTTPUpdater==true) {  printSerialTelnet(F("HTTPUpdater is on\r\n")); } else { printSerialTelnet(F("HTTPUpdater is off\r\n")); }
            break;
          case 99:
            printSerialTelnet(F("ESP is going to restart. Bye\r\n"));
            Serial.flush();
            ESP.restart();
            break;
          default:
            break;
        }
      }
    }

    else if (command == "j") {                                          // test json output
      char payLoad[256];
      timeJSON(payLoad);   printSerialTelnet(payLoad); sprintf_P(tmpStr, PSTR(" len: %u\r\n"), strlen(payLoad)); printSerialTelnet(tmpStr);
      dateJSON(payLoad);   printSerialTelnet(payLoad); sprintf_P(tmpStr, PSTR(" len: %u\r\n"), strlen(payLoad)); printSerialTelnet(tmpStr);
      bme280JSON(payLoad); printSerialTelnet(payLoad); sprintf_P(tmpStr, PSTR(" len: %u\r\n"), strlen(payLoad)); printSerialTelnet(tmpStr);
      bme680JSON(payLoad); printSerialTelnet(payLoad); sprintf_P(tmpStr, PSTR(" len: %u\r\n"), strlen(payLoad)); printSerialTelnet(tmpStr);
      ccs811JSON(payLoad); printSerialTelnet(payLoad); sprintf_P(tmpStr, PSTR(" len: %u\r\n"), strlen(payLoad)); printSerialTelnet(tmpStr);
      mlxJSON(payLoad);    printSerialTelnet(payLoad); sprintf_P(tmpStr, PSTR(" len: %u\r\n"), strlen(payLoad)); printSerialTelnet(tmpStr);
      scd30JSON(payLoad);  printSerialTelnet(payLoad); sprintf_P(tmpStr, PSTR(" len: %u\r\n"), strlen(payLoad)); printSerialTelnet(tmpStr);
      sgp30JSON(payLoad);  printSerialTelnet(payLoad); sprintf_P(tmpStr, PSTR(" len: %u\r\n"), strlen(payLoad)); printSerialTelnet(tmpStr);
      sps30JSON(payLoad);  printSerialTelnet(payLoad); sprintf_P(tmpStr, PSTR(" len: %u\r\n"), strlen(payLoad)); printSerialTelnet(tmpStr);
      #if defined(DEBUG)
      printSerialTelnet(F("DBG:DISPLAY: JSON"));
      #endif
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

    else if (command == "i") {                                       // simpler display on LCD
      mySettings.consumerLCD = !bool(mySettings.consumerLCD);
      if (mySettings.consumerLCD) {
        printSerialTelnet(F("Simpler LCD display"));
      } else {
        printSerialTelnet(F("Engineering LCD display"));
      }
    }

    else {                                                          // unrecognized command
      helpMenu();
    } // end if

  } // end serial available
} // end Input

void printSettings() {
  printSerialTelnet(FPSTR(doubleSeparator)); //================================================================//
  printSerialTelnet(F("-System-----------------------------\r\n"));
  sprintf_P(tmpStr, PSTR("Debug level: .................. %u\r\n"),   mySettings.debuglevel ); printSerialTelnet(tmpStr);
  sprintf_P(tmpStr, PSTR("Reboot Minute: ................ %i\r\n"),   mySettings.rebootMinute); printSerialTelnet(tmpStr);
  sprintf_P(tmpStr, PSTR("Runtime [min]: ................ %lu\r\n"),  mySettings.runTime / 60); printSerialTelnet(tmpStr);
  sprintf_P(tmpStr, PSTR("Simpler Display: .............. %s\r\n"),  (mySettings.consumerLCD) ? FPSTR(mON) : FPSTR(mOFF));  printSerialTelnet(tmpStr);
  sprintf_P(tmpStr, PSTR("Serial terminal: .............. %s\r\n"),  (mySettings.useSerial) ? FPSTR(mON) : FPSTR(mOFF));  printSerialTelnet(tmpStr);
  sprintf_P(tmpStr, PSTR("Telnet terminal: .............. %s\r\n"),  (mySettings.useTelnet) ? FPSTR(mON) : FPSTR(mOFF));  printSerialTelnet(tmpStr);
  printSerialTelnet(FPSTR(doubleSeparator)); //================================================================//
  printSerialTelnet(F("-Network----------------------------\r\n"));
  sprintf_P(tmpStr, PSTR("HTTP: ......................... %s\r\n"),  (mySettings.useHTTP) ? FPSTR(mON) : FPSTR(mOFF)); printSerialTelnet(tmpStr);
  sprintf_P(tmpStr, PSTR("OTA: .......................... %s\r\n"),  (mySettings.useOTA)  ? FPSTR(mON) : FPSTR(mOFF)); printSerialTelnet(tmpStr);
  sprintf_P(tmpStr, PSTR("mDNS: ......................... %s\r\n"),  (mySettings.usemDNS) ? FPSTR(mON) : FPSTR(mOFF)); printSerialTelnet(tmpStr);
  sprintf_P(tmpStr, PSTR("HTTPUpdater: .................. %s\r\n"),  (mySettings.useHTTPUpdater) ? FPSTR(mON) : FPSTR(mOFF)); printSerialTelnet(tmpStr);
  printSerialTelnet(F("-Credentials------------------------\r\n"));
  sprintf_P(tmpStr, PSTR("WiFi: ......................... %s\r\n"),  (mySettings.useWiFi) ? FPSTR(mON) : FPSTR(mOFF)); printSerialTelnet(tmpStr);
  sprintf_P(tmpStr, PSTR("SSID: ......................... %s\r\n"),   mySettings.ssid1);  printSerialTelnet(tmpStr);
  sprintf_P(tmpStr, PSTR("PW: ........................... %s\r\n"),   mySettings.pw1);  printSerialTelnet(tmpStr);
  sprintf_P(tmpStr, PSTR("SSID: ......................... %s\r\n"),   mySettings.ssid2);  printSerialTelnet(tmpStr);
  sprintf_P(tmpStr, PSTR("PW: ........................... %s\r\n"),   mySettings.pw2);  printSerialTelnet(tmpStr);
  sprintf_P(tmpStr, PSTR("SSID: ......................... %s\r\n"),   mySettings.ssid3);  printSerialTelnet(tmpStr);
  sprintf_P(tmpStr, PSTR("PW: ........................... %s\r\n"),   mySettings.pw3);  printSerialTelnet(tmpStr);
  printSerialTelnet(F("-Time-------------------------------\r\n"));
  sprintf_P(tmpStr, PSTR("NTP: .......................... %s\r\n"),  (mySettings.useNTP) ? FPSTR(mON) : FPSTR(mOFF)); printSerialTelnet(tmpStr);
  sprintf_P(tmpStr, PSTR("NTP Server: ................... %s\r\n"),   mySettings.ntpServer);  printSerialTelnet(tmpStr);
  sprintf_P(tmpStr, PSTR("Time Zone: .................... %s\r\n"),   mySettings.timeZone);  printSerialTelnet(tmpStr);
  printSerialTelnet(F("-MQTT-------------------------------\r\n"));
  sprintf_P(tmpStr, PSTR("MQTT: ......................... %s\r\n"),  (mySettings.useMQTT) ? FPSTR(mON) : FPSTR(mOFF)); printSerialTelnet(tmpStr);
  sprintf_P(tmpStr, PSTR("MQTT: ......................... %s\r\n"),   mySettings.mqtt_server);  printSerialTelnet(tmpStr);
  sprintf_P(tmpStr, PSTR("MQTT fallback: ................ %s\r\n"),   mySettings.mqtt_fallback);  printSerialTelnet(tmpStr);
  sprintf_P(tmpStr, PSTR("MQTT user: .................... %s\r\n"),   mySettings.mqtt_username);  printSerialTelnet(tmpStr);
  sprintf_P(tmpStr, PSTR("MQTT password: ................ %s\r\n"),   mySettings.mqtt_password);  printSerialTelnet(tmpStr);
  sprintf_P(tmpStr, PSTR("MQTT send immediatly: ......... %s\r\n"),  (mySettings.sendMQTTimmediate) ? FPSTR(mON) : FPSTR(mOFF)); printSerialTelnet(tmpStr);
  sprintf_P(tmpStr, PSTR("MQTT main topic: .............. %s\r\n"),   mySettings.mqtt_mainTopic);  printSerialTelnet(tmpStr);
  printSerialTelnet(FPSTR(doubleSeparator)); //================================================================//
  printSerialTelnet(F("-Sensors----------------------------\r\n"));
  sprintf_P(tmpStr, PSTR("SCD30: ........................ %s\r\n"),  (mySettings.useSCD30)  ? FPSTR(mON) : FPSTR(mOFF)); printSerialTelnet(tmpStr);
  sprintf_P(tmpStr, PSTR("SPS30: ........................ %s\r\n"),  (mySettings.useSPS30)  ? FPSTR(mON) : FPSTR(mOFF)); printSerialTelnet(tmpStr);
  sprintf_P(tmpStr, PSTR("SGP30: ........................ %s\r\n"),  (mySettings.useSGP30)  ? FPSTR(mON) : FPSTR(mOFF)); printSerialTelnet(tmpStr);
  sprintf_P(tmpStr, PSTR("MAX30: ........................ %s\r\n"),  (mySettings.useMAX30)  ? FPSTR(mON) : FPSTR(mOFF)); printSerialTelnet(tmpStr);
  sprintf_P(tmpStr, PSTR("MLX: .......................... %s\r\n"),  (mySettings.useMLX)    ? FPSTR(mON) : FPSTR(mOFF)); printSerialTelnet(tmpStr);
  sprintf_P(tmpStr, PSTR("BME680: ....................... %s\r\n"),  (mySettings.useBME680) ? FPSTR(mON) : FPSTR(mOFF)); printSerialTelnet(tmpStr);
  sprintf_P(tmpStr, PSTR("BME280: ....................... %s\r\n"),  (mySettings.useBME280) ? FPSTR(mON) : FPSTR(mOFF)); printSerialTelnet(tmpStr);
  sprintf_P(tmpStr, PSTR("CCS811: ....................... %s\r\n"),  (mySettings.useCCS811) ? FPSTR(mON) : FPSTR(mOFF)); printSerialTelnet(tmpStr);
  printSerialTelnet(F("-Offsets&Baselines------------------\r\n"));
  sprintf_P(tmpStr, PSTR("Base SGP30 valid: ............. %X\r\n"),   mySettings.baselineSGP30_valid); printSerialTelnet(tmpStr);
  sprintf_P(tmpStr, PSTR("Base eCO2 SGP30: .............. %u\r\n"),   mySettings.baselineeCO2_SGP30); printSerialTelnet(tmpStr);
  sprintf_P(tmpStr, PSTR("Base tVOC SGP30: .............. %u\r\n"),   mySettings.baselinetVOC_SGP30); printSerialTelnet(tmpStr);
  sprintf_P(tmpStr, PSTR("Base CCS811 valid: ............ %X\r\n"),   mySettings.baselineCCS811_valid); printSerialTelnet(tmpStr);
  sprintf_P(tmpStr, PSTR("Base CCS811: .................. %u\r\n"),   mySettings.baselineCCS811); printSerialTelnet(tmpStr);
  sprintf_P(tmpStr, PSTR("Temp Offset SCD30 valid: ...... %X\r\n"),   mySettings.tempOffset_SCD30_valid); printSerialTelnet(tmpStr);
  sprintf_P(tmpStr, PSTR("Temp Offset SCD30: [C]......... %f\r\n"),   mySettings.tempOffset_SCD30); printSerialTelnet(tmpStr);
  sprintf_P(tmpStr, PSTR("MLX Temp Offset valid: ........ %X\r\n"),   mySettings.tempOffset_MLX_valid); printSerialTelnet(tmpStr);
  sprintf_P(tmpStr, PSTR("MLX Temp Offset: [C]........... %f\r\n"),   mySettings.tempOffset_MLX); printSerialTelnet(tmpStr);
  sprintf_P(tmpStr, PSTR("Average Pressure: [mbar]....... %f\r\n"),   mySettings.avgP/100.); printSerialTelnet(tmpStr);
  printSerialTelnet(FPSTR(doubleSeparator)); //================================================================//
  printSerialTelnet(F("-LCD--------------------------------\r\n"));
  sprintf_P(tmpStr, PSTR("LCD: .......................... %s\r\n"),  (mySettings.useLCD) ? FPSTR(mON) : FPSTR(mOFF)); printSerialTelnet(tmpStr);
  sprintf_P(tmpStr, PSTR("LCD Backlight: ................ %s\r\n"),  (mySettings.useBacklight) ? FPSTR(mON) : FPSTR(mOFF)); printSerialTelnet(tmpStr);
  sprintf_P(tmpStr, PSTR("Night Start: [min][hrs]........ %u, %u\r\n"),   mySettings.nightBegin, mySettings.nightBegin/60); printSerialTelnet(tmpStr);
  sprintf_P(tmpStr, PSTR("Night End: [min][hrz].......... %u, %u\r\n"),   mySettings.nightEnd, mySettings.nightEnd/60); printSerialTelnet(tmpStr);
  printSerialTelnet(FPSTR(doubleSeparator)); //================================================================//
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
  strcpy_P(mySettings.mqtt_mainTopic,        PSTR("Sensi"));
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
  strcpy_P(mySettings.ntpServer,           PSTR("time.nist.gov")); 
  strcpy_P(mySettings.timeZone,            PSTR("MST7")); // https://raw.githubusercontent.com/nayarsystems/posix_tz_db/master/zones.csv
}

void printSensors() {

  printSerialTelnet(FPSTR(singleSeparator)); //----------------------------------------------------------------//
  
  if (lcd_avail && mySettings.useLCD) {
    sprintf_P(tmpStr,PSTR("LCD interval: %d Port: %d SDA %d SCL %d\r\n"), intervalLCD, uint32_t(lcd_port), lcd_i2c[0], lcd_i2c[1]); printSerialTelnet(tmpStr);
    printSerialTelnet(FPSTR(singleSeparator)); //----------------------------------------------------------------//
  } else {
    printSerialTelnet(F("LCD: not available\r\n"));
    printSerialTelnet(FPSTR(singleSeparator)); //----------------------------------------------------------------//
  }

  if (scd30_avail && mySettings.useSCD30) {
    char qualityMessage[16];
    scd30_port->begin(scd30_i2c[0], scd30_i2c[1]);
    sprintf_P(tmpStr, PSTR("SCD30 CO2:%hu[ppm], rH:%4.1f[%%], T:%4.1f[C] T_offset:%4.1f[C}\r\n"), scd30_ppm, scd30_hum, scd30_temp, scd30.getTemperatureOffset() ); printSerialTelnet(tmpStr);
    checkCO2(float(scd30_ppm),qualityMessage,15);
    sprintf_P(tmpStr, PSTR("SCD30 CO2 is: %s\r\n"), qualityMessage);  printSerialTelnet(tmpStr);
    sprintf_P(tmpStr, PSTR("SCD30 interval: %d Port: %d SDA %d SCL %d \r\n"), intervalSCD30, uint32_t(scd30_port), scd30_i2c[0], scd30_i2c[1] );  printSerialTelnet(tmpStr);
  } else {
    printSerialTelnet(F("SCD30: not available\r\n"));
  }
  printSerialTelnet(FPSTR(singleSeparator)); //----------------------------------------------------------------//

  if (bme680_avail && mySettings.useBME680) {
    char qualityMessage[16];
    sprintf_P(tmpStr,PSTR("BME680 T:%+5.1f[C] P:%5.1f[mbar] P_ave:%5.1f[mbar] rH:%4.1f[%%] aH:%4.1f[g/m^3] Gas resistance:%d[Ohm]\r\n"), bme680.temperature, bme680.pressure/100., bme680_pressure24hrs/100., bme680.humidity, bme680_ah, bme680.gas_resistance);  printSerialTelnet(tmpStr);
    checkAmbientTemperature(bme680.temperature,qualityMessage, 15);
    sprintf_P(tmpStr,PSTR("Temperature is %s, "),qualityMessage); printSerialTelnet(tmpStr);
    checkdP((bme680.pressure - bme680_pressure24hrs)/100.0,qualityMessage,15),
    sprintf_P(tmpStr, PSTR("Change in pressure is %s, "),qualityMessage);  printSerialTelnet(tmpStr);
    checkHumidity(bme680.humidity,qualityMessage,15);
    sprintf_P(tmpStr,PSTR("Humidity is %s\r\n"),qualityMessage); printSerialTelnet(tmpStr);
    checkGasResistance(bme680.gas_resistance,qualityMessage,15);
    sprintf_P(tmpStr,PSTR("Gas resistance is %s\r\n"),qualityMessage); printSerialTelnet(tmpStr);
    sprintf_P(tmpStr, PSTR("BME680 interval: %d Port: %d SDA %d SCL %d \r\n"), intervalBME680, uint32_t(bme680_port), bme680_i2c[0], bme680_i2c[1]);  printSerialTelnet(tmpStr);
  } else {
    printSerialTelnet(F("BME680: not available\r\n"));
  }
  printSerialTelnet(FPSTR(singleSeparator)); //----------------------------------------------------------------//

  if (bme280_avail && mySettings.useBME280) {
    char qualityMessage[16];
    sprintf_P(tmpStr,PSTR("BME280 T:%+5.1f[C] P:%5.1f[mbar] P_ave:%5.1f[mbar]"), bme280_temp, bme280_pressure/100., bme280_pressure24hrs/100.);  printSerialTelnet(tmpStr);
    if (BMEhum_avail) { sprintf_P(tmpStr, PSTR(" rH:%4.1f[%%] aH:%4.1f[g/m^3]\r\n"), bme280_hum, bme280_ah); printSerialTelnet(tmpStr); }
    checkAmbientTemperature(bme280_temp,qualityMessage,15);
    sprintf_P(tmpStr, PSTR("Temperature is %s, "),qualityMessage); printSerialTelnet(tmpStr);
    checkdP((bme280_pressure - bme280_pressure24hrs)/100.0,qualityMessage,15);
    sprintf_P(tmpStr, PSTR("Change in pressure is %s"),qualityMessage); printSerialTelnet(tmpStr);
    if (BMEhum_avail) {
      checkHumidity(bme280_hum,qualityMessage,15);
      sprintf_P(tmpStr,PSTR(" Humidity is %s\r\n"),qualityMessage); printSerialTelnet(tmpStr);
    }
    sprintf_P(tmpStr, PSTR("BME280 interval: %d Port: %d SDA %d SCL %d \r\n"), intervalBME280, uint32_t(bme280_port), bme280_i2c[0], bme280_i2c[1] ); printSerialTelnet(tmpStr);
  } else {
    printSerialTelnet(F("BME280: not available\r\n"));
  }
  printSerialTelnet(FPSTR(singleSeparator)); //----------------------------------------------------------------//

  if (sgp30_avail && mySettings.useSGP30) {
    char qualityMessage[16];
    sprintf_P(tmpStr,PSTR("SGP30 CO2:%hu[ppm] tVOC:%hu[ppb] baseline_CO2:%hu baseline_tVOC:%hu H2:%hu[ppb] Ethanol:%hu[ppm]\r\n"), sgp30.CO2, sgp30.TVOC, sgp30.baselineCO2, sgp30.baselineTVOC,sgp30.H2, sgp30.ethanol);  printSerialTelnet(tmpStr);
    checkCO2(float(sgp30.CO2),qualityMessage,15);
    sprintf_P(tmpStr,PSTR("CO2 conentration is %s, "),qualityMessage); printSerialTelnet(tmpStr);
    checkTVOC(sgp30.TVOC,qualityMessage,15);  
    sprintf_P(tmpStr, PSTR("tVOC conentration is %s\r\n"),qualityMessage);  printSerialTelnet(tmpStr);
    sprintf_P(tmpStr, PSTR("SGP30 interval: %d Port: %d SDA %d SCL %d \r\n"), intervalSGP30, uint32_t(sgp30_port), sgp30_i2c[0], sgp30_i2c[1]);  printSerialTelnet(tmpStr);
  } else {
    printSerialTelnet(F("SGP30: not available\r\n"));
  }
  printSerialTelnet(FPSTR(singleSeparator)); //----------------------------------------------------------------//

  if (ccs811_avail && mySettings.useCCS811) {
    char qualityMessage[16]; 
    uint16_t CO2uI = ccs811.getCO2();
    uint16_t TVOCuI = ccs811.getTVOC();
    ccs811_port->begin(ccs811_i2c[0], ccs811_i2c[1]);
    uint16_t BaselineuI = ccs811.getBaseline();
    sprintf_P(tmpStr,PSTR("CCS811 CO2:%hu tVOC:%hu CCS811_baseline:%hu\r\n"), CO2uI, TVOCuI, BaselineuI);  printSerialTelnet(tmpStr);
    checkCO2(float(CO2uI),qualityMessage,15);                                          
    sprintf_P(tmpStr,PSTR("CO2 concentration is %s, "), qualityMessage); printSerialTelnet(tmpStr);
    checkTVOC(TVOCuI,qualityMessage,15);
    sprintf_P(tmpStr,PSTR("tVOC concentration is %s\r\n"), qualityMessage); printSerialTelnet(tmpStr);
    sprintf_P(tmpStr,PSTR("CCS811 mode: %d Port: %d SDA %d SCL %d \r\n"), ccs811Mode, uint32_t(sgp30_port), sgp30_i2c[0], sgp30_i2c[1]); printSerialTelnet(tmpStr);
    printSerialTelnet(F("[4=0.25s, 3=60s, 2=10sec, 1=1sec]\r\n"));
  } else {
    printSerialTelnet(F("CCS811: not available\r\n"));
  }
  printSerialTelnet(FPSTR(singleSeparator)); //----------------------------------------------------------------//

  if (sps30_avail && mySettings.useSPS30) {
    char qualityMessage[16];
    sprintf_P(tmpStr,PSTR("SPS30 P1.0:%4.1f[μg/m3] P2.5:%4.1f[μg/m3] P4.0:%4.1f[μg/m3] P10:%4.1f[μg/m3]\r\n"), valSPS30.MassPM1, valSPS30.MassPM2, valSPS30.MassPM4, valSPS30.MassPM10); printSerialTelnet(tmpStr);
    sprintf_P(tmpStr,PSTR("SPS30 P1.0:%4.1f[#/cm3] P2.5:%4.1f[#/cm3] P4.0:%4.1f[#/cm3] P10:%4.1f[#/cm3]\r\n"), valSPS30.NumPM0, valSPS30.NumPM2, valSPS30.NumPM4, valSPS30.NumPM10); printSerialTelnet(tmpStr);
    sprintf_P(tmpStr,PSTR("SPS30 Average %4.1f[μm]\r\n"), valSPS30.PartSize);  printSerialTelnet(tmpStr);
    checkPM2(valSPS30.MassPM2,qualityMessage,15);   
    sprintf_P(tmpStr,PSTR("PM2.5 is: %s, "), qualityMessage);  printSerialTelnet(tmpStr);
    checkPM10(valSPS30.MassPM10,qualityMessage,15);
    sprintf_P(tmpStr,PSTR("PM10 is %s\r\n"),  qualityMessage); printSerialTelnet(tmpStr);
    sprintf_P(tmpStr,PSTR("Autoclean interval: %lu\r\n"),autoCleanIntervalSPS30); printSerialTelnet(tmpStr);
    sprintf_P(tmpStr,PSTR("SPS30 interval: %d Port: %d SDA %d SCL %d \r\n"), intervalSPS30, uint32_t(sps30_port), sps30_i2c[0], sps30_i2c[1]); printSerialTelnet(tmpStr);
  } else {
    printSerialTelnet(F("SPS30: not available\r\n"));
  }
  printSerialTelnet(FPSTR(singleSeparator)); //----------------------------------------------------------------//

  if (therm_avail && mySettings.useMLX) {
    char qualityMessage[16];
    mlx_port->begin(mlx_i2c[0], mlx_i2c[1]);
    float tmpOF = therm.object();
    float tmpAF = therm.ambient();
    sprintf_P(tmpStr,PSTR("MLX Object:%+5.1f[C] MLX Ambient:%+5.1f[C]\r\n"), tmpOF, tmpAF); printSerialTelnet(tmpStr);
    float tmpEF = therm.readEmissivity();
    sprintf_P(tmpStr,PSTR("MLX Emissivity:%4.1f MLX Offset:%4.1f\r\n"), tmpEF, mlxOffset); printSerialTelnet(tmpStr);
    checkFever(tmpOF,qualityMessage, 15);
    sprintf_P(tmpStr,PSTR("Object temperature is %s, "), qualityMessage);  printSerialTelnet(tmpStr);
    checkAmbientTemperature(tmpOF,qualityMessage,15);
    sprintf_P(tmpStr,PSTR("Ambient temperature is %s\r\n"), qualityMessage); printSerialTelnet(tmpStr);
    sprintf_P(tmpStr,PSTR("MLX interval: %d Port: %d SDA %d SCL %d \r\n"), intervalMLX, uint32_t(mlx_port), mlx_i2c[0], mlx_i2c[1]); printSerialTelnet(tmpStr);
  } else {
    printSerialTelnet(F("MLX: not available\r\n"));
  }
  printSerialTelnet(FPSTR(singleSeparator)); //----------------------------------------------------------------//

  if (max_avail && mySettings.useMAX30) {
    char qualityMessage[16];
    // max_port->begin(max_i2c[0], max_i2c[1]);
    sprintf_P(tmpStr,PSTR("MAX interval: %d Port: %d SDA %d SCL %d \r\n"), intervalMAX, uint32_t(max_port), max_i2c[0], max_i2c[1]); printSerialTelnet(tmpStr);
  } else {
    printSerialTelnet(F("MAX: not available\r\n"));
  }
  printSerialTelnet(FPSTR(singleSeparator)); //----------------------------------------------------------------//

  sprintf_P(tmpStr,PSTR("NTP: %s, time is %s, daylight saving time is %s\r\n"), 
          ntp_avail ? "available" : "not available", 
          timeSynced ? "synchronized" : "not synchronized", 
          (localTime->tm_isdst>0) ? "observed" : "not observed"); 
  printSerialTelnet(tmpStr);
  
  if (localTime->tm_wday>=0 && localTime->tm_wday<7 && localTime->tm_mon>=0 && localTime->tm_mon<12) {
  sprintf_P(tmpStr,PSTR("%s %s %d %u, "), 
          weekDays[localTime->tm_wday], 
          months[localTime->tm_mon], 
          localTime->tm_mday, 
          (localTime->tm_year+1900));  
  printSerialTelnet(tmpStr); }
  
  sprintf_P(tmpStr,PSTR("%d%d:%d%d:%d%d\r\n"), 
         localTime->tm_hour/10 ? localTime->tm_hour/10 : 0 , localTime->tm_hour%10,
         localTime->tm_min/10,    localTime->tm_min%10,
         localTime->tm_sec/10,    localTime->tm_sec%10);
  printSerialTelnet(tmpStr);
  printSerialTelnet(FPSTR(singleSeparator)); //----------------------------------------------------------------//
}

void printState() {
  if (wifi_avail && mySettings.useWiFi) {
    // sprintf_P(tmpStr, PSTR("WiFi status: %s ", WiFi.status() ? FPSTR(mON) : FPSTR(mOFF));  printSerialTelnet(tmpStr);
    sprintf_P(tmpStr, PSTR("WiFi host: %s "), hostName); printSerialTelnet(tmpStr);
    if (wifi_connected) {
      sprintf_P(tmpStr, PSTR("is connected to: %s "), WiFi.SSID().c_str()); printSerialTelnet(tmpStr);
      sprintf_P(tmpStr, PSTR("with IP: %s\r\n"), WiFi.localIP().toString().c_str()); printSerialTelnet(tmpStr);
    } else {
      printSerialTelnet(F("WiFi not connected"));
    }
    
    printSerialTelnet(FPSTR(singleSeparator)); //---------------------------------------------------------------------------------------------------//
    printSerialTelnet(F("WiFi:         "));
    switch (stateWiFi){
      case START_UP:         printSerialTelnet(FPSTR(mStarting));    break;
      case CHECK_CONNECTION: printSerialTelnet(FPSTR(mMonitoring));  break;
      case IS_SCANNING:      printSerialTelnet(FPSTR(mScanning));    break;
      case IS_CONNECTING:    printSerialTelnet(FPSTR(mConnecting));  break;
      case IS_WAITING:       printSerialTelnet(FPSTR(mWaiting));     break;
      default:               break;
    }
    
    printSerialTelnet(F("mDNS:         "));
    if (mySettings.usemDNS){
      switch (stateMDNS){
        case START_UP:         printSerialTelnet(FPSTR(mStarting));     break;
        case CHECK_CONNECTION: printSerialTelnet(FPSTR(mMonitoring));   break;
        case IS_SCANNING:      printSerialTelnet(FPSTR(mScanning));     break;
        case IS_CONNECTING:    printSerialTelnet(FPSTR(mConnecting));   break;
        case IS_WAITING:       printSerialTelnet(FPSTR(mWaiting));      break;
        default:               break;
      }
    } else {                   printSerialTelnet(FPSTR(mNotEnabled)); }
    printSerialTelnet(F("\r\n"));
  
    printSerialTelnet(F("MQTT:         "));
    if (mySettings.useMQTT){
      switch (stateMQTT){
        case START_UP:         printSerialTelnet(FPSTR(mStarting));     break;
        case CHECK_CONNECTION: printSerialTelnet(FPSTR(mMonitoring));   break;
        case IS_SCANNING:      printSerialTelnet(FPSTR(mScanning));     break;
        case IS_CONNECTING:    printSerialTelnet(FPSTR(mConnecting));   break;
        case IS_WAITING:       printSerialTelnet(FPSTR(mWaiting));      break;
        default:               break;
      }
    } else {                   printSerialTelnet(FPSTR(mNotEnabled)); }
  
    printSerialTelnet(F("WebSocket:    "));
    if (mySettings.useHTTP){
      switch (stateWebSocket){
        case START_UP:         printSerialTelnet(FPSTR(mStarting));       break;
        case CHECK_CONNECTION: printSerialTelnet(FPSTR(mMonitoring));     break;
        case IS_SCANNING:      printSerialTelnet(FPSTR(mScanning));       break;
        case IS_CONNECTING:    printSerialTelnet(FPSTR(mConnecting));     break;
        case IS_WAITING:       printSerialTelnet(FPSTR(mWaiting));        break;
        default:               break;
      }
    } else {                   printSerialTelnet(FPSTR(mNotEnabled)); }
    printSerialTelnet(F("\r\n"));
  
    printSerialTelnet(F("OTA:          "));
    if (mySettings.useOTA){
      switch (stateOTA){
        case START_UP:         printSerialTelnet(FPSTR(mStarting));     break;
        case CHECK_CONNECTION: printSerialTelnet(FPSTR(mMonitoring));   break;
        case IS_SCANNING:      printSerialTelnet(FPSTR(mScanning));     break;
        case IS_CONNECTING:    printSerialTelnet(FPSTR(mConnecting));   break;
        case IS_WAITING:       printSerialTelnet(FPSTR(mWaiting));      break;
        default:               break;
      }
    } else {                   printSerialTelnet(FPSTR(mNotEnabled)); }
  
    printSerialTelnet(F("HTTP:         "));
    if (mySettings.useHTTP){
      switch (stateHTTP){
        case START_UP:         printSerialTelnet(FPSTR(mStarting));     break;
        case CHECK_CONNECTION: printSerialTelnet(FPSTR(mMonitoring));   break;
        case IS_SCANNING:      printSerialTelnet(FPSTR(mScanning));     break;
        case IS_CONNECTING:    printSerialTelnet(FPSTR(mConnecting));   break;
        case IS_WAITING:       printSerialTelnet(FPSTR(mWaiting));      break;
        default:               break;
      }
    } else {                   printSerialTelnet(FPSTR(mNotEnabled)); }
    printSerialTelnet(F("\r\n"));
  
    printSerialTelnet(F("HTTP Updater: "));
    if (mySettings.useHTTPUpdater){
      switch (stateHTTPUpdater){
        case START_UP:         printSerialTelnet(FPSTR(mStarting));   break;
        case CHECK_CONNECTION: printSerialTelnet(FPSTR(mMonitoring)); break;
        case IS_SCANNING:      printSerialTelnet(FPSTR(mScanning));   break;
        case IS_CONNECTING:    printSerialTelnet(FPSTR(mConnecting)); break;
        case IS_WAITING:       printSerialTelnet(FPSTR(mWaiting));    break;
        default:               break;
      }
    } else {                   printSerialTelnet(FPSTR(mNotEnabled)); }
  
    printSerialTelnet(F("NTP:          "));
    if (mySettings.useNTP){
      switch (stateNTP){
        case START_UP:         printSerialTelnet(FPSTR(mStarting));    break;
        case CHECK_CONNECTION: printSerialTelnet(FPSTR(mMonitoring));  break;
        case IS_SCANNING:      printSerialTelnet(FPSTR(mScanning));    break;
        case IS_CONNECTING:    printSerialTelnet(FPSTR(mConnecting));  break;
        case IS_WAITING:       printSerialTelnet(FPSTR(mWaiting));     break;
        default:               break;
      }
    } else {                   printSerialTelnet(FPSTR(mNotEnabled)); }
    printSerialTelnet(F("\r\n"));
  
    printSerialTelnet(F("Telnet:       "));
    if (mySettings.useTelnet){
      switch (stateTelnet){
        case START_UP:         printSerialTelnet(FPSTR(mStarting));    break;
        case CHECK_CONNECTION: printSerialTelnet(FPSTR(mMonitoring));  break;
        case IS_SCANNING:      printSerialTelnet(FPSTR(mScanning));    break;
        case IS_CONNECTING:    printSerialTelnet(FPSTR(mConnecting));  break;
        case IS_WAITING:       printSerialTelnet(FPSTR(mWaiting));     break;
        default:               break;
      }
    } else {                   printSerialTelnet(FPSTR(mNotEnabled)); }
    printSerialTelnet(F("\r\n"));
  
  } // wifi avail
}
