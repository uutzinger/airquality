/******************************************************************************************************/
// HTTP Updater
/******************************************************************************************************/
#include <ESP8266HTTPUpdateServer.h>                       // Allows code update via web

// #define intervalHTTPUpdater 1000
//
File uploadFile;

const char* update_path = "/firmware";
const char* update_username = "admin";
const char* update_password = "w1ldc8ts";
unsigned long lastHTTPUpdater;	    					   // last time we checked for http update requests
volatile WiFiStates stateHTTPUpdater= IS_WAITING;          // keeping track of webserver state
void updateHTTPUpdater(void);
