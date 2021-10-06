/******************************************************************************************************/
// Web Socket Server 
/******************************************************************************************************/
// https://www.hackster.io/s-wilson/nodemcu-home-weather-station-with-websocket-7c77a3
// https://www.mischianti.org/2020/12/21/websocket-on-arduino-esp8266-and-esp32-temperature-and-humidity-realtime-update-3/
// http://www.martyncurrey.com/esp8266-and-the-arduino-ide-part-10a-iot-website-temperature-and-humidity-monitor/
#include "VSC.h"
#ifdef EDITVSC
#include "src/WebSocket.h"
#include "src/Config.h"
#include "src/Sensi.h"
#include "src/WiFi.h"
#include "src/BME280.h"
#include "src/BME680.h"
#include "src/CCS811.h"
#include "src/MLX.h"
#include "src/SCD30.h"
#include "src/SGP30.h"
#include "src/SPS30.h"
#endif

void updateWebSocket() {
  // Operation:

  switch(stateWebSocket) {
    
    case IS_WAITING : { //---------------------
      // just wait...
      if ((currentTime - lastWebSocket) >= intervalWiFi) {
        lastWebSocket = currentTime;
        if (mySettings.debuglevel  == 3) { printSerialTelnet(F("WebSocket: is waiting for network to come up\r\n")); }          
      }
       break;
    }
    
    case START_UP : { //---------------------
      if ((currentTime - lastWebSocket) >= intervalWiFi) {        
        lastWebSocket = currentTime;
        D_printSerialTelnet(F("DBG:STARTUP: WS\r\n"));
        webSocket.begin();                              // Start server
        webSocket.onEvent(webSocketEvent);
        // webSocket.setReconnectInterval(5000);;
        // webSocket.setAuthorization("user", "Password");        
        if (mySettings.debuglevel  > 0) { printSerialTelnet(F("WebSocket: server started\r\n")); }
        stateWebSocket = CHECK_CONNECTION;
        AllmaxUpdateWS = 0;
      }
      break;
    }
    
    case CHECK_CONNECTION : { //---------------------
      //if ((currentTime - lastWebSocket) >= intervalWiFi) {
      //  lastWebSocket = currentTime;
        webSocket.loop();
        if (webSocket.connectedClients(false) == 0) { ws_connected = false; }      
      //}
      break;          
    }
          
    default: {if (mySettings.debuglevel > 0) { printSerialTelnet(F("WS Error: invalid switch statement")); break;}}
    
  } // end switch state
} // http

