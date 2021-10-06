/******************************************************************************************************/
// Stored System settings
/******************************************************************************************************/
#include <EEPROM.h>
#include <FS.h>
//#include <ArduinoJson.h>                                   // Encoding Decoding JSON text
//#define JSONSIZE 4096                                      // crashes program if too small, use online tool to check recommended size
#define EEPROM_SIZE 2048                                   // make sure this value is larger than the space required by the settings below and lowwer than the max Settings of the microcontroller
//#define intervalSettingsJSON 604800000                     // 7 days
#define intervalSettings 43200000                          // 12 hrs, min write interval is 6,650,000 for 20 year liftime: 100,000/20yrs/365days = 13 writes per day
int eepromAddress = 0;                                     // 
unsigned long lastSaveSettings;                            // last time we updated EEPROM, should occur every couple days
unsigned long lastSaveSettingsJSON;                        // last time we updated JSON, should occur every couple days
// ========================================================// The EEPROM section
// This table has grown over time. So its not in order.
// Appending new settings will keeps the already stored settings.
// Boolean settings are stored as a byte.
// This structure is 608 bytes.
struct Settings {
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
  char          pw1[33];                                   // WiFi passwrod 64 chars max
  char          ssid2[33];                                 // 2nd set of WiFi credentials
  char          pw2[33];                                   //
  char          ssid3[33];                                 // 3rd set of WiFi credentials
  char          pw3[33];                                   //
  char          mqtt_server[65];                           // your mqtt server
  char          mqtt_username[33];                         // username for MQTT server, leave blank if no password
  char          mqtt_password[33];                         // password for MQTT server
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
  char          mqtt_fallback[65];                         // your fallback mqtt server if initial server fails, useful when on private home network
  char          mqtt_mainTopic[33];                        // name of this sensing device for mqtt broker
  bool          consumerLCD;                               // simplified display
  bool          useBME280;                                 // ...
  float         avgP;                                      // averagePressure
  bool          useBacklight;                              // backlight on/off
  uint16_t      nightBegin;                                // minutes from midnight when to stop changing backlight because of low airquality
  uint16_t		  nightEnd;                                  // minutes from midnight when to start changing backight because of low airquality
  int16_t       rebootMinute;                              // when should we attempt rebooting in minutes after midnight
  bool          useHTTP;                                   // provide webserver
  bool          useNTP;                                    // want network time
  bool          useMQTT;                                   // provide MQTT data
  bool          useOTA;                                    // porivide over the air programming
  bool          usemDNS;                                   // provide mDNS
  char          ntpServer[65];                             // ntp server
  char          timeZone[41];                              // The time zone according to second row in https://raw.githubusercontent.com/nayarsystems/posix_tz_db/master/zones.csv 
  bool          useHTTPUpdater;                            // upload firmware with HTTP interface
  bool          useTelnet;                                 // porivide telnet
  bool          useSerial;                                 // porivide Serial interface on USB
};

Settings mySettings;                                       // the settings

//void saveConfiguration(const Settings &config);
//void loadConfiguration(Settings &config);
