//******************************************************************************************************//
// Network Time Protocol
//******************************************************************************************************//
#include "VSC.h"
#ifdef EDITVSC
#include "src/NTP.h"
#include "src/Config.h"
#include "src/WiFi.h"
#include "src/Sensi.h"
#endif

// https://tttapa.github.io/ESP8266/Chap15%20-%20NTP.html
void updateNTP() {

  switch(stateNTP) {
    
    case IS_WAITING : { //---------------------
      if ((currentTime - lastNTP) >= intervalWiFi) {
        lastNTP = currentTime;
        if ((mySettings.debuglevel == 3) && mySettings.useNTP) { printSerialTelnet(F("NTP: is waiting for network to come up\r\n")); }          
      }
      break;
    }
    
    case START_UP : { //---------------------

      D_printSerialTelnet("DBG:STARTUP: NTP\r\n");
      if (mySettings.debuglevel == 3) { sprintf_P(tmpStr, PSTR("NTP: connecting to %s\r\n"), mySettings.ntpServer); printSerialTelnet(tmpStr); }

      NTP.onNTPSyncEvent ([] (NTPEvent_t event) {
          ntpEvent = event;
          syncEventTriggered = true;
      });

      ntpSetupError = false;
      // setting up NTP client
      NTP.setTimeZone(mySettings.timeZone);
      NTP.setMinSyncAccuracy(NTP_MIN_SYNC_ACCURACY_US);
      NTP.settimeSyncThreshold(TIME_SYNC_THRESHOLD);
      NTP.setMaxNumSyncRetry(NTP_MAX_RESYNC_RETRY);
      NTP.setnumAveRounds(NTP_NUM_OFFSET_AVE_ROUNDS);
      if ( NTP.setInterval(NTP_INTERVAL) && NTP.setNTPTimeout(NTP_TIMEOUT) ) {
        if (NTP.begin (mySettings.ntpServer)) {
          stateNTP = CHECK_CONNECTION; // move on
          if (mySettings.debuglevel  > 0) { printSerialTelnet("NTP: client initialized\r\n"); }
        } else { if (mySettings.debuglevel  > 0) { printSerialTelnet(F("NTP: could not start client\r\n")); } }
      } else { if (mySettings.debuglevel  > 0) { printSerialTelnet(F("NTP: client setup error\r\n")); } }
      break;
    }
    
    case CHECK_CONNECTION : { //---------------------

      if (syncEventTriggered==true) {
          syncEventTriggered = false;
          processSyncEvent (ntpEvent);
      }
      break;          
    }

    default: {if (mySettings.debuglevel > 0) { printSerialTelnet(F("NTP Error: invalid switch statement")); break;}}

  } // end switch state
} // ntp


void processSyncEvent (NTPEvent_t ntpEvent) {
    switch (ntpEvent.event) {
        case timeSyncd:
          // got NTP time
          ntp_avail = true;
          timeSynced = true;
          if (mySettings.debuglevel  == 3) { sprintf_P(tmpStr, PSTR("NTP: %s\r\n"), NTP.ntpEvent2str(ntpEvent)); printSerialTelnet(tmpStr); }
          break;
        case partlySync:
          // partial sync
          ntp_avail = true;
          timeSynced = true;
          if (mySettings.debuglevel  == 3) { sprintf_P(tmpStr, PSTR("NTP: %s\r\n"), NTP.ntpEvent2str(ntpEvent)); printSerialTelnet(tmpStr); }
          break;
        case syncNotNeeded:
          // sync not needed
          ntp_avail = true;
          timeSynced = true;
          if (mySettings.debuglevel  == 3) { sprintf_P(tmpStr, PSTR("NTP: %s\r\n"), NTP.ntpEvent2str(ntpEvent)); printSerialTelnet(tmpStr); }
          break;
        case accuracyError:
          // accuracy error
          ntp_avail = true;
          timeSynced = false;
          if (mySettings.debuglevel   > 0) { sprintf_P(tmpStr, PSTR("NTP: %s\r\n"), NTP.ntpEvent2str(ntpEvent)); printSerialTelnet(tmpStr); }
          break;
        case noResponse:
          // no response from NTP server
          ntp_avail = false;
          if (mySettings.debuglevel   > 0) { sprintf_P(tmpStr, PSTR("NTP: %s\r\n"), NTP.ntpEvent2str(ntpEvent)); printSerialTelnet(tmpStr); }
          break;
        case invalidAddress:
          // invalid server address
          ntp_avail = false;
          if (mySettings.debuglevel   > 0) { sprintf_P(tmpStr, PSTR("NTP: %s\r\n"), NTP.ntpEvent2str(ntpEvent)); printSerialTelnet(tmpStr); }
          break;
        case invalidPort:
          // invalid port
          ntp_avail = false;
          if (mySettings.debuglevel   > 0) { sprintf_P(tmpStr, PSTR("NTP: %s\r\n"), NTP.ntpEvent2str(ntpEvent)); printSerialTelnet(tmpStr); }
          break;
        case errorSending:
          // error sending ntp request
          ntp_avail = false;
          if (mySettings.debuglevel   > 0) { sprintf_P(tmpStr, PSTR("NTP: %s\r\n"), NTP.ntpEvent2str(ntpEvent)); printSerialTelnet(tmpStr); }
          break;
        case responseError:
          // NTP response error
          ntp_avail = false;
          if (mySettings.debuglevel   > 0) { sprintf_P(tmpStr, PSTR("NTP: %s\r\n"), NTP.ntpEvent2str(ntpEvent)); printSerialTelnet(tmpStr); }
          break;
        case syncError:
          // error applyting sync
          ntp_avail = false;
          if (mySettings.debuglevel   > 0) { sprintf_P(tmpStr, PSTR("NTP: %s\r\n"), NTP.ntpEvent2str(ntpEvent)); printSerialTelnet(tmpStr); }
          break;
        case requestSent:
          // NTP request sent
          if (mySettings.debuglevel  == 3) { sprintf_P(tmpStr, PSTR("NTP: %s\r\n"), NTP.ntpEvent2str(ntpEvent)); printSerialTelnet(tmpStr); }
          break;
        default:
          // unknown error
          break;
    }
}
