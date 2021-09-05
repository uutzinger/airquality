/******************************************************************************************************/
// HTTP Server
/******************************************************************************************************/
#include "VSC.h"
#ifdef EDITVSC
#include "src/WiFi.h"
#include "src/HTTP.h"
#endif
//
// https://www.mischianti.org/2020/05/24/rest-server-on-esp8266-and-esp32-get-and-json-formatter-part-2/
// https://tttapa.github.io/ESP8266/Chap16%20-%20Data%20Logging.html
// https://gist.github.com/maditnerd/c08c50c8eb2bb841bfd993f4f2beee7b
// https://github.com/Links2004/arduinoWebSockets
//
//Working with JSON:
// https://en.wikipedia.org/wiki/JSON
// https://circuits4you.com/2019/01/11/nodemcu-esp8266-arduino-json-parsing-example/
//
//Creating Webcontent on ESP
// GET & POST https://randomnerdtutorials.com/esp8266-nodemcu-http-get-post-arduino/
// JSON & autoupdate https://circuits4you.com/2019/03/22/esp8266-weather-station-arduino/
// JSON & tabulated data https://circuits4you.com/2019/01/25/esp8266-dht11-humidity-temperature-data-logging/
// Simple autorefresh https://circuits4you.com/2018/02/04/esp8266-ajax-update-part-of-web-page-without-refreshing/


void updateHTTP() {
  // Operation:

  switch(stateHTTP) {
    
    case IS_WAITING : { //---------------------
      // just wait...
      if ((currentTime - lastHTTP) >= intervalWiFi) {
        lastHTTP = currentTime;
        if ((mySettings.debuglevel  == 3) && mySettings.useHTTP) { printSerialTelnet(F("HTTP server: is waiting for network to come up")); }          
      }
       break;
    }
    
    case START_UP : { //---------------------
      if ((currentTime - lastHTTP) >= intervalWiFi) {        
        lastHTTP = currentTime;
        #if defined(DEBUG) 
        printSerialTelnet(F("DBG:STARTUP: HTTP\r\n"));
        #endif
        httpServer.on("/",         handleRoot);          //Which routine to handle at root location. This is display page
        httpServer.on("/bme280",   handleBME280);
        httpServer.on("/bme680",   handleBME680);
        httpServer.on("/ccs811",   handleCCS811);
        httpServer.on("/max",      handleMAX30);
        httpServer.on("/mlx",      handleMLX);
        httpServer.on("/scd30",    handleSCD30);
        httpServer.on("/sgp30",    handleSGP30);
        httpServer.on("/sps30",    handleSPS30);
        httpServer.on("/date",     handleDate);
        httpServer.on("/time",     handleTime);
        httpServer.on("/hostname", handleHostname);          
        httpServer.on("/ip",       handleIP);          
        httpServer.on("/config",   handleConfig);
        httpServer.on("/upload", HTTP_GET, []() { if (!handleFileRead("/upload.htm")) httpServer.send(404, "text/plain", "404: Not Found"); });        
        httpServer.on("/upload", HTTP_POST, [](){ httpServer.send(200); }, handleFileUpload );
        httpServer.onNotFound(     handleNotFound);      // When a client requests an unknown URI (i.e. something other than "/"), call function "handleNotFound"
        httpServer.begin();                              // Start server
        if (mySettings.debuglevel  > 0) { printSerialTelnet(F("HTTP Server: initialized\r\n")); }
        stateHTTP = CHECK_CONNECTION;
        AllmaxUpdateHTTP = 0;
      }
      break;
    }
    
    case CHECK_CONNECTION : { //---------------------
      //if ((currentTime - lastHTTP) >= intervalHTTP) {
      //  lastHTTP = currentTime;
        httpServer.handleClient();
      //}
      break;          
    }
          
  } // end switch state
} // http

void handleRoot() {
  handleFileRead("/index.htm"); 
  if (mySettings.debuglevel == 3) { printSerialTelnet(F("HTTP: root request received\r\n")); }
}

void handleAnimation() {
  handleFileRead("/animation.htm"); 
 if (mySettings.debuglevel == 3) { printSerialTelnet(F("HTTP: animation request received\r\n")); }
}

void handleConfig() {
  handleFileRead("/Sensi.json"); 
 if (mySettings.debuglevel == 3) { printSerialTelnet(F("HTTP: config request received\r\n")); }
}

void handleNotFound(){
  if (!handleFileRead(httpServer.uri())) {                // check if the file exists in the flash memory (SPIFFS), if so, send it
   String message= "File Not Found \r\n\n";
   message += "URI: ";
   message += httpServer.uri();
   message += "\r\nMethod: ";
   message += ( httpServer.method() == HTTP_GET ) ? "GET" : "POST";
   message += httpServer.args();
   message += "\r\n";
   for (int8_t i=0; i < httpServer.args(); i++ ) {
    message += " " + httpServer.argName(i) + ": " + httpServer.arg(i) + "\r\n";
   }
   httpServer.send(404, "text/plain", message); // Send HTTP status 404
  }
  if (mySettings.debuglevel == 3) { printSerialTelnet(F("HTTP: invalid request received"));}
}

