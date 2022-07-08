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
        if (!mySettings.useOTA) {
          D_printSerialTelnet(F("D:S:mDNS:HN.."));
          if (MDNS.begin(hostName) == false) { 
            if (mySettings.debuglevel > 0) { 
              snprintf_P(tmpStr, sizeof(tmpStr), PSTR("MDNS: error setting up MDNS responder")); 
              R_printSerialTelnetLogln(tmpStr);
            } 
          }
        }

        D_printSerialTelnet(F("D:S:mDNS:R.."));
        if ( MDNS.isRunning() ) {
          AllmaxUpdatemDNS = 0;
          stateMDNS = CHECK_CONNECTION;
          // mDNS announce service for website
          if ( mySettings.useHTTP == true) {
            D_printSerialTelnet(F("D:S:mDNS:HTTP.."));
            if (!MDNS.addService("http", "tcp", 80)) { 
              if (mySettings.debuglevel > 0) { 
                R_printSerialTelnetLogln(F("MDNS: could not add service for tcp 80")); 
                } 
              }
            D_printSerialTelnet(F("D:S:mDNS:WS.."));
            if (!MDNS.addService("ws", "tcp", 81)) { 
              if (mySettings.debuglevel > 0) { 
                R_printSerialTelnetLogln(F("MDNS: could not add service for ws 81")); 
              }
            }
          }

          if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("MDNS: initialized")); }
          
          // query mDNS on network
          /*
          int n = MDNS.queryService("esp", "tcp"); // Send out query for esp tcp services
          if (mySettings.debuglevel > 0) { R_printSerialTelnetLog(F("MDNS: query completed")); }
          if (n == 0) {
            if (mySettings.debuglevel > 0) { Serial.println(F("MDNS: no services found")); }
          } else {
            if (mySettings.debuglevel > 0) {
              Serial.println(F("MDNS: service(s) found"));
              for (int i = 0; i < n; ++i) {
                // Print details for each service found
                printSerialTelnetLog(F("MDNS: ")); 
                printSerialTelnetLog(i + 1); 
                printSerialTelnetLog(F(": ")); 
                printSerialTelnetLog(MDNS.hostname(i)); 
                printSerialTelnetLog(F(" (")); 
                printSerialTelnetLog(MDNS.IP(i)); 
                printSerialTelnetLog(F(":"));  
                printSerialTelnetLog(MDNS.port(i)); 
                printSerialTelnetLogln(F(")"));
              } // for all mDNS services
            } 
          } // end found mDNS announcements
          */
        } // end mDNS
      } // end time interval
      break;
    } // end startup

    case CHECK_CONNECTION : { //---------------------
      D_printSerialTelnet(F("D:S:mDNS:CC.."));
      if (mySettings.useOTA == false) { // Update MDNS but only if OTA is not enabled, OTA update also updates mDNS by default 
        MDNS.update(); 
      }
      break;          
    }

    default: {
      if (mySettings.debuglevel > 0) { 
        R_printSerialTelnetLogln(F("mDNS Error: invalid switch statement")); 
      break;
      }
    }

  } // switch
} // update mDNS
