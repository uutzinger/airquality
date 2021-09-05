/******************************************************************************************************/
// Stored System settings
/******************************************************************************************************/
#define EEPROM_SIZE 2048                                   // make sure this value is larger than the space required by the settings below!
int eepromAddress = 0;                                     // So far this location is not oeverwritten by uploading other programs
// ========================================================// The EEPROM section
// This table has grown over time. So its not in order.
// Appending new settings will keeps the already stored settings.
// Boolean settings are stored as a byte.
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
  bool          useBME280;                                 // ...
  float         avgP;                                      // averagePressure
  bool          useBacklight;                              // backlight on/off
  uint16_t      nightBegin;                                // minutes from midnight when to stop changing backlight because of low airquality
  uint16_t    nightEnd;                                  // minutes from midnight when to start changing backight because of low airquality
  int16_t       rebootMinute;                              // when should we attempt rebooting in minutes after midnight
  bool          useHTTP;                                   // provide webserver
  bool          useNTP;                                    // want network time
  bool          useMQTT;                                   // provide MQTT data
  bool          useOTA;                                    // porivude over the air programming
  bool          usemDNS;                                   // provide mDNS
  char          ntpServer[255];                            // ntp server
  int           utcOffset;                                 // time offset from GMT in hours 
  bool          enableDST;                                 // enable day light sving time
};

#include <SPI.h>
#include <EEPROM.h>
Settings mySettings;                                 // the settings
#include <LittleFS.h>
unsigned long tmpTime;
#include <ArduinoJson.h>

void setup() {

  Serial.begin(115200);
  Serial.println();
  delay(2000);

  tmpTime = millis();
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(eepromAddress, mySettings);
  Serial.print(F("EEPROM read in: ")); Serial.print((millis() - tmpTime)); Serial.println(F("ms"));

  printSettings();

  while (Serial.available()) { Serial.read(); }
  Serial.println("Enter to continue");
  while (!Serial.available()) { delay(1000); }
  
  tmpTime = millis();
  if(!LittleFS.begin()){
    Serial.println("An Error has occurred while mounting LittleFS");
    return;
  }
  File myFile = LittleFS.open("/Sensi.config", "w+");
  myFile.write((byte *)&mySettings, sizeof(mySettings));
  myFile.close();
  Serial.print(F("LittleFS saved in: ")); Serial.print((millis() - tmpTime)); Serial.println(F("ms"));

  tmpTime = millis();
  saveConfiguration("/Sensi.json", mySettings);
  Serial.println();
  Serial.print(F("LittleFS JSON saved in: ")); Serial.print((millis() - tmpTime)); Serial.println(F("ms"));
}

void loop() {
}

