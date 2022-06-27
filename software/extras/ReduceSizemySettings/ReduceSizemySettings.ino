/******************************************************************************************************/
// Stored System settings
// This will reduce the settings in global ram by 700bytes
/******************************************************************************************************/
#include <EEPROM.h>
#define EEPROM_SIZE 2048                                   // make sure this value is larger than the space required by the settings below!
int eepromAddress = 0;                                     // 

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
  uint16_t      nightEnd;                                  // minutes from midnight when to start changing backight because of low airquality
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
  bool          useLog;                                    // keep track of serial prints in logfile
};

struct SettingsNew {
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
  char          ssid1[32];                                 // WiFi SSID 32 bytes max
  char          pw1[32];                                   // WiFi passwrod 64 chars max
  char          ssid2[32];                                 // 2nd set of WiFi credentials
  char          pw2[32];                                   //
  char          ssid3[32];                                 // 3rd set of WiFi credentials
  char          pw3[32];                                   //
  char          mqtt_server[64];                           // your mqtt server
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
  bool          useHTTP;                                   // provide webserver
  bool          useNTP;                                    // want network time
  bool          useMQTT;                                   // provide MQTT data
  bool          useOTA;                                    // porivide over the air programming
  bool          usemDNS;                                   // provide mDNS
  bool          useTelnet;                                 // porivide telnet
  bool          useSerial;                                 // porivide Serial interface on USB
  bool          useLog;                                    // keep track of serial prints in logfile
  bool          sendMQTTimmediate;                         // true: update MQTT right away when new data is availablk, otherwise send one unified message
  bool          consumerLCD;                               // simplified display
  bool          useBME280;                                 // ...
  bool          useHTTPUpdater;                            // upload firmware with HTTP interface
  bool          useBacklight;                              // backlight on/off
  char          mqtt_fallback[64];                         // your fallback mqtt server if initial server fails, useful when on private home network
  char          mqtt_mainTopic[32];                        // name of this sensing device for mqtt broker
  float         avgP;                                      // averagePressure
  uint16_t      nightBegin;                                // minutes from midnight when to stop changing backlight because of low airquality
  uint16_t      nightEnd;                                  // minutes from midnight when to start changing backight because of low airquality
  int16_t       rebootMinute;                              // when should we attempt rebooting in minutes after midnight
  char          ntpServer[64];                             // ntp server
  char          timeZone[64];                              // The time zone according to second row in https://raw.githubusercontent.com/nayarsystems/posix_tz_db/master/zones.csv 
};

Settings mySettings;                                       // the settings
SettingsNew mySettingsNew;                                 // the settings
unsigned long tmpTime;

