/******************************************************************************************************/
// Web Socket Server 
/******************************************************************************************************/
// https://www.hackster.io/s-wilson/nodemcu-home-weather-station-with-websocket-7c77a3
// https://www.mischianti.org/2020/12/21/websocket-on-arduino-esp8266-and-esp32-temperature-and-humidity-realtime-update-3/
// http://www.martyncurrey.com/esp8266-and-the-arduino-ide-part-10a-iot-website-temperature-and-humidity-monitor/

#include "src/WebSocket.h"
#include "src/Config.h"
#include "src/Sensi.h"
#include "src/WiFi.h"
#include "src/BME280.h"
#include "src/BME68x.h"
#include "src/CCS811.h"
#include "src/MLX.h"
#include "src/SCD30.h"
#include "src/SGP30.h"
#include "src/SPS30.h"
#include "src/Weather.h"
#include "src/Print.h"


bool ws_connected = false;                                 // mqtt connection established?
unsigned long lastWebSocket;                               // last time we checked for websocket
volatile WiFiStates stateWebSocket  = IS_WAITING;          // keeping track of websocket

WebSocketsServer webSocket = WebSocketsServer(81);         // The Websocket interface

// External Variables
extern unsigned long yieldTime;        // Sensi
extern unsigned long lastYield;        // Sensi
extern unsigned long AllmaxUpdateWS;   // Sensi
extern Settings      mySettings;       // Config
extern unsigned long currentTime;      // Sensi
extern char          tmpStr[256];      // Sensi

extern bool          bme280NewDataWS;
extern bool          bme68xNewDataWS;
extern bool          scd30NewDataWS;
extern bool          ccs811NewDataWS;
extern bool          sgp30NewDataWS;
extern bool          sps30NewDataWS;
extern bool          mlxNewDataWS;
extern bool          weatherNewDataWS;
extern bool          timeNewDataWS;
extern bool          dateNewDataWS;
extern bool          max30NewDataWS;

/******************************************************************************************************/
// Initialize Web Socket Server 
/******************************************************************************************************/

void intializeWebSocket() {
  D_printSerialTelnet(F("D:U:WS:IN.."));  
  delay(50); lastYield = millis();
}

/******************************************************************************************************/
// Update Web Socket Server 
/******************************************************************************************************/

void updateWebSocket() {
  // Operation:

  switch(stateWebSocket) {
    
    case IS_WAITING : { //---------------------
      // just wait...
      if ((currentTime - lastWebSocket) >= intervalWiFi) {
        D_printSerialTelnet(F("D:U:WS:IW.."));
        lastWebSocket = currentTime;
        if (mySettings.debuglevel  == 3) { R_printSerialTelnetLogln(F("WebSocket: is waiting for network to come up")); }          
      }
       break;
    }
    
    case START_UP : { //---------------------
      if ((currentTime - lastWebSocket) >= intervalWiFi) {        
        D_printSerialTelnet(F("D:U:WS:S.."));
        lastWebSocket = currentTime;
        webSocket.begin();                              // Start server
        webSocket.onEvent(webSocketEvent);
        // webSocket.setReconnectInterval(5000);;
        // webSocket.setAuthorization("user", "Password");        
        if (mySettings.debuglevel  > 0) { R_printSerialTelnetLogln(F("WebSocket: server started")); }
        stateWebSocket = CHECK_CONNECTION;
        AllmaxUpdateWS = 0;
      }
      break;
    }
    
    case CHECK_CONNECTION : { //---------------------
      //if ((currentTime - lastWebSocket) >= intervalWiFi) {
      //  lastWebSocket = currentTime;
        D_printSerialTelnet(F("D:U:WS:CC.."));
        webSocket.loop();
        if (webSocket.connectedClients(false) == 0) { ws_connected = false; }      
      //}
      break;          
    }
          
    default: {if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("WS Error: invalid switch statement")); break;}}
    
  } // end switch state
} // http