//--------------------------------------------------------Websocket Event----------------------------------------------------
void webSocketEvent(uint8_t num, WStype_t type, uint8_t * payload, size_t lenght) {

  switch (type) {
    case WStype_DISCONNECTED:
      if (mySettings.debuglevel  == 3) { sprintf_P(tmpStr, PSTR("Websocket [%u] disconnected!\r\n"), num); printSerialTelnet(tmpStr); }
      webSocket.disconnect(num);
      if (webSocket.connectedClients(false) == 0) { ws_connected = false; }
      break;
    case WStype_CONNECTED:
      {
        IPAddress ip = webSocket.remoteIP(num);
        if (mySettings.debuglevel  == 3) { sprintf_P(tmpStr, PSTR("Websocket [%u] connection from %u.%u.%u.%u url: %s\r\n"), num, ip[0], ip[1], ip[2], ip[3], payload); printSerialTelnet(tmpStr); }
        ws_connected = true;
        //ForceSendValues = 1;
        webSocket.sendTXT(num, "Connected");
      }
      break;
    case WStype_TEXT:
      if (mySettings.debuglevel  == 3) { sprintf_P(tmpStr, PSTR("Websocket [%u] got text data: %s\r\n"), num, payload); printSerialTelnet(tmpStr); }
      break;
    case WStype_BIN:
      if (mySettings.debuglevel  == 3) { 
        sprintf_P(tmpStr, PSTR("Websocket [%u] got binary data length: %u\r\n"), num, lenght); printSerialTelnet(tmpStr);
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
      if (mySettings.debuglevel  == 3) { printSerialTelnet(F("Websocket got ping\r\n")); }
      break;
    case WStype_PONG:
      break;
    default:
      if (mySettings.debuglevel  == 3) { printSerialTelnet(F("Websocket got pong\r\n")); }
      break;
  }
}

void hexdump(const void *mem, uint32_t len, uint8_t cols) {
    const uint8_t* src = (const uint8_t*) mem;
    sprintf_P(tmpStr, PSTR("\r\n[HEXDUMP] Address: 0x%08X len: 0x%X (%d)"), (ptrdiff_t)src, len, len);
    printSerialTelnet(tmpStr);
    for(uint32_t i = 0; i < len; i++) {
        if(i % cols == 0) {
            sprintf(tmpStr,"\r\n[0x%08X] 0x%08X: ", (ptrdiff_t)src, i);
            printSerialTelnet(tmpStr);
        }
        sprintf(tmpStr,"%02X ", *src);
        printSerialTelnet(tmpStr);
        src++;
    }
    printSerialTelnet("\r\n");
}

void updateWebSocketMessage() {
    char payLoad[512];
    
    if (bme280NewDataWS) {
      bme680JSON(payLoad);
      webSocket.broadcastTXT(payLoad);      
      bme280NewDataWS = false;
      if (mySettings.debuglevel == 3) { sprintf_P(tmpStr, PSTR("BME280 WebSocket data boradcasted, len: %u\r\n"), strlen(payLoad)); printSerialTelnet(tmpStr); }      
    }

    if (scd30NewDataWS)  {
      scd30JSON(payLoad);
      webSocket.broadcastTXT(payLoad);      
      scd30NewDataWS = false;
      if (mySettings.debuglevel == 3) { sprintf_P(tmpStr, PSTR("SCD30 WebSocket data boradcasted, len: %u\r\n"), strlen(payLoad)); printSerialTelnet(tmpStr); }
    }
    
    if (sgp30NewDataWS)  {
      sgp30JSON(payLoad);
      webSocket.broadcastTXT(payLoad);      
      sgp30NewDataWS = false;
      if (mySettings.debuglevel == 3) { sprintf_P(tmpStr, PSTR("SGP30 WebSocket data boradcasted, len: %u\r\n"), strlen(payLoad)); printSerialTelnet(tmpStr); }
    }
    
    if (sps30NewDataWS)  {
      sps30JSON(payLoad);
      webSocket.broadcastTXT(payLoad);      
      sps30NewDataWS = false;
      if (mySettings.debuglevel == 3) { sprintf_P(tmpStr, PSTR("SPS30 WebSocket data boradcasted, len: %u\r\n"), strlen(payLoad)); printSerialTelnet(tmpStr); }      
    }
    
    if (ccs811NewDataWS) {
      ccs811JSON(payLoad);
      webSocket.broadcastTXT(payLoad);      
      ccs811NewDataWS = false;
      if (mySettings.debuglevel == 3) { sprintf_P(tmpStr, PSTR("CCS811 WebSocket data boradcasted, len: %u\r\n"), strlen(payLoad)); printSerialTelnet(tmpStr); }      
    }

    if (bme680NewDataWS) {
      bme680JSON(payLoad);
      webSocket.broadcastTXT(payLoad);      
      bme680NewDataWS = false;
      if (mySettings.debuglevel == 3) { sprintf_P(tmpStr, PSTR("BME680 WebSocket data boradcasted, len: %u\r\n"), strlen(payLoad)); printSerialTelnet(tmpStr); }      
    }

    if (mlxNewDataWS) {
      mlxJSON(payLoad);
      webSocket.broadcastTXT(payLoad);      
      mlxNewDataWS = false;
      if (mySettings.debuglevel == 3) { sprintf_P(tmpStr, PSTR("MLX WebSocket data boradcasted, len: %u\r\n"), strlen(payLoad)); printSerialTelnet(tmpStr); }      
    }  

    if (timeNewDataWS) {
      timeJSON(payLoad);
      webSocket.broadcastTXT(payLoad);      
      timeNewDataWS = false;
      if (mySettings.debuglevel == 3) { sprintf_P(tmpStr, PSTR("Time WebSocket data boradcasted, len: %u\r\n"), strlen(payLoad)); printSerialTelnet(tmpStr); }      
    }
    
    if (dateNewDataWS) {
      dateJSON(payLoad);
      webSocket.broadcastTXT(payLoad);      
      dateNewDataWS = false;
      if (mySettings.debuglevel == 3) { sprintf_P(tmpStr, PSTR("Date WebSocket data boradcasted, len: %u\r\n"), strlen(payLoad)); printSerialTelnet(tmpStr); }      
    }
}
