/******************************************************************************************************/
// OTA Over the air programming
/******************************************************************************************************/
#include "src/Config.h"
#include "src/WiFi.h"
#include "src/Sensi.h"
#include "src/Print.h"

// Extern variables
extern unsigned long yieldTime;        // Sensi
extern unsigned long lastYield;        // Sensi
extern unsigned long lastOTA;          // WiFi
extern unsigned long AllmaxUpdateOTA;  // Sensi
extern Settings      mySettings;       // Config
extern unsigned long currentTime;      // Sensi
extern char          tmpStr[256];      // Sensi
extern char          hostName[16];     // WiFi
extern bool          otaInProgress;    // WiFi
extern volatile WiFiStates stateOTA;   // WiFi

// Over the air program, standard functions from many web tutorials
void onstartOTA() {
  if (mySettings.debuglevel > 0) { 
    if (ArduinoOTA.getCommand() == U_FLASH) { R_printSerialTelnetLogln(F("OTA: starting sketch")); }
    else { R_printSerialTelnetLogln(F("OTA: starting filesystem")); }  // U_SPIFFS, LittleFS
 }
 LittleFS.end();
 otaInProgress = true;
}

void onendOTA() {
  if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("\r\nOTA: End")); } 
  otaInProgress = false;
  Serial.flush();
  ESP.restart();  
}

void onprogressOTA(unsigned int progress, unsigned int total) {
  if (mySettings.debuglevel > 1) { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("OTA: progress: %u%%\r"),(progress / (total / 100))); printSerialTelnetLog(tmpStr); } 
}

void onerrorOTA(ota_error_t error) {
  if (mySettings.debuglevel > 0) {
    snprintf_P(tmpStr, sizeof(tmpStr), PSTR("OTA: error[%u]: "), error); 
    R_printSerialTelnetLog(tmpStr);
    if      (error == OTA_AUTH_ERROR)    { printSerialTelnetLogln(F("Auth Failed")); }
    else if (error == OTA_BEGIN_ERROR)   { printSerialTelnetLogln(F("Begin Failed")); }
    else if (error == OTA_CONNECT_ERROR) { printSerialTelnetLogln(F("Connect Failed")); }
    else if (error == OTA_RECEIVE_ERROR) { printSerialTelnetLogln(F("Receive Failed")); }
    else if (error == OTA_END_ERROR)     { printSerialTelnetLogln(F("End Failed")); }
  }
  otaInProgress = false;  // get out of exclusive update loop
  LittleFS.begin();       // get FS back online
}

/******************************************************************************************************/
// Initialize OTA
/******************************************************************************************************/

void initializeOTA() {
  D_printSerialTelnet(F("D:U:OTA:IN.."));
  ArduinoOTA.setPort(8266);                        // OTA port
  ArduinoOTA.setHostname(hostName);                // hostname
  ArduinoOTA.setPassword("w1ldc8ts");              // That should be set by programmer and not user 
  // call back functions
  ArduinoOTA.onStart( onstartOTA );
  ArduinoOTA.onEnd( onendOTA );
  ArduinoOTA.onProgress( onprogressOTA );
  ArduinoOTA.onError( onerrorOTA );
  delay(50); lastYield = millis();
}

/******************************************************************************************************/
// Update OTA
/******************************************************************************************************/

void updateOTA() {
  
  switch(stateOTA) {
    
    case IS_WAITING : { //---------------------
      // just wait...
      if ((currentTime - lastOTA) >= intervalWiFi) {
        D_printSerialTelnet(F("D:U:OTA:IW.."));
        if ((mySettings.debuglevel == 3) && mySettings.useOTA) { R_printSerialTelnetLogln(F("OTA: waiting for network to come up")); }
        lastOTA = currentTime;
      }
      break;
    }

    case START_UP : { //---------------------
      if ((currentTime - lastOTA) >= intervalWiFi) {
        D_printSerialTelnet(F("D:U:OTA:S.."));
        lastOTA = currentTime;
        // get it going
        ArduinoOTA.begin(true);                            //start and use mDNS
        if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("OTA: ready")); }
        stateOTA = CHECK_CONNECTION;
        AllmaxUpdateOTA = 0;
      } // currentTime
      break;
    } // startup

    case CHECK_CONNECTION : { //---------------------
      D_printSerialTelnet(F("D:U:OTA:CC.."));
      ArduinoOTA.handle();
      break;          
    }

    default: {
      if (mySettings.debuglevel > 0) { 
        R_printSerialTelnetLogln(F("OTA Error: invalid switch statement"));
      } 
      break;
    }

  } // end switch
} // end update OTA
