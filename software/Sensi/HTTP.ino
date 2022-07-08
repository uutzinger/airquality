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
        D_printSerialTelnet(F("D:U:HTTP:IW.."));
        lastHTTP = currentTime;
        if ((mySettings.debuglevel  == 3) && mySettings.useHTTP) { R_printSerialTelnetLog(F("HTTP server: is waiting for network to come up")); }          
      }
       break;
    }
    
    case START_UP : { //---------------------
      if ((currentTime - lastHTTP) >= intervalWiFi) {        
        lastHTTP = currentTime;
        D_printSerialTelnet(F("D:U:HTTP:S.."));
        httpServer.on("/",         handleRoot);          //Which routine to handle at root location. This is display page
        httpServer.on("/bme280",   handleBME280);
        httpServer.on("/bme680",   handleBME680);
        httpServer.on("/ccs811",   handleCCS811);
        httpServer.on("/max",      handleMAX30); // crashed here
        httpServer.on("/mlx",      handleMLX);
        httpServer.on("/scd30",    handleSCD30);
        httpServer.on("/sgp30",    handleSGP30);
        httpServer.on("/sps30",    handleSPS30);
        httpServer.on("/date",     handleDate);
        httpServer.on("/time",     handleTime);
        httpServer.on("/hostname", handleHostname);          
        httpServer.on("/ip",       handleIP);          
        httpServer.on("/config",   handleConfig);
        httpServer.on("/system",   handleSystem);
        httpServer.on("/edit",     handleEdit);
        httpServer.on("/upload", HTTP_GET, []() { if (!handleFileRead("/upload.htm")) httpServer.send(404, "text/plain", "404: Not Found"); });        
        httpServer.on("/upload", HTTP_POST, [](){ httpServer.send(200); }, handleFileUpload );
        httpServer.onNotFound(     handleNotFound);      // When a client requests an unknown URI (i.e. something other than "/"), call function "handleNotFound"
        httpServer.begin();                              // Start server
        if (mySettings.debuglevel  > 0) { R_printSerialTelnetLogln(F("HTTP Server: initialized")); }
        stateHTTP = CHECK_CONNECTION;
        AllmaxUpdateHTTP = 0;
      }
      break;
    }
    
    case CHECK_CONNECTION : { //---------------------
      //if ((currentTime - lastHTTP) >= intervalHTTP) {
      //  lastHTTP = currentTime;
        D_printSerialTelnet(F("D:U:HTTP:CC.."));
        httpServer.handleClient();
      //}
      break;          
    }

    default: {if (mySettings.debuglevel > 0) { R_printSerialTelnetLogln(F("HTTP Error: invalid switch statement")); break;}}
      
  } // end switch state
} // http

void handleRoot() {
  handleFileRead("/index.htm"); 
  if (mySettings.debuglevel == 3) { R_printSerialTelnetLogln(F("HTTP: root request received")); }
}

void handleAnimation() {
  handleFileRead("/animation.htm"); 
  if (mySettings.debuglevel == 3) { R_printSerialTelnetLogln(F("HTTP: animation request received")); }
}

void handleConfig() {
  handleFileRead("/Sensi.json"); 
  if (mySettings.debuglevel == 3) { R_printSerialTelnetLogln(F("HTTP: config request received")); }
}

void handleNotFound(){
  if (!handleFileRead(httpServer.uri())) {                // check if the file exists in the flash memory, if so, send it
   String message = "File Not Found \r\n\n";
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
  if (mySettings.debuglevel == 3) { R_printSerialTelnetLogln(F("HTTP: invalid request received"));}
  yieldTime += yieldOS(); 
}

void handleEdit(){
  String message = "You need to reprogram ESP with FSBrowser (extras folder)!\r\n";
  httpServer.send(404, "text/plain", message); // Send can not use file editor/browser with this program
  if (mySettings.debuglevel == 3) { R_printSerialTelnetLogln(F("HTTP: edit request received"));}
  yieldTime += yieldOS(); 
}

// { "system": { "freeheap": 30000, "heapfragmentation": 33, "maxfreeblock": 30000,"maxlooptime": 10000}}
void handleSystem() {
  char HTTPpayloadStr[128];  
  systemJSON(HTTPpayloadStr, sizeof(HTTPpayloadStr));
  httpServer.send(200, "text/json", HTTPpayloadStr);
  if (mySettings.debuglevel == 3) { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("HTTP: system request received. Sent: %u"), strlen(HTTPpayloadStr)); R_printSerialTelnetLogln(tmpStr); }
  yieldTime += yieldOS(); 
}