// Saves the configuration to a file
void saveConfiguration(const char *filename, const Settings &config) {
  // Delete existing file, otherwise the configuration is appended to the file
  LittleFS.remove(filename);

  // Open file for writing
  File file = LittleFS.open(filename, "w+");
  if (!file) {
    Serial.println(F("Failed to create file"));
    return;
  }

  // Allocate a temporary JsonDocument
  // Use arduinojson.org/assistant to compute the capacity.
  StaticJsonDocument<1024> doc;

  // Set the values in the document
  doc["runTime"]    = config.runTime;                         // keep track of total sensor run time
  doc["debuglevel"] =  config.debuglevel;                                // amount of debug output on serial port

  doc["baselineSGP30_valid"]=config.baselineSGP30_valid;                       // 0xF0 = valid
  doc["baselineeCO2_SGP30"]=config.baselineeCO2_SGP30;                        //
  doc["baselinetVOC_SGP30"]=config.baselinetVOC_SGP30;                        //
  doc["baselineCCS811_valid"]=config.baselineCCS811_valid;                      // 0xF0 = valid
  doc["baselineCCS811"]=config.baselineCCS811;                            // baseline is an internal value, not ppm
  doc["tempOffset_SCD30_valid"]=config.tempOffset_SCD30_valid;                    // 0xF0 = valid
  doc["tempOffset_SCD30"]=config.tempOffset_SCD30;                          // in C
  doc["forcedCalibration_SCD30_valid"]=config.forcedCalibration_SCD30_valid;             // 0xF0 = valid, not used as the sensor is not designed to prepoluate its internal calibration
  doc["forcedCalibration_SCD30"]=config.forcedCalibration_SCD30;                   // not used
  doc["tempOffset_MLX_valid"]=config.tempOffset_MLX_valid;                      // 0xF0 = valid
  doc["tempOffset_MLX"]=config.tempOffset_MLX;                            // in C

  doc["useWiFi"]=config.useWiFi;                                   // use/not use WiFi and MQTT

  doc["ssid1"]=config.ssid1;                                 // WiFi SSID 32 bytes max
  doc["pw1"]=config.pw1;                                   // WiFi passwrod 64 chars max
  doc["ssid2"]=config.ssid2;                                 // 2nd set of WiFi credentials
  doc["pw2"]=config.pw2;                                   //
  doc["ssid3"]=config.ssid3;                                 // 3rd set of WiFi credentials
  doc["pw3"]=config.pw3;                                   //

  doc["useMQTT"]=config.useMQTT;                                   // provide MQTT data
  doc["mqtt_server"]=config.mqtt_server;                          // your mqtt server
  doc["mqtt_username"]=config.mqtt_username;                         // username for MQTT server, leave blank if no password
  doc["mqtt_password"]=config.mqtt_password;                         // password for MQTT server
  doc["sendMQTTimmediate"]=config.sendMQTTimmediate;                         // true: update MQTT right away when new data is availablk, otherwise send one unified message
  doc["mqtt_fallback"]=config.mqtt_fallback;                        // your fallback mqtt server if initial server fails, useful when on private home network
  doc["mqtt_mainTopic"]=config.mqtt_mainTopic;                        // name of this sensing device for mqtt broker

  doc["useLCD"]=config.useLCD;                                    // use/not use LCD even if it is connected
  doc["consumerLCD"]=config.consumerLCD;                               // simplified display
  doc["useBacklight"]=config.useBacklight;                              // backlight on/off
  doc["nightBegin"]=config.nightBegin;                                // minutes from midnight when to stop changing backlight because of low airquality
  doc["nightEnd"]=config.nightEnd;                                  // minutes from midnight when to start changing backight because of low airquality

  doc["useSCD30"]=config.useSCD30;                                  // ...
  doc["useSPS30"]=config.useSPS30;                                  // ...
  doc["useSGP30"]=config.useSGP30;                                  // ...
  doc["useMAX30"]=config.useMAX30;                                  // ...
  doc["useMLX"]=config.useMLX;                                    // ...
  doc["useBME680"]=config.useBME680;                                 // ...
  doc["useCCS811"]=config.useCCS811;                                 // ...
  doc["useBME280"]=config.useBME280;                                 // ...
  doc["avgP"]=config.avgP;                                      // averagePressure
  
  doc["useHTTP"]=config.useHTTP;                                   // provide webserver
  doc["useOTA"]=config.useOTA;                                    // porivude over the air programming
  doc["usemDNS"]=config.usemDNS;                                   // provide mDNS
  
  doc["useNTP"]=config.useNTP;                                    // want network time
  doc["ntpServer"]=config.ntpServer;                            // ntp server
  doc["utcOffset"]=config.utcOffset;                                 // time offset from GMT in hours 
  doc["enableDST"]=config.enableDST;                                 // enable day light sving time=
  doc["rebootMinute"]=config.rebootMinute;                              // when should we attempt rebooting in minutes after midnight

  // Serialize JSON to file
  if (serializeJson(doc, file) == 0) {
    Serial.println(F("Failed to write to file"));
  }

  serializeJsonPretty(doc, Serial);

  // Close the file
  file.close();
}