//--------------------------------------------------------Websocket Event----------------------------------------------------
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght) {

  switch (type) {
    
    case WStype_DISCONNECTED:
      if (mySettings.debuglevel  == 3) { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Websocket [%u] disconnected!"), num); R_printSerialTelnetLogln(tmpStr); }
      webSocket.disconnect(num);
      if (webSocket.connectedClients(false) == 0) { ws_connected = false; }
      break;
      
    case WStype_CONNECTED:
      {
        IPAddress ip = webSocket.remoteIP(num);
        if (mySettings.debuglevel  == 3) { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Websocket [%u] connection from %u.%u.%u.%u url: %s"), num, ip[0], ip[1], ip[2], ip[3], payload); R_printSerialTelnetLogln(tmpStr); }
        ws_connected = true;
        //ForceSendValues = 1;
        webSocket.sendTXT(num, "Connected");
      }
      break;
      
    case WStype_TEXT:
      if (mySettings.debuglevel  == 3) { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Websocket [%u] got text data: %s"), num, payload); R_printSerialTelnetLogln(tmpStr); }
      break;
      
    case WStype_BIN:
      if (mySettings.debuglevel  == 3) { 
        snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Websocket [%u] got binary data length: %u"), num, lenght); R_printSerialTelnetLogln(tmpStr);
        hexdump(payload, lenght, 16);
      }
      break;
      
    case WStype_ERROR:
      break;
      
    case WStype_FRAGMENT_TEXT_START:
      break;
      
    case WStype_FRAGMENT_BIN_START:
      break;
      
    case WStype_FRAGMENT:
      break;
      
    case WStype_FRAGMENT_FIN:
      break;
      
    case WStype_PING:
      if (mySettings.debuglevel  == 3) { R_printSerialTelnetLogln(F("Websocket got ping")); }
      break;
      
    case WStype_PONG:
      break;
      
    default:
      if (mySettings.debuglevel  == 3) { R_printSerialTelnetLogln(F("Websocket got pong")); }
      break;
  }
}

void hexdump(const void *mem, uint32_t len, uint8_t cols) {
    const uint8_t* src = (const uint8_t*) mem;
    snprintf_P(tmpStr, sizeof(tmpStr), PSTR("\r\n[HEXDUMP] Address: 0x%08X len: 0x%X (%d)"), (ptrdiff_t)src, len, len);
    R_printSerialTelnetLog(tmpStr);
    for(uint32_t i = 0; i < len; i++) {
        if(i % cols == 0) {
            snprintf(tmpStr, sizeof(tmpStr), "\r\n[0x%08X] 0x%08X: ", (ptrdiff_t)src, i);
            printSerialTelnetLog(tmpStr);
        }
        snprintf(tmpStr, sizeof(tmpStr), "%02X ", *src);
        printSerialTelnetLog(tmpStr);
        src++;
    }
    printSerialTelnetLog("\r\n");
}

void updateWebSocketMessage() {
    char payLoad[512];
        
    if (bme280NewDataWS) {
      bme280JSON(payLoad, sizeof(payLoad));
      webSocket.broadcastTXT(payLoad);      
      bme280NewDataWS = false;
      if (mySettings.debuglevel == 3) { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("BME280 WebSocket data boradcasted, len: %u"), strlen(payLoad)); R_printSerialTelnetLogln(tmpStr); }      
      yieldTime += yieldOS(); 
    }

    if (scd30NewDataWS)  {
      scd30JSON(payLoad, sizeof(payLoad));
      webSocket.broadcastTXT(payLoad);      
      scd30NewDataWS = false;
      if (mySettings.debuglevel == 3) { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SCD30 WebSocket data boradcasted, len: %u"), strlen(payLoad)); R_printSerialTelnetLogln(tmpStr); }
      yieldTime += yieldOS(); 
    }
    
    if (sgp30NewDataWS)  {
      sgp30JSON(payLoad, sizeof(payLoad));
      webSocket.broadcastTXT(payLoad);      
      sgp30NewDataWS = false;
      if (mySettings.debuglevel == 3) { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SGP30 WebSocket data boradcasted, len: %u"), strlen(payLoad)); R_printSerialTelnetLogln(tmpStr); }
      yieldTime += yieldOS(); 
    }
    
    if (sps30NewDataWS)  {
      sps30JSON(payLoad, sizeof(payLoad));
      webSocket.broadcastTXT(payLoad);      
      sps30NewDataWS = false;
      if (mySettings.debuglevel == 3) { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("SPS30 WebSocket data boradcasted, len: %u"), strlen(payLoad)); R_printSerialTelnetLogln(tmpStr); }      
      yieldTime += yieldOS(); 
    }
    
    if (ccs811NewDataWS) {
      ccs811JSON(payLoad, sizeof(payLoad));
      webSocket.broadcastTXT(payLoad);      
      ccs811NewDataWS = false;
      if (mySettings.debuglevel == 3) { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("CCS811 WebSocket data boradcasted, len: %u"), strlen(payLoad)); R_printSerialTelnetLogln(tmpStr); }      
      yieldTime += yieldOS(); 
    }

    if (bme68xNewDataWS) {
      bme68xJSON(payLoad, sizeof(payLoad));
      webSocket.broadcastTXT(payLoad);      
      bme68xNewDataWS = false;
      if (mySettings.debuglevel == 3) { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("BME68x WebSocket data boradcasted, len: %u"), strlen(payLoad)); R_printSerialTelnetLogln(tmpStr); }      
      yieldTime += yieldOS(); 
    }

    if (mlxNewDataWS) {
      mlxJSON(payLoad, sizeof(payLoad));
      webSocket.broadcastTXT(payLoad);      
      mlxNewDataWS = false;
      if (mySettings.debuglevel == 3) { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("MLX WebSocket data boradcasted, len: %u"), strlen(payLoad)); R_printSerialTelnetLogln(tmpStr); }      
      yieldTime += yieldOS(); 
    }  

    if (weatherNewDataWS) {
      weatherJSON(payLoad, sizeof(payLoad));
      webSocket.broadcastTXT(payLoad);      
      weatherNewDataWS = false;
      if (mySettings.debuglevel == 3) { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Weather WebSocket data boradcasted, len: %u"), strlen(payLoad)); R_printSerialTelnetLogln(tmpStr); }      
      yieldTime += yieldOS(); 
    }  

    if (timeNewDataWS) {
      timeJSON(payLoad, sizeof(payLoad));
      webSocket.broadcastTXT(payLoad);      
      timeNewDataWS = false;
      if (mySettings.debuglevel == 3) { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Time WebSocket data boradcasted, len: %u"), strlen(payLoad)); R_printSerialTelnetLogln(tmpStr); }      
      yieldTime += yieldOS(); 
    }
    
    if (dateNewDataWS) {
      dateJSON(payLoad, sizeof(payLoad));
      webSocket.broadcastTXT(payLoad);      
      dateNewDataWS = false;
      if (mySettings.debuglevel == 3) { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("Date WebSocket data boradcasted, len: %u"), strlen(payLoad)); R_printSerialTelnetLogln(tmpStr); }      
      yieldTime += yieldOS(); 
    }
}