// { "time": { "hour": 20, "minute": 19, "second": 18, "microsecond": 648567}}
void handleTime() {
  char HTTPpayloadStr[96]; 
  timeJSON(HTTPpayloadStr, sizeof(HTTPpayloadStr));
  httpServer.send(200, "text/json", HTTPpayloadStr);
  if (mySettings.debuglevel == 3) { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("HTTP: time request received. Sent: %u"), strlen(HTTPpayloadStr)); R_printSerialTelnetLogln(tmpStr); }
  yieldTime += yieldOS(); 
}

// { "date": { "day": 26, "month": 06, "year": 2022}}
void handleDate() {
  char HTTPpayloadStr[64]; 
  dateJSON(HTTPpayloadStr, sizeof(HTTPpayloadStr));
  httpServer.send(200, "text/json", HTTPpayloadStr);
  if (mySettings.debuglevel == 3) { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("HTTP: date request received. Sent: %u"), strlen(HTTPpayloadStr)); R_printSerialTelnetLogln(tmpStr); }
  yieldTime += yieldOS(); 
}

// {"hostname":"12345678901234567890123456789012"}
void handleHostname() {
  char HTTPpayloadStr[64];
  hostnameJSON(HTTPpayloadStr, sizeof(HTTPpayloadStr));
  httpServer.send(200, "text/json", HTTPpayloadStr);
  if (mySettings.debuglevel == 3) { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("HTTP: hostname request received. Sent: %u"), strlen(HTTPpayloadStr)); R_printSerialTelnetLogln(tmpStr); }
  yieldTime += yieldOS(); 
}

// { "ip": "123.123.123.123"}
void handleIP() {
  char HTTPpayloadStr[32];
  ipJSON(HTTPpayloadStr, sizeof(HTTPpayloadStr));
  httpServer.send(200, "text/json", HTTPpayloadStr);
  if (mySettings.debuglevel == 3) { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("HTTP: time request received. Sent: %u"), strlen(HTTPpayloadStr)); R_printSerialTelnetLogln(tmpStr); }
  yieldTime += yieldOS(); 
}

// { "bme280": { "avail": false, "p": 1234, "pavg": 1234.445, "rH": -1.0, "aH": -1.0, "T": -35.0, "dp_airquality": "1234567890123456", "rH_airquality": "1234567890123456", "T_airquality": "1234567890123456"}}
void handleBME280() {
  char HTTPpayloadStr[224]; 
  bme280JSON(HTTPpayloadStr, sizeof(HTTPpayloadStr));
  httpServer.send(200, "text/json", HTTPpayloadStr);
  if (mySettings.debuglevel == 3) { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("HTTP: BME280 request received. Sent: %u"), strlen(HTTPpayloadStr)); R_printSerialTelnetLogln(tmpStr); }
  yieldTime += yieldOS(); 
}

// { "bme680": { "avail": false, "p": 1234.0, "pavg": 1234.0, "rH":100.0, "aH": 123.0, "T": 123.0, "resistance": 123123, "dp_airquality": "1234567890123456", "rH_airquality": "1234567890123456", "resistance_airquality": "1234567890123456","T_airquality": "1234567890123456"}}
void handleBME680() {
  char HTTPpayloadStr[288]; 
  bme680JSON(HTTPpayloadStr, sizeof(HTTPpayloadStr));
  httpServer.send(200, "text/json", HTTPpayloadStr);
  if (mySettings.debuglevel == 3) { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("HTTP: BME680 request received. Sent: %u"), strlen(HTTPpayloadStr)); R_printSerialTelnetLogln(tmpStr); }
  yieldTime += yieldOS(); 
}

