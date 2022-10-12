/******************************************************************************************************/
// HTTP WebServer
/******************************************************************************************************/
#ifndef HTTP_H_
#define HTTP_H_

#include <ESP8266WebServer.h>

void initializeHTTP();
void initializeHTTPUpdater();
void updateHTTP(void);
void updateHTTPUpdater(void);

void handleRoot(void);
void handleBME280(void);
void handleBME68x(void);
void handleCCS811(void);
void handleSCD30(void);
void handleSGP30(void);
void handleSPS30(void);
void handleMLX(void);
void handleMAX30(void);
void handleHostname(void);
void handleIP(void);
void handleDate(void);
void handleTime(void);
void handleAnimation(void);
void handleNotFound(void);
void handleSystem(void);
void handleEdit(void);
void handleConfig(void);
void handleFileUpload(void);
void handleWeather(void);

String getContentType(String filename);
bool handleFileRead(String filePath); 
bool streamFile(String path);           // send the right file to the client (if it exists)

#endif