void setup() {

  Serial.begin(115200);
  Serial.println();
  delay(2000);

  tmpTime = millis();
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(eepromAddress, mySettings);
  if (mySettings.debuglevel > 0) { Serial.print(F("EEPROM read in: ")); Serial.print((millis() - tmpTime)); Serial.println(F("ms")); }
  
  //tmpTime = millis();
  //if(!LittleFS.begin()){ Serial.println("LittelFS mount failed!"); }
  //myFile = LittleFS.open("/Sensi.config", "r");
  //myFile.read((byte *)&mySettings, sizeof(mySettings));
  //myFile.close();
  
  printSettings();

  while (Serial.available()) { Serial.read(); }
  Serial.println("Enter to continue");
  while (!Serial.available()) { delay(1000); }

  // copy settings to smaller construct
  mySettingsNew.runTime                       = mySettings.runTime;
  mySettingsNew.debuglevel                    = mySettings.debuglevel;
  mySettingsNew.baselineSGP30_valid           = mySettings.baselineSGP30_valid;
  mySettingsNew.baselineeCO2_SGP30            = mySettings.baselineeCO2_SGP30;
  mySettingsNew.baselinetVOC_SGP30            = mySettings.baselinetVOC_SGP30;
  mySettingsNew.baselineCCS811_valid          = mySettings.baselineCCS811_valid;
  mySettingsNew.baselineCCS811                = mySettings.baselineCCS811;
  mySettingsNew.tempOffset_SCD30_valid        = mySettings.tempOffset_SCD30_valid;
  mySettingsNew.tempOffset_SCD30              = mySettings.tempOffset_SCD30;
  mySettingsNew.forcedCalibration_SCD30_valid = mySettings.forcedCalibration_SCD30_valid;
  mySettingsNew.forcedCalibration_SCD30       = mySettings.forcedCalibration_SCD30;
  mySettingsNew.tempOffset_MLX_valid          = mySettings.tempOffset_MLX_valid;
  mySettingsNew.tempOffset_MLX                = mySettings.tempOffset_MLX;
  
  strlcpy(mySettingsNew.ssid1,                mySettings.ssid1, sizeof(mySettingsNew.ssid1));
  strlcpy(mySettingsNew.pw1,                  mySettings.pw1, sizeof(mySettingsNew.pw1));;
  strlcpy(mySettingsNew.ssid2,                mySettings.ssid2, sizeof(mySettingsNew.ssid2));;
  strlcpy(mySettingsNew.pw2,                  mySettings.pw2, sizeof(mySettingsNew.pw2));;
  strlcpy(mySettingsNew.ssid3,                mySettings.ssid3, sizeof(mySettingsNew.ssid3));
  strlcpy(mySettingsNew.pw3,                  mySettings.pw3, sizeof(mySettingsNew.pw3));
  strlcpy(mySettingsNew.mqtt_server,          mySettings.mqtt_server, sizeof(mySettingsNew.mqtt_server));
  strlcpy(mySettingsNew.mqtt_username,        mySettings.mqtt_username, sizeof(mySettingsNew.mqtt_username));
  strlcpy(mySettingsNew.mqtt_password,        mySettings.mqtt_password, sizeof(mySettingsNew.mqtt_password));
  
  mySettingsNew.useLCD                        = mySettings.useLCD;
  mySettingsNew.useWiFi                       = mySettings.useWiFi;
  mySettingsNew.useSCD30                      = mySettings.useSCD30;
  mySettingsNew.useSPS30                      = mySettings.useSPS30;
  mySettingsNew.useSGP30                      = mySettings.useSGP30;
  mySettingsNew.useMAX30                      = mySettings.useMAX30;
  mySettingsNew.useMLX                        = mySettings.useMLX;
  mySettingsNew.useBME680                     = mySettings.useBME680;
  mySettingsNew.useCCS811                     = mySettings.useCCS811;
  mySettingsNew.sendMQTTimmediate             = mySettings.sendMQTTimmediate;
  
  strlcpy(mySettingsNew.mqtt_fallback,        mySettings.mqtt_fallback, sizeof(mySettingsNew.mqtt_fallback));;
  strlcpy(mySettingsNew.mqtt_mainTopic,       mySettings.mqtt_mainTopic, sizeof(mySettingsNew.mqtt_mainTopic));
 
  mySettingsNew.consumerLCD                   = mySettings.consumerLCD;
  mySettingsNew.useBME280                     = mySettings.useBME280;
  mySettingsNew.avgP                          = mySettings.avgP;
  mySettingsNew.useBacklight                  = mySettings.useBacklight;
  mySettingsNew.nightBegin                    = mySettings.nightBegin;
  mySettingsNew.nightEnd                      = mySettings.nightEnd;
  mySettingsNew.rebootMinute                  = mySettings.rebootMinute;
  mySettingsNew.useHTTP                       = mySettings.useHTTP;
  mySettingsNew.useNTP                        = mySettings.useNTP;
  mySettingsNew.useMQTT                       = mySettings.useMQTT;
  mySettingsNew.useOTA                        = mySettings.useOTA;
  mySettingsNew.usemDNS                       = mySettings.usemDNS;
  strlcpy(mySettingsNew.ntpServer, mySettings.ntpServer, sizeof(mySettingsNew.ntpServer));
  strlcpy(mySettingsNew.timeZone, mySettings.timeZone, sizeof(mySettingsNew.timeZone));
  mySettingsNew.useHTTPUpdater                = mySettings.useHTTPUpdater;
  mySettingsNew.useTelnet                     = mySettings.useTelnet;
  mySettingsNew.useSerial                     = mySettings.useSerial;
  mySettingsNew.useLog                        = mySettings.useLog;

  tmpTime = millis();
  EEPROM.put(0, mySettingsNew);
  if (EEPROM.commit()) {
    Serial.print(F("EEPROM saved in: "));
    Serial.print(millis() - tmpTime);
    Serial.println(F("ms"));
  }
  
  //tmpTime = millis();
  //if(!LittleFS.begin()){
  //  Serial.println("An Error has occurred while mounting LittleFS");
  //  return;
  //}
  //myFile = LittleFS.open("/Sensi.config", "w+");
  //myFile.write((byte *)&mySettingsNew, sizeof(mySettingsNew));
  //myFile.close();
  //Serial.print(F("LittleFS saved in: ")); Serial.print((millis() - tmpTime)); Serial.println(F("ms"));

}

void loop() {
}


void printSettings() {
  Serial.println(F("=System=========================================================================="));
  Serial.print(F("Debug level: .................. ")); Serial.println((unsigned int)mySettings.debuglevel);
  Serial.print(F("Reboot Minute: ................ ")); Serial.println(mySettings.rebootMinute);
  Serial.print(F("Runtime [min]: ................ ")); Serial.println((unsigned long)mySettings.runTime / 60);
  Serial.print(F("Simpler Display: .............. ")); Serial.println((mySettings.consumerLCD) ? "on" : "off");
  Serial.print(F("Log: .......................... ")); Serial.println((mySettings.useLog) ? "on" : "off");
  Serial.println(F("=Network==========================================================================="));
  Serial.print(F("WiFi: ......................... ")); Serial.println((mySettings.useWiFi) ? "on" : "off");
  Serial.print(F("HTML: ......................... ")); Serial.println((mySettings.useHTTP) ? "on" : "off"); 
  Serial.print(F("MQTT: ......................... ")); Serial.println((mySettings.useMQTT) ? "on" : "off"); 
  Serial.print(F("OTA: .......................... ")); Serial.println((mySettings.useOTA) ? "on" : "off"); 
  Serial.print(F("mDNS: ......................... ")); Serial.println((mySettings.usemDNS) ? "on" : "off"); 
  Serial.print(F("HTTPUpdater: .................. ")); Serial.println((mySettings.useHTTPUpdater) ? "on" : "off"); 
  Serial.print(F("Telnet: .,..................... ")); Serial.println((mySettings.useTelnet) ? "on" : "off"); 
  Serial.print(F("Serial: ....................... ")); Serial.println((mySettings.useSerial) ? "on" : "off");   
  Serial.println(F("-Time-------------------------------------------------------------------------------"));
  Serial.print(F("NTP: .......................... ")); Serial.println((mySettings.useNTP) ? "on" : "off"); 
  Serial.print(F("NTP Server: ................... ")); Serial.println(mySettings.ntpServer); 
  Serial.print(F("Time Zone ..................... ")); Serial.println(mySettings.timeZone); 
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
  Serial.print(F("Forced Calibration SCD30 valid: ")); Serial.println((int)mySettings.forcedCalibration_SCD30_valid);
  Serial.print(F("Forced Calibration SCD30:[ppm]. ")); Serial.println((float)mySettings.forcedCalibration_SCD30);
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