// { "ccs811": { "avail": false, "eCO2": 123456, "tVOC": 123456, "eCO2_airquality": "1234567890123456", "tVOC_airquality": "1234567890123456"}}
void handleCCS811() {
  char HTTPpayloadStr[160];
  ccs811JSON(HTTPpayloadStr, sizeof(HTTPpayloadStr));
  httpServer.send(200, "text/json", HTTPpayloadStr);
  if (mySettings.debuglevel == 3) { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("HTTP: CCS811 request received. Sent: %u"), strlen(HTTPpayloadStr)); R_printSerialTelnetLogln(tmpStr); }
  yieldTime += yieldOS(); 
}

// { "scd30": { "avail": false, "CO2": 0, "rH": -1.0, "aH": 1234.0, "T": -999.0, "CO2_airquality": "1234567890123456", "rH_airquality": "1234567890123456", "T_airquality": "1234567890123456"}}
void handleSCD30() {
  char HTTPpayloadStr[224];  
  scd30JSON(HTTPpayloadStr, sizeof(HTTPpayloadStr));
  httpServer.send(200, "text/json", HTTPpayloadStr);
  if (mySettings.debuglevel == 3) { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("HTTP: SCD30 request received. Sent: %u"), strlen(HTTPpayloadStr)); R_printSerialTelnetLogln(tmpStr); }
  yieldTime += yieldOS(); 
}

// { "sgp30": { "avail": true, "eCO2": 123456, "tVOC": 123456, "eCO2_airquality": "1234567890123456", "tVOC_airquality": "1234567890123456"}}
void handleSGP30() {
  char HTTPpayloadStr[160];   
  sgp30JSON(HTTPpayloadStr, sizeof(HTTPpayloadStr));
  httpServer.send(200, "text/json", HTTPpayloadStr);
  if (mySettings.debuglevel == 3) { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("HTTP: SGP30 request received. Sent: %u"), strlen(HTTPpayloadStr)); R_printSerialTelnetLogln(tmpStr); }
  yieldTime += yieldOS(); 
}

// { "sps30": { "avail": false, "PM1": -100.0, "PM2": -100.0, "PM4": -100.0, "PM10": -100.0, "nPM0": -100.0, "nPM1": -100.0, "nPM2": -100.0, "nPM4": -100.0, "nPM10": -1000.0, "PartSize": -1000.0, "PM2_airquality": "1234567890123456", "PM10_airquality": "1234567890123456"}}
void handleSPS30() {
  char HTTPpayloadStr[288];  
  sps30JSON(HTTPpayloadStr, sizeof(HTTPpayloadStr));
  httpServer.send(200, "text/json", HTTPpayloadStr);
  if (mySettings.debuglevel == 3) { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("HTTP: SPS30 request received. Sent: %u"), strlen(HTTPpayloadStr)); R_printSerialTelnetLogln(tmpStr); }
  yieldTime += yieldOS(); 
}

// { "mlx": { "avail": false, "To": 123456, "Ta": 123456, "fever": "1234567890123456", "T_airquality": "1234567890123456"} }
void handleMLX() {
  char HTTPpayloadStr[152]; 
  mlxJSON(HTTPpayloadStr, sizeof(HTTPpayloadStr));
  httpServer.send(200, "text/json", HTTPpayloadStr);
  if (mySettings.debuglevel == 3) { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("HTTP: MLX request received. Sent: %u"), strlen(HTTPpayloadStr)); R_printSerialTelnetLogln(tmpStr); }
  yieldTime += yieldOS(); 
}