void handleTime() {
  char HTTPpayloadStr[64];                                 // String allocated for HTTP message  
  timeJSON(HTTPpayloadStr);
  httpServer.send(200, "text/json", HTTPpayloadStr);
  if (mySettings.debuglevel == 3) { sprintf(tmpStr, "HTTP: time request received. Sent: %u\r\n", strlen(HTTPpayloadStr)); printSerialTelnet(tmpStr); }
}

void handleDate() {
  char HTTPpayloadStr[64];                                 // String allocated for HTTP message  
  dateJSON(HTTPpayloadStr);
  httpServer.send(200, "text/json", HTTPpayloadStr);
  if (mySettings.debuglevel == 3) { sprintf(tmpStr, "HTTP: date request received. Sent: %u\r\n", strlen(HTTPpayloadStr)); printSerialTelnet(tmpStr); }
}

void handleHostname() {
  //Create JSON data
  String response = "{\"hostname\":\"" + String(hostName) +"\"}";  
  httpServer.send(200, "text/json", response);
  if (mySettings.debuglevel == 3) { sprintf(tmpStr, "HTTP: hostname request received. Sent: %u\r\n", response.length()); printSerialTelnet(tmpStr); }
}

void handleIP() {
  //Create JSON data
  String response = "{\"ip\":\"" + WiFi.localIP().toString() +"\"}"; 
  httpServer.send(200, "text/json", response);
  if (mySettings.debuglevel == 3) { sprintf(tmpStr, "HTTP: time request received. Sent: %u\r\n", response.length()); printSerialTelnet(tmpStr);}
}

void handleBME280() {
  //Create JSON data
  //{"Sensi":{"bme280":{ "avail":true, "p":123.4,"rH":123.4,"aH":123.4,"T":+25.0,"rh_airquality":"normal"}}}
  char HTTPpayloadStr[256];                                 // String allocated for HTTP message  
  bme280JSON(HTTPpayloadStr);
  httpServer.send(200, "text/json", HTTPpayloadStr);
  if (mySettings.debuglevel == 3) { sprintf(tmpStr, "HTTP: BME280 request received. Sent: %u\r\n", strlen(HTTPpayloadStr)); printSerialTelnet(tmpStr); }
}

void handleBME680() {
  //Create JSON data
  //{"Sensi":{"bme680":{"p":123.4,"rH":12.3,"ah":123.4,"T":+25.1,"resistance":1234,"rH_airquality":"normal","resistance_airquality":"normal"}}}
  char HTTPpayloadStr[256];                                 // String allocated for HTTP message  
  bme680JSON(HTTPpayloadStr);
  httpServer.send(200, "text/json", HTTPpayloadStr);
  if (mySettings.debuglevel == 3) { sprintf(tmpStr, "HTTP: BME680 request received. Sent: %u\r\n", strlen(HTTPpayloadStr)); printSerialTelnet(tmpStr); }
}

void handleCCS811() {
  //Create JSON data
  char HTTPpayloadStr[256];                                 // String allocated for HTTP message  
  ccs811JSON(HTTPpayloadStr);
  httpServer.send(200, "text/json", HTTPpayloadStr);
  if (mySettings.debuglevel == 3) { sprintf(tmpStr, "HTTP: CCS811 request received. Sent: %u\r\n", strlen(HTTPpayloadStr)); printSerialTelnet(tmpStr); }
}

void handleSCD30() {
  //Create JSON data
  char HTTPpayloadStr[256];                                 // String allocated for HTTP message  
  scd30JSON(HTTPpayloadStr);
  httpServer.send(200, "text/json", HTTPpayloadStr);
  if (mySettings.debuglevel == 3) { sprintf(tmpStr, "HTTP: SCD30 request received. Sent: %u\r\n", strlen(HTTPpayloadStr)); printSerialTelnet(tmpStr); }
}

void handleSGP30() {
  //Create JSON data
  char HTTPpayloadStr[256];                                 // String allocated for HTTP message  
  sgp30JSON(HTTPpayloadStr);
  httpServer.send(200, "text/json", HTTPpayloadStr);
  if (mySettings.debuglevel == 3) { sprintf(tmpStr, "HTTP: SGP30 request received. Sent: %u\r\n", strlen(HTTPpayloadStr)); printSerialTelnet(tmpStr); }
}

void handleSPS30() {
  //Create JSON data
  char HTTPpayloadStr[512];                                 // String allocated for HTTP message  
  sps30JSON(HTTPpayloadStr);
  httpServer.send(200, "text/json", HTTPpayloadStr);
  if (mySettings.debuglevel == 3) { sprintf(tmpStr, "HTTP: SPS30 request received. Sent: %u\r\n", strlen(HTTPpayloadStr)); printSerialTelnet(tmpStr); }
}

