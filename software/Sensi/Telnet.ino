/******************************************************************************************************/
// Telnet
/******************************************************************************************************/
#include "src/Config.h"
#include "src/Sensi.h"
#include "src/Telnet.h"
#include "src/WiFi.h"
#include "src/Print.h"

String        telnetInput;
char          telnetInputBuff[64];
bool          telnetConnected = false; 
bool          telnetReceived = false;
unsigned long lastTelnet;                                     // last time we checked telnet input
unsigned long lastTelnetInput = 0;                            // keep track of telnet flood

volatile WiFiStates stateTelnet = IS_WAITING;                 // keeping track of network time state

ESPTelnet      Telnet;                                       // The Telnet interface

// External Variables
extern Settings      mySettings;   // Config
extern bool          fastMode;     // Sensi
extern bool          BMEhum_avail; // BME280
extern unsigned long lastYield;    // Sensi
extern unsigned long currentTime;  // Sensi
extern unsigned long yieldTime;    // Sensi
extern char          tmpStr[256];  // Sensi

void onTelnetInputReceived(String str){
  strlcpy(telnetInputBuff, str.c_str(), sizeof(telnetInputBuff)); // Null termination enforced
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
  Telnet.print(F("? for help\r\n"));
  yieldTime += yieldOS(); 
  // helpMenu();
}

void onTelnetDisconnect(String ip) {
  if ( (mySettings.debuglevel > 0 ) && mySettings.useSerial) {
    R_printSerialTelnetLog(F("Telnet: "));
      printSerialTelnetLog(ip);
      printSerialTelnetLogln(F(" disconnected"));
      yieldTime += yieldOS(); 
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
      printSerialTelnetLog(ip); // crashed here
      printSerialTelnetLogln(F(" tried to connect"));
  }
  telnetConnected = false;
}

/******************************************************************************************************/
// Initialize Telnet
/******************************************************************************************************/

void initializeTelnet() {
  D_printSerialTelnet(F("D:U:Telnet:IN.."));
  // Telnetsetup
  Telnet.onConnect(onTelnetConnect);
  Telnet.onConnectionAttempt(onTelnetConnectionAttempt);
  Telnet.onReconnect(onTelnetReconnect);
  Telnet.onDisconnect(onTelnetDisconnect);
  Telnet.onInputReceived(onTelnetInputReceived);
  delay(50); lastYield = millis();
}

/******************************************************************************************************/
// Update Telnet
/******************************************************************************************************/

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
