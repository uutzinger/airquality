/******************************************************************************************************/
// OTA
/******************************************************************************************************/
#include "VSC.h"
#ifdef EDITVSC
#include "src/Config.h"
#include "src/WiFi.h"
#include "src/Sensi.h"
#endif
// Over the air program, standard functions from many web tutorials
void onstartOTA() {
  if (mySettings.debuglevel > 0) { 
    if (ArduinoOTA.getCommand() == U_FLASH) { printSerialTelnet(F("OTA: starting sketch\r\n")); }
    else { printSerialTelnet(F("OTA: starting filesystem\r\n")); }  // U_SPIFFS, LittleFS
    otaInProgress = true;
 }
}

void onendOTA() {
  if (mySettings.debuglevel > 0) { printSerialTelnet(F("\r\nOTA: End\r\n")); } 
  otaInProgress = false;
}

void onprogressOTA(unsigned int progress, unsigned int total) {
  if (mySettings.debuglevel > 0) { sprintf(tmpStr, "OTA: progress: %u%%\r",(progress / (total / 100))); printSerialTelnet(tmpStr); } 
}

void onerrorOTA(ota_error_t error) {
  if (mySettings.debuglevel > 0) {
    sprintf(tmpStr, "OTA: error[%u]: ", error); 
    printSerialTelnet(tmpStr);
    if      (error == OTA_AUTH_ERROR)    { printSerialTelnet(F("Auth Failed\r\n")); }
    else if (error == OTA_BEGIN_ERROR)   { printSerialTelnet(F("Begin Failed\r\n")); }
    else if (error == OTA_CONNECT_ERROR) { printSerialTelnet(F("Connect Failed\r\n")); }
    else if (error == OTA_RECEIVE_ERROR) { printSerialTelnet(F("Receive Failed\r\n")); }
    else if (error == OTA_END_ERROR)     { printSerialTelnet(F("End Failed\r\n")); }
  }
  otaInProgress = false;
}

void updateOTA() {
  
  switch(stateOTA) {
    
    case IS_WAITING : { //---------------------
      // just wait...
      if ((currentTime - lastOTA) >= intervalWiFi) {
        if ((mySettings.debuglevel == 3) && mySettings.useOTA) { printSerialTelnet(F("OTA: waiting for network to come up\r\n")); }
        lastOTA = currentTime;
      }
      break;
    }

    case START_UP : { //---------------------
      if ((currentTime - lastOTA) >= intervalWiFi) {
        lastOTA = currentTime;
        #if defined(DEBUG)
        printSerialTelnet(F("DBG:STARTUP: OTA\r\n"));
        #endif
        ArduinoOTA.setPort(8266);                        // OTA port
        ArduinoOTA.setHostname(hostName);                // hostname
        ArduinoOTA.setPassword("w1ldc8ts");              // That should be set by programmer and not user 
        // call back functions
        ArduinoOTA.onStart( onstartOTA );
        ArduinoOTA.onEnd( onendOTA );
        ArduinoOTA.onProgress( onprogressOTA );
        ArduinoOTA.onError( onerrorOTA );
        // get it going
        ArduinoOTA.begin(true);                            //start and use mDNS
        if (mySettings.debuglevel > 0) { printSerialTelnet(F("OTA: ready\r\n")); }
        stateOTA = CHECK_CONNECTION;
        AllmaxUpdateOTA = 0;
      } // currentTime
      break;
    } // startup

    case CHECK_CONNECTION : { //---------------------
      ArduinoOTA.handle();
      break;          
    }
  } // end switch
} // end update OTA
