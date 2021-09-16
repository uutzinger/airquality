/******************************************************************************************************/
// Initialize Configuration
/******************************************************************************************************/
#include "VSC.h"
#ifdef EDITVSC
#include "src/Config.h"
#endif

// Save and Read Settings
// Uses JSON structure on LittleFS Filespace
//
// These routines are not used for bootup
// JSON saving settings to LittelFS regularly results in system crashes.
// The JSON conversion might complete but opening and writting the file does not.

/**
void saveConfiguration(const Settings &config) {
  if (ESP.getMaxFreeBlockSize() > JSONSIZE) {
    if (fsOK) { // file system is mounted
    
      // Allocate a temporary JsonDocument
      // Use arduinojson.org/assistant to compute the capacity.
  
      StaticJsonDocument<JSONSIZE> doc;
    
      doc["runTime"]                      = config.runTime;                                  // keep track of total sensor run time
      doc["debuglevel"]                   = config.debuglevel;                               // amount of debug output on serial port
    
      doc["baselineSGP30_valid"]          = config.baselineSGP30_valid;                      // 0xF0 = valid
      doc["baselineeCO2_SGP30"]           = config.baselineeCO2_SGP30;                       //
      doc["baselinetVOC_SGP30"]           = config.baselinetVOC_SGP30;                       //
      doc["baselineCCS811_valid"]         = config.baselineCCS811_valid;                     // 0xF0 = valid
      doc["baselineCCS811"]               = config.baselineCCS811;                           // baseline is an internal value, not ppm
      doc["tempOffset_SCD30_valid"]       = config.tempOffset_SCD30_valid;                   // 0xF0 = valid
      doc["tempOffset_SCD30"]             = config.tempOffset_SCD30;                         // in C
      doc["forcedCalibration_SCD30_valid"]= config.forcedCalibration_SCD30_valid;            // 0xF0 = valid, not used as the sensor is not designed to prepoluate its internal calibration
      doc["forcedCalibration_SCD30"]      = config.forcedCalibration_SCD30;                  // not used
      doc["tempOffset_MLX_valid"]         = config.tempOffset_MLX_valid;                     // 0xF0 = valid
      doc["tempOffset_MLX"]               = config.tempOffset_MLX;                           // in C
      
      //printSerialTelnet(F("DBG:JSON 2\r\n"));
    
      doc["useWiFi"]                      = config.useWiFi;                                  // use/not use WiFi and MQTT
    
      doc["ssid1"]                        = config.ssid1;                                    // WiFi SSID 32 bytes max
      doc["pw1"]                          = config.pw1;                                      // WiFi passwrod 64 chars max
      doc["ssid2"]                        = config.ssid2;                                    // 2nd set of WiFi credentials
      doc["pw2"]                          = config.pw2;                                      //
      doc["ssid3"]                        = config.ssid3;                                    // 3rd set of WiFi credentials
      doc["pw3"]                          = config.pw3;                                      //
  
      //printSerialTelnet(F("DBG:JSON 3\r\n"));
    
      doc["useMQTT"]                      = config.useMQTT;                                  // provide MQTT data
      doc["mqtt_server"]                  = config.mqtt_server;                              // your mqtt server
      doc["mqtt_username"]                = config.mqtt_username;                            // username for MQTT server, leave blank if no password
      doc["mqtt_password"]                = config.mqtt_password;                            // password for MQTT server
      doc["sendMQTTimmediate"]            = config.sendMQTTimmediate;                        // true: update MQTT right away when new data is availablk, otherwise send one unified message
      doc["mqtt_fallback"]                = config.mqtt_fallback;                            // your fallback mqtt server if initial server fails, useful when on private home network
      doc["mqtt_mainTopic"]               = config.mqtt_mainTopic;                           // name of this sensing device for mqtt broker
    
      doc["useLCD"]                       = config.useLCD;                                   // use/not use LCD even if it is connected
      doc["consumerLCD"]                  = config.consumerLCD;                              // simplified display
      doc["useBacklight"]                 = config.useBacklight;                             // backlight on/off
      doc["nightBegin"]                   = config.nightBegin;                               // minutes from midnight when to stop changing backlight because of low airquality
      doc["nightEnd"]                     = config.nightEnd;                                 // minutes from midnight when to start changing backight because of low airquality
  
      //printSerialTelnet(F("DBG:JSON 4\r\n"));
    
      doc["useSCD30"]                     = config.useSCD30;                                 // ...
      doc["useSPS30"]                     = config.useSPS30;                                 // ...
      doc["useSGP30"]                     = config.useSGP30;                                 // ...
      doc["useMAX30"]                     = config.useMAX30;                                 // ...
      doc["useMLX"]                       = config.useMLX;                                   // ...
      doc["useBME680"]                    = config.useBME680;                                // ...
      doc["useCCS811"]                    = config.useCCS811;                                // ...
      doc["useBME280"]                    = config.useBME280;                                // ...
      doc["avgP"]                         = config.avgP;                                     // averagePressure
      
      doc["useHTTP"]                      = config.useHTTP;                                  // provide webserver
      doc["useOTA"]                       = config.useOTA;                                   // porivude over the air programming
      doc["usemDNS"]                      = config.usemDNS;                                  // provide mDNS
      doc["useHTTPUpdater"]               = config.useHTTPUpdater;                           // use HTTP updating 
      doc["useTelnet"]                    = config.useTelnet;                                // use Telnet for Serial
      doc["useSerial"]                    = config.useSerial;                                // use USB for Serial
    
      //printSerialTelnet(F("DBG:JSON 5\r\n"));
      doc["useNTP"]                       = config.useNTP;                                   // want network time
      doc["ntpServer"]                    = config.ntpServer;                                // ntp server
      doc["timeZone"]                     = config.timeZone;                                 // time zone according second column in https://raw.githubusercontent.com/nayarsystems/posix_tz_db/master/zones.csv
      doc["rebootMinute"]                 = config.rebootMinute;                             // if sensor error, when should we attempt rebooting in minutes after midnight?
    
      //serializeJsonPretty(doc, Serial);  Serial.println();
      
      #if defined(DEBUG)
      printSerialTelnet(F("DBG:JSON OPEN\r\n"));
      #endif
      // Open file for writing
      #if defined(DEBUG)
      sprintf_P(tmpStr,PSTR("Filename is: %s\r\n","/Sensi.json")); printSerialTelnet(tmpStr);
      sprintf_P(tmpStr,PSTR("Free Heap Size: %dbytes Heap Fragmentation: %d%% Max Block Size: %dbytes\r\n"), ESP.getFreeHeap(), ESP.getHeapFragmentation(), ESP.getMaxFreeBlockSize()); printSerialTelnet(tmpStr);
      #endif
      File cfgFile = LittleFS.open("/Sensi.json", "w+");
      if (!cfgFile) { printSerialTelnet(F("Failed to create file\r\n")); return; }
      else {
        #if defined(DEBUG)
        printSerialTelnet(F("DBG:JSON Serialze to File\r\n"));
        #endif
        // Serialize JSON to file      
        if (serializeJsonPretty(doc, cfgFile) == 0) { printSerialTelnet(F("Failed to write to file\r\n")); }
        #if defined(DEBUG)
        printSerialTelnet(F("DBG:JSON CLOSE\r\n"));
        #endif
        // Close the file
        cfgFile.close();
      }
      //printSerialTelnet(F("DBG:JSON DONE\r\n"));
    }
  }
}

void loadConfiguration(Settings &config) {
  if (ESP.getMaxFreeBlockSize() > JSONSIZE) {
    if (fsOK) {
      if (LittleFS.exists("/Sensi.json")) {
        // Open file for reading
        File cfgFile = LittleFS.open("/Sensi.json", "r");
      
        // Allocate a temporary JsonDocument
        // Use arduinojson.org/v6/assistant to compute the capacity.
        StaticJsonDocument<JSONSIZE> doc;
      
        // Deserialize the JSON document
        DeserializationError error = deserializeJson(doc, cfgFile);
        if (error) { printSerialTelnet(F("Failed to read file, using default configuration\r\n")); }
      
        config.runTime                = doc["runTime"]                              | 0;
        config.debuglevel             = doc["debuglevel"]                           | 1;
      
        config.baselineSGP30_valid    = doc["baselineSGP30_valid"]                  | 0x00;
        config.baselineeCO2_SGP30     = doc["baselineeCO2_SGP30"]                   | 0;
        config.baselinetVOC_SGP30     = doc["baselinetVOC_SGP30"]                   | 0;
        config.baselineCCS811_valid   = doc["baselineCCS811_valid"]                 | 0x00;
        config.baselineCCS811         = doc["baselineCCS811"]                       | 0;
        config.tempOffset_SCD30_valid = doc["tempOffset_SCD30_valid"]               | 0x00;
        config.tempOffset_SCD30       = doc["tempOffset_SCD30"]                     | 0.01;
        config.forcedCalibration_SCD30_valid = doc["forcedCalibration_SCD30_valid"] | 0x00;
        config.forcedCalibration_SCD30 = doc["forcedCalibration_SCD30"]             | 0.0;
        config.tempOffset_MLX_valid   = doc["tempOffset_MLX_valid"]                 | 0x00;
        config.tempOffset_MLX         = doc["tempOffset_MLX"]                       | 0.0;
      
        config.useWiFi                = doc["useWiFi"]                              | true;
        strlcpy(config.ssid1,           doc["ssid1"]                                | "MEDDEV", sizeof(config.ssid1));
        strlcpy(config.pw1,             doc["pw1"]                                  | "", sizeof(config.pw1));
        strlcpy(config.ssid2,           doc["ssid2"]                                | "UAGUest", sizeof(config.ssid2));
        strlcpy(config.pw2,             doc["pw2"]                                  | "", sizeof(config.pw3));
        strlcpy(config.ssid3,           doc["ssid3"]                                | "MuchoCoolioG", sizeof(config.ssid3));
        strlcpy(config.pw3,             doc["pw3"]                                  | "", sizeof(config.pw3));
      
        config.useMQTT                = doc["useMQTT"]                              | false;
        strlcpy(config.mqtt_server,     doc["mqtt_server"]                          | "my.mqqtt.server.org", sizeof(config.mqtt_server));
        strlcpy(config.mqtt_username,   doc["mqtt_username"]                        | "mqtt", sizeof(config.mqtt_username));
        strlcpy(config.mqtt_password,   doc["mqtt_password"]                        | "", sizeof(config.mqtt_password));
        strlcpy(config.mqtt_fallback,   doc["mqtt_fallback"]                        | "192.168.1.1", sizeof(config.mqtt_password));
        strlcpy(config.mqtt_mainTopic,  doc["mqtt_mainTopic"]                       | "Senso", sizeof(config.mqtt_mainTopic));
        config.sendMQTTimmediate =      doc["sendMQTTimmediate"]                    | true;
        
        config.useLCD                 = doc["useLCD"]                               | true;
        config.consumerLCD            = doc["consumerLCD"]                          | true;
        config.useBacklight           = doc["useBacklight"]                         | true;
        config.nightBegin             = doc["nightBegin"]                           | 1320;
        config.nightEnd               = doc["nightEnd"]                             | 420;
      
        config.useSCD30               = doc["useSCD30"]                             | true;
        config.useSPS30               = doc["useSPS30"]                             | true;
        config.useSGP30               = doc["useSGP30"]                             | true;
        config.useMAX30               = doc["useMAX30"]                             | true;
        config.useMLX                 = doc["useMLX"]                               | true;
        config.useBME680              = doc["useBME680"]                            | true;
        config.useCCS811              = doc["useCCS811"]                            | true;
        config.useBME280              = doc["useBME280"]                            | false;
        config.avgP                   = doc["avgP"]                                 | 90000.0;
        
        config.useHTTP                = doc["useHTTP"]                              | true;
        config.useOTA                 = doc["useOTA"]                               | false;
        config.usemDNS                = doc["usemDNS"]                              | true;
        config.useHTTPUpdater         = doc["useHTTPUpdater"]                       | false;
        config.useTelnet              = doc["useTelnet"]                            | false;
        config.useSerial              = doc["useSerial"]                            | true;
        
        config.useNTP                 = doc["useNTP"]                               | true;
        strlcpy(config.ntpServer,       doc["ntpServer"]                            | "us.pool.ntp.org", sizeof(config.ntpServer));
        strlcpy(config.timeZone,        doc["timeZone"]                             | "MST7", sizeof(config.timeZone));
        config.rebootMinute           = doc["rebootMinute"]                         | -1;
      
        cfgFile.close();   // Close the file 
      } else { printSerialTelnet(F("Config file does not exist")); }
    }
  }
}
**/