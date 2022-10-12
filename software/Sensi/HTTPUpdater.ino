/******************************************************************************************************/
// HTTP Updae Server / Upload Code
/******************************************************************************************************/
#include "src/WiFi.h"
#include "src/HTTPUpdater.h"
#include "src/Sensi.h"
#include "src/Config.h"

// #define intervalHTTPUpdater 1000
//
const char*         update_path     = "/firmware";
const char*         update_username = "admin";
const char*         update_password = "w1ldc8ts";
unsigned long       lastHTTPUpdater;                   // last time we checked for http update requests
volatile WiFiStates stateHTTPUpdater = IS_WAITING;     // keeping track of webserver state

ESP8266WebServer httpUpdateServer(8890);
ESP8266HTTPUpdateServer httpUpdater;                       // Code update server

// External Variables
extern unsigned long yieldTime;        // Sensi
extern unsigned long lastYield;        // Sensi
extern Settings      mySettings;       // Config
extern unsigned long currentTime;      // Sensi
extern char          tmpStr[256];           // Sensi
extern unsigned long maxUpdateHTTPUPDATER; // Sensi

void initializeHTTPUpdater() {
  D_printSerialTelnet(F("D:U:HTTPUpdater:IN.."));
  delay(50); lastYield = millis();
}

void updateHTTPUpdater() {
  // Operation:
  // Wait for WiFi to come up
  // Setup and announce mDNS 
  // Handle connections
  
  switch(stateHTTPUpdater) {
    
    case IS_WAITING : { //---------------------
      // just wait...
      if ((currentTime - lastHTTPUpdater) >= intervalWiFi) {
        D_printSerialTelnet(F("D:U:HTTPUPDATER:IW.."));
        lastHTTPUpdater = currentTime;
        if (mySettings.debuglevel  == 3) { R_printSerialTelnetLogln(F("HTTP Update Server: is waiting for network to come up")); }          
      }
       break;
    }
    
    case START_UP : { //---------------------
      if ((currentTime - lastHTTPUpdater) >= intervalWiFi) {        
        D_printSerialTelnet(F("D:U:HTTPUPDATER:S.."));
        lastHTTPUpdater = currentTime;
        httpUpdater.setup(&httpUpdateServer, update_path, update_username, update_password);
        httpUpdateServer.begin();                              // Start server
        if (!MDNS.addService("http", "tcp", 8090)) {
          if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("MDNS: could not add service for tcp 8090")); }
        }
        if (mySettings.debuglevel  > 0) { R_printSerialTelnetLogln(F("HTTP Update Server: initialized")); }
        stateHTTPUpdater = CHECK_CONNECTION;
        maxUpdateHTTPUPDATER = 0;
      }
      break;
    }
    
    case CHECK_CONNECTION : { //---------------------
      //if ((currentTime - lastHTTPUpdater) >= intervalHTTPUpdater) {
      //  lastHTTPUpdater = currentTime;
      D_printSerialTelnet(F("D:U:HTTPUPDATER:CC.."));
      httpUpdateServer.handleClient();
      //}
      break;          
    }

    default: {if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("HTTP Updater Error: invalid switch statement")); break;}}
    
  } // end switch state
} // http
