/******************************************************************************************************/
// Initialize WiFi
// https://arduino-esp8266.readthedocs.io/en/latest/esp8266wifi/
/******************************************************************************************************/
#include "VSC.h"
#ifdef EDITVSC
#include "src/WiFi.h"
#include "src/MQTT.h"
#include "src/NTP.h"
#include "src/HTTP.h"
#include "src/HTTPUpdater.h"
#include "src/Config.h"
#include "src/Sensi.h"
#include "src/WebSocket.h"
#endif

void initializeWiFi() {    
  if (WiFi.status() == WL_NO_SHIELD) {
    wifi_avail = false;
    stateWiFi = IS_WAITING;
    stateMQTT = IS_WAITING;
    stateMDNS = IS_WAITING;
    stateOTA  = IS_WAITING;
    stateNTP  = IS_WAITING;
    if (mySettings.debuglevel > 0) { Serial.println(F("WiFi: is not available")); }
  } else {
    wifi_avail = true;
    if (mySettings.debuglevel > 0) { Serial.println(F("WiFi: is available")); }
    if (mySettings.debuglevel > 0) { Serial.print("Wifi: MAC: "); Serial.println(WiFi.macAddress()); }
    if (WiFi.status() == WL_CONNECTED) { WiFi.disconnect(); } // make sure we are not connected
    if (mySettings.useWiFi == true) {
      sprintf(hostName, "esp8266-%06x", ESP.getChipId());
      WiFi.hostname(hostName);
      WiFi.mode(WIFI_STA);
      stateWiFi = START_UP;
      stateMQTT = IS_WAITING;
      stateMDNS = IS_WAITING;
      stateOTA  = IS_WAITING;
      stateNTP  = IS_WAITING;
      if (mySettings.debuglevel > 0) { Serial.println(F("WiFi: mode set to client")); }
    } else {
      wifi_avail = false;
      stateWiFi = IS_WAITING;
      WiFi.mode(WIFI_OFF);
      if (mySettings.debuglevel > 0) { Serial.println(F("WiFi: turned off")); }
    }
    stateMQTT = IS_WAITING;
    stateMDNS = IS_WAITING;
    stateOTA  = IS_WAITING;
    stateNTP  = IS_WAITING;
    delay(50);
  }
} // init wifi

/******************************************************************************************************/
// Update WiFi
/******************************************************************************************************/
void updateWiFi() {  
  
  switch(stateWiFi) {
    
    case IS_WAITING : { //---------------------
      break;
    }

    case START_UP : { //---------------------
      if (mySettings.debuglevel == 3) { Serial.println(F("WiFi: setting up known APs")); }
      if (strlen(mySettings.pw1) > 0) { wifiMulti.addAP(mySettings.ssid1, mySettings.pw1); } else { wifiMulti.addAP(mySettings.ssid1); }
      if (strlen(mySettings.pw2) > 0) { wifiMulti.addAP(mySettings.ssid2, mySettings.pw2); } else { wifiMulti.addAP(mySettings.ssid2); }
      if (strlen(mySettings.pw3) > 0) { wifiMulti.addAP(mySettings.ssid3, mySettings.pw3); } else { wifiMulti.addAP(mySettings.ssid3); }
      stateWiFi        = IS_CONNECTING;
      stateMQTT        = IS_WAITING;   
      stateMDNS        = IS_WAITING;     
      stateOTA         = IS_WAITING;     
      stateNTP         = IS_WAITING;
      stateHTTP        = IS_WAITING;
      stateHTTPUpdater = IS_WAITING;
      stateTelnet      = IS_WAITING;
      lastWiFi = currentTime;      
      break;
    }
    
    case IS_CONNECTING : { //---------------------
      if ((currentTime - lastWiFi) >= intervalWiFi) {
        if (wifiMulti.run() == WL_CONNECTED) {
          if (mySettings.debuglevel == 3) { Serial.print(F("WiFi: connected to: ")); Serial.print(WiFi.SSID()); }
          if (mySettings.debuglevel == 3) { Serial.print(F(" at IP address: "));     Serial.println(WiFi.localIP()); }
          wifi_connected = true;
          stateWiFi        = CHECK_CONNECTION;
          stateMQTT        = START_UP;
          stateOTA         = START_UP;
          stateNTP         = START_UP;
          stateHTTP        = START_UP;
          stateHTTPUpdater = START_UP;
          stateMDNS        = START_UP;
          stateWebSocket   = START_UP;
          stateTelnet      = START_UP;
          randomSeed(micros()); // init random generator (time until connect is random)
          AllmaxUpdateWifi = 0;
        } else {
          if (mySettings.debuglevel == 3) { Serial.println(F("WiFi: is searching for known AP")); }
          wifi_connected = false;
        }
        lastWiFi = currentTime;
      }

      break;        
    } // end is connecting
    
    case CHECK_CONNECTION : { //---------------------
      if ((currentTime - lastWiFi) >= intervalWiFi) {
        if (wifiMulti.run(connectTimeOut) != WL_CONNECTED) { // if connected returns status, does not seem to need frequent calls
          if (mySettings.debuglevel  > 0) { Serial.println(F("WiFi: is searching for known AP")); }
          wifi_connected = false;
          AllmaxUpdateWifi = 0;
        } else {
          if (mySettings.debuglevel  == 3) { 
            IPAddress ip = WiFi.localIP(); // uint8_t first_octet, uint8_t second_octet, uint8_t third_octet, uint8_t fourth_octet
            printSerialTelnet(F("WiFi: remains connected to: ")); 
            printSerialTelnet(WiFi.SSID());
            sprintf(tmpStr, " with IP address %hu.%hu.%hu.%hu\r\n", ip[0], ip[1], ip[2], ip[3]); 
            printSerialTelnet(tmpStr); 
          }
          wifi_connected = true;
        }
        lastWiFi = currentTime;
      }
      break;
    }
  
  } // end switch case
} // update wifi
