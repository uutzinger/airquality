/******************************************************************************************************/
// Network Time Protocol
/******************************************************************************************************/
#include "src/NTP.h"
#include "src/Config.h"
#include "src/WiFi.h"
#include "src/Sensi.h"
#include "src/Print.h"

unsigned long lastNTP;                                     // last time we checked network time
unsigned long ntp_lastError;
unsigned long ntp_lastSync;
unsigned int  ntp_noresponseCount = 0;                     // how many times tried to connect

bool          syncEventTriggered = false;                  // True if NTP client triggered a time event
bool          ntp_avail = true;                            // True if NTP client communicted with NTP server
bool          timeSynced = false;                          // True if successful sync occured
bool          updateDate = true;                           // Ready to broadcast date (websocket and mqtt)
bool          updateTime = true;                           // Ready to broadcast time (websocket and mqtt) 
bool          ntpSetupError = false;
bool          ntp_onfallback = false;                      // using the fall back server?

int           lastYearDay = 400;                           // keep track of date change, initialize with out of range values
int           lastMin = 61;                                // keep track of time change, intialize with out of range values

volatile WiFiStates stateNTP = IS_WAITING;                 // keeping track of network time state
NTPEvent_t    ntpEvent;                                    // Last triggered event

// Extern variables
extern Settings      mySettings;                           // Config
extern unsigned long currentTime;                          // Sensi
extern char          tmpStr[256];                          // Sensi

void onNTPeventReceived(NTPEvent_t event){
  D_printSerialTelnet(F("D:U:NTP:IN.."));
  ntpEvent = event; // store received event
  syncEventTriggered = true; // trigger event processing
}

/******************************************************************************************************/
// Initialize Network Time Protocol
/******************************************************************************************************/

void initializeNTP() {
  // does not need initialization
  D_printSerialTelnet(F("D:U:NTP:I.."));
  NTP.onNTPSyncEvent(onNTPeventReceived);
}

/******************************************************************************************************/
// Update Network Time Protocol
/******************************************************************************************************/

void updateNTP() {

  switch(stateNTP) {
    
    // WiFi is not up yet
    case IS_WAITING : { //---------------------
      if ((currentTime - lastNTP) >= intervalWiFi) {
        D_printSerialTelnet(F("D:U:NTP:IW.."));
        lastNTP = currentTime;
        if ((mySettings.debuglevel == 3) && mySettings.useNTP) { R_printSerialTelnetLogln(F("NTP: is waiting for network to come up")); }          
      }
      break;
    }
    
    case START_UP : { //---------------------
      
      D_printSerialTelnet(F("D:U:NTP:S.."));
      if (mySettings.debuglevel == 3) { 
        snprintf_P(tmpStr, sizeof(tmpStr), PSTR("NTP: connecting to %s"), mySettings.ntpServer); 
        R_printSerialTelnetLogln(tmpStr); 
      }

      // setting up NTP client
      NTP.setTimeZone(mySettings.timeZone);
      NTP.setMinSyncAccuracy(NTP_MIN_SYNC_ACCURACY_US);
      NTP.settimeSyncThreshold(TIME_SYNC_THRESHOLD);
      NTP.setMaxNumSyncRetry(NTP_MAX_RESYNC_RETRY);
      NTP.setnumAveRounds(NTP_NUM_OFFSET_AVE_ROUNDS);
      if ( NTP.setInterval(NTP_INTERVAL) && NTP.setNTPTimeout(NTP_TIMEOUT) ) {
        if (NTP.begin(mySettings.ntpServer)) {
          stateNTP = CHECK_CONNECTION;
          ntp_onfallback = false;
          if (mySettings.debuglevel  > 0) { R_printSerialTelnetLogln(F("NTP: client initialized")); }
        } else { 
          if (mySettings.debuglevel  > 0) { R_printSerialTelnetLogln(F("NTP: could not start NTP, check server name")); } 
          stateNTP = WF_ERROR;
        }
      } else { 
        if (mySettings.debuglevel  > 0) { R_printSerialTelnetLogln(F("NTP: error setup client, check interval and timeout")); } 
        stateNTP = WF_ERROR;
      }
      ntp_avail = true;
      break;
    }
    
    case CHECK_CONNECTION : { //---------------------

      D_printSerialTelnet(F("D:U:NTP:CC.."));
      if (syncEventTriggered==true) {
          syncEventTriggered = false;
          processSyncEvent(ntpEvent);
      }
      break;          
    }

    case WF_ERROR : {
      // startup failed
      D_printSerialTelnet(F("D:S:NTP:ERR.."));
      ntp_avail = false;
      if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("NTP: no longer available")); }
      break;          
    }

    default: {
      if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("NTP Error: invalid switch statement")); }
      break;
    }

  } // end switch state
} // end ntp

