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
    if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("WiFi: is not available")); }
  } else {
    wifi_avail = true;
    if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("WiFi: is available"));
      uint8_t mac[6]; 
      WiFi.macAddress(mac);
      sprintf_P(tmpStr, PSTR("WiFi MAC: %02X:%02X:%02X:%02X:%02X:%02X"), mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]); 
      R_printSerialTelnetLogln(tmpStr); 
    }
    if (WiFi.status() == WL_CONNECTED) { WiFi.disconnect(); } // make sure we are not connected
    if (mySettings.useWiFi == true) {
      #if defined(SHORTHOSTNAME)
      sprintf(hostName, mySettings.mqtt_mainTopic); // use the mqtt main topic as hostname
      #else
      sprintf(hostName, "esp8266-%06x", ESP.getChipId()); // create unique host name
      #endif
      WiFi.hostname(hostName);
      WiFi.mode(WIFI_STA);
      stateWiFi = START_UP;
      stateMQTT = IS_WAITING;
      stateMDNS = IS_WAITING;
      stateOTA  = IS_WAITING;
      stateNTP  = IS_WAITING;
      if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("WiFi: mode set to client")); }
    } else {
      wifi_avail = false;
      stateWiFi = IS_WAITING;
      WiFi.mode(WIFI_OFF);
      if (mySettings.debuglevel > 0) {R_printSerialTelnetLogln(F("WiFi: turned off")); }
    }
    stateMQTT = IS_WAITING;
    stateMDNS = IS_WAITING;
    stateOTA  = IS_WAITING;
    stateNTP  = IS_WAITING;
    delay(50); lastYield = millis();
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
      D_printSerialTelnet(F("D:U:WiFi:SU.."));
      if (mySettings.debuglevel == 3) { R_printSerialTelnetLogln(F("WiFi: setting up known APs")); }
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
        D_printSerialTelnet(F("D:U:WiFi:IC.."));
        if (wifiMulti.run() == WL_CONNECTED) {
          if (mySettings.debuglevel == 3) { 
            R_printSerialTelnetLog(F("WiFi: connected to: ")); R_printSerialTelnetLogln(WiFi.SSID());
            IPAddress lip = WiFi.localIP();
            sprintf_P(tmpStr, PSTR("Local IP: %d.%d.%d.%d"), lip[0], lip[1], lip[2], lip[3]); 
            R_printSerialTelnetLogln(tmpStr); 
          }
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
          if (mySettings.debuglevel == 3) { R_printSerialTelnetLogln(F("WiFi: is searching for known APs")); }
          wifi_connected = false;
        }
        lastWiFi = currentTime;
      }

      break;        
    } // end is connecting
    
    case CHECK_CONNECTION : { //---------------------
      if ((currentTime - lastWiFi) >= intervalWiFi) {
        D_printSerialTelnet(F("D:U:WiFi:CC.."));
        if (wifiMulti.run(connectTimeOut) != WL_CONNECTED) { // if connected returns status, does not seem to need frequent calls
          if (mySettings.debuglevel  > 0) { R_printSerialTelnetLogln(F("WiFi: is searching for known AP")); }
          wifi_connected = false;
          AllmaxUpdateWifi = 0;
        } else {
          if (mySettings.debuglevel  == 3) { 
            IPAddress ip = WiFi.localIP(); // uint8_t first_octet, uint8_t second_octet, uint8_t third_octet, uint8_t fourth_octet
            R_printSerialTelnetLog(F("WiFi: remains connected to: ")); 
            printSerialTelnetLog(WiFi.SSID());
            sprintf_P(tmpStr, PSTR(" with IP address %hu.%hu.%hu.%hu\r\n"), ip[0], ip[1], ip[2], ip[3]); 
            printSerialTelnetLogln(tmpStr); 
          }
          wifi_connected = true;
        }
        lastWiFi = currentTime;
      }
      break;
    }

    default: {if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("WiFi Error: invalid switch statement")); break;}}
  
  } // end switch case
} // update wifi
