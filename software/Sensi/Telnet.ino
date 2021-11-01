/******************************************************************************************************/
// Telnet
/******************************************************************************************************/
#include "VSC.h"
#ifdef EDITVSC
#include "src/Config.h"
#include "src/Sensi.h"
#include "src/Telnet.h"
#endif

void onTelnetInputReceived(String str){
  telnetInputBuff = str;
  telnetReceived = true;
}

// (optional) callback functions for telnet events
void onTelnetConnect(String ip) { 
  if ( (mySettings.debuglevel > 0 ) && mySettings.useSerial) {
    R_printSerialTelnetLog(F("Telnet: "));
      printSerialTelnetLog(ip);
      printSerialTelnetLogln(F(" connected"));
  }
  telnetConnected = true;
  Telnet.print(F("\nWelcome "));
  Telnet.print(Telnet.getIP());
  Telnet.print(F("\r\nUse ^] + q  to disconnect\r\n"));
  helpMenu();
}

void onTelnetDisconnect(String ip) {
  if ( (mySettings.debuglevel > 0 ) && mySettings.useSerial) {
    R_printSerialTelnetLog(F("Telnet: "));
      printSerialTelnetLog(ip);
      printSerialTelnetLogln(F(" disconnected"));
  }
  telnetConnected = false;
}

void onTelnetReconnect(String ip) {
  if ( (mySettings.debuglevel > 0 ) && mySettings.useSerial) {
    R_printSerialTelnetLog(F("Telnet: "));
      printSerialTelnetLog(ip);
      printSerialTelnetLogln(F(" reconnected"));
  }
  telnetConnected = true;
}

void onTelnetConnectionAttempt(String ip) {
  if ( (mySettings.debuglevel > 0 ) && mySettings.useSerial) {
    R_printSerialTelnetLog(F("- Telnet: "));
      printSerialTelnetLog(ip);
      printSerialTelnetLogln(F(" tried to connect"));
  }
  telnetConnected = false;
}

void updateTelnet() {

  switch(stateTelnet) {
    
    case IS_WAITING : { //---------------------
      // just wait...
      if ((currentTime - lastTelnet) >= intervalWiFi) {
        D_printSerialTelnet(F("D:U:Tel:IW.."));
        if ((mySettings.debuglevel == 3) && mySettings.useTelnet) { printSerialTelnetLogln(F("Telnet: waiting for network to come up")); }          
        lastTelnet = currentTime;
      }
      break;
    }

    case START_UP : { //---------------------
      if ((currentTime - lastTelnet) >= intervalWiFi) {
        D_printSerialTelnet(F("D:U:Tel:S.."));
        lastTelnet = currentTime;
                          
        // Telnetsetup
        Telnet.onConnect(onTelnetConnect);
        Telnet.onConnectionAttempt(onTelnetConnectionAttempt);
        Telnet.onReconnect(onTelnetReconnect);
        Telnet.onDisconnect(onTelnetDisconnect);
        Telnet.onInputReceived(onTelnetInputReceived);
                  
        if (Telnet.begin()) {
          if ( (mySettings.debuglevel > 0 ) && mySettings.useSerial) { R_printSerialTelnetLogln(F("Telnet: running")); }
          stateTelnet = CHECK_CONNECTION;
        } else {
          if ( (mySettings.debuglevel > 0 ) && mySettings.useSerial) { R_printSerialTelnetLogln(F("Telnet: error")); }
        }
          
        D_printSerialTelnet(F("D:U:Tel:SE.."));
        
      } // end time interval
      break;
    } // end startup

    case CHECK_CONNECTION : { //---------------------
      //if ((currentTime - lastTelnet) >= intervalWiFi) {
      D_printSerialTelnet(F("D:U:Tel:CC.."));
      Telnet.loop();
      //  lastTelnet = currentTime;
      //}
      break;          
    }

    default: {if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("Telnet Error: invalid switch statement")); break;}}

  } // switch
} // update Telnet
