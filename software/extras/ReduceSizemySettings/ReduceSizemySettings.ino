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

struct SettingsSmall {
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
  char          mqtt_server[65];                          // your mqtt server
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
  char          mqtt_fallback[65];                        // your fallback mqtt server if initial server fails, useful when on private home network
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
  bool          useOTA;                                    // porivude over the air programming
  bool          usemDNS;                                   // provide mDNS
  char          ntpServer[65];                            // ntp server
  int           utcOffset;                                 // time offset from GMT in hours 
  bool          enableDST;                                 // enable day light sving time
};

Settings mySettings;                                 // the settings
SettingsSmall mySettingsSmall;                                 // the settings
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
  mySettingsSmall.runTime                       = mySettings.runTime;
  mySettingsSmall.debuglevel                    = mySettings.debuglevel;
  mySettingsSmall.baselineSGP30_valid           = mySettings.baselineSGP30_valid;
  mySettingsSmall.baselineeCO2_SGP30            = mySettings.baselineeCO2_SGP30;
  mySettingsSmall.baselinetVOC_SGP30            = mySettings.baselinetVOC_SGP30;
  mySettingsSmall.baselineCCS811_valid          = mySettings.baselineCCS811_valid;
  mySettingsSmall.baselineCCS811                = mySettings.baselineCCS811;
  mySettingsSmall.tempOffset_SCD30_valid        = mySettings.tempOffset_SCD30_valid;
  mySettingsSmall.tempOffset_SCD30              = mySettings.tempOffset_SCD30;
  mySettingsSmall.forcedCalibration_SCD30_valid = mySettings.forcedCalibration_SCD30_valid;
  mySettingsSmall.forcedCalibration_SCD30       = mySettings.forcedCalibration_SCD30;
  mySettingsSmall.tempOffset_MLX_valid          = mySettings.tempOffset_MLX_valid;
  mySettingsSmall.tempOffset_MLX                = mySettings.tempOffset_MLX;
  
  strlcpy(mySettingsSmall.ssid1,                mySettings.ssid1, sizeof(mySettingsSmall.ssid1));
  strlcpy(mySettingsSmall.pw1,                  mySettings.pw1, sizeof(mySettingsSmall.pw1));;
  strlcpy(mySettingsSmall.ssid2,                mySettings.ssid2, sizeof(mySettingsSmall.ssid2));;
  strlcpy(mySettingsSmall.pw2,                  mySettings.pw2, sizeof(mySettingsSmall.pw2));;
  strlcpy(mySettingsSmall.ssid3,                mySettings.ssid3, sizeof(mySettingsSmall.ssid3));
  strlcpy(mySettingsSmall.pw3,                  mySettings.pw3, sizeof(mySettingsSmall.pw3));
  strlcpy(mySettingsSmall.mqtt_server,          mySettings.mqtt_server, sizeof(mySettingsSmall.mqtt_server));
  strlcpy(mySettingsSmall.mqtt_username,        mySettings.mqtt_username, sizeof(mySettingsSmall.mqtt_username));
  strlcpy(mySettingsSmall.mqtt_password,        mySettings.mqtt_password, sizeof(mySettingsSmall.mqtt_password));
  
  mySettingsSmall.useLCD                        = mySettings.useLCD;
  mySettingsSmall.useWiFi                       = mySettings.useWiFi;
  mySettingsSmall.useSCD30                      = mySettings.useSCD30;
  mySettingsSmall.useSPS30                      = mySettings.useSPS30;
  mySettingsSmall.useSGP30                      = mySettings.useSGP30;
  mySettingsSmall.useMAX30                      = mySettings.useMAX30;
  mySettingsSmall.useMLX                        = mySettings.useMLX;
  mySettingsSmall.useBME680                     = mySettings.useBME680;
  mySettingsSmall.useCCS811                     = mySettings.useCCS811;
  mySettingsSmall.sendMQTTimmediate             = mySettings.sendMQTTimmediate;
  
  strlcpy(mySettingsSmall.mqtt_fallback,        mySettings.mqtt_fallback, sizeof(mySettingsSmall.mqtt_fallback));;
  strlcpy(mySettingsSmall.mqtt_mainTopic,       mySettings.mqtt_mainTopic, sizeof(mySettingsSmall.mqtt_mainTopic));
 
  mySettingsSmall.consumerLCD                   = mySettings.consumerLCD;
  mySettingsSmall.useBME280                     = mySettings.useBME280;
  mySettingsSmall.avgP                          = mySettings.avgP;
  mySettingsSmall.useBacklight                  = mySettings.useBacklight;
  mySettingsSmall.nightBegin                    = mySettings.nightBegin;
  mySettingsSmall.nightEnd                      = mySettings.nightEnd;
  mySettingsSmall.rebootMinute                  = mySettings.rebootMinute;
  mySettingsSmall.useHTTP                       = mySettings.useHTTP;
  mySettingsSmall.useNTP                        = mySettings.useNTP;
  mySettingsSmall.useMQTT                       = mySettings.useMQTT;
  mySettingsSmall.useOTA                        = mySettings.useOTA;
  mySettingsSmall.usemDNS                       = mySettings.usemDNS;
  strlcpy(mySettingsSmall.ntpServer, mySettings.ntpServer, sizeof(mySettingsSmall.ntpServer));
  mySettingsSmall.utcOffset                     = mySettings.utcOffset;
  mySettingsSmall.enableDST                     = mySettings.enableDST;

  tmpTime = millis();
  EEPROM.put(0, mySettingsSmall);
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
  //myFile.write((byte *)&mySettingsSmall, sizeof(mySettingsSmall));
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
