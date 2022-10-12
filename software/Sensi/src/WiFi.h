/******************************************************************************************************/
// WiFi, mDNSm, OTA, NTP
/******************************************************************************************************/
#ifndef WiFi_H_
#define WiFi_H_

#include <ESP8266WiFi.h>                                   // Wifi Module
#include <ESP8266WiFiMulti.h>                              // Automatically connect and reconnect to several SSIDs
#include <ArduinoOTA.h>                                    // Prepare for over the air programming
#include <ESP8266mDNS.h>                                   // announce services to Domain Name Server
#include <WiFiUdp.h>                                       // UDP protocol (for network time)

#define intervalWiFi      1000                             // We check if WiFi is connected every 1 sec, if disconnected we attempted reconnection every 1 sec
#define connectTimeOut    5000                             // WiFi Connection attempt time out

#define SHORTHOSTNAME                                      // mqtt main topic, or eps8266-chipid
//#undef  SHORTHOSTNAME 

enum WiFiStates{START_UP = 0, CHECK_CONNECTION, IS_SCANNING, IS_CONNECTING, IS_WAITING, WF_ERROR};

//
void initializeWiFi(void);
void initializeOTA(void);
void initializeMDNS(void);

void updateWiFi(void);
void updateOTA(void);
void updateMDNS(void);

void onstartOTA(void);
void onendOTA(void);
void onprogressOTA(unsigned int progress, unsigned int total);
void onerrorOTA(ota_error_t error);

void ipJSON(char *payload, size_t len);                         // provide ip
void hostnameJSON(char *payload, size_t len);                   // provide hostname
void wifiJSON(char *payLoad, size_t len);                       // provide wifi stats

#endif
