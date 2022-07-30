/******************************************************************************************************/
// HTTP Updae Server / Upload Code
/******************************************************************************************************/
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