// 12345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678901234567890123456789012345678
// { "max30": { "avail": false, "HR": 123456, "O2Sat": 123456, "MAX_quality": "1234567890123456"} }
void handleMAX30() {
  char HTTPpayloadStr[128];
  max30JSON(HTTPpayloadStr, sizeof(HTTPpayloadStr));
  httpServer.send(200, "text/json", HTTPpayloadStr); //
  if (mySettings.debuglevel == 3) { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("HTTP: MAX30 request received. Sent: %u"), strlen(HTTPpayloadStr)); R_printSerialTelnetLogln(tmpStr); }
  yieldTime += yieldOS(); 
  yieldTime += yieldOS(); 
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
bool handleFileRead(String filePath) { 
  if (mySettings.debuglevel == 3) { R_printSerialTelnetLog(F("handleFileRead: ")); printSerialTelnetLog(filePath); }
  if (filePath.endsWith("/")) filePath += "index.htm";                // If a folder is requested, send the index file
  String contentType = getContentType(filePath);                      // Get the MIME type
  String filePathWithGz = filePath + ".gz";                           // Also check for compressed version
  if (LittleFS.exists(filePathWithGz) || LittleFS.exists(filePath)) { // If either file exists
    if(LittleFS.exists(filePathWithGz)) {filePath = filePathWithGz;}  // If there's a compressed version available, use the compressed version
    File file = LittleFS.open(filePath, "r");                         // Open it
    size_t sent = httpServer.streamFile(file, contentType);           // And send it to the client
    if (sent != file.size()) { if (mySettings.debuglevel > 0) { snprintf_P(tmpStr, sizeof(tmpStr), PSTR("HTTP: Error sent less data than expected: %lu %lu"), sent, file.size()); R_printSerialTelnetLogln(tmpStr); } }
    file.close();                                                     // Then close the file again
    yieldTime += yieldOS(); 
    return true;
  } else {
    if (mySettings.debuglevel > 0) { 
      R_printSerialTelnetLog(F("HTTP: File Not Found: " ));
        printSerialTelnetLogln(filePath); 
    }
    yieldTime += yieldOS(); 
    return false;                                                     // If the file doesn't exist, return false
  }
}

void handleFileUpload() { // upload a new file to the SPIFFS
  HTTPUpload& upload = httpServer.upload();
  String upfilePath;
  if (upload.status == UPLOAD_FILE_START) {
    upfilePath = upload.filename;
    if (!upfilePath.startsWith("/")) upfilePath = "/" + upfilePath;
    if (!upfilePath.endsWith(".gz")) {                                // The file server always prefers a compressed version of a file
      String upfilePathWithGz = upfilePath + ".gz";                   // So if an uploaded file is not compressed, the existing compressed
      if (LittleFS.exists(upfilePathWithGz))                          // version of that file must be deleted (if it exists)
        LittleFS.remove(upfilePathWithGz);
    }

    if (mySettings.debuglevel == 3) {
      R_printSerialTelnetLog(F("handleFileUpload Name: " ));
      printSerialTelnetLogln(upfilePath); 
      yieldTime += yieldOS(); 
    }
    
    uploadFile = LittleFS.open(upfilePath, "w");                      // Open the file for writing in SPIFFS (create if it doesn't exist)
    upfilePath = String();
    yieldTime += yieldOS(); 
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (uploadFile) {
      uploadFile.write(upload.buf, upload.currentSize);               // Write the received bytes to the file
      yieldTime += yieldOS(); 
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (uploadFile) {                                                 // If the file was successfully created
      uploadFile.close();                                             // Close the file again
      if (mySettings.debuglevel == 3) {
        snprintf_P(tmpStr, sizeof(tmpStr), PSTR("handleFileUpload Size: %lu"), upload.totalSize); 
        R_printSerialTelnetLogln(tmpStr);
      }
      httpServer.sendHeader("Location", "/success.htm");              // Redirect the client to the success page
      httpServer.send(303);
    } else {
      httpServer.send(500, "text/plain", "500: couldn't create file");
    }
    yieldTime += yieldOS(); 
  }
}