void handleMLX() {
  //Create JSON data
  char HTTPpayloadStr[256];                                 // String allocated for HTTP message  
  mlxJSON(HTTPpayloadStr);
  httpServer.send(200, "text/json", HTTPpayloadStr);
  if (mySettings.debuglevel == 3) { sprintf(tmpStr, "HTTP: MLX request received. Sent: %u\r\n", strlen(HTTPpayloadStr)); printSerialTelnet(tmpStr); }
}

void handleMAX30() {
  //Create JSON data
  // Not Implemented Yet
  String response = "{\"max\":{\"avail\":" +String(max_avail) +",";
  if (max_avail) {
    response += "\"HR\":-1.0,";
    response += "\"O2Sat\":-1.0}}";
  } else {
    response += "\"HR\":-1.0,";
    response += "\"O2Sat\":-1.0}}";    
  }
  httpServer.send(200, "text/json", response); //
  if (mySettings.debuglevel == 3) { sprintf(tmpStr, "HTTP: MAX30 request received. Sent: %u\r\n", response.length()); printSerialTelnet(tmpStr); }
}

String getContentType(String filename){
  if (httpServer.hasArg("download"))  return "application/octet-stream";
  else if(filename.endsWith(".htm"))  return "text/html";
  else if(filename.endsWith(".html")) return "text/html";
  else if(filename.endsWith(".css"))  return "text/css";
  else if(filename.endsWith(".js"))   return "application/javascript";
  else if(filename.endsWith(".png"))  return "image/png";
  else if(filename.endsWith(".gif"))  return "image/gif";
  else if(filename.endsWith(".jpg"))  return "image/jpeg";
  else if(filename.endsWith(".ico"))  return "image/x-icon";
  else if(filename.endsWith(".xml"))  return "text/xml";
  else if(filename.endsWith(".json")) return "text/json";
  else if(filename.endsWith(".pdf"))  return "application/x-pdf";
  else if(filename.endsWith(".zip"))  return "application/x-zip";
  else if(filename.endsWith(".gz"))   return "application/x-gzip";
  return "text/plain";
}

// send the right file to the client (if it exists)
bool handleFileRead(String path) { 
  if (mySettings.debuglevel == 3) { sprintf(tmpStr, "\r\n"); printSerialTelnet(tmpStr); Serial.println("handleFileRead: " + path); }
  if (path.endsWith("/")) path += "index.htm";                // If a folder is requested, send the index file
  String contentType = getContentType(path);                  // Get the MIME type
  String pathWithGz = path + ".gz";                           // Also check for compressed version
  if (LittleFS.exists(pathWithGz) || LittleFS.exists(path)) { // If either file exists
    if(LittleFS.exists(pathWithGz)) {path = pathWithGz;}      // If there's a compressed version available, se the compressed version
    File file = LittleFS.open(path, "r");                     // Open it
    size_t sent = httpServer.streamFile(file, contentType);   // And send it to the client
    if (sent != file.size()) { if (mySettings.debuglevel > 0) { sprintf(tmpStr, "HTTP: Error sent less data than expected: %lu %lu\r\n", sent, file.size()); printSerialTelnet(tmpStr); } }
    file.close();                                             // Then close the file again
    return true;
  } else {
    if (mySettings.debuglevel > 0) { 
      printSerialTelnet(F("HTTP: File Not Found: " ));
      printSerialTelnet(path); 
      printSerialTelnet(F("\r\n"));
    }
    return false;                                             // If the file doesn't exist, return false
  }
}

void handleFileUpload() { // upload a new file to the SPIFFS
  HTTPUpload& upload = httpServer.upload();
  String path;
  if (upload.status == UPLOAD_FILE_START) {
    path = upload.filename;
    if (!path.startsWith("/")) path = "/" + path;
    if (!path.endsWith(".gz")) {                              // The file server always prefers a compressed version of a file
      String pathWithGz = path + ".gz";                       // So if an uploaded file is not compressed, the existing compressed
      if (LittleFS.exists(pathWithGz))                        // version of that file must be deleted (if it exists)
        LittleFS.remove(pathWithGz);
    }
    printSerialTelnet(F("handleFileUpload Name: " ));
    printSerialTelnet(path); 
    printSerialTelnet(F("\r\n"));

    uploadFile = LittleFS.open(path, "w");                    // Open the file for writing in SPIFFS (create if it doesn't exist)
    path = String();
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (uploadFile)
      uploadFile.write(upload.buf, upload.currentSize);       // Write the received bytes to the file
  } else if (upload.status == UPLOAD_FILE_END) {
    if (uploadFile) {                                         // If the file was successfully created
      uploadFile.close();                                     // Close the file again
      sprintf(tmpStr, "handleFileUpload Size: %lu\r\n", upload.totalSize); printSerialTelnet(tmpStr);
      httpServer.sendHeader("Location", "/success.htm");      // Redirect the client to the success page
      httpServer.send(303);
    } else {
      httpServer.send(500, "text/plain", "500: couldn't create file");
    }
  }
}
