/******************************************************************************************************/
// Printing functions
/******************************************************************************************************/
#ifndef PRINT_H_
#define PRINT_H_

/******************************************************************************************************/
// DEBUG Print Configurations
/******************************************************************************************************/
#if defined DEBUG
  #define D_printSerial(X)            printSerial(X)                                      // debug output
  #define D_printSerialln(X)          printSerialln(X)                                    // debug output
  #define D_printSerialTelnet(X)      printSerialTelnet(X)                                // debug output to serial and telnet
  #define D_printSerialTelnetln(X)    printSerialTelnetln(X)                              // debug output to serial and telnet
  #define R_printSerial(X)            printSerialln(); printSerial(X)                     // regular output, with /r/n prepended
  #define R_printSerialln(X)          printSerialln(); printSerialln(X)                   // regular output, with /r/n appended and prepended
  #define R_printSerialLog(X)         printSerialLogln(); printSerialLog(X)               // regular output, with /r/n prepended
  #define R_printSerialLogln(X)       printSerialLogln(); printSerialLogln(X)             // regular output, with /r/n appended and prepended
  #define R_printSerialTelnetLog(X)   printSerialTelnetLogln(); printSerialTelnetLog(X)   // regular output, with /r/n prepended
  #define R_printSerialTelnetLogln(X) printSerialTelnetLogln(); printSerialTelnetLogln(X) // regular output with /r/n appended
#else
  #define D_printSerial(X)                                                                // no debug output
  #define D_printSerialln(X)                                                              // no debug output
  #define D_printSerialTelnet(X)                                                          // no debug output
  #define D_printSerialTelnetln(X)                                                        // no debug output
  #define R_printSerial(X)            printSerial(X)                                      // regular output
  #define R_printSerialln(X)          printSerialln(X)                                    // regular output with /r/n appended
  #define R_printSerialLog(X)         printSerialLog(X)                                   // regular output
  #define R_printSerialLogln(X)       printSerialLogln(X)                                 // regular output with /r/n appended
  #define R_printSerialTelnetLog(X)   printSerialTelnetLog(X)                             // regular output
  #define R_printSerialTelnetLogln(X) printSerialTelnetLogln(X)                           // regular output with /r/n appended
#endif

void printSerial(char* str);
void printSerial(String str);
void printSerialln(char* str);
void printSerialln(String str);
void printSerialln();
void printTelnet(char* str);
void printTelnet(String str);
void printTelnetln(char* str);
void printTelnetln(String str);
void printTelnetln();
void printLog(char* str);
void printLog(String str);
void printLog(char* str);
void printLogln(String str);
void printLogln();
void printSerialTelnet(char* str);                              // Serial.print to serial and telnet
void printSerialTelnetln(char* str);                            // Serial.println to serial and telnet
void printSerialTelnet(String str);                             // Serial.print to serial and telnet
void printSerialTelnetln(String str);                           // Serial.println to serial and telnet
void printSerialTelnetLog(char* str);                           // Serial.print to serial, telnet and logfile
void printSerialTelnetLogln(char* str);                         // Serial.println to serial, telnet and logfile
void printSerialTelnetLog(String str);                          // Serial.print to serial, telnet and logfile
void printSerialTelnetLogln(String str);                        // Serial.println to serial, telnet and logfile
void printSerialTelnetLogln();                                  // Serial.println to serial, telnet and logfile
void printSerialLog(char* str);                                 // Serial.print to serial and logfile
void printSerialLogln(char* str);                               // Serial.println to serial and logfile
void printSerialLog(String str);                                // Serial.print to serial and logfile
void printSerialLogln(String str);                              // Serial.println to serial and logfile
void printSerialLogln();                                        // Serial.println to serial and logfile

#endif
