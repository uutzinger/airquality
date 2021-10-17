/******************************************************************************************************/
// Telnet
/******************************************************************************************************/
#include "VSC.h"
#ifdef EDITVSC
#include "src/Config.h"
#include "src/Sensi.h"
#endif

void printSerialTelnet(char* str) {
  char buf[19];
  if ( mySettings.useSerial )                    { Serial.print(str); }
  if ( mySettings.useTelnet && telnetConnected ) { Telnet.print(str); } 
  if ( mySettings.useLog ) { 
    if (time_avail==true){
      sprintf_P(buf,PSTR("%2d.%2d.%2d %2d:%2d:%2d "), 
              (localTime->tm_mon+1), 
              localTime->tm_mday,
              (localTime->tm_year%100),
              localTime->tm_hour, 
              localTime->tm_min,    
              localTime->tm_sec);
      logFile.print(buf);
    }
    if ( !logFile ) { logFile = LittleFS.open("/Sensi.log", "a"); }
    if (  logFile ) { logFile.print(str); }
  }
}

void printSerialTelnet(String str) {
  char buf[19];
  if ( mySettings.useSerial )                    { Serial.print(str); }
  if ( mySettings.useTelnet && telnetConnected ) { Telnet.print(str); } 
  if ( mySettings.useLog ) {
    if ( !logFile ) { logFile = LittleFS.open("/Sensi.log", "a"); }  // we close log file regularly in main loop
    if (time_avail==true){
      sprintf_P(buf,PSTR("%2d.%2d.%2d %2d:%2d:%2d "), 
              (localTime->tm_mon+1), 
              localTime->tm_mday, 
              (localTime->tm_year%100),
              localTime->tm_hour, 
              localTime->tm_min,    
              localTime->tm_sec);
      logFile.print(buf);
    }
    if (  logFile ) { logFile.print(str); } 
  } 
}

void onTelnetInputReceived(String str){
  telnetInputBuff = str;
  telnetReceived = true;
}

// (optional) callback functions for telnet events
void onTelnetConnect(String ip) { 
  if ( (mySettings.debuglevel > 0 ) && mySettings.useSerial) {
    Serial.print(F("Telnet: "));
    Serial.print(ip);
    Serial.println(F(" connected"));
  }
  telnetConnected = true;
  Telnet.print(F("\nWelcome "));
  Telnet.print(Telnet.getIP());
  Telnet.print(F("\r\nUse ^] + q  to disconnect\r\n"));
  helpMenu();
}

void onTelnetDisconnect(String ip) {
  if ( (mySettings.debuglevel > 0 ) && mySettings.useSerial) {
    Serial.print(F("Telnet: "));
    Serial.print(ip);
    Serial.print(F(" disconnected\r\n"));
  }
  telnetConnected = false;
}

void onTelnetReconnect(String ip) {
  if ( (mySettings.debuglevel > 0 ) && mySettings.useSerial) {
    Serial.print(F("Telnet: "));
    Serial.print(ip);
    Serial.print(F(" reconnected\r\n"));
  }
  telnetConnected = true;
}

void onTelnetConnectionAttempt(String ip) {
  if ( (mySettings.debuglevel > 0 ) && mySettings.useSerial) {
    Serial.print(F("- Telnet: "));
    Serial.print(ip);
    Serial.print(F(" tried to connect\r\n"));
  }
  telnetConnected = false;
}

void updateTelnet() {

  switch(stateTelnet) {
    
    case IS_WAITING : { //---------------------
      // just wait...
      if ((currentTime - lastTelnet) >= intervalWiFi) {
        if ((mySettings.debuglevel == 3) && mySettings.useTelnet) { Serial.println(F("Telnet: waiting for network to come up")); }          
        lastTelnet = currentTime;
      }
      break;
    }

    case START_UP : { //---------------------
      if ((currentTime - lastTelnet) >= intervalWiFi) {
        lastTelnet = currentTime;

        #if defined(DEBUG)
        Serial.println(F("DBG:STARTUP: Telnet"));
        #endif
                          
        // Telnetsetup
        Telnet.onConnect(onTelnetConnect);
        Telnet.onConnectionAttempt(onTelnetConnectionAttempt);
        Telnet.onReconnect(onTelnetReconnect);
        Telnet.onDisconnect(onTelnetDisconnect);
        Telnet.onInputReceived(onTelnetInputReceived);
                  
        if (Telnet.begin()) {
          if ( (mySettings.debuglevel > 0 ) && mySettings.useSerial) { Serial.println(F("Telnet: running")); }
          stateTelnet = CHECK_CONNECTION;
        } else {
          if ( (mySettings.debuglevel > 0 ) && mySettings.useSerial) { Serial.println(F("Telnet: error")); }
        }
          
        #if defined(DEBUG) 
        Serial.println(F("DBG:STARTUP: Telnet END"));
        #endif 
        
      } // end time interval
      break;
    } // end startup

    case CHECK_CONNECTION : { //---------------------
      //if ((currentTime - lastTelnet) >= intervalWiFi) {
      Telnet.loop();
      //  lastTelnet = currentTime;
      //}
      break;          
    }

    default: {if (mySettings.debuglevel > 0) { printSerialTelnet(F("Telnet Error: invalid switch statement")); break;}}

    
  } // switch
} // update Telnet
