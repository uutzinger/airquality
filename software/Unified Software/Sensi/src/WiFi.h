/******************************************************************************************************/
// WiFi, mDNSm, OTA, NTP
/******************************************************************************************************/
#include <ESP8266WiFi.h>                                   // Wifi Module
#include <ESP8266WiFiMulti.h>                              // Automatically connect and reconnect to several SSIDs
#include <ArduinoOTA.h>                                    // Prepare for over the air programming
#include <ESP8266mDNS.h>                                   // announce services to Domain Name Server
#include <WiFiUdp.h>                                       // UDP protocol (for network time)

#define intervalWiFi      1000                             // We check if WiFi is connected every 1 sec, if disconnected we attempted reconnection every 1 sec
//#define intervalOTA       1000                             // NOT USED, NO THROTTELLING, over the airprogramming checking interval
#define connectTimeOut    5000                             // WiFi Connection attempt time out
//
char hostName[16] = {0};
enum WiFiStates{START_UP = 0, CHECK_CONNECTION, IS_SCANNING, IS_CONNECTING, IS_WAITING};
bool wifi_avail = false;                                   // do we have wifi?
bool wifi_connected = false;                               // is wifi connected to known ssid?
bool otaInProgress = false;                                // stop handling any other updates when OTA is inprogress
unsigned long lastWiFi;                                    // last time we checked if we are connected
unsigned long lastMDNS;                                    // last time we checked if mqtt is connected
unsigned long lastOTA;                                     // last time we checked if mqtt is connected

unsigned long prevActualTime = 0;//
volatile WiFiStates stateWiFi       = START_UP;            // keeping track of WiFi state
volatile WiFiStates stateMDNS       = IS_WAITING;          // keeping track of MDNS state
volatile WiFiStates stateOTA        = IS_WAITING;          // keeping track of over the air programming state
//
void initializeWiFi(void);
void updateWiFi(void);
void updateOTA(void);
void onstartOTA();
void onendOTA();
void onprogressOTA(unsigned int progress, unsigned int total);
void onerrorOTA(ota_error_t error);
void updateMDNS(void);

//https://arduino-esp8266.readthedocs.io/en/latest/PROGMEM.html
const char mStarting[] PROGMEM      = {"Starting up                "};
const char mMonitoring[] PROGMEM    = {"Monitoring connection      "};
const char mScanning[] PROGMEM      = {"Scanning for known network "};
const char mConnecting[] PROGMEM    = {"Connecting                 "};
const char mWaiting[] PROGMEM       = {"Waiting                    "};
const char mNotEnabled[] PROGMEM    = {"Not Enabled                "};
