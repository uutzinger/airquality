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
        D_printSerialTelnet(F("D:U:NTP:IW.."));
        lastNTP = currentTime;
        if ((mySettings.debuglevel == 3) && mySettings.useNTP) { R_printSerialTelnetLogln(F("NTP: is waiting for network to come up")); }          
      }
      break;
    }
    
    case START_UP : { //---------------------

      D_printSerialTelnet(F("D:U:NTP:S.."));
      if (mySettings.debuglevel == 3) { sprintf_P(tmpStr, PSTR("NTP: connecting to %s"), mySettings.ntpServer); R_printSerialTelnetLogln(tmpStr); }

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
          if (mySettings.debuglevel  > 0) { R_printSerialTelnetLogln("NTP: client initialized"); }
        } else { if (mySettings.debuglevel  > 0) { R_printSerialTelnetLogln(F("NTP: could not start client")); } }
      } else { if (mySettings.debuglevel  > 0) { R_printSerialTelnetLogln(F("NTP: client setup error")); } }
      break;
    }
    
    case CHECK_CONNECTION : { //---------------------

      D_printSerialTelnet(F("D:U:NTP:CC.."));
      if (syncEventTriggered==true) {
          syncEventTriggered = false;
          processSyncEvent (ntpEvent);
      }
      break;          
    }

    default: {if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("NTP Error: invalid switch statement")); break;}}

  } // end switch state
} // end ntp

void processSyncEvent (NTPEvent_t ntpEvent) {
    switch (ntpEvent.event) {
        case timeSyncd:
          // got NTP time
          ntp_avail = true;
          timeSynced = true;
          if (mySettings.debuglevel  == 3) { sprintf_P(tmpStr, PSTR("NTP: %s"), NTP.ntpEvent2str(ntpEvent)); R_printSerialTelnetLogln(tmpStr); }
          break;
        case partlySync:
          // partial sync
          ntp_avail = true;
          timeSynced = true;
          if (mySettings.debuglevel  == 3) { sprintf_P(tmpStr, PSTR("NTP: %s"), NTP.ntpEvent2str(ntpEvent)); R_printSerialTelnetLogln(tmpStr); }
          break;
        case syncNotNeeded:
          // sync not needed
          ntp_avail = true;
          timeSynced = true;
          if (mySettings.debuglevel  == 3) { sprintf_P(tmpStr, PSTR("NTP: %s"), NTP.ntpEvent2str(ntpEvent)); R_printSerialTelnetLogln(tmpStr); }
          break;
        case accuracyError:
          // accuracy error
          ntp_avail = true;
          timeSynced = false;
          if (mySettings.debuglevel   > 0) { sprintf_P(tmpStr, PSTR("NTP: %s"), NTP.ntpEvent2str(ntpEvent)); R_printSerialTelnetLogln(tmpStr); }
          break;
        case noResponse:
          // no response from NTP server
          ntp_avail = false;
          if (mySettings.debuglevel   > 0) { sprintf_P(tmpStr, PSTR("NTP: %s"), NTP.ntpEvent2str(ntpEvent)); R_printSerialTelnetLogln(tmpStr); }
          break;
        case invalidAddress:
          // invalid server address
          ntp_avail = false;
          if (mySettings.debuglevel   > 0) { sprintf_P(tmpStr, PSTR("NTP: %s"), NTP.ntpEvent2str(ntpEvent)); R_printSerialTelnetLogln(tmpStr); }
          break;
        case invalidPort:
          // invalid port
          ntp_avail = false;
          if (mySettings.debuglevel   > 0) { sprintf_P(tmpStr, PSTR("NTP: %s"), NTP.ntpEvent2str(ntpEvent)); R_printSerialTelnetLogln(tmpStr); }
          break;
        case errorSending:
          // error sending ntp request
          ntp_avail = false;
          if (mySettings.debuglevel   > 0) { sprintf_P(tmpStr, PSTR("NTP: %s"), NTP.ntpEvent2str(ntpEvent)); R_printSerialTelnetLogln(tmpStr); }
          break;
        case responseError:
          // NTP response error
          ntp_avail = false;
          if (mySettings.debuglevel   > 0) { sprintf_P(tmpStr, PSTR("NTP: %s"), NTP.ntpEvent2str(ntpEvent)); R_printSerialTelnetLogln(tmpStr); }
          break;
        case syncError:
          // error applyting sync
          ntp_avail = false;
          if (mySettings.debuglevel   > 0) { sprintf_P(tmpStr, PSTR("NTP: %s"), NTP.ntpEvent2str(ntpEvent)); R_printSerialTelnetLogln(tmpStr); }
          break;
        case requestSent:
          // NTP request sent
          if (mySettings.debuglevel  == 3) { sprintf_P(tmpStr, PSTR("NTP: %s"), NTP.ntpEvent2str(ntpEvent)); R_printSerialTelnetLogln(tmpStr); }
          break;
        default:
          // unknown error
          break;
    }
}
