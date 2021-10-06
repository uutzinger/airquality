/******************************************************************************************************/
// HTTP Updae Server / Upload Code
/******************************************************************************************************/
// https://www.hackster.io/s-wilson/nodemcu-home-weather-station-with-websocket-7c77a3
#include "VSC.h"
#ifdef EDITVSC
#include "src/WiFi.h"
#include "src/HTTPUpdater.h"
#endif

void updateHTTPUpdater() {
  // Operation:

  switch(stateHTTPUpdater) {
    
    case IS_WAITING : { //---------------------
      // just wait...
      if ((currentTime - lastHTTPUpdater) >= intervalWiFi) {
        lastHTTPUpdater = currentTime;
        if (mySettings.debuglevel  == 3) { printSerialTelnet(F("HTTP Update Server: is waiting for network to come up\r\n")); }          
      }
       break;
    }
    
    case START_UP : { //---------------------
      if ((currentTime - lastHTTPUpdater) >= intervalWiFi) {        
        lastHTTPUpdater = currentTime;
        D_printSerialTelnet(F("DBG:STARTUP: HTTP UPDATER"));
        httpUpdater.setup(&httpUpdateServer, update_path, update_username, update_password);
        httpUpdateServer.begin();                              // Start server
        if (!MDNS.addService("http", "tcp", 8090)) {
          if (mySettings.debuglevel > 0) { printSerialTelnet(F("MDNS: could not add service for tcp 8090\r\n")); }
        }
        if (mySettings.debuglevel  > 0) { printSerialTelnet(F("HTTP Update Server: initialized\r\n")); }
        stateHTTPUpdater = CHECK_CONNECTION;
        maxUpdateHTTPUPDATER = 0;
      }
      break;
    }
    
    case CHECK_CONNECTION : { //---------------------
      //if ((currentTime - lastHTTPUpdater) >= intervalHTTPUpdater) {
      //  lastHTTPUpdater = currentTime;
      httpUpdateServer.handleClient();
      //}
      break;          
    }

    default: {if (mySettings.debuglevel > 0) { printSerialTelnet(F("HTTP Updater Error: invalid switch statement")); break;}}
    
          
  } // end switch state
} // http
