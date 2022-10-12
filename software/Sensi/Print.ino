/******************************************************************************************************/
// Printing functions
/******************************************************************************************************/
#include "src/Config.h"
#include "src/Sensi.h"
#include "src/Print.h"
#include "src/Telnet.h"

// External Variables
extern Settings      mySettings;       // Config
extern bool          telnetConnected;
extern ESPTelnet     Telnet;
extern File          logFile;
extern tm           *localTime;

/******************************************************************************************************/
// Printing Options for Serial, Telnet, Logfile
// Print to one of the 3 devices
// Print to Serial and Telnet
// Print to all of the devices
// Print with /r/n appended
// Just print /r/n
// Print strings and char*
// Printing to log file adds time stamp
/******************************************************************************************************/

void printSerial(char* str) {
  if ( mySettings.useSerial ) { Serial.print(str); }
}

void printSerial(String str) {
  if ( mySettings.useSerial ) { Serial.print(str); }
}

void printSerialln(char* str) {
  if ( mySettings.useSerial ) { Serial.print(str); Serial.print("\r\n"); }
}

void printSerialln() {
  if ( mySettings.useSerial ) { Serial.print("\r\n"); }
}

void printSerialln(String str) {
  if ( mySettings.useSerial ) { Serial.print(str); Serial.print("\r\n"); }
}

void printTelnet(char* str) {
  if ( mySettings.useTelnet && telnetConnected ) { Telnet.print(str); } 
}

void printTelnet(String str) {
  if ( mySettings.useTelnet && telnetConnected ) { Telnet.print(str); } 
}

void printTelnetln(char* str) {
  if ( mySettings.useTelnet && telnetConnected ) { Telnet.print(str); Telnet.print("\r\n"); } 
}

void printTelnetln(String str) {
  if ( mySettings.useTelnet && telnetConnected ) { Telnet.print(str); Telnet.print("\r\n"); } 
}

void printTelnetln() {
  if ( mySettings.useTelnet && telnetConnected ) { Telnet.print("\r\n"); } 
}

void printLog(char* str) {
  if ( mySettings.useLog ) { 
    if ( !logFile ) { logFile = LittleFS.open("/Sensi.log", "a"); }  // we close log file regularly in main loop
    if (  logFile ) { 
      char buf[24];
      snprintf_P(buf,sizeof(buf),PSTR("%2d.%2d.%2d %2d:%2d:%2d "), 
              (localTime->tm_mon+1), 
               localTime->tm_mday,
              (localTime->tm_year%100),
               localTime->tm_hour, 
               localTime->tm_min,    
               localTime->tm_sec);
      logFile.print(buf); 
      logFile.print(str); }
  }
}

void printLogln(char* str) {
  if ( mySettings.useLog ) { 
    if ( !logFile ) { logFile = LittleFS.open("/Sensi.log", "a"); }  // we close log file regularly in main loop
    if (  logFile ) { 
      char buf[24];
      snprintf_P(buf,sizeof(buf),PSTR("%2d.%2d.%2d %2d:%2d:%2d "), 
              (localTime->tm_mon+1), 
               localTime->tm_mday,
              (localTime->tm_year%100),
               localTime->tm_hour, 
               localTime->tm_min,    
               localTime->tm_sec);
      logFile.print(buf); 
      logFile.print(str); 
      logFile.print("\r\n"); }
  }
}

void printLog(String str) {
  if ( mySettings.useLog ) { 
    if ( !logFile ) { logFile = LittleFS.open("/Sensi.log", "a"); }  // we close log file regularly in main loop
    if (  logFile ) { 
      char buf[24];
      snprintf_P(buf,sizeof(buf),PSTR("%2d.%2d.%2d %2d:%2d:%2d "), 
              (localTime->tm_mon+1), 
               localTime->tm_mday,
              (localTime->tm_year%100),
               localTime->tm_hour, 
               localTime->tm_min,    
               localTime->tm_sec);
      logFile.print(buf); 
      logFile.print(str); }
  }
}

void printLogln(String str) {
  if ( mySettings.useLog ) { 
    if ( !logFile ) { logFile = LittleFS.open("/Sensi.log", "a"); }  // we close log file regularly in main loop
    if (  logFile ) { 
      char buf[24];
      snprintf_P(buf,sizeof(buf),PSTR("%2d.%2d.%2d %2d:%2d:%2d "), 
              (localTime->tm_mon+1), 
               localTime->tm_mday,
              (localTime->tm_year%100),
               localTime->tm_hour, 
               localTime->tm_min,    
               localTime->tm_sec);
      logFile.print(buf); 
      logFile.print(str); 
      logFile.print("\r\n"); 
    }
  }
}

void printLogln() {
  if ( mySettings.useLog ) { 
    if ( !logFile ) { logFile = LittleFS.open("/Sensi.log", "a"); }  // we close log file regularly in main loop
    if (  logFile ) { logFile.print("\r\n"); }                       // only if we were able to open logfile
  }
}

void printSerialTelnet(char* str) {
  printTelnet(str);
  printSerial(str);
}

void printSerialTelnet(String str) {
  printTelnet(str);
  printSerial(str);
}

void printSerialTelnetln(char* str) {
  printTelnetln(str);
  printSerialln(str);
}

void printSerialTelnetln(String str) {
  printTelnetln(str);
  printSerialln(str);
}


void printSerialTelnetLog(char* str) {
  printTelnet(str);
  printLog(str);
  printSerial(str);
}

void printSerialTelnetLog(String str) {
  printTelnet(str);
  printLog(str);
  printSerial(str);
}

void printSerialTelnetLogln(char* str) {
  printTelnetln(str);
  printLogln(str);
  printSerialln(str);
}

void printSerialTelnetLogln(String str) {
  printTelnetln(str);
  printLogln(str);
  printSerialln(str);
}

void printSerialTelnetLogln() {
  printTelnetln();
  printLogln();
  printSerialln();
}

void printSerialLog(char* str) {
  printLog(str);
  printSerial(str);
}

void printSerialLog(String str) {
  printLog(str);
  printSerial(str);
}

void printSerialLogln(char* str) {
  printLogln(str);
  printSerialln(str);
}

void printSerialLogln(String str) {
  printLogln(str);
  printSerialln(str);
}

void printSerialLogln() {
  printLogln();
  printSerialln();
}
