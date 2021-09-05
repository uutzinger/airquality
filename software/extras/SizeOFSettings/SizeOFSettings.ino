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
  // int           utcOffset;                                 // time offset from GMT in hours 
  // bool          enableDST;                                 // enable day light sving time
  char          timeZone[41];                              // The time zone according to second row in https://raw.githubusercontent.com/nayarsystems/posix_tz_db/master/zones.csv 
  bool          useHTTPUpdater;                            // porivide over the air programming
};

Settings mySettings;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  const char waitmsg[] = "Waiting 10 seconds, skip by hitting enter";      // Allows user to open serial terminal to observe the debug output
  serialTrigger((char *) waitmsg, 10000);

  Serial.print(F("Size of settings structure is: "));;
  Serial.println(sizeof(mySettings));
}

void loop() {
  // put your main code here, to run repeatedly:

}

// Boot helper
// Prints message and waits until timeout or user send character on serial terminal
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