void processSyncEvent(NTPEvent_t ntpEvent) {

    switch (ntpEvent.event) {
    
        case timeSyncd:
          // got NTP time
          timeSynced = true;
          ntp_lastSync = currentTime;
          if (mySettings.debuglevel  == 3) { 
            snprintf_P(tmpStr, sizeof(tmpStr), PSTR("NTP: %s"), NTP.ntpEvent2str(ntpEvent)); 
            R_printSerialTelnetLogln(tmpStr); 
          }
          ntp_noresponseCount = 0;
          break;
        
        case partlySync:
          // partial sync
          timeSynced = true;
          ntp_lastSync = currentTime;
          if (mySettings.debuglevel  == 3) { 
            snprintf_P(tmpStr, sizeof(tmpStr), PSTR("NTP: %s"), NTP.ntpEvent2str(ntpEvent)); 
            R_printSerialTelnetLogln(tmpStr); 
          }
          ntp_noresponseCount = 0;
          break;
        
        case syncNotNeeded:
          // sync not needed
          timeSynced = true;
          ntp_lastSync = currentTime;
          if (mySettings.debuglevel  == 3) { 
            snprintf_P(tmpStr, sizeof(tmpStr), PSTR("NTP: %s"), NTP.ntpEvent2str(ntpEvent)); 
            R_printSerialTelnetLogln(tmpStr); 
          }
          ntp_noresponseCount = 0;
          break;
        
        case accuracyError:
          // accuracy error
          timeSynced = false;
          ntp_lastError = currentTime;
          if (mySettings.debuglevel   > 0) { 
            snprintf_P(tmpStr, sizeof(tmpStr), PSTR("NTP: %s"), NTP.ntpEvent2str(ntpEvent)); 
            R_printSerialTelnetLogln(tmpStr); 
          }
          ntp_noresponseCount = 0;
          break;
        
        case noResponse:
          // no response from NTP server
          timeSynced = false;
          ntp_lastError = currentTime;
          if (mySettings.debuglevel   == 3) { 
            snprintf_P(tmpStr, sizeof(tmpStr), PSTR("NTP: %s"), NTP.ntpEvent2str(ntpEvent)); 
            R_printSerialTelnetLogln(tmpStr); 
          }
          if (ntp_noresponseCount++ > 3) { 
            if (ntp_onfallback) {
              NTP.begin (mySettings.ntpServer); // try again with default pool 
              ntp_onfallback = false;
            } else {
              NTP.begin (mySettings.ntpFallback); // try the fallback server
              ntp_onfallback = true;
            }
            ntp_noresponseCount = 0;           
          }
          break;
        
        case invalidAddress:
          // invalid server address
          timeSynced = false;
          ntp_lastError = currentTime;
          if (mySettings.debuglevel   == 3) { 
            snprintf_P(tmpStr, sizeof(tmpStr), PSTR("NTP: %s"), NTP.ntpEvent2str(ntpEvent)); 
            R_printSerialTelnetLogln(tmpStr); 
          }
          break;
        case invalidPort:
          // invalid port
          timeSynced = false;
          ntp_lastError = currentTime;
          if (mySettings.debuglevel   == 3) { 
            snprintf_P(tmpStr, sizeof(tmpStr), PSTR("NTP: %s"), NTP.ntpEvent2str(ntpEvent));
            R_printSerialTelnetLogln(tmpStr); 
          }
          break;
        
        case errorSending:
          // error sending ntp request
          timeSynced = false;
          ntp_lastError = currentTime;
          if (mySettings.debuglevel   > 0) { 
            snprintf_P(tmpStr, sizeof(tmpStr), PSTR("NTP: %s"), NTP.ntpEvent2str(ntpEvent)); 
            R_printSerialTelnetLogln(tmpStr); 
          }
          break;
        
        case responseError:
          // NTP response error
          timeSynced = false;
          ntp_lastError = currentTime;
          if (mySettings.debuglevel   > 0) {
            snprintf_P(tmpStr, sizeof(tmpStr), PSTR("NTP: %s"), NTP.ntpEvent2str(ntpEvent)); 
            R_printSerialTelnetLogln(tmpStr); 
          }
          break;
        
        case syncError:
          // error applyting sync
          timeSynced = false;
          ntp_lastError = currentTime;
          if (mySettings.debuglevel   > 0) { 
            snprintf_P(tmpStr, sizeof(tmpStr), PSTR("NTP: %s"), NTP.ntpEvent2str(ntpEvent)); 
            R_printSerialTelnetLogln(tmpStr); 
          }
          break;
        
        case requestSent:
          // NTP request sent
          if (mySettings.debuglevel  == 3) { 
            snprintf_P(tmpStr, sizeof(tmpStr), PSTR("NTP: %s"), NTP.ntpEvent2str(ntpEvent)); 
            R_printSerialTelnetLogln(tmpStr); 
          }
          break;
          
        default:
          if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("NTP Error: invalid switch statement")); }
          break;
    }
}
