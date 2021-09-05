/******************************************************************************************************/
// MDNS
/******************************************************************************************************/
#include "VSC.h"
#ifdef EDITVSC
#include "src/Wifi.h"
#include "src/Sensi.h"
#include "src/Config.h"
#endif

void updateMDNS() {

  switch(stateMDNS) {
    
    case IS_WAITING : { //---------------------
      // just wait...
      if ((currentTime - lastMDNS) >= intervalWiFi) {
        if ((mySettings.debuglevel == 3) && mySettings.usemDNS) { printSerialTelnet(F("MDNS: waiting for network to come up\r\n")); }          
        lastMDNS = currentTime;
      }
      break;
    }

    case START_UP : { //---------------------
      if ((currentTime - lastMDNS) >= intervalWiFi) {
        lastMDNS = currentTime;
        #if defined(DEBUG)
        printSerialTelnet(F("DBG:STARTUP: MDNS\r\n"));
        #endif                          
        // mDNS responder setup
        // by default OTA service is starting mDNS, if OTA is enabled we need to skip begin and update
        bool mDNSsuccess = true;
        if (!mySettings.useOTA) {
          #if defined(DEBUG)   
          printSerialTelnet(F("DBG:STARTUP: MDNS HOSTNAME\r\n"));
          #endif
          mDNSsuccess = MDNS.begin(hostName);
          if (mDNSsuccess == false) { if (mySettings.debuglevel > 0) { sprintf(tmpStr, "MDNS: error setting up MDNS responder\r\n"); printSerialTelnet(tmpStr); } }
        } else {
          #if defined(DEBUG)
          printSerialTelnet(F("DBG:STARTUP: MDNS IS RUNNING\r\n"));
          #endif
          if (MDNS.isRunning()) { // need to wait until OTA startedup mDNS otherwise addService crashes
            mDNSsuccess = true;
          } else {
            mDNSsuccess = false;
          }
        }

        if (mDNSsuccess) {
          stateMDNS = CHECK_CONNECTION;
          AllmaxUpdatemDNS = 0;
        
          // mDNS announce service for website
          if (bool(mySettings.useHTTP)) {
            #if defined(DEBUG)
            printSerialTelnet(F("DBG:STARTUP: MDNS HTTP&WS ANNOUNCE\r\n"));
            #endif
            if (!MDNS.addService("http", "tcp", 80)) {
              if (mySettings.debuglevel > 0) { printSerialTelnet(F("MDNS: could not add service for tcp 80\r\n")); }
            }
            if (!MDNS.addService("ws", "tcp", 81)) {
              if (mySettings.debuglevel > 0) { printSerialTelnet(F("MDNS: could not add service for ws 81\r\n")); }
            }
          }

          if (mySettings.debuglevel > 0) { printSerialTelnet(F("MDNS: initialized\r\n")); }
          
          // query mDNS on network
          /*
          int n = MDNS.queryService("esp", "tcp"); // Send out query for esp tcp services
          if (mySettings.debuglevel > 0) { Serial.println(F("MDNS: query completed")); }
          if (n == 0) {
            if (mySettings.debuglevel > 0) { Serial.println(F("MDNS: no services found")); }
          } else {
            if (mySettings.debuglevel > 0) {
              Serial.println(F("MDNS: service(s) found"));
              for (int i = 0; i < n; ++i) {
                // Print details for each service found
                Serial.print(F("MDNS: ")); Serial.print(i + 1); Serial.print(F(": ")); Serial.print(MDNS.hostname(i)); Serial.print(F(" (")); Serial.print(MDNS.IP(i)); Serial.print(F(":"));  Serial.print(MDNS.port(i)); Serial.println(F(")"));
              } // for all mDNS services
            } 
          } // end found mDNS announcements
          */
        } // end mDNS
        #if defined(DEBUG)
        printSerialTelnet(F("DBG:STARTUP: MDNS END\r\n"));
        #endif  
      } // end time interval
      break;
    } // end startup

    case CHECK_CONNECTION : { //---------------------
      if (mySettings.useOTA == false) { MDNS.update(); }       // Update MDNS but only if OTA is not enabled, OTA uses mDNS by default
      break;          
    }
    
  } // switch
} // update mDNS