void printSettings() {
  Serial.println(F("=System=========================================================================="));
  Serial.print(F("Debug level: .................. ")); Serial.println((unsigned int)mySettings.debuglevel);
  Serial.print(F("Reboot Minute: ................ ")); Serial.println(mySettings.rebootMinute);
  Serial.print(F("Runtime [min]: ................ ")); Serial.println((unsigned long)mySettings.runTime / 60);
  Serial.print(F("Simpler Display: .............. ")); Serial.println((mySettings.consumerLCD) ? "on" : "off");
  Serial.println(F("=Network==========================================================================="));
  Serial.print(F("WiFi: ......................... ")); Serial.println((mySettings.useWiFi) ? "on" : "off");
  Serial.print(F("HTML: ......................... ")); Serial.println((mySettings.useHTTP) ? "on" : "off"); 
  Serial.print(F("MQTT: ......................... ")); Serial.println((mySettings.useMQTT) ? "on" : "off"); 
  Serial.print(F("OTA: .......................... ")); Serial.println((mySettings.useOTA) ? "on" : "off"); 
  Serial.print(F("mDNS: ......................... ")); Serial.println((mySettings.usemDNS) ? "on" : "off"); 
  Serial.println(F("-Time-------------------------------------------------------------------------------"));
  Serial.print(F("NTP: .......................... ")); Serial.println((mySettings.useNTP) ? "on" : "off"); 
  Serial.print(F("NTP Server: ................... ")); Serial.println(mySettings.ntpServer); 
  Serial.print(F("UTC Offset: ................... ")); Serial.println(mySettings.utcOffset); 
  Serial.print(F("Dayight Saving: ............... ")); Serial.println((mySettings.enableDST) ? "observed" : "not observed"); 
  Serial.println(F("-Credentials------------------------------------------------------------------------"));
  Serial.print(F("SSID: ......................... ")); Serial.println(mySettings.ssid1);
  Serial.print(F("PW: ........................... ")); Serial.println(mySettings.pw1);
  Serial.print(F("SSID: ......................... ")); Serial.println(mySettings.ssid2);
  Serial.print(F("PW: ........................... ")); Serial.println(mySettings.pw2);
  Serial.print(F("SSID: ......................... ")); Serial.println(mySettings.ssid3);
  Serial.print(F("PW: ........................... ")); Serial.println(mySettings.pw3);
  Serial.println(F("-MQTT-------------------------------------------------------------------------------"));
  Serial.print(F("MQTT: ......................... ")); Serial.println(mySettings.mqtt_server);
  Serial.print(F("MQTT fallback: ................ ")); Serial.println(mySettings.mqtt_fallback);
  Serial.print(F("MQTT user: .................... ")); Serial.println(mySettings.mqtt_username);
  Serial.print(F("MQTT password: ................ ")); Serial.println(mySettings.mqtt_password);
  Serial.print(F("MQTT send immediatly: ......... ")); Serial.println((mySettings.sendMQTTimmediate) ? "on" : "off");
  Serial.print(F("MQTT main topic: .............. ")); Serial.println(mySettings.mqtt_mainTopic);
  Serial.println(F("=Sensors============================================================================"));
  Serial.print(F("SCD30: ........................ ")); Serial.println((mySettings.useSCD30) ? "on" : "off");
  Serial.print(F("SPS30: ........................ ")); Serial.println((mySettings.useSPS30) ? "on" : "off");
  Serial.print(F("SGP30: ........................ ")); Serial.println((mySettings.useSGP30) ? "on" : "off");
  Serial.print(F("MAX30: ........................ ")); Serial.println((mySettings.useMAX30) ? "on" : "off");
  Serial.print(F("MLX: .......................... ")); Serial.println((mySettings.useMLX) ? "on" : "off");
  Serial.print(F("BME680: ....................... ")); Serial.println((mySettings.useBME680) ? "on" : "off");
  Serial.print(F("BME280: ....................... ")); Serial.println((mySettings.useBME280) ? "on" : "off");
  Serial.print(F("CCS811: ....................... ")); Serial.println((mySettings.useCCS811) ? "on" : "off");
  Serial.println(F("-Offsets&Baselines-------------------------------------------------------------------"));
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
  Serial.print(F("Average Pressure: ............. ")); Serial.println(mySettings.avgP);
  Serial.println(F("=LCD============================================================================="));  
  Serial.print(F("LCD: .......................... ")); Serial.println((mySettings.useLCD) ? "on" : "off");
  Serial.print(F("LCD Backlight: ................ ")); Serial.println((mySettings.useBacklight) ? "on" : "off");
  Serial.print(F("Night Start: .................. ")); Serial.println(mySettings.nightBegin);
  Serial.print(F("Night End: .................... ")); Serial.println(mySettings.nightEnd);
  Serial.println(F("================================================================================="));
}

void serialTrigger(char* mess, int timeout) {
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
