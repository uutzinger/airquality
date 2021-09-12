/******************************************************************************************************/
// HTTP WebServer
/******************************************************************************************************/
#include <ESP8266WebServer.h>

// #define intervalHTTP      100                              // NOT USER, NO LOOP DELAY, We check for HTTP requests every 0.1 seconds
unsigned long lastHTTP;									   // last time we checked for http requests
volatile WiFiStates stateHTTP       = IS_WAITING;          // keeping track of webserver state

void updateHTTP(void);
void updateHTTPUpdater(void);
void handleRoot(void);
void handleBME280(void);
void handleBME680(void);
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

String getContentType(String filename); // convert the file extension to the MIME type
bool streamFile(String path);           // send the right file to the client (if it exists)
