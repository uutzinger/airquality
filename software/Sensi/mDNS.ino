/******************************************************************************************************/
// mDNS multicast Domain Name Service
/******************************************************************************************************/
#include "src/Wifi.h"
#include "src/Sensi.h"
#include "src/Config.h"

// External variables
extern unsigned long yieldTime;        // Sensi
extern unsigned long lastYield;        // Sensi
extern Settings      mySettings;       // Config
extern unsigned long currentTime;      // Sensi
extern char          tmpStr[256];      // Sensi
extern char          hostName[16];     // WiFi

extern unsigned long AllmaxUpdatemDNS; // Sensi
extern volatile WiFiStates    stateMDNS;        // WiFi
extern unsigned long lastMDNS;         // WiFi
extern unsigned long mDNS_lastError;   // WiFi
extern bool          mdns_avail;       // WiFi

/******************************************************************************************************/
// Initialize mDNS
/******************************************************************************************************/

void initializeMDNS() {
  D_printSerialTelnet(F("D:U:MDNS:IN.."));
  delay(50); lastYield = millis();
}

/******************************************************************************************************/
// Update mDNS
/******************************************************************************************************/

void updateMDNS() {

  switch(stateMDNS) {
    
    case IS_WAITING : { //---------------------
      // just wait...
      if ((currentTime - lastMDNS) >= intervalWiFi) {
        D_printSerialTelnet(F("D:U:mDNS:IW.."));
        if ((mySettings.debuglevel == 3) && mySettings.usemDNS) { 
          R_printSerialTelnetLogln(F("MDNS: waiting for network to come up")); 
        }          
        lastMDNS = currentTime;
      }
      break;
    }

    case START_UP : { //---------------------
      if ((currentTime - lastMDNS) >= intervalWiFi) {
        D_printSerialTelnet(F("D:U:mDNS:S.."));
        lastMDNS = currentTime;
        // mDNS responder setup
        // by default OTA service is starting mDNS, if OTA is enabled we need to skip begin and update
        if (mySettings.useOTA == false) {
          D_printSerialTelnet(F("D:S:mDNS:HN.."));
          if (MDNS.begin(hostName) ) {
            if (mySettings.debuglevel == 3) { R_printSerialTelnetLogln(F("MDNS: started")); }
          } else {
            stateMDNS = WF_ERROR;
            mDNS_lastError = currentTime;
            if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("MDNS: could not start")); }
            break;
          } 
        }

        D_printSerialTelnet(F("D:S:mDNS:R.."));
        if ( MDNS.isRunning() ) {
          AllmaxUpdatemDNS = 0;
          stateMDNS = CHECK_CONNECTION;
          // mDNS announce service for website
          if ( mySettings.useHTTP == true) {
            D_printSerialTelnet(F("D:S:mDNS:HTTP.."));
            if (MDNS.addService("http", "tcp", 80)) {
              if (mySettings.debuglevel == 3) { R_printSerialTelnetLogln(F("MDNS: http added")); }
            } else { 
              stateMDNS = WF_ERROR;
              mDNS_lastError = currentTime;
              if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("MDNS: could not add service for tcp 80")); } 
            }
            D_printSerialTelnet(F("D:S:mDNS:WS.."));
            if (MDNS.addService("ws", "tcp", 81)) {
              if (mySettings.debuglevel == 3) { R_printSerialTelnetLogln(F("MDNS: ws added")); }
            } else {
              stateMDNS = WF_ERROR;
              mDNS_lastError = currentTime;
              if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("MDNS: could not add service for ws 81")); } 
            }
          }
        } else {
          stateMDNS = WF_ERROR;
          mDNS_lastError = currentTime;
          if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("MDNS: error, not running")); }
        }

      } // end time interval
      break;
    } // end startup

    case CHECK_CONNECTION : { //---------------------
      D_printSerialTelnet(F("D:S:mDNS:CC.."));
      // Update MDNS but only if OTA is not enabled, OTA update also updates mDNS by default 
      if (mySettings.useOTA == false) { MDNS.update(); }
      break;
    }

    case WF_ERROR : { //---------------------
      D_printSerialTelnet(F("D:S:mDNS:ERR.."));
      mdns_avail =  false;
      if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("MDNS: no longer available")); }
      break;          
    }

    default: { // ----------------------------
      if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("MDNS: wrong switch statement")); }
      break;
    }

  } // switch
} // update mDNS
